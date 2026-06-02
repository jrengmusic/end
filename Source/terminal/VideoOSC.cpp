/**
 * @file VideoOSC.cpp
 * @brief OSC dispatch and basic OSC metadata handlers.
 *
 * This translation unit implements `Video::applyOSC()` and the basic
 * metadata handlers: title, cwd, clipboard, desktop notification, and cursor
 * color.  Protocol-extension handlers (hyperlink OSC 8, shell integration
 * OSC 133, iTerm2 image OSC 1337) live in `VideoOSCExt.cpp`.
 * ESC-level dispatch lives in `VideoESC.cpp`; DCS and APC passthrough live
 * in `VideoDCS.cpp`.
 *
 * @par OSC sequence structure
 * An OSC sequence has the form:
 * @code
 *   ESC ] <command> ; <data> BEL
 *   ESC ] <command> ; <data> ESC \   (ST — String Terminator)
 * @endcode
 * The numeric command code and data payload are accumulated in the hybrid
 * `oscBuffer` and dispatched by `applyOSC()` when the terminator is received.
 *
 * @par Thread model
 * All functions in this file run exclusively on the **READER THREAD**.
 * Callbacks that update UI state are dispatched to the message thread via
 * `juce::MessageManager::callAsync`.
 *
 * @see Video.h         — class declaration and full API documentation
 * @see VideoOSCExt.cpp — hyperlink, shell integration, iTerm2 image handlers
 * @see VideoESC.cpp    — ESC sequence dispatch
 * @see VideoDCS.cpp    — DCS and APC passthrough
 */

#include "Video.h"

