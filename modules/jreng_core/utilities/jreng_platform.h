/**
 * @file jreng_platform.h
 * @brief Windows platform version detection utilities.
 *
 * Provides `isWindows10()` — a cached runtime check that returns true when
 * the process is running on Windows 10 (build < 22000).  Windows 11 and later
 * is the canonical path; Windows 10 is the special-case branch.
 *
 * @note This header is Windows-only.  Include it only inside `#if JUCE_WINDOWS`
 *       guards at the call site.
 */

#pragma once

#include <windows.h>

/**
 * @brief Returns true if the current OS is Windows 10 (build < 22000).
 *
 * Uses `RtlGetVersion` from `ntdll.dll` to obtain the real OS build number,
 * bypassing the application compatibility manifest shim that causes
 * `GetVersionEx` to lie.
 *
 * Result is cached in a `static const` — computed once per process lifetime.
 *
 * Windows 11 is build >= 22000.  Windows 10 is build < 22000.  This function
 * returns `false` (Windows 11 path) when the build number cannot be determined,
 * which is the safe default — the Windows 11 path is canon.
 *
 * @return @c true  on Windows 10 (build < 22000).
 * @return @c false on Windows 11+ or if version detection fails.
 */
static bool isWindows10() noexcept
{
    using FnRtlGetVersion = NTSTATUS (NTAPI*) (OSVERSIONINFOEXW*);

    static const bool cached { []() noexcept -> bool
    {
        bool result { false };

        const HMODULE ntdll { GetModuleHandleW (L"ntdll.dll") };

        if (ntdll != nullptr)
        {
            const FnRtlGetVersion rtlGetVersion
            {
                reinterpret_cast<FnRtlGetVersion> (
                    GetProcAddress (ntdll, "RtlGetVersion"))
            };

            if (rtlGetVersion != nullptr)
            {
                OSVERSIONINFOEXW osvi {};
                osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEXW);

                if (rtlGetVersion (&osvi) == 0)
                    result = osvi.dwBuildNumber < 22000;
            }
        }

        return result;
    }() };

    return cached;
}
