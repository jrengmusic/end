/**
 * @file Notifications.cpp
 * @brief Windows and Linux desktop notification implementations.
 */

#include "Notifications.h"

#if JUCE_WINDOWS

#include <windows.h>

namespace Terminal
{
namespace Notifications
{

void show (const juce::String& title, const juce::String& body)
{
    // Windows: use MessageBeep + stderr fallback.
    // WinRT ToastNotification requires COM setup and app identity —
    // use simple stderr output until Windows packaging is finalised.
    const juce::String message { title.isNotEmpty() ? title + ": " + body : body };
    std::fputs (message.toRawUTF8(), stderr);
    std::fputc ('\n', stderr);
    MessageBeep (MB_ICONINFORMATION);
}

} // namespace Notifications
} // namespace Terminal

#elif JUCE_LINUX || JUCE_BSD

#include <cstdlib>

namespace Terminal
{
namespace Notifications
{

void show (const juce::String& title, const juce::String& body)
{
    // Linux: try notify-send (libnotify CLI), fall back to stderr.
    const juce::String escapedTitle { title.replace ("'", "'\\''") };
    const juce::String escapedBody { body.replace ("'", "'\\''") };

    juce::String command { "notify-send" };

    if (title.isNotEmpty())
    {
        command += " '" + escapedTitle + "'";
    }

    command += " '" + escapedBody + "'";
    command += " 2>/dev/null &";

    std::system (command.toRawUTF8());
}

} // namespace Notifications
} // namespace Terminal

#endif