namespace terminal
{
/*____________________________________________________________________________*/

// ============================================================================
// OSC header parsing
// ============================================================================

/**
 * @brief Parsed header of an OSC payload.
 *
 * An OSC payload has the form `<command>;<data>`.  `parseOscHeader()`
 * extracts the numeric command code and the byte offset at which the data
 * portion begins.
 *
 * @see parseOscHeader()
 * @see applyOSC()
 */
struct OscHeader
{
    int commandNumber { 0 };  ///< Numeric OSC command code (e.g. 0, 2, 52).
    int dataStart { 0 };      ///< Byte offset of the first data byte after ';'.
};

/**
 * @brief Parses the numeric command code and data offset from an OSC payload.
 *
 * Scans `payload` for the first `;` separator.  Bytes before the separator
 * that are ASCII digits are accumulated into `commandNumber`.  `dataStart`
 * is set to the index immediately after the `;`.
 *
 * If no `;` is found, `dataStart` remains 0 and the caller treats the
 * payload as having no data portion.
 *
 * @param payload  Pointer to the raw OSC payload bytes (not null-terminated).
 * @param length   Number of valid bytes in `payload`.
 *
 * @return Parsed `OscHeader` with `commandNumber` and `dataStart`.
 *
 * @note Pure function — no side effects.
 * @note READER THREAD only.
 */
static OscHeader parseOscHeader (const uint8_t* payload, int length) noexcept
{
    OscHeader h;
    for (int i { 0 }; i < length; ++i)
    {
        if (payload[i] == ';')
        {
            h.dataStart = i + 1;
            break;
        }

        if (payload[i] >= '0' and payload[i] <= '9')
        {
            h.commandNumber = h.commandNumber * 10 + (payload[i] - '0');
        }
    }
    return h;
}

// ============================================================================
// OSC handlers
// ============================================================================

void Video::handleOscTitle (const uint8_t* data, int dataLength) noexcept
{
    // READER THREAD
    int safeLen { juce::jmin (dataLength, MAX_OSC_TITLE_LENGTH - 1) };

    while (safeLen > 0 and (data[safeLen - 1] & 0xC0) == 0x80)
        --safeLen;

    if (events.contains (id::title)) events.get (id::title, reinterpret_cast<const char*> (data), int (safeLen));
}

void Video::handleOscCwd (const uint8_t* data, int dataLength) noexcept
{
    // READER THREAD
    // OSC 7 format: file://hostname/path
    // Find the path after "file://hostname" by locating the third '/'
    const auto* begin { reinterpret_cast<const char*> (data) };
    const auto* end { begin + dataLength };

    int slashCount { 0 };
    const char* pathStart { nullptr };

    for (const char* p { begin }; p < end; ++p)
    {
        if (*p == '/')
        {
            ++slashCount;

            if (slashCount == 3)
            {
                pathStart = p;
                break;
            }
        }
    }

    if (pathStart != nullptr)
    {
        const int length { juce::jmin (static_cast<int> (end - pathStart), MAX_OSC_TITLE_LENGTH - 1) };

#if JUCE_WINDOWS
        // Convert MSYS2 POSIX paths (/c/Users/...) and native Windows paths (/C:/Users/...)
        // to the canonical forward-slash form (C:/Users/...).
        if (length >= 3
            and pathStart[0] == '/'
            and std::isalpha (static_cast<unsigned char> (pathStart[1])))
        {
            const char next { pathStart[2] };
            const bool isMsys    { next == '/' };
            const bool isNative  { next == ':' };

            if (isMsys or isNative)
            {
                const char driveLetter { static_cast<char> (std::toupper (static_cast<unsigned char> (pathStart[1]))) };
                juce::String converted;
                converted += driveLetter;

                if (isMsys)
                    converted += ':';

                converted += juce::String (pathStart + 2, length - 2);
                if (events.contains (id::cwd)) events.get (id::cwd, converted.toRawUTF8(), static_cast<int> (converted.getNumBytesAsUTF8()));
            }
            else
            {
                if (events.contains (id::cwd)) events.get (id::cwd, static_cast<const char*> (pathStart), int (length));
            }
        }
        else
        {
            if (events.contains (id::cwd)) events.get (id::cwd, static_cast<const char*> (pathStart), int (length));
        }
#else
        if (events.contains (id::cwd)) events.get (id::cwd, static_cast<const char*> (pathStart), int (length));
#endif
    }
}

void Video::handleOscClipboard (const uint8_t* data, int dataLength) noexcept
{
    if (dataLength > 2)
    {
        // Find the semicolon after the selection parameter (e.g., "c;" or "s0;")
        int payloadStart { 0 };

        for (int i { 0 }; i < dataLength; ++i)
        {
            if (*(data + i) == ';')
            {
                payloadStart = i + 1;
                break;
            }
        }

        if (payloadStart > 0 and payloadStart < dataLength)
        {
            const juce::String base64 (reinterpret_cast<const char*> (data + payloadStart),
                                       static_cast<size_t> (dataLength - payloadStart));
            juce::MemoryBlock decoded;

            if (decoded.fromBase64Encoding (base64))
            {
                const juce::String text (juce::String::fromUTF8 (static_cast<const char*> (decoded.getData()),
                                                                  static_cast<int> (decoded.getSize())));

                if (events.contains (id::clipboardChanged))
                    juce::MessageManager::callAsync ([this, text] { /* MESSAGE THREAD */ events.get (id::clipboardChanged, text); });
            }
        }
    }
}

void Video::handleOscNotification (const uint8_t* data, int dataLength) noexcept
{
    if (dataLength > 0 and events.contains (id::desktopNotification))
    {
        const juce::String body { juce::String::fromUTF8 (
            reinterpret_cast<const char*> (data), dataLength) };

        juce::MessageManager::callAsync ([this, body] { events.get (id::desktopNotification, juce::String {}, body); });
    }
}

void Video::handleOsc777 (const uint8_t* data, int dataLength) noexcept
{
    // Format: notify;title;body
    // data points after "777;", so it starts with "notify;..."
    if (dataLength > 7 and events.contains (id::desktopNotification))
    {
        // Verify "notify;" prefix
        static constexpr uint8_t prefix[] { 'n', 'o', 't', 'i', 'f', 'y', ';' };

        if (std::memcmp (data, prefix, 7) == 0)
        {
            const uint8_t* titleStart { data + 7 };
            const int remaining { dataLength - 7 };

            // Find semicolon separating title from body
            int titleLen { 0 };

            while (titleLen < remaining and titleStart[titleLen] != ';')
            {
                ++titleLen;
            }

            const juce::String title { juce::String::fromUTF8 (
                reinterpret_cast<const char*> (titleStart), titleLen) };

            juce::String body;

            if (titleLen + 1 < remaining)
            {
                body = juce::String::fromUTF8 (
                    reinterpret_cast<const char*> (titleStart + titleLen + 1),
                    remaining - titleLen - 1);
            }

            juce::MessageManager::callAsync ([this, title, body] { events.get (id::desktopNotification, title, body); });
        }
    }
}

void Video::handleOscCursorColor (const uint8_t* data, int dataLength) noexcept
{
    // READER THREAD
    if (dataLength >= 7)
    {
        const juce::String colorStr { juce::String::fromUTF8 (reinterpret_cast<const char*> (data),
                                                               dataLength) };

        if (colorStr.startsWith ("rgb:") and colorStr.length() >= 12)
        {
            const auto parts { juce::StringArray::fromTokens (colorStr.substring (4), "/", "") };

            if (parts.size() == 3)
            {
                const int r { parts.getReference (0).getHexValue32() };
                const int g { parts.getReference (1).getHexValue32() };
                const int b { parts.getReference (2).getHexValue32() };

                const int rr { parts.getReference (0).length() > 2 ? (r >> 8) : r };
                const int gg { parts.getReference (1).length() > 2 ? (g >> 8) : g };
                const int bb { parts.getReference (2).length() > 2 ? (b >> 8) : b };

                const juce::Colour color { static_cast<uint8_t> (juce::jlimit (0, 255, rr)),
                                           static_cast<uint8_t> (juce::jlimit (0, 255, gg)),
                                           static_cast<uint8_t> (juce::jlimit (0, 255, bb)) };
                if (events.contains (id::cursorColor))
                    events.get (id::cursorColor, static_cast<int> (activeScreen), juce::Colour (color));
            }
        }
        else if (colorStr.startsWith ("#") and colorStr.length() >= 7)
        {
            const juce::Colour color { juce::Colour::fromString ("FF" + colorStr.substring (1, 7)) };
            if (events.contains (id::cursorColor))
                events.get (id::cursorColor, static_cast<int> (activeScreen), juce::Colour (color));
        }
    }
}

void Video::handleOscResetCursorColor() noexcept
{
    // READER THREAD
    if (events.contains (id::resetCursorColor))
        events.get (id::resetCursorColor, static_cast<int> (activeScreen));
}

// ============================================================================
// OSC dispatch
// ============================================================================

/**
 * @brief Dispatches a complete OSC (Operating System Command) string.
 *
 * Called by `performAction()` when the `oscEnd` action fires (BEL or ST
 * terminator received).  Parses the numeric command code from the start of
 * `payload` via `parseOscHeader()` and routes to the appropriate handler.
 *
 * @param payload  Pointer to the raw OSC payload bytes (not null-terminated).
 *                 Includes the command number, `;` separator, and data.
 * @param length   Number of valid bytes in `payload`.
 *
 * @note READER THREAD only.
 *
 * @see handleOscTitle()
 * @see handleOscClipboard()
 * @see parseOscHeader()
 */
void Video::applyOSC (const uint8_t* payload, int length) noexcept
{
    if (length >= 2)
    {
        const OscHeader h { parseOscHeader (payload, length) };

        if (h.dataStart > 0)
        {
            const int dataLength { length - h.dataStart };
            const uint8_t* data { payload + h.dataStart };

            switch (h.commandNumber)
            {
                case 0:
                case 2:     handleOscTitle (data, dataLength);                   break;
                case 7:     handleOscCwd (data, dataLength);                     break;
                case 8:     handleOsc8 (data, dataLength);                       break;
                case 9:     handleOscNotification (data, dataLength);            break;
                case 12:    handleOscCursorColor (data, dataLength);             break;
                case 52:    handleOscClipboard (data, dataLength);               break;
                case 133:   handleOsc133 (activeScreen, data, dataLength);       break;
                case 777:   handleOsc777 (data, dataLength);                     break;
                case 1337:  handleOsc1337 (data, dataLength);                    break;
                default:    break;
            }
        }

        if (h.commandNumber == 112)
        {
            handleOscResetCursorColor();
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
