/**
 * @file WindowsTTY.cpp
 * @brief Windows ConPTY PTY implementation: duplex overlapped pipe, process creation, I/O, resize.
 *
 * This file implements the WindowsTTY interface using the Windows Pseudo Console
 * (ConPTY) API with a single duplex overlapped named pipe, following the I/O
 * model used by Microsoft Terminal's ConptyConnection.
 *
 * ### Architecture overview
 *
 * ```
 *  WindowsTTY (parent)                  ConPTY                  shell (child)
 *  ─────────────────────────────────    ──────────────────────  ──────────────
 *  pipe (server, DUPLEX, overlapped) ◄──► client (hInput)  ──► child stdin
 *                                    ◄──► client (hOutput) ◄── child stdout/stderr
 * ```
 *
 * ### Pipe topology
 * A single named pipe is created with `PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED`.
 * The client end is opened with `CreateFileW` and passed for **both** `hInput`
 * and `hOutput` to `CreatePseudoConsole`.  After `CreateProcessW` the client
 * handle is closed — ConPTY owns it internally.  The parent retains only the
 * server handle (`pipe`) for all I/O.
 *
 * ### Overlapped I/O model
 * - `waitForData()` issues `ReadFile` with `readOverlapped`, waits on `readEvent`,
 *   and stores the result in `readBuffer` / `readBufferBytes`.
 * - `read()` copies from `readBuffer` to the caller's buffer.
 * - `write()` issues `WriteFile` with `writeOverlapped`, waits on `writeEvent`.
 *
 * ### Handle ownership after open()
 *
 * | Handle           | Owner after open()  | Purpose                              |
 * |------------------|---------------------|--------------------------------------|
 * | `pipe`           | WindowsTTY (parent) | Duplex overlapped I/O with ConPTY    |
 * | `client`         | Closed              | Passed to CreatePseudoConsole        |
 * | `pseudoConsole`  | WindowsTTY          | ConPTY handle for resize / close     |
 * | `process`        | WindowsTTY          | Child process handle for exit checks |
 * | `readEvent`      | WindowsTTY          | Manual-reset event for read OVERLAPPED  |
 * | `writeEvent`     | WindowsTTY          | Manual-reset event for write OVERLAPPED |
 *
 * ### Shutdown sequence
 * 1. `signalThreadShouldExit()` — tell reader thread to stop.
 * 2. `ClosePseudoConsole()` — sends CTRL_CLOSE_EVENT to clients, then breaks
 *    the pipe.  **Must** happen while the reader thread is still alive.
 * 3. `stopThread (5000)` — wait for reader thread to exit.
 * 4. Clean up process handle (TerminateProcess as last resort).
 * 5. Close pipe and event handles.
 *
 * @see WindowsTTY.h  Class declaration and member documentation
 * @see TTY.cpp       Shared reader thread loop
 * @see https://github.com/microsoft/terminal/blob/main/src/cascadia/TerminalConnection/ConptyConnection.cpp
 * @see https://docs.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session
 */

#include "WindowsTTY.h"

#ifdef JUCE_WINDOWS

#include <BinaryData.h>

#pragma comment(lib, "kernel32.lib")

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

// =============================================================================
// ConPTY function pointer table
// =============================================================================

/**
 * @brief Holds function pointers for the three ConPTY API functions.
 *
 * All three must come from the same source (either the sideloaded conpty.dll
 * or the inbox kernel32.dll) so that the HPCON handle is valid across calls.
 *
 * The flag PSEUDOCONSOLE_WIN32_INPUT_MODE (0x4) is passed to CreatePseudoConsole
 * when using the sideloaded conpty.dll, enabling mouse tracking and alternate
 * screen detection that the inbox conhost.exe on Windows 10 22H2 does not support.
 */
struct ConPtyFuncs
{
    using CreateFunc = HRESULT (WINAPI*) (COORD, HANDLE, HANDLE, DWORD, HPCON*);
    using ResizeFunc = HRESULT (WINAPI*) (HPCON, COORD);
    using CloseFunc  = void    (WINAPI*) (HPCON);

    CreateFunc create { nullptr };
    ResizeFunc resize { nullptr };
    CloseFunc  close  { nullptr };

    bool isValid() const noexcept { return create != nullptr and resize != nullptr and close != nullptr; }
};

// =============================================================================
// Internal helpers
// =============================================================================

/**
 * @brief Safely close a Win32 HANDLE and reset it to INVALID_HANDLE_VALUE.
 *
 * No-op if the handle is already INVALID_HANDLE_VALUE or nullptr.
 *
 * @param h  Reference to the handle to close.
 *
 * @note Called from `WindowsTTY::close()` on the message thread.
 */
static void safeCloseHandle (HANDLE& h) noexcept
{
    if (h != INVALID_HANDLE_VALUE and h != nullptr)
    {
        CloseHandle (h);
        h = INVALID_HANDLE_VALUE;
    }
}

// =============================================================================

/**
 * @brief Extract conpty.dll and OpenConsole.exe from BinaryData to the config directory.
 *
 * Target directory: `~/.config/end/conpty/`
 *
 * Each file is written only if it does not already exist or if the on-disk size
 * differs from the embedded size (simple update check — avoids unnecessary writes
 * on every launch while still picking up updated binaries after an app update).
 *
 * OpenConsole.exe must reside in the same directory as conpty.dll so that
 * conpty.dll can locate it at runtime via a relative path search.
 *
 * @return  The path to the extracted conpty.dll, or an empty File on failure.
 *
 * @note Called once per process from `loadConPtyFuncs()`.
 */
static juce::File extractConPtyBinaries() noexcept
{
    const juce::File conptyDir
    {
        juce::File::getSpecialLocation (juce::File::userHomeDirectory)
            .getChildFile (".config/end/conpty")
    };

    if (not conptyDir.exists())
        conptyDir.createDirectory();

    struct BinaryEntry
    {
        const char* name;
        const char* data;
        int         size;
    };

    const std::array<BinaryEntry, 2> entries
    {{
        { "conpty.dll",     BinaryData::conpty_dll,     BinaryData::conpty_dllSize     },
        { "OpenConsole.exe", BinaryData::OpenConsole_exe, BinaryData::OpenConsole_exeSize }
    }};

    bool allOk { true };

    for (const auto& entry : entries)
    {
        const juce::File dest { conptyDir.getChildFile (entry.name) };
        const bool needsWrite { not dest.existsAsFile()
                                or dest.getSize() != static_cast<juce::int64> (entry.size) };

        if (needsWrite)
        {
            juce::FileOutputStream stream { dest };

            if (stream.openedOk())
            {
                stream.write (entry.data, static_cast<size_t> (entry.size));
            }
            else
            {
                allOk = false;
            }
        }
    }

    const juce::File dllPath { conptyDir.getChildFile ("conpty.dll") };

    return (allOk and dllPath.existsAsFile()) ? dllPath : juce::File {};
}

// isWindows10() — defined in jreng_platform.h, included via WindowsTTY.h → JuceHeader.h → windows.h
// The header is safe to include here because windows.h is already pulled in by WindowsTTY.h.
#include "../../../modules/jreng_core/utilities/jreng_platform.h"

