/**
 * @file WindowsTTY.cpp
 * @brief Windows ConPTY PTY implementation: pipe setup, process creation, I/O, resize.
 *
 * This file implements the WindowsTTY interface using the Windows Pseudo Console
 * (ConPTY) API.  Two helper functions handle the two-phase setup:
 *
 * 1. `createPseudoConsoleAndPipes()` — creates the anonymous pipe pairs and
 *    calls `CreatePseudoConsole()`.
 * 2. `spawnProcess()` — builds a `STARTUPINFOEXW` with the ConPTY attribute
 *    and calls `CreateProcessW()` to launch `cmd.exe`.
 *
 * ### Pipe ownership after open()
 *
 * | Handle          | Owner after open()  | Purpose                        |
 * |-----------------|---------------------|--------------------------------|
 * | `inputWriter`   | WindowsTTY (parent) | Send keystrokes to shell       |
 * | `outputReader`  | WindowsTTY (parent) | Receive VT output from shell   |
 * | `pipeReadEnd`   | Closed              | ConPTY internal (input side)   |
 * | `pipeWriteEnd`  | Closed              | ConPTY internal (output side)  |
 *
 * @see WindowsTTY.h  Class declaration and member documentation
 * @see TTY.cpp       Shared reader thread loop
 * @see https://docs.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session
 */

#ifdef JUCE_WINDOWS

#include "WindowsTTY.h"

#pragma comment(lib, "kernel32.lib")

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

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
 * @brief Create the ConPTY handle and the two anonymous pipe pairs.
 *
 * Creates two anonymous pipes:
 * - Input pipe: `pipeReadEnd` (ConPTY reads) ← `inputWriter` (parent writes)
 * - Output pipe: `outputReader` (parent reads) ← `pipeWriteEnd` (ConPTY writes)
 *
 * Then calls `CreatePseudoConsole()` with the initial terminal size and the
 * two ConPTY-side pipe ends.
 *
 * @param[out] pseudoConsole  Receives the ConPTY handle on success.
 * @param[out] pipeReadEnd    Receives the read end of the input pipe (ConPTY side).
 * @param[out] inputWriter    Receives the write end of the input pipe (parent side).
 * @param[out] outputReader   Receives the read end of the output pipe (parent side).
 * @param[out] pipeWriteEnd   Receives the write end of the output pipe (ConPTY side).
 * @param      size           Initial terminal dimensions as a `COORD`.
 * @return                    `true` if all Win32 calls succeeded.
 *
 * @note Called from `WindowsTTY::open()` on the message thread.
 */
static bool createPseudoConsoleAndPipes (HPCON& pseudoConsole, HANDLE& pipeReadEnd, HANDLE& inputWriter,
                                          HANDLE& outputReader, HANDLE& pipeWriteEnd, COORD size)
{
    if (not CreatePipe (&pipeReadEnd, &inputWriter, nullptr, 0))
    {
        return false;
    }

    if (not CreatePipe (&outputReader, &pipeWriteEnd, nullptr, 0))
    {
        return false;
    }

    HRESULT hr { CreatePseudoConsole (size, pipeReadEnd, pipeWriteEnd, 0, &pseudoConsole) };
    return not FAILED (hr);
}

/**
 * @brief Spawn `cmd.exe` with the ConPTY attribute and retrieve the process handle.
 *
 * Builds a `STARTUPINFOEXW` with a `PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE`
 * attribute so that ConPTY intercepts the child's console I/O.  After a
 * successful `CreateProcessW()` the ConPTY-side pipe ends are closed — they
 * are now owned by ConPTY internally.
 *
 * @param      pseudoConsole  The ConPTY handle to attach to the child.
 * @param[in,out] pipeReadEnd   Input pipe read end — closed on success.
 * @param[in,out] pipeWriteEnd  Output pipe write end — closed on success.
 * @param[out] process         Receives the child process handle on success.
 * @return                     `true` if `CreateProcessW()` succeeded.
 *
 * @note Called from `WindowsTTY::open()` on the message thread.
 */
static bool spawnProcess (HPCON pseudoConsole, HANDLE& pipeReadEnd, HANDLE& pipeWriteEnd, HANDLE& process)
{
    size_t attrSize { 0 };
    InitializeProcThreadAttributeList (nullptr, 1, 0, &attrSize);

    STARTUPINFOEXW si {};
    si.StartupInfo.cb = sizeof (si.StartupInfo);
    si.lpAttributeList = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST> (HeapAlloc (GetProcessHeap(), 0, attrSize));

    if (not si.lpAttributeList)
    {
        return false;
    }

    InitializeProcThreadAttributeList (si.lpAttributeList, 1, 0, &attrSize);
    UpdateProcThreadAttribute (si.lpAttributeList, 0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pseudoConsole, sizeof (HPCON), nullptr, nullptr);

    WCHAR cmd[] = L"cmd.exe";
    PROCESS_INFORMATION pi {};

    BOOL ok { CreateProcessW (nullptr, cmd, nullptr, nullptr, FALSE,
        CREATE_NEW_PROCESS_GROUP | EXTENDED_STARTUPINFO_PRESENT,
        nullptr, nullptr, &si.StartupInfo, &pi) };

    HeapFree (GetProcessHeap(), 0, si.lpAttributeList);

    CloseHandle (pipeReadEnd);
    pipeReadEnd = INVALID_HANDLE_VALUE;
    CloseHandle (pipeWriteEnd);
    pipeWriteEnd = INVALID_HANDLE_VALUE;

    if (ok)
    {
        CloseHandle (pi.hThread);
        process = pi.hProcess;
    }

    return ok != FALSE;
}

// =============================================================================

