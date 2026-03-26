/**
 * @file Notifications.h
 * @brief Cross-platform desktop notification dispatch.
 *
 * Provides a single free function `Terminal::Notifications::show()` that
 * dispatches a native desktop notification on macOS (UNUserNotificationCenter),
 * Windows (WinRT ToastNotification), and Linux (libnotify / stderr fallback).
 *
 * @see Parser::onDesktopNotification
 * @see Session::onDesktopNotification
 */

#pragma once
#include <JuceHeader.h>

namespace Terminal
{
namespace Notifications
{

/**
 * @brief Show a native desktop notification.
 *
 * @param title  Notification title (may be empty for OSC 9).
 * @param body   Notification body text.
 *
 * @note MESSAGE THREAD only.
 */
void show (const juce::String& title, const juce::String& body);

} // namespace Notifications
} // namespace Terminal