// =============================================================================

/**
 * @brief Load the three ConPTY function pointers, preferring the sideloaded conpty.dll.
 *
 * Strategy (mirrors wezterm's approach):
 * 1. On Windows 10 only: extract conpty.dll + OpenConsole.exe from BinaryData
 *    to ~/.config/end/conpty/ and attempt to load them.
 * 2. Try `LoadLibraryW` on the extracted conpty.dll.
 * 3. If loaded, resolve all three functions from it.
 * 4. If any step fails, or if running on Windows 11+, fall back to kernel32.dll
 *    (inbox ConPTY).
 *
 * The result is cached in a static local — loaded once per process lifetime.
 *
 * @return  A fully populated `ConPtyFuncs` (sideloaded or inbox).
 *
 * @note Called from `createPseudoConsole()` on the message thread (first call only).
 */
static const ConPtyFuncs& loadConPtyFuncs() noexcept
{
    static const ConPtyFuncs funcs { []() noexcept -> ConPtyFuncs
    {
        ConPtyFuncs result {};

        // --- Sideloaded conpty.dll (all Windows versions) ---
        // The inbox kernel32 ConPTY is broken on both Windows 10 (missing
        // DECSET passthrough) and Windows 11 (sends STATUS_CONTROL_C_EXIT to
        // child processes immediately after spawn).  The sideloaded DLL from
        // Microsoft Terminal works correctly on both.
        const juce::File dllPath { extractConPtyBinaries() };

        if (dllPath.existsAsFile())
        {
            const HMODULE conptyModule { LoadLibraryW (dllPath.getFullPathName().toWideCharPointer()) };

            if (conptyModule != nullptr)
            {
                result.create = reinterpret_cast<ConPtyFuncs::CreateFunc> (
                    GetProcAddress (conptyModule, "CreatePseudoConsole"));
                result.resize = reinterpret_cast<ConPtyFuncs::ResizeFunc> (
                    GetProcAddress (conptyModule, "ResizePseudoConsole"));
                result.close  = reinterpret_cast<ConPtyFuncs::CloseFunc> (
                    GetProcAddress (conptyModule, "ClosePseudoConsole"));

                if (not result.isValid())
                {
                    // Partial load — reset and fall through to kernel32.
                    result = {};
                    FreeLibrary (conptyModule);
                }
            }
        }

        // --- Fallback: inbox kernel32.dll ---
        if (not result.isValid())
        {
            const HMODULE kernel32 { GetModuleHandleW (L"kernel32.dll") };

            if (kernel32 != nullptr)
            {
                result.create = reinterpret_cast<ConPtyFuncs::CreateFunc> (
                    GetProcAddress (kernel32, "CreatePseudoConsole"));
                result.resize = reinterpret_cast<ConPtyFuncs::ResizeFunc> (
                    GetProcAddress (kernel32, "ResizePseudoConsole"));
                result.close  = reinterpret_cast<ConPtyFuncs::CloseFunc> (
                    GetProcAddress (kernel32, "ClosePseudoConsole"));
            }
        }

        return result;
    }() };

    return funcs;
}

// =============================================================================
// NT API type definitions for NtCreateNamedPipeFile / NtCreateFile
// =============================================================================

// NTSTATUS and NT_SUCCESS are not always available without <winternl.h>.
// Define them locally so we have no extra header dependency.
#ifndef NT_SUCCESS
using NTSTATUS = LONG;
#define NT_SUCCESS(status) (static_cast<NTSTATUS> (status) >= 0)
#endif

// UNICODE_STRING — the NT string descriptor used for object names.
struct UnicodeString
{
    USHORT Length        { 0 };
    USHORT MaximumLength { 0 };
    PWSTR  Buffer        { nullptr };
};

// IO_STATUS_BLOCK — receives the result of an NT I/O call.
struct IoStatusBlock
{
    union
    {
        NTSTATUS Status  { 0 };
        PVOID    Pointer;
    };
    ULONG_PTR Information { 0 };
};

// OBJECT_ATTRIBUTES — describes the object to open or create.
struct ObjectAttributes
{
    ULONG           Length                   { sizeof (ObjectAttributes) };
    HANDLE          RootDirectory            { nullptr };
    UnicodeString*  ObjectName               { nullptr };
    ULONG           Attributes               { 0 };
    PVOID           SecurityDescriptor       { nullptr };
    PVOID           SecurityQualityOfService { nullptr };
};

// Named-pipe type / read-mode / completion-mode constants (from ntifs.h).
static constexpr ULONG filePipeByteStreamType    { 0x00000000 };
static constexpr ULONG filePipeByteStreamMode    { 0x00000000 };
static constexpr ULONG filePipeQueueOperation    { 0x00000000 };

// OBJ_CASE_INSENSITIVE attribute flag.
static constexpr ULONG objCaseInsensitive        { 0x00000040 };

// NtCreateFile / NtCreateNamedPipeFile CreateDisposition values.
static constexpr ULONG ntFileOpen                { 0x00000001 };
static constexpr ULONG ntFileCreate              { 0x00000002 };

// NtCreateFile CreateOptions flags.
static constexpr ULONG ntFileNonDirectoryFile    { 0x00000040 };
static constexpr ULONG ntFileSynchronousIoNonAlert { 0x00000020 };

// NtCreateNamedPipeFile / NtCreateFile function pointer types.
using FnNtCreateNamedPipeFile = NTSTATUS (NTAPI*) (
    PHANDLE            FileHandle,
    ULONG              DesiredAccess,
    ObjectAttributes*  ObjectAttributes,
    IoStatusBlock*     IoStatusBlock,
    ULONG              ShareAccess,
    ULONG              CreateDisposition,
    ULONG              CreateOptions,
    ULONG              NamedPipeType,
    ULONG              ReadMode,
    ULONG              CompletionMode,
    ULONG              MaximumInstances,
    ULONG              InboundQuota,
    ULONG              OutboundQuota,
    PLARGE_INTEGER     DefaultTimeout);

using FnNtCreateFile = NTSTATUS (NTAPI*) (
    PHANDLE            FileHandle,
    ACCESS_MASK        DesiredAccess,
    ObjectAttributes*  ObjectAttributes,
    IoStatusBlock*     IoStatusBlock,
    PLARGE_INTEGER     AllocationSize,
    ULONG              FileAttributes,
    ULONG              ShareAccess,
    ULONG              CreateDisposition,
    ULONG              CreateOptions,
    PVOID              EaBuffer,
    ULONG              EaLength);

// =============================================================================