/**
 * @brief Open the ConPTY and spawn the shell process.
 *
 * @par Sequence
 * 1. Convert @p cols / @p rows to a `COORD`.
 * 2. `createPseudoConsoleAndPipes()` — allocate pipe pairs and ConPTY handle.
 * 3. `spawnProcess()` — launch `cmd.exe` with the ConPTY attribute.
 * 4. `startThread()` — begin the TTY reader loop.
 *
 * @param cols  Initial terminal width in character columns.
 * @param rows  Initial terminal height in character rows.
 * @return      `true` on success; `false` if any Win32 call fails.
 *
 * @note MESSAGE THREAD context.
 */
bool WindowsTTY::open (int cols, int rows)
{
    COORD size { static_cast<short> (cols), static_cast<short> (rows) };

    bool result { false };

    if (createPseudoConsoleAndPipes (pseudoConsole, pipeReadEnd, inputWriter, outputReader, pipeWriteEnd, size))
    {
        if (spawnProcess (pseudoConsole, pipeReadEnd, pipeWriteEnd, process))
        {
            startThread();
            result = true;
        }
    }

    return result;
}

/**
 * @brief Close the ConPTY, pipes, and child process.
 *
 * @par Shutdown sequence
 * 1. `signalThreadShouldExit()` — ask the reader thread to stop.
 * 2. `TerminateProcess()` — forcibly kill the shell (no graceful SIGTERM
 *    equivalent on Windows ConPTY).
 * 3. `ClosePseudoConsole()` — release the ConPTY handle.
 * 4. Close all remaining pipe handles via a local `safeClose` lambda.
 * 5. `stopThread (5000)` — wait up to 5 s for the reader thread to exit.
 *
 * @note MESSAGE THREAD context.
 */
void WindowsTTY::close()
{
    signalThreadShouldExit();

    if (process != INVALID_HANDLE_VALUE)
    {
        TerminateProcess (process, 0);
        CloseHandle (process);
        process = INVALID_HANDLE_VALUE;
    }

    if (pseudoConsole != nullptr)
    {
        ClosePseudoConsole (pseudoConsole);
        pseudoConsole = nullptr;
    }

    auto safeClose = [] (HANDLE& h)
    {
        if (h != INVALID_HANDLE_VALUE)
        {
            CloseHandle (h);
            h = INVALID_HANDLE_VALUE;
        }
    };

    safeClose (outputReader);
    safeClose (inputWriter);
    safeClose (pipeReadEnd);
    safeClose (pipeWriteEnd);

    stopThread (5000);
}

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

/**
 * @brief Read available bytes from the ConPTY output pipe.
 *
 * Attempts a synchronous `ReadFile()`.  If the pipe is in overlapped mode and
 * returns `ERROR_IO_PENDING`, falls back to a non-blocking `GetOverlappedResult`
 * check.  Return value mapping:
 * - `bytesRead > 0` → return bytesRead
 * - `ERROR_IO_PENDING` + `GetOverlappedResult` succeeds → return bytesRead
 * - `ERROR_BROKEN_PIPE` → return -1 (shell exited / pipe closed)
 * - `ERROR_IO_INCOMPLETE` → return 0 (no data yet)
 * - other error → return -1
 *
 * @param buf       Destination buffer.
 * @param maxBytes  Maximum bytes to read.
 * @return          Bytes read (> 0), 0 if no data available, -1 on error or EOF.
 *
 * @note READER THREAD context.
 */
int WindowsTTY::read (char* buf, int maxBytes)
{
    OVERLAPPED ov {};
    DWORD bytesRead { 0 };

    if (ReadFile (outputReader, buf, maxBytes, &bytesRead, &ov))
    {
        return static_cast<int> (bytesRead);
    }

    const DWORD err { GetLastError() };

    if (err != ERROR_IO_PENDING)
    {
        return (err == ERROR_BROKEN_PIPE) ? -1 : 0;
    }

    if (GetOverlappedResult (outputReader, &ov, &bytesRead, FALSE))
    {
        return static_cast<int> (bytesRead);
    }

    return (GetLastError() == ERROR_IO_INCOMPLETE) ? 0 : -1;
}

/**
 * @brief Write bytes to the ConPTY input pipe (keyboard input to the shell).
 *
 * A single synchronous `WriteFile()` call.  Returns `true` only if all
 * @p len bytes were accepted.
 *
 * @param buf  Data to write.
 * @param len  Number of bytes.
 * @return     `true` if all bytes were written successfully.
 *
 * @note MESSAGE THREAD context.
 */
bool WindowsTTY::write (const char* buf, int len)
{
    DWORD written { 0 };
    return WriteFile (inputWriter, buf, len, &written, nullptr) and static_cast<int> (written) == len;
}

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
void WindowsTTY::resize (int cols, int rows)
{
    if (pseudoConsole != nullptr)
    {
        COORD size { static_cast<short> (cols), static_cast<short> (rows) };
        ResizePseudoConsole (pseudoConsole, size);
    }
}

/**
 * @brief Block until data is available on the output pipe or the timeout expires.
 *
 * Uses `WaitForSingleObject()` on the `outputReader` handle.  Returns `true`
 * when the handle is signalled (data available or pipe closed).
 *
 * @param timeoutMs  Maximum wait time in milliseconds.
 * @return           `true` if `WAIT_OBJECT_0` is returned; `false` on timeout
 *                   or if the handle is invalid.
 *
 * @note READER THREAD context.
 */
bool WindowsTTY::waitForData (int timeoutMs)
{
    if (outputReader == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD result { WaitForSingleObject (outputReader, static_cast<DWORD> (timeoutMs)) };
    return result == WAIT_OBJECT_0;
}

#endif