/**
 * @brief Create a duplex overlapped unnamed pipe using the NT API.
 *
 * Replicates Microsoft Terminal's `CreateOverlappedPipe` from `utils.cpp`
 * exactly.  Loads `NtCreateNamedPipeFile` and `NtCreateFile` from `ntdll.dll`
 * via `GetProcAddress` (cached in statics).  A handle to `\Device\NamedPipe\`
 * is also cached in a static local so the directory is opened only once per
 * process lifetime.
 *
 * The server end is created with `CreateOptions = 0` (asynchronous /
 * overlapped).  The client end is opened relative to the server handle with
 * `FILE_NON_DIRECTORY_FILE` and an empty `UNICODE_STRING` name.  Both ends
 * receive `SYNCHRONIZE | GENERIC_READ | GENERIC_WRITE` access (duplex).
 *
 * @param[out] serverOut  Receives the server (parent) end of the pipe on success.
 * @param[out] clientOut  Receives the client (ConPTY) end of the pipe on success.
 * @return                `true` if both handles were created successfully.
 *
 * @note Called from `WindowsTTY::open()` on the message thread.
 */
static bool createDuplexOverlappedPipe (HANDLE& serverOut, HANDLE& clientOut) noexcept
{
    // Load NT function pointers once — ntdll.dll is always mapped into every
    // Windows process so GetModuleHandleW cannot fail here.
    static const FnNtCreateNamedPipeFile ntCreateNamedPipeFile
    {
        reinterpret_cast<FnNtCreateNamedPipeFile> (
            GetProcAddress (GetModuleHandleW (L"ntdll.dll"), "NtCreateNamedPipeFile"))
    };

    static const FnNtCreateFile ntCreateFile
    {
        reinterpret_cast<FnNtCreateFile> (
            GetProcAddress (GetModuleHandleW (L"ntdll.dll"), "NtCreateFile"))
    };

    bool result { false };

    if (ntCreateNamedPipeFile != nullptr and ntCreateFile != nullptr)
    {
        // Cache a handle to the pipe driver directory — opened once per process.
        static const HANDLE pipeDirectory { []() noexcept -> HANDLE
        {
            wchar_t pathBuf[] { L"\\Device\\NamedPipe\\" };
            UnicodeString path
            {
                static_cast<USHORT> (wcslen (pathBuf) * sizeof (wchar_t)),
                static_cast<USHORT> ((wcslen (pathBuf) + 1) * sizeof (wchar_t)),
                pathBuf
            };

            ObjectAttributes attrs {};
            attrs.Length     = sizeof (ObjectAttributes);
            attrs.ObjectName = &path;

            IoStatusBlock iosb {};
            HANDLE dir { INVALID_HANDLE_VALUE };

            const NTSTATUS status { reinterpret_cast<FnNtCreateFile> (
                GetProcAddress (GetModuleHandleW (L"ntdll.dll"), "NtCreateFile")) (
                    &dir,
                    SYNCHRONIZE | GENERIC_READ,
                    &attrs,
                    &iosb,
                    nullptr,
                    0,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    ntFileOpen,
                    ntFileSynchronousIoNonAlert,
                    nullptr,
                    0) };

            return NT_SUCCESS (status) ? dir : INVALID_HANDLE_VALUE;
        }() };

        if (pipeDirectory != INVALID_HANDLE_VALUE)
        {
            static constexpr DWORD bufferSize   { 128 * 1024 };
            static constexpr DWORD duplexAccess { SYNCHRONIZE | GENERIC_READ | GENERIC_WRITE };
            static constexpr DWORD duplexShare  { FILE_SHARE_READ | FILE_SHARE_WRITE };

            // 1 second timeout expressed as a negative 100-nanosecond interval.
            LARGE_INTEGER timeout {};
            timeout.QuadPart = -10'000'000'00LL;  // -1 000 000 000 * 100 ns = 1 s

            UnicodeString emptyPath {};  // Unnamed pipe — empty name.

            ObjectAttributes serverAttrs {};
            serverAttrs.Length          = sizeof (ObjectAttributes);
            serverAttrs.RootDirectory   = pipeDirectory;
            serverAttrs.ObjectName      = &emptyPath;
            serverAttrs.Attributes      = objCaseInsensitive;

            IoStatusBlock iosb {};

            HANDLE server { INVALID_HANDLE_VALUE };
            const NTSTATUS serverStatus { ntCreateNamedPipeFile (
                &server,
                duplexAccess,
                &serverAttrs,
                &iosb,
                duplexShare,
                ntFileCreate,
                0,                          // CreateOptions = 0 → overlapped (async) I/O
                filePipeByteStreamType,
                filePipeByteStreamMode,
                filePipeQueueOperation,
                1,                          // MaximumInstances
                bufferSize,                 // InboundQuota
                bufferSize,                 // OutboundQuota
                &timeout) };

            if (NT_SUCCESS (serverStatus))
            {
                // Open the client end relative to the server handle with an empty name.
                ObjectAttributes clientAttrs {};
                clientAttrs.Length        = sizeof (ObjectAttributes);
                clientAttrs.RootDirectory = server;
                clientAttrs.ObjectName    = &emptyPath;
                clientAttrs.Attributes    = objCaseInsensitive;

                HANDLE client { INVALID_HANDLE_VALUE };
                const NTSTATUS clientStatus { ntCreateFile (
                    &client,
                    duplexAccess,
                    &clientAttrs,
                    &iosb,
                    nullptr,
                    0,
                    duplexShare,
                    ntFileOpen,
                    ntFileNonDirectoryFile,   // overlapped (async) I/O — no FILE_SYNCHRONOUS_IO_NONALERT
                    nullptr,
                    0) };

                if (NT_SUCCESS (clientStatus))
                {
                    serverOut = server;
                    clientOut = client;
                    result = true;
                }
                else
                {
                    CloseHandle (server);
                }
            }
        }
    }

    return result;
}

// =============================================================================

/**
 * @brief Create the ConPTY handle using the duplex pipe client end.
 *
 * Passes the same `client` handle for both `hInput` and `hOutput` to
 * `CreatePseudoConsole`, matching the Microsoft Terminal ConptyConnection
 * topology.
 *
 * Uses the sideloaded conpty.dll when available (loaded via `loadConPtyFuncs()`),
 * falling back to the inbox kernel32.dll implementation.  The sideloaded DLL
 * supports `PSEUDOCONSOLE_WIN32_INPUT_MODE` (0x4) on Windows 10 22H2 and earlier,
 * enabling mouse tracking and alternate screen detection.
 *
 * @param[out] pseudoConsoleOut  Receives the ConPTY handle on success.
 * @param      client            Client end of the duplex pipe.
 * @param      size              Initial terminal dimensions as a `COORD`.
 * @return                       `true` if `CreatePseudoConsole` succeeded.
 *
 * @note Called from `WindowsTTY::open()` on the message thread.
 */
static bool createPseudoConsole (HPCON& pseudoConsoleOut, HANDLE client, COORD size) noexcept
{
    static constexpr DWORD pseudoconsoleWin32InputMode { 0x4 };

    const ConPtyFuncs& funcs { loadConPtyFuncs() };
    bool result { false };

    if (funcs.isValid())
    {
        const HRESULT hr { funcs.create (size, client, client, pseudoconsoleWin32InputMode, &pseudoConsoleOut) };
        result = not FAILED (hr);
    }

    return result;
}

// =============================================================================

/**
 * @brief Build a UTF-16 environment block from the parent environment plus `TERM=xterm-256color`.
 *
 * Copies all existing environment variables from the parent process, then
 * appends `TERM=xterm-256color`.  The resulting block is double-null-terminated
 * as required by `CreateProcessW` with `CREATE_UNICODE_ENVIRONMENT`.
 *
 * @param[out] envBlock     Receives the constructed environment block.
 * @param      shellEnvVars Shell integration env vars to inject after parent env.
 *
 * @note Called from `spawnProcess()` on the message thread.
 */
static void buildEnvironmentBlock (std::wstring& envBlock,
                                   const std::vector<std::pair<std::string, std::string>>& shellEnvVars) noexcept
{
    wchar_t* parentEnv { GetEnvironmentStringsW() };

    if (parentEnv != nullptr)
    {
        const wchar_t* p { parentEnv };

        while (*p != L'\0')
        {
            const size_t len { wcslen (p) + 1 };
            envBlock.append (p, len);
            p += len;
        }

        FreeEnvironmentStringsW (parentEnv);
    }

    for (const auto& [key, value] : shellEnvVars)
    {
        const juce::String entry { key + "=" + value };
        envBlock += entry.toWideCharPointer();
        envBlock += L'\0';
    }

    envBlock += L"TERM=xterm-256color";
    envBlock += L'\0';
    envBlock += L'\0';
}

// =============================================================================

/**
 * @brief Spawn the configured shell with the ConPTY attribute.
 *
 * Builds a `STARTUPINFOEXW` with a `PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE`
 * attribute so that ConPTY intercepts the child's console I/O.  After a
 * successful `CreateProcessW()` the client handle is closed — it is now owned
 * by ConPTY internally.
 *
 * @param      pseudoConsole  The ConPTY handle to attach to the child.
 * @param[in,out] client      Client pipe end — closed on success (ConPTY owns it).
 * @param[out] processOut     Receives the child process handle on success.
 * @param[out] pidOut         Receives the child process ID on success.
 * @param      shell          Shell program as a wide string.
 * @param      args           Shell arguments as a wide string (space-separated).
 * @param      workingDirectory Initial cwd for the child process.
 * @param      shellEnvVars  Shell integration env vars to inject into the child environment.
 * @return                    `true` if `CreateProcessW()` succeeded.
 *
 * @note Called from `WindowsTTY::open()` on the message thread.
 */
static bool spawnProcess (HPCON pseudoConsole, HANDLE& client, HANDLE& processOut, DWORD& pidOut,
                          const std::wstring& shell, const std::wstring& args,
                          const std::wstring& workingDirectory,
                          const std::vector<std::pair<std::string, std::string>>& shellEnvVars) noexcept
{
    size_t attrSize { 0 };
    InitializeProcThreadAttributeList (nullptr, 1, 0, &attrSize);

    STARTUPINFOEXW si {};
    si.StartupInfo.cb = sizeof (si);
    si.lpAttributeList = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST> (
        HeapAlloc (GetProcessHeap(), 0, attrSize));

    bool result { false };

    if (si.lpAttributeList != nullptr)
    {
        InitializeProcThreadAttributeList (si.lpAttributeList, 1, 0, &attrSize);

        UpdateProcThreadAttribute (si.lpAttributeList, 0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pseudoConsole,
            sizeof (HPCON), nullptr, nullptr);

        std::wstring cmd { shell };

        if (not args.empty())
        {
            cmd += L' ';
            cmd += args;
        }

        std::wstring envBlock;
        buildEnvironmentBlock (envBlock, shellEnvVars);

        const wchar_t* cwd { workingDirectory.empty() ? nullptr : workingDirectory.c_str() };

        PROCESS_INFORMATION pi {};

        const BOOL ok { CreateProcessW (
            nullptr,
            cmd.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NEW_PROCESS_GROUP | EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
            envBlock.data(),
            cwd,
            &si.StartupInfo,
            &pi) };

        DeleteProcThreadAttributeList (si.lpAttributeList);
        HeapFree (GetProcessHeap(), 0, si.lpAttributeList);

        // Close the client handle — ConPTY now owns it internally.
        safeCloseHandle (client);

        if (ok != FALSE)
        {
            CloseHandle (pi.hThread);
            processOut = pi.hProcess;
            pidOut     = pi.dwProcessId;
            result = true;
        }
    }

    return result;
}

// =============================================================================
// WindowsTTY member function implementations
// =============================================================================

/**
 * @brief Destructor — delegates to close() for full ConPTY and process cleanup.
 *
 * @note MESSAGE THREAD context.
 */
WindowsTTY::~WindowsTTY()
{
    close();
}

// =============================================================================

/**
 * @brief Open the ConPTY and spawn the shell process.
 *
 * @par Sequence
 * 1. Allocate the internal read buffer (`READ_CHUNK_SIZE` bytes).
 * 2. Create manual-reset events for read and write overlapped operations.
 * 3. `createDuplexOverlappedPipe()` — create the server and client pipe ends.
 * 4. `createPseudoConsole()` — call `CreatePseudoConsole` with the client end
 *    for both `hInput` and `hOutput`.
 * 5. `spawnProcess()` — launch the configured shell with the ConPTY attribute.
 *    The client handle is closed inside `spawnProcess()`.
 * 6. Initialise the `readOverlapped` and `writeOverlapped` structures with
 *    their respective events.
 * 7. `startThread()` — begin the TTY reader loop.
 *
 * @param cols             Initial terminal width in character columns.
 * @param rows             Initial terminal height in character rows.
 * @param shell            Shell program name or absolute path.
 * @param args             Space-separated arguments for the shell.
 * @param workingDirectory Optional initial working directory for the shell.
 * @return                 `true` on success; `false` if any Win32 call fails.
 *
 * @note MESSAGE THREAD context.
 */
bool WindowsTTY::open (int cols, int rows, const juce::String& shell,
                       const juce::String& args, const juce::String& workingDirectory)
{
    readBuffer.malloc (READ_CHUNK_SIZE);

    HANDLE rEvent { CreateEventW (nullptr, TRUE, FALSE, nullptr) };
    HANDLE wEvent { CreateEventW (nullptr, TRUE, FALSE, nullptr) };

    bool result { false };

    if (rEvent != nullptr and rEvent != INVALID_HANDLE_VALUE
        and wEvent != nullptr and wEvent != INVALID_HANDLE_VALUE)
    {
        readEvent  = rEvent;
        writeEvent = wEvent;

        HANDLE client { INVALID_HANDLE_VALUE };

        if (createDuplexOverlappedPipe (pipe, client))
        {
            const COORD size { static_cast<short> (cols), static_cast<short> (rows) };

            if (createPseudoConsole (pseudoConsole, client, size))
            {
                const std::wstring shellWide { shell.toWideCharPointer() };
                const std::wstring argsWide  { args.toWideCharPointer() };
                const std::wstring cwdWide { workingDirectory.toWideCharPointer() };

                if (spawnProcess (pseudoConsole, client, process, childPid, shellWide, argsWide, cwdWide, shellIntegrationEnv))
                {
                    readOverlapped  = {};
                    writeOverlapped = {};
                    readOverlapped.hEvent  = readEvent;
                    writeOverlapped.hEvent = writeEvent;

                    startThread();
                    result = true;
                }
            }

            // If spawnProcess failed, client may still be open — close it.
            safeCloseHandle (client);
        }
    }

    if (not result)
    {
        // Clean up any partially-initialised state.
        if (pseudoConsole != nullptr)
        {
            const ConPtyFuncs& funcs { loadConPtyFuncs() };

            if (funcs.close != nullptr)
                funcs.close (pseudoConsole);

            pseudoConsole = nullptr;
        }

        safeCloseHandle (pipe);
        safeCloseHandle (rEvent);
        safeCloseHandle (wEvent);
        readEvent  = INVALID_HANDLE_VALUE;
        writeEvent = INVALID_HANDLE_VALUE;
    }

    return result;
}

// =============================================================================

/**
 * @brief Close the ConPTY, pipe, and child process.
 *
 * @par Shutdown sequence
 * 1. `signalThreadShouldExit()` — ask the reader thread to stop.
 * 2. `ClosePseudoConsole()` — sends CTRL_CLOSE_EVENT to clients, then breaks
 *    the pipe.  Called while the reader thread is still alive so that ConPTY
 *    does not deadlock.  The broken pipe unblocks the reader's overlapped read.
 * 3. `stopThread (5000)` — wait up to 5 s for the reader thread to exit.
 * 4. Check if child process is still alive; if so, `TerminateProcess()`.
 * 5. Close all remaining handles.
 *
 * @note MESSAGE THREAD context.
 */
void WindowsTTY::close()
{
    signalThreadShouldExit();

    // Step 2: Close the pseudo console while the reader thread is still alive.
    // ClosePseudoConsole sends CTRL_CLOSE_EVENT to all attached clients and
    // then breaks the pipe, which unblocks the reader's overlapped ReadFile.
    if (pseudoConsole != nullptr)
    {
        const ConPtyFuncs& funcs { loadConPtyFuncs() };

        if (funcs.close != nullptr)
        {
            funcs.close (pseudoConsole);
        }

        pseudoConsole = nullptr;
    }

    // Step 3: Wait for the reader thread to see the broken pipe and exit.
    stopThread (5000);

    // Step 4: Clean up the child process.
    if (process != INVALID_HANDLE_VALUE)
    {
        DWORD exitCode { 0 };

        if (GetExitCodeProcess (process, &exitCode) and exitCode == STILL_ACTIVE)
        {
            TerminateProcess (process, 0);
            WaitForSingleObject (process, 2000);
        }

        CloseHandle (process);
        process = INVALID_HANDLE_VALUE;
    }

    // Step 5: Close all remaining handles.
    safeCloseHandle (pipe);
    safeCloseHandle (readEvent);
    safeCloseHandle (writeEvent);

    readBufferBytes  = 0;
    readBufferOffset = 0;
    readPending      = false;
    childPid         = 0;
}

// =============================================================================

/**
 * @brief Query whether the shell process is still alive.
 *
 * Calls `GetExitCodeProcess()` and compares the exit code to `STILL_ACTIVE`.
 *
 * @return `true` if the child process has not yet exited.
 *
 * @note May be called from any thread.
 */
bool WindowsTTY::isRunning() const
{
    bool running { false };

    if (process != INVALID_HANDLE_VALUE)
    {
        DWORD code { 0 };
        running = GetExitCodeProcess (process, &code) and code == STILL_ACTIVE;
    }

    return running;
}

// =============================================================================

/**
 * @brief Returns the PID of the child shell process.
 *
 * ConPTY does not expose a foreground process group like Unix tcgetpgrp.
 * Returns the shell PID stored at open() time.
 *
 * @return The shell PID, or -1 if not running.
 * @note Any thread.
 */
int WindowsTTY::getForegroundPid() const noexcept
{
    int result { -1 };

    if (childPid != 0)
        result = static_cast<int> (childPid);

    return result;
}

// =============================================================================

/**
 * @brief Writes the process name for the given PID into the buffer.
 *
 * Uses QueryFullProcessImageNameW to obtain the executable path,
 * then extracts the filename stem.
 *
 * @param pid        The process ID to query.
 * @param buffer     Destination buffer for the null-terminated name.
 * @param maxLength  Size of the destination buffer in bytes.
 * @return Number of bytes written (excluding null terminator), or 0 on failure.
 * @note Any thread.
 */
int WindowsTTY::getProcessName (int pid, char* buffer, int maxLength) const noexcept
{
    int result { 0 };

    if (pid > 0 and buffer != nullptr and maxLength > 0)
    {
        const HANDLE hProcess { OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                             static_cast<DWORD> (pid)) };

        if (hProcess != nullptr)
        {
            WCHAR pathBuf[MAX_PATH] {};
            DWORD pathLen { MAX_PATH };

            if (QueryFullProcessImageNameW (hProcess, 0, pathBuf, &pathLen) != 0)
            {
                const juce::String fullPath { pathBuf };
                const juce::String name { juce::File (fullPath).getFileNameWithoutExtension() };
                const int length { juce::jmin (name.length(), maxLength - 1) };
                std::memcpy (buffer, name.toRawUTF8(), static_cast<size_t> (length));
                buffer[length] = '\0';
                result = length;
            }

            CloseHandle (hProcess);
        }
    }

    return result;
}

// =============================================================================
// PEB offsets for x64 Windows — fixed by ABI, not subject to change.
// =============================================================================

/** Offset of PebBaseAddress within PROCESS_BASIC_INFORMATION (x64). */
static constexpr SIZE_T pbiOffsetPebBaseAddress        { 0x08 };

/** Offset of ProcessParameters pointer within PEB (x64). */
static constexpr SIZE_T pebOffsetProcessParameters     { 0x20 };

/** Offset of CurrentDirectory (CURDIR) within RTL_USER_PROCESS_PARAMETERS (x64).
 *  CURDIR begins with a UNICODE_STRING (DosPath) immediately at offset 0x38. */
static constexpr SIZE_T rtlOffsetCurrentDirectory      { 0x38 };

/** Byte offset of the UNICODE_STRING.Length field within UNICODE_STRING. */
static constexpr SIZE_T unicodeStringOffsetLength      { 0x00 };

/** Byte offset of the UNICODE_STRING.Buffer field within UNICODE_STRING (x64). */
static constexpr SIZE_T unicodeStringOffsetBuffer      { 0x08 };

/** Maximum wide-char path length we will read off the heap (chars, not bytes).
 *  Windows path limit is 32767 wide chars; we cap at MAX_PATH for stack safety. */
static constexpr SIZE_T cwdMaxWideChars                { MAX_PATH };

// =============================================================================

// =============================================================================
// getCwd helpers — file-local static functions
// =============================================================================

/**
 * @brief Holds the buffer address and wide-char count for a UNICODE_STRING read
 *        from target process memory.
 *
 * `bufferAddress` is the in-target-process pointer to the wide character buffer.
 * `wideCharCount` is the character count (bytes / sizeof(WCHAR)).
 * Both fields are zero/null when the read failed.
 */
struct UnicodeStringRef
{
    void*  bufferAddress { nullptr };
    size_t wideCharCount { 0 };
};

// NtQueryInformationProcess function pointer type (see getCwd).
// Signature: (HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG) → NTSTATUS
using FnNtQueryInformationProcess = NTSTATUS (NTAPI*) (
    HANDLE  ProcessHandle,
    ULONG   ProcessInformationClass,
    PVOID   ProcessInformation,
    ULONG   ProcessInformationLength,
    PULONG  ReturnLength);

// ProcessBasicInformation class index — documented in MSDN.
static constexpr ULONG processBasicInformation { 0 };

// Raw PROCESS_BASIC_INFORMATION blob size (x64).
// We only need PebBaseAddress at pbiOffsetPebBaseAddress; the full 48-byte read
// satisfies the API size requirement without depending on winternl.h.
static constexpr ULONG pbiSize { 48 };

// Size of a UNICODE_STRING in memory (x64):
//   +0x00  USHORT  Length         (byte count, not char count)
//   +0x02  USHORT  MaximumLength
//   +0x04  ULONG   padding
//   +0x08  PWSTR   Buffer
static constexpr SIZE_T unicodeStringSize { 0x10 };

// =============================================================================

/**
 * @brief Queries the PEB base address of @p hProcess via NtQueryInformationProcess.
 *
 * Resolves `NtQueryInformationProcess` from ntdll.dll once per process lifetime
 * (cached in a static local).  Reads a 48-byte PROCESS_BASIC_INFORMATION blob
 * and extracts the PebBaseAddress pointer at byte offset `pbiOffsetPebBaseAddress`.
 *
 * @param hProcess  Open process handle with PROCESS_QUERY_INFORMATION access.
 * @return          PEB base address in the target process, or nullptr on failure.
 * @note Any thread.  noexcept — no heap allocation.
 */
static void* queryPebAddress (HANDLE hProcess) noexcept
{
    static const FnNtQueryInformationProcess ntQueryInformationProcess
    {
        reinterpret_cast<FnNtQueryInformationProcess> (
            GetProcAddress (GetModuleHandleW (L"ntdll.dll"), "NtQueryInformationProcess"))
    };

    void* pebAddress { nullptr };

    if (ntQueryInformationProcess != nullptr)
    {
        BYTE pbiBlob[pbiSize] {};
        const NTSTATUS status
        {
            ntQueryInformationProcess (hProcess, processBasicInformation,
                                       pbiBlob, pbiSize, nullptr)
        };

        if (NT_SUCCESS (status))
            std::memcpy (&pebAddress, pbiBlob + pbiOffsetPebBaseAddress, sizeof (PVOID));
    }

    return pebAddress;
}

// =============================================================================

/**
 * @brief Reads the ProcessParameters pointer from the target process's PEB.
 *
 * Issues a single `ReadProcessMemory` call to read the pointer-sized field at
 * `pebAddress + pebOffsetProcessParameters` from the target process.
 *
 * @param hProcess    Open process handle with PROCESS_VM_READ access.
 * @param pebAddress  PEB base address in the target process (from queryPebAddress).
 * @return            RTL_USER_PROCESS_PARAMETERS address in the target process,
 *                    or nullptr on failure.
 * @note Any thread.  noexcept — no heap allocation.
 */
static void* readProcessParametersAddress (HANDLE hProcess, void* pebAddress) noexcept
{
    PVOID ppAddress { nullptr };
    SIZE_T bytesRead { 0 };

    const BOOL ok
    {
        ReadProcessMemory (hProcess,
                           static_cast<BYTE*> (pebAddress) + pebOffsetProcessParameters,
                           &ppAddress,
                           sizeof (PVOID),
                           &bytesRead)
    };

    return (ok != 0 and bytesRead == sizeof (PVOID)) ? ppAddress : nullptr;
}

// =============================================================================

/**
 * @brief Reads the CurrentDirectory UNICODE_STRING from RTL_USER_PROCESS_PARAMETERS.
 *
 * Issues a single `ReadProcessMemory` call to read the 16-byte UNICODE_STRING at
 * `processParametersAddress + rtlOffsetCurrentDirectory`.  Extracts the Length
 * (in bytes) and Buffer pointer, converts byte count to wide-char count.
 *
 * @param hProcess                  Open process handle with PROCESS_VM_READ access.
 * @param processParametersAddress  RTL_USER_PROCESS_PARAMETERS base address in target process.
 * @return                          UnicodeStringRef with buffer address and wide-char count.
 *                                  Both fields are zero/null on failure.
 * @note Any thread.  noexcept — no heap allocation.
 */
static UnicodeStringRef readCurrentDirectoryUnicodeString (HANDLE hProcess,
                                                            void* processParametersAddress) noexcept
{
    BYTE blob[unicodeStringSize] {};
    SIZE_T bytesRead { 0 };

    const BOOL ok
    {
        ReadProcessMemory (hProcess,
                           static_cast<BYTE*> (processParametersAddress) + rtlOffsetCurrentDirectory,
                           blob,
                           unicodeStringSize,
                           &bytesRead)
    };

    UnicodeStringRef ref {};

    if (ok != 0 and bytesRead == unicodeStringSize)
    {
        USHORT lengthBytes { 0 };
        std::memcpy (&lengthBytes, blob + unicodeStringOffsetLength, sizeof (USHORT));

        PVOID bufPtr { nullptr };
        std::memcpy (&bufPtr, blob + unicodeStringOffsetBuffer, sizeof (PVOID));

        const size_t wideCharCount { static_cast<size_t> (lengthBytes) / sizeof (WCHAR) };

        if (wideCharCount > 0 and wideCharCount <= cwdMaxWideChars and bufPtr != nullptr)
        {
            ref.bufferAddress = bufPtr;
            ref.wideCharCount = wideCharCount;
        }
    }

    return ref;
}

// =============================================================================

/**
 * @brief Reads a wide-character path from target process memory and converts it to UTF-8.
 *
 * Issues a single `ReadProcessMemory` to fetch `wideString.wideCharCount` wide chars
 * from `wideString.bufferAddress` in the target process.  Converts backslashes to
 * forward slashes in-place.  Strips the trailing slash unless the path is a drive
 * root (≤ 3 wide chars, e.g. `C:/`).  Converts to UTF-8 via `WideCharToMultiByte`
 * and writes the result into @p utf8Buffer (null-terminated).
 *
 * @param hProcess      Open process handle with PROCESS_VM_READ access.
 * @param wideString    Ref from readCurrentDirectoryUnicodeString — buffer address + count.
 * @param utf8Buffer    Caller-supplied destination for the null-terminated UTF-8 path.
 * @param utf8MaxLength Size of @p utf8Buffer in bytes (includes null terminator).
 * @return              Bytes written to @p utf8Buffer (excluding null terminator), or 0 on failure.
 * @note Any thread.  noexcept — no heap allocation.
 */
static int readAndConvertWidePath (HANDLE hProcess, const UnicodeStringRef& wideString,
                                   char* utf8Buffer, int utf8MaxLength) noexcept
{
    const USHORT lengthBytes { static_cast<USHORT> (wideString.wideCharCount * sizeof (WCHAR)) };
    WCHAR widePath[cwdMaxWideChars + 1] {};
    SIZE_T bytesRead { 0 };

    const BOOL ok
    {
        ReadProcessMemory (hProcess,
                           wideString.bufferAddress,
                           widePath,
                           lengthBytes,
                           &bytesRead)
    };

    int result { 0 };

    if (ok != 0 and bytesRead == static_cast<SIZE_T> (lengthBytes))
    {
        widePath[wideString.wideCharCount] = L'\0';

        // Replace backslashes with forward slashes in-place.
        for (SIZE_T i { 0 }; i < wideString.wideCharCount; ++i)
        {
            if (widePath[i] == L'\\')
                widePath[i] = L'/';
        }

        // Strip trailing slash — PEB CWD always has one; OSC 7 does not.
        // Preserve drive roots (e.g. "C:/") — effectiveCount must stay > 3.
        SIZE_T effectiveCount { wideString.wideCharCount };

        if (effectiveCount > 3 and widePath[effectiveCount - 1] == L'/')
            --effectiveCount;

        widePath[effectiveCount] = L'\0';

        // Convert to UTF-8.
        const int utf8Len
        {
            WideCharToMultiByte (CP_UTF8, 0,
                                 widePath,
                                 static_cast<int> (effectiveCount),
                                 utf8Buffer,
                                 utf8MaxLength - 1,
                                 nullptr, nullptr)
        };

        if (utf8Len > 0)
        {
            utf8Buffer[utf8Len] = '\0';
            result = utf8Len;
        }
    }

    return result;
}

// =============================================================================

/**
 * @brief Reads the current working directory of the given process via its PEB.
 *
 * Walks PEB → ProcessParameters → CurrentDirectory.DosPath using four helpers:
 * - queryPebAddress                    — NtQueryInformationProcess → PEB address
 * - readProcessParametersAddress       — PEB → RTL_USER_PROCESS_PARAMETERS address
 * - readCurrentDirectoryUnicodeString  — RTL_USER_PROCESS_PARAMETERS → UNICODE_STRING
 * - readAndConvertWidePath             — wide buffer → UTF-8 with slash normalisation
 *
 * @param pid        The process ID to query.
 * @param buffer     Destination buffer for the null-terminated UTF-8 path.
 * @param maxLength  Size of the destination buffer in bytes.
 * @return           Number of bytes written (excluding null terminator), or 0 on failure.
 * @note Any thread.
 */
int WindowsTTY::getCwd (int pid, char* buffer, int maxLength) const noexcept
{
    int result { 0 };

    if (pid > 0 and buffer != nullptr and maxLength > 0)
    {
        const HANDLE hProcess { OpenProcess (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                             FALSE, static_cast<DWORD> (pid)) };

        if (hProcess != nullptr)
        {
            void* pebAddress { queryPebAddress (hProcess) };

            if (pebAddress != nullptr)
            {
                void* ppAddress { readProcessParametersAddress (hProcess, pebAddress) };

                if (ppAddress != nullptr)
                {
                    const UnicodeStringRef cwdString { readCurrentDirectoryUnicodeString (hProcess, ppAddress) };

                    if (cwdString.wideCharCount > 0)
                        result = readAndConvertWidePath (hProcess, cwdString, buffer, maxLength);
                }
            }

            CloseHandle (hProcess);
        }
    }

    return result;
}

// =============================================================================

/**
 * @brief Copy bytes from a read buffer into a caller-supplied destination.
 *
 * Loads `bufferBytes = bytesAvailable` and `bufferOffset = 0`, then copies
 * up to `maxBytes` bytes from `src` to `dst`.  Advances `bufferOffset` by the
 * number of bytes copied.  When the buffer is fully consumed (`bufferOffset >=
 * bufferBytes`), resets both fields to zero.
 *
 * @param dst            Destination buffer.
 * @param maxBytes       Maximum bytes to copy into `dst`.
 * @param src            Source pointer (start of the data to copy from).
 * @param bufferBytes    Reference to the byte-count field; set to `bytesAvailable`.
 * @param bufferOffset   Reference to the cursor field; reset to 0 then advanced.
 * @param bytesAvailable Number of bytes available starting at `src`.
 * @return               Number of bytes copied.
 */
static int consumeReadBuffer (char* dst, int maxBytes, const char* src,
                               int& bufferBytes, int& bufferOffset,
                               int bytesAvailable) noexcept
{
    bufferBytes  = bytesAvailable;
    bufferOffset = 0;

    const int toCopy { juce::jmin (bufferBytes, maxBytes) };
    std::memcpy (dst, src, static_cast<size_t> (toCopy));
    bufferOffset = toCopy;

    if (bufferOffset >= bufferBytes)
    {
        bufferBytes  = 0;
        bufferOffset = 0;
    }

    return toCopy;
}

// =============================================================================

/**
 * @brief Copy bytes from the internal overlapped read buffer to the caller.
 *
 * Returns bytes from the internal buffer, which is filled by `waitForData()`.
 * When the buffer is exhausted, attempts an immediate overlapped read with a
 * zero-timeout wait so the TTY::run() drain loop can pull multiple chunks
 * without re-entering `waitForData()` between each one.  Returns 0 only when
 * no data is available on the pipe right now.
 *
 * Returns -1 if the pipe is broken (indicated by `readBufferBytes == -1`).
 * Returns 0 if no data is available.
 *
 * @param buf       Destination buffer.
 * @param maxBytes  Maximum bytes to copy.
 * @return          Bytes copied (> 0), 0 if no data available, -1 on EOF or error.
 *
 * @note READER THREAD context.
 */
int WindowsTTY::read (char* buf, int maxBytes)
{
    int result { 0 };

    if (readBufferBytes == -1)
    {
        result = -1;
    }
    else if (readBufferBytes > 0)
    {
        const int remaining { readBufferBytes - readBufferOffset };
        result = consumeReadBuffer (buf, maxBytes,
                                    readBuffer.getData() + readBufferOffset,
                                    readBufferBytes, readBufferOffset, remaining);
    }
    else if (pipe != INVALID_HANDLE_VALUE and not readPending)
    {
        // Buffer empty — try to issue and immediately complete another
        // overlapped read so the TTY::run() drain loop can pull multiple
        // chunks without re-entering waitForData() between each one.
        ResetEvent (readEvent);

        DWORD bytesRead { 0 };
        const BOOL ok { ReadFile (pipe,
                                   readBuffer.getData(),
                                   static_cast<DWORD> (READ_CHUNK_SIZE),
                                   &bytesRead,
                                   &readOverlapped) };

        if (ok != FALSE)
        {
            // Completed synchronously — data was already in the pipe buffer.
            if (bytesRead == 0)
            {
                readBufferBytes = -1;
                result = -1;
            }
            else
            {
                result = consumeReadBuffer (buf, maxBytes,
                                            readBuffer.getData(),
                                            readBufferBytes, readBufferOffset,
                                            static_cast<int> (bytesRead));
            }
        }
        else
        {
            const DWORD err { GetLastError() };

            if (err == ERROR_IO_PENDING)
            {
                // No data available right now — check with zero timeout.
                const DWORD waitResult { WaitForSingleObject (readEvent, 0) };

                if (waitResult == WAIT_OBJECT_0)
                {
                    const BOOL gotResult { GetOverlappedResult (pipe, &readOverlapped, &bytesRead, FALSE) };

                    if (gotResult != FALSE and bytesRead > 0)
                    {
                        result = consumeReadBuffer (buf, maxBytes,
                                                    readBuffer.getData(),
                                                    readBufferBytes, readBufferOffset,
                                                    static_cast<int> (bytesRead));
                    }
                    else
                    {
                        readBufferBytes = -1;
                        result = -1;
                    }
                }
                else
                {
                    // Data not available yet — leave the read pending so
                    // waitForData() picks it up on the next iteration.
                    readPending = true;
                    result = 0;
                }
            }
            else if (err == ERROR_BROKEN_PIPE or err == ERROR_NO_DATA)
            {
                readBufferBytes = -1;
                result = -1;
            }
        }
    }

    return result;
}

// =============================================================================

/**
 * @brief Write bytes to the ConPTY pipe (keyboard input to the shell).
 *
 * Issues an overlapped `WriteFile` on the pipe server end and waits for
 * completion via `GetOverlappedResult`.  Serialised by `writeLock` to prevent
 * interleaving from concurrent callers (e.g. bracketed paste sequences).
 *
 * @param buf  Data to write.
 * @param len  Number of bytes.
 * @return     `true` if all bytes were written successfully.
 *
 * @note MESSAGE THREAD context.
 */
bool WindowsTTY::write (const char* buf, int len)
{
    const juce::ScopedLock lock { writeLock };

    bool result { false };

    if (pipe != INVALID_HANDLE_VALUE and len > 0)
    {
        ResetEvent (writeEvent);

        DWORD written { 0 };
        const BOOL ok { WriteFile (pipe, buf, static_cast<DWORD> (len), nullptr, &writeOverlapped) };

        if (ok != FALSE)
        {
            result = true;
        }
        else if (GetLastError() == ERROR_IO_PENDING)
        {
            result = GetOverlappedResult (pipe, &writeOverlapped, &written, TRUE) != FALSE;
        }
    }

    return result;
}

// =============================================================================

/**
 * @brief Resize the ConPTY window.
 *
 * Calls `ResizePseudoConsole()` with the new dimensions.  ConPTY propagates
 * the resize to the child process as a `WINDOW_BUFFER_SIZE_EVENT` console
 * event, which the shell or foreground TUI application can handle.
 *
 * @param cols  New terminal width in character columns.
 * @param rows  New terminal height in character rows.
 *
 * @note READER THREAD context (dispatched via TTY::run resize handling).
 */
void WindowsTTY::platformResize (int cols, int rows)
{
    if (pseudoConsole != nullptr)
    {
        const ConPtyFuncs& funcs { loadConPtyFuncs() };

        if (funcs.resize != nullptr)
        {
            const COORD size { static_cast<short> (cols), static_cast<short> (rows) };
            funcs.resize (pseudoConsole, size);
        }
    }
}

// =============================================================================

/**
 * @brief Issue an overlapped read and block until data arrives or the timeout expires.
 *
 * If no read is already pending (`readPending == false`) and the buffer is
 * empty, issues a new overlapped `ReadFile` into `readBuffer`.  Then waits on
 * `readEvent` for up to `timeoutMs` milliseconds.
 *
 * On completion, calls `GetOverlappedResult` to retrieve the byte count:
 * - If bytes were read, stores the count in `readBufferBytes` and returns `true`.
 * - If the pipe is broken (`ERROR_BROKEN_PIPE` or 0 bytes), sets
 *   `readBufferBytes = -1` and returns `true` so the caller can detect EOF.
 * - On timeout, leaves `readPending = true` and returns `false`.
 *
 * @param timeoutMs  Maximum wait time in milliseconds.
 * @return           `true` if data is available or the pipe is broken;
 *                   `false` on timeout.
 *
 * @note READER THREAD context.
 */
bool WindowsTTY::waitForData (int timeoutMs)
{
    bool dataReady { false };

    if (pipe != INVALID_HANDLE_VALUE)
    {
        // Only issue a new read if the buffer has been fully consumed and no
        // read is already in flight.
        if (not readPending and readBufferBytes == 0)
        {
            ResetEvent (readEvent);

            DWORD bytesRead { 0 };
            const BOOL ok { ReadFile (pipe,
                                      readBuffer.getData(),
                                      static_cast<DWORD> (READ_CHUNK_SIZE),
                                      &bytesRead,
                                      &readOverlapped) };

            if (ok != FALSE)
            {
                // Completed synchronously.
                if (bytesRead == 0)
                {
                    readBufferBytes = -1;  // EOF
                }
                else
                {
                    readBufferBytes  = static_cast<int> (bytesRead);
                    readBufferOffset = 0;
                }

                dataReady = true;
            }
            else
            {
                const DWORD err { GetLastError() };

                if (err == ERROR_IO_PENDING)
                {
                    readPending = true;
                }
                else if (err == ERROR_BROKEN_PIPE or err == ERROR_NO_DATA)
                {
                    readBufferBytes = -1;  // EOF
                    dataReady = true;
                }
            }
        }

        // If a read is pending, wait for data or process exit.
        if (readPending)
        {
            HANDLE handles[2] { readEvent, process };
            const DWORD handleCount { process != INVALID_HANDLE_VALUE ? 2u : 1u };
            const DWORD waitResult { WaitForMultipleObjects (handleCount, handles, FALSE,
                                                             static_cast<DWORD> (timeoutMs)) };

            if (waitResult == WAIT_OBJECT_0 + 1)
            {
                // Child process exited — cancel pending read and signal EOF.
                CancelIo (pipe);
                readPending = false;
                readBufferBytes = -1;
                dataReady = true;
            }
            else if (waitResult == WAIT_OBJECT_0)
            {
                readPending = false;

                DWORD bytesRead { 0 };
                const BOOL ok { GetOverlappedResult (pipe, &readOverlapped, &bytesRead, FALSE) };

                if (ok != FALSE)
                {
                    if (bytesRead == 0)
                    {
                        readBufferBytes = -1;  // EOF
                    }
                    else
                    {
                        readBufferBytes  = static_cast<int> (bytesRead);
                        readBufferOffset = 0;
                    }
                }
                else
                {
                    readBufferBytes = -1;  // Pipe broken or other error
                }

                dataReady = true;
            }
        }
        else if (readBufferBytes != 0)
        {
            // Data already in buffer from a synchronous completion above.
            dataReady = true;
        }
    }

    return dataReady;
}

#endif
