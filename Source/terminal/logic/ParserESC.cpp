/**
 * @file ParserESC.cpp
 * @brief ESC sequence dispatch, OSC handling, and DCS passthrough stubs.
 *
 * This translation unit implements `Parser::escDispatch()` and all ESC-level
 * handler functions, plus the OSC (Operating System Command) and DCS (Device
 * Control String) dispatch infrastructure.
 *
 * @par ESC sequence structure
 * An ESC sequence has the form:
 * @code
 *   ESC <intermediates> <final>
 * @endcode
 * where `<intermediates>` is zero or more bytes in 0x20–0x2F and `<final>`
 * is a single byte in 0x30–0x7E.  `escDispatch()` routes based on the
 * intermediate count and value:
 *
 * | Intermediates | Handler                    | Examples                   |
 * |---------------|----------------------------|----------------------------|
 * | None          | `escDispatchNoIntermediate()` | ESC D, ESC M, ESC c, ESC H |
 * | `(`           | `escDispatchCharset()`     | ESC ( 0, ESC ( B           |
 * | `#`           | `escDispatchDEC()`         | ESC # 8 (DECALN)           |
 *
 * @par OSC sequence structure
 * An OSC sequence has the form:
 * @code
 *   ESC ] <command> ; <data> BEL
 *   ESC ] <command> ; <data> ESC \   (ST — String Terminator)
 * @endcode
 * The numeric command code and data payload are accumulated in the hybrid
 * `oscBuffer` and dispatched by `oscDispatch()` when the terminator is received.
 *
 * @par DCS passthrough
 * DCS (Device Control String) sequences are accepted by the state machine
 * accumulated in `dcsBuffer` via `appendToBuffer()`.  `dcsHook()` and
 * `dcsUnhook()` are the entry and exit points for future decoder dispatch
 * (e.g. Sixel graphics via DECRQSS).
 *
 * @par Thread model
 * All functions in this file run exclusively on the **READER THREAD**.
 * OSC title and clipboard callbacks are dispatched to the message thread via
 * `juce::MessageManager::callAsync`.
 *
 * @see Parser.h      — class declaration and full API documentation
 * @see ParserVT.cpp  — ground-state print and execute handlers
 * @see ParserCSI.cpp — CSI sequence dispatch
 * @see State         — atomic terminal parameter store
 * @see Grid          — screen buffer written by DECALN
 */

#include "Parser.h"
#include "Grid.h"
#include "ITerm2Decoder.h"
#include "KittyDecoder.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// Cursor save / restore (DECSC / DECRC)
// ============================================================================

void Parser::saveCursor (ActiveScreen scr) noexcept
{
    auto& sc { savedCursor.at (static_cast<size_t> (scr)) };
    sc.row         = state.getRawValue<int>  (state.screenKey (scr, ID::cursorRow));
    sc.col         = state.getRawValue<int>  (state.screenKey (scr, ID::cursorCol));
    sc.pen         = pen;
    sc.wrapPending = state.getRawValue<bool> (state.screenKey (scr, ID::wrapPending));
    sc.originMode  = state.getRawValue<bool> (state.modeKey (ID::originMode));
    sc.lineDrawing = useLineDrawing;
}

void Parser::restoreCursor (ActiveScreen scr) noexcept
{
    const auto& sc { savedCursor.at (static_cast<size_t> (scr)) };
    state.setCursorRow   (scr, sc.row);
    state.setCursorCol   (scr, sc.col);
    pen            = sc.pen;
    stamp          = sc.pen;
    state.setWrapPending (scr, sc.wrapPending);
    state.setMode        (ID::originMode, sc.originMode);
    useLineDrawing = sc.lineDrawing;
}

// ============================================================================
// VT Handler: ESC dispatch
// ============================================================================

/**
 * @brief Handles ESC sequences with no intermediate bytes.
 *
 * Covers the most common two-byte ESC sequences.  The final byte selects the
 * action:
 *
 * | Final | Name  | Action                                                       |
 * |-------|-------|--------------------------------------------------------------|
 * | 'D'   | IND   | Index — line feed without CR (scroll if at bottom)           |
 * | 'E'   | NEL   | Next Line — CR + line feed (scroll if at bottom)             |
 * | 'H'   | HTS   | Horizontal Tab Set — set tab stop at current column          |
 * | 'M'   | RI    | Reverse Index — move cursor up, scroll down if at top        |
 * | 'c'   | RIS   | Reset to Initial State — full terminal reset                 |
 *
 * @par Sequences
 * @code
 *   ESC D    — IND (Index)
 *   ESC E    — NEL (Next Line)
 *   ESC H    — HTS (Horizontal Tab Set)
 *   ESC M    — RI  (Reverse Index)
 *   ESC c    — RIS (Reset to Initial State)
 * @endcode
 *
 * @par IND (ESC D)
 * Moves the cursor down one line.  If the cursor is at the bottom of the
 * scrolling region, the region is scrolled up by one line instead.
 *
 * @par NEL (ESC E)
 * Moves the cursor to column 0 and then performs an IND.
 *
 * @par RI (ESC M)
 * Reverse Index — moves the cursor up one line.  If the cursor is at the top
 * of the scrolling region, the region is scrolled down by one line instead.
 *
 * @param scr        Target screen buffer (normal or alternate).
 * @param finalByte  The ESC final byte (0x30–0x7E).
 *
 * @note READER THREAD only.
 *
 * @see cursorGoToNextLine()
 * @see setTabStop()
 * @see reset()
 */
void Parser::escDispatchNoIntermediate (ActiveScreen scr, uint8_t finalByte) noexcept
{
    switch (finalByte)
    {
        case 'D':
        {
            if (not cursorGoToNextLine (scr, activeScrollBottom(), state.getRawValue<int> (ID::visibleRows)))
            {
                Cell fill {};
                fill.bg = stamp.bg;
                writer.scrollRegionUp (state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)), activeScrollBottom(), 1, fill);
            }
            break;
        }

        case 'E':
            state.setCursorCol (scr, 0);
            state.setWrapPending (scr, false);
            if (not cursorGoToNextLine (scr, activeScrollBottom(), state.getRawValue<int> (ID::visibleRows)))
            {
                Cell fill {};
                fill.bg = stamp.bg;
                writer.scrollRegionUp (state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)), activeScrollBottom(), 1, fill);
            }
            break;

        case 'H':
            setTabStop (scr);
            break;

        case 'M':
        {
            if (state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) == state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)))
            {
                Cell fill {};
                fill.bg = stamp.bg;
                writer.scrollRegionDown (state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)), activeScrollBottom(), 1, fill);
            }
            else if (state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) > 0)
            {
                state.setCursorRow (scr, state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) - 1);
                state.setWrapPending (scr, false);
            }
            break;
        }

        case 'c':
            reset();
            break;

        case '7':
            saveCursor (scr);
            break;

        case '8':
            restoreCursor (scr);
            break;

        case '=':
            state.setMode (ID::applicationKeypad, true);
            break;

        case '>':
            state.setMode (ID::applicationKeypad, false);
            break;

        default:
            break;
    }
}

/**
 * @brief Handles ESC sequences that designate a character set (G0–G3 slots).
 *
 * Intermediate bytes 0x28–0x2B select the target charset slot:
 * - `(` (0x28) → G0
 * - `)` (0x29) → G1
 * - `*` (0x2A) → G2
 * - `+` (0x2B) → G3
 *
 * The final byte selects the character set to load into that slot:
 * - `B` → ASCII (ISO 646 US)
 * - `0` → DEC Special Graphics (VT100 line-drawing characters)
 *
 * The G0 slot (`(`) sets `useLineDrawing` and `g0LineDrawing`.  The G1 slot
 * (`)`) sets `g1LineDrawing` but does not immediately affect `useLineDrawing`.
 *
 * @par Sequences
 * @code
 *   ESC ( 0    — Designate G0 = DEC Special Graphics (line drawing)
 *   ESC ( B    — Designate G0 = ASCII (default)
 *   ESC ) 0    — Designate G1 = DEC Special Graphics (ignored)
 * @endcode
 *
 * @param interByte  The intermediate byte (`(`, `)`, `*`, or `+`).
 * @param finalByte  The charset designator byte (`B`, `0`, etc.).
 *
 * @note READER THREAD only.
 *
 * @see useLineDrawing
 * @see translateCharset()
 */
void Parser::escDispatchCharset (uint8_t interByte, uint8_t finalByte) noexcept
{
    if (interByte == '(')
    {
        g0LineDrawing = (finalByte == '0');
        useLineDrawing = g0LineDrawing;
    }
    else if (interByte == ')')
    {
        g1LineDrawing = (finalByte == '0');
    }
}

/**
 * @brief Handles DEC-private ESC sequences with intermediate byte `#`.
 *
 * Currently implements only DECALN (DEC Screen Alignment Test, ESC # 8),
 * which fills the entire visible screen with the letter `E` and homes the
 * cursor.  DECALN is used by terminal conformance tests to verify that all
 * cells are addressable and that the screen geometry is correct.
 *
 * @par Sequence
 * @code
 *   ESC # 8    — DECALN (DEC Screen Alignment Test)
 * @endcode
 *
 * @par DECALN behaviour
 * Every cell in the visible grid is overwritten with `Cell { codepoint='E',
 * style=0, width=1, layout=0 }`.  The cursor is moved to row 0, column 0.
 *
 * @param scr        Target screen buffer (normal or alternate).
 * @param finalByte  The ESC final byte following `#`.
 *
 * @note READER THREAD only.
 *
 * @see Grid::activeWriteCell()
 * @see cursorSetPosition()
 */
void Parser::escDispatchDEC (ActiveScreen scr, uint8_t finalByte) noexcept
{
    if (finalByte == '8')
    {
        const int cols { state.getRawValue<int> (ID::cols) };
        const int visibleRows { state.getRawValue<int> (ID::visibleRows) };

        for (int row { 0 }; row < visibleRows; ++row)
        {
            for (int col { 0 }; col < cols; ++col)
            {
                Cell st {};
                st.codepoint = 'E';
                st.style = 0;
                st.width = 1;
                st.layout = 0;
                writer.activeWriteCell (row, col, st);
                writer.activeEraseGrapheme (row, col);
            }
        }

        cursorSetPosition (scr, 0, 0, cols, visibleRows);
    }
}

/**
 * @brief Dispatches a complete ESC sequence to the appropriate sub-handler.
 *
 * Called by `performAction()` when the `escDispatch` action fires.  Routes
 * based on the number and value of intermediate bytes:
 *
 * | Condition                          | Handler                      |
 * |------------------------------------|------------------------------|
 * | `interCount == 0`                  | `escDispatchNoIntermediate()` |
 * | `interCount == 1 && inter[0]=='('` | `escDispatchCharset()`       |
 * | `interCount == 1 && inter[0]=='#'` | `escDispatchDEC()`           |
 * | Other                              | silently ignored             |
 *
 * @par Sequence format
 * @code
 *   ESC <final>              — no intermediate
 *   ESC ( <final>            — G0 charset designation
 *   ESC # <final>            — DEC private sequence
 * @endcode
 *
 * @param inter              Pointer to the intermediate byte buffer.
 * @param interCount         Number of valid bytes in `inter`.
 * @param finalByte          The ESC final byte (0x30–0x7E).
 *
 * @note READER THREAD only.
 *
 * @see escDispatchNoIntermediate()
 * @see escDispatchCharset()
 * @see escDispatchDEC()
 */
void Parser::escDispatch (const uint8_t* inter, uint8_t interCount, uint8_t finalByte) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };

    if (interCount == 0)
    {
        escDispatchNoIntermediate (scr, finalByte);
    }
    else if (interCount == 1 and (inter[0] == '(' or inter[0] == ')'))
    {
        escDispatchCharset (inter[0], finalByte);
    }
    else if (interCount == 1 and inter[0] == '#')
    {
        escDispatchDEC (scr, finalByte);
    }
}

// ============================================================================
// VT Handler: OSC dispatch
// ============================================================================

/**
 * @brief Parsed header of an OSC payload.
 *
 * An OSC payload has the form `<command>;<data>`.  `parseOscHeader()`
 * extracts the numeric command code and the byte offset at which the data
 * portion begins.
 *
 * @see parseOscHeader()
 * @see oscDispatch()
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
static inline OscHeader parseOscHeader (const uint8_t* payload, int length) noexcept
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

/**
 * @brief Handles OSC 0 / OSC 2 — window title change.
 *
 * Trims `dataLength` to avoid splitting a multi-byte UTF-8 sequence at the
 * boundary, then passes the raw bytes directly to `state.setTitle()`.  State
 * owns the backing buffer — no intermediate `titleBuffer` is needed.
 *
 * @par Sequences
 * @code
 *   ESC ] 0 ; <title> BEL    — set icon name and window title
 *   ESC ] 2 ; <title> BEL    — set window title only
 * @endcode
 *
 * @param data        Pointer to the title bytes (after the `"0;"` or `"2;"` prefix).
 *                    Not null-terminated.
 * @param dataLength  Number of bytes in `data`.
 *
 * @note READER THREAD only.
 *
 * @see State::setTitle()
 * @see oscDispatch()
 */
void Parser::handleOscTitle (const uint8_t* data, int dataLength) noexcept
{
    // READER THREAD
    int safeLen { juce::jmin (dataLength, State::maxStringLength - 1) };

    while (safeLen > 0 and (data[safeLen - 1] & 0xC0) == 0x80)
        --safeLen;

    state.setTitle (reinterpret_cast<const char*> (data), safeLen);
}

void Parser::handleOscCwd (const uint8_t* data, int dataLength) noexcept
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
        const int length { juce::jmin (static_cast<int> (end - pathStart), State::maxStringLength - 1) };

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
                state.setCwd (converted.toRawUTF8(), static_cast<int> (converted.getNumBytesAsUTF8()));
            }
            else
            {
                state.setCwd (pathStart, length);
            }
        }
        else
        {
            state.setCwd (pathStart, length);
        }
#else
        state.setCwd (pathStart, length);
#endif
    }
}

/**
 * @brief Handles OSC 52 — clipboard write.
 *
 * Decodes a base64-encoded clipboard payload and invokes `onClipboardChanged`
 * on the message thread.  The OSC 52 payload format is:
 * @code
 *   <selection> ; <base64-data>
 * @endcode
 * where `<selection>` is a string like `c` (clipboard) or `s0` (selection 0).
 * This implementation ignores the selection parameter and always writes to the
 * system clipboard via the callback.
 *
 * @par Sequence
 * @code
 *   ESC ] 52 ; c ; <base64> BEL    — write base64-decoded text to clipboard
 * @endcode
 *
 * @par Validation
 * - Payloads shorter than 3 bytes are silently ignored.
 * - If no `;` separator is found after the selection parameter, the payload
 *   is silently ignored.
 * - Malformed base64 (rejected by `juce::MemoryBlock::fromBase64Encoding`)
 *   is silently ignored.
 *
 * @param data        Pointer to the OSC 52 payload bytes (after `"52;"`).
 *                    Not null-terminated.
 * @param dataLength  Number of bytes in `data`.
 *
 * @note READER THREAD only.  The `onClipboardChanged` callback is dispatched
 *       to the message thread.
 *
 * @see onClipboardChanged
 * @see oscDispatch()
 */
void Parser::handleOscClipboard (const uint8_t* data, int dataLength) noexcept
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

                if (onClipboardChanged)
                    juce::MessageManager::callAsync ([this, text] { /* MESSAGE THREAD */ onClipboardChanged (text); });
            }
        }
    }
}

/**
 * @brief Handles OSC 9 — desktop notification (body only).
 *
 * The entire payload is treated as the notification body; title is empty.
 * Invokes `onDesktopNotification ({}, body)` on the message thread.
 *
 * @par Sequence
 * @code
 *   ESC ] 9 ; <message> BEL
 * @endcode
 *
 * @param data        Pointer to the OSC 9 payload bytes (after "9;").
 *                    Not null-terminated.
 * @param dataLength  Number of bytes in `data`.
 *
 * @note READER THREAD only.  `onDesktopNotification` dispatched via `callAsync`.
 *
 * @see onDesktopNotification
 * @see oscDispatch()
 */
void Parser::handleOscNotification (const uint8_t* data, int dataLength) noexcept
{
    if (dataLength > 0 and onDesktopNotification)
    {
        const juce::String body { juce::String::fromUTF8 (
            reinterpret_cast<const char*> (data), dataLength) };

        juce::MessageManager::callAsync ([this, body] { onDesktopNotification ({}, body); });
    }
}

/**
 * @brief Handles OSC 777 — desktop notification with title and body.
 *
 * Verifies the `notify;` prefix, then extracts title and body separated by `;`.
 * Invokes `onDesktopNotification (title, body)` on the message thread.
 *
 * @par Sequence
 * @code
 *   ESC ] 777 ; notify ; <title> ; <body> BEL
 * @endcode
 *
 * @par Validation
 * - Payloads shorter than 8 bytes are silently ignored.
 * - Payloads not beginning with `notify;` are silently ignored.
 *
 * @param data        Pointer to the OSC 777 payload bytes (after "777;").
 *                    Not null-terminated.
 * @param dataLength  Number of bytes in `data`.
 *
 * @note READER THREAD only.  `onDesktopNotification` dispatched via `callAsync`.
 *
 * @see onDesktopNotification
 * @see oscDispatch()
 */
void Parser::handleOsc777 (const uint8_t* data, int dataLength) noexcept
{
    // Format: notify;title;body
    // data points after "777;", so it starts with "notify;..."
    if (dataLength > 7 and onDesktopNotification)
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

            juce::MessageManager::callAsync ([this, title, body] { onDesktopNotification (title, body); });
        }
    }
}

/**
 * @brief Dispatches a complete OSC (Operating System Command) string.
 *
 * Called by `performAction()` when the `oscEnd` action fires (BEL or ST
 * terminator received).  Parses the numeric command code from the start of
 * `payload` via `parseOscHeader()` and routes to the appropriate handler.
 *
 * @par Supported OSC commands
 * | Command | Name                     | Handler                       |
 * |---------|--------------------------|-------------------------------|
 * | 0       | Icon + title             | `handleOscTitle()`            |
 * | 2       | Window title             | `handleOscTitle()`            |
 * | 7       | Working directory         | `handleOscCwd()`              |
 * | 9       | Desktop notification     | `handleOscNotification()`     |
 * | 12      | Set cursor color         | `handleOscCursorColor()`      |
 * | 52      | Clipboard write          | `handleOscClipboard()`        |
 * | 112     | Reset cursor color       | `handleOscResetCursorColor()` |
 * | 133     | Shell integration marker | `handleOsc133()`              |
 * | 777     | Desktop notification     | `handleOsc777()`              |
 * | 1337    | iTerm2 inline image      | `handleOsc1337()`             |
 * | Other   | —                        | silently ignored              |
 *
 * @par Sequence format
 * @code
 *   ESC ] <command> ; <data> BEL
 *   ESC ] <command> ; <data> ESC \
 * @endcode
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
void Parser::oscDispatch (const uint8_t* payload, int length) noexcept
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
                case 133:   handleOsc133 (state.getRawValue<ActiveScreen> (ID::activeScreen), data, dataLength);  break;
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

void Parser::handleOscCursorColor (const uint8_t* data, int dataLength) noexcept
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

                state.setCursorColor (state.getRawValue<ActiveScreen> (ID::activeScreen),
                                      juce::jlimit (0, 255, rr),
                                      juce::jlimit (0, 255, gg),
                                      juce::jlimit (0, 255, bb));
            }
        }
        else if (colorStr.startsWith ("#") and colorStr.length() >= 7)
        {
            const juce::Colour c { juce::Colour::fromString ("FF" + colorStr.substring (1, 7)) };
            state.setCursorColor (state.getRawValue<ActiveScreen> (ID::activeScreen),
                                  c.getRed(), c.getGreen(), c.getBlue());
        }
    }
}

void Parser::handleOscResetCursorColor() noexcept
{
    // READER THREAD
    state.resetCursorColor (state.getRawValue<ActiveScreen> (ID::activeScreen));
}

/**
 * @brief Handles OSC 8 — explicit hyperlink start/end.
 *
 * Payload (the data after the "8;" separator) has the form:
 *   params ; uri
 *
 * - Non-empty URI: records the current cursor position as the link start.
 * - Empty URI:     closes the open span and writes it to State via
 *                  `state.storeHyperlink()`.
 *
 * @param data        Pointer to OSC payload bytes (after the "8;" separator).
 * @param dataLength  Number of bytes in `data`.
 *
 * @note READER THREAD only.
 */
void Parser::handleOsc8 (const uint8_t* data, int dataLength) noexcept
{
    // READER THREAD
    // Find the semicolon that separates params from uri
    int semiPos { -1 };

    for (int i { 0 }; i < dataLength; ++i)
    {
        if (data[i] == ';')
        {
            semiPos = i;
            break;
        }
    }

    if (semiPos < 0)
    {
        // Malformed — no separator found; ignore
        activeOsc8Uri = {};
        osc8StartRow  = -1;
        osc8StartCol  = -1;
    }
    else
    {
        const int uriStart  { semiPos + 1 };
        const int uriLength { dataLength - uriStart };

        if (uriLength > 0)
        {
            // Start of a new hyperlink — record in-flight tracking state
            const int len { juce::jmin (uriLength, State::maxStringLength - 1) };
            std::memcpy (activeOsc8Uri.buffer,
                         reinterpret_cast<const char*> (data + uriStart),
                         static_cast<size_t> (len));
            activeOsc8Uri.buffer[len] = '\0';

            const ActiveScreen scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
            osc8StartRow = state.getRawValue<int> (ID::scrollbackUsed) + state.getRawValue<int> (state.screenKey (scr, ID::cursorRow));
            osc8StartCol = state.getRawValue<int> (state.screenKey (scr, ID::cursorCol));
        }
        else
        {
            // End of hyperlink — close the span if one was open and write to State
            if (activeOsc8Uri.buffer[0] != '\0' and osc8StartRow >= 0)
            {
                const ActiveScreen scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
                const int endCol { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };

                // Derive a unique span key from start position: "row_startCol"
                const juce::String spanKey { juce::String (osc8StartRow) + "_"
                                             + juce::String (osc8StartCol) };
                const juce::Identifier spanId { spanKey };

                const int uriLen { static_cast<int> (std::strlen (activeOsc8Uri.buffer)) };
                state.storeHyperlink (spanId, activeOsc8Uri.buffer, uriLen,
                                      osc8StartRow, osc8StartCol, endCol);
            }

            activeOsc8Uri = {};
            osc8StartRow  = -1;
            osc8StartCol  = -1;
        }
    }
}

/**
 * @brief Handles OSC 133 — shell integration semantic prompt markers.
 *
 * Subcommands A and B are accepted and silently ignored; they exist in the
 * protocol but carry no information needed for output-block tracking.
 * Subcommand C marks the start of command output: the current cursor row is
 * recorded as the output block top and the scan-active flag is set, so that
 * subsequent LF events extend the tracked row range.  Subcommand D marks the
 * end of command output: the current cursor row is recorded as the block
 * bottom and the scan flag is cleared.
 *
 * @param scr         Active screen buffer selected at dispatch time.
 * @param data        Pointer to the OSC 133 subcommand byte(s) after `"133;"`.
 * @param dataLength  Number of bytes in `data` (must be >= 1 for a valid subcommand).
 * @note READER THREAD only.
 */
void Parser::handleOsc133 (ActiveScreen scr, const uint8_t* data, int dataLength) noexcept
{
    if (dataLength >= 1)
    {
        const int cursorRow { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
        const int absoluteRow { state.getRawValue<int> (ID::scrollbackUsed) + cursorRow };

        switch (data[0])
        {
            case 'A':
                state.setPromptRow (absoluteRow);
                break;

            case 'C':
                state.setOutputBlockStart (absoluteRow);
                break;

            case 'D':
                state.setOutputBlockEnd (absoluteRow);
                break;

            default:
                break;
        }
    }
}

/**
 * @brief Handles OSC 1337 — iTerm2 inline image display.
 *
 * Delegates to `ITerm2Decoder` to parse the `File=` key=value header,
 * base64-decode the payload, and produce an RGBA8 `DecodedImage`.  On success,
 * reserves an image ID, writes image cells to the grid, and stores the decoded
 * image in Grid for the MESSAGE THREAD to pull on first atlas encounter.
 *
 * Silently skips when:
 * - `inline=0` or the key is absent (download-only mode).
 * - Cell dimensions are not yet calibrated (zero values).
 * - Decoder returns an invalid image.
 *
 * @par Sequence
 * @code
 *   ESC ] 1337 ; File=[key=value;...] : <base64> BEL
 * @endcode
 *
 * @param data        Pointer to OSC payload bytes after "1337;".
 *                    Expected to begin with "File=".
 * @param dataLength  Number of bytes in @p data.
 *
 * @note READER THREAD only.
 *
 * @see ITerm2Decoder
 * @see Grid::reserveImageId()
 * @see Grid::storeDecodedImage()
 */
void Parser::handleOsc1337 (const uint8_t* data, int dataLength) noexcept
{
    // READER THREAD
    const int cellW { physCellWidthAtomic.load  (std::memory_order_relaxed) };
    const int cellH { physCellHeightAtomic.load (std::memory_order_relaxed) };

    if (cellW > 0 and cellH > 0)
    {
        ITerm2Decoder decoder;
        DecodedImage image { decoder.decode (data, dataLength) };

        if (image.isValid())
        {
            const ActiveScreen scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
            const int cursorRow    { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
            const int cursorCol    { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };

            const uint32_t imageId { writer.reserveImageId() };

            writer.activeWriteImage (cursorRow, cursorCol, imageId,
                                     image.width, image.height, cellW, cellH);

            const int cellRows { (image.height + cellH - 1) / cellH };
            cursorMoveDown (scr, cellRows, effectiveClampBottom (scr));
            state.setCursorCol (scr, 0);

            PendingImage pending;
            pending.imageId = imageId;
            pending.rgba    = std::move (image.rgba);
            pending.width   = image.width;
            pending.height  = image.height;

            writer.storeDecodedImage (std::move (pending));
        }
    }
}

// ============================================================================
// VT Handler: DCS
// ============================================================================

/**
 * @brief Called when a DCS (Device Control String) sequence is hooked.
 *
 * Invoked on entry to the `dcsPassthrough` state when the DCS final byte is
 * received.  The `dcsFinalByte` member is recorded by `performAction()` prior
 * to this call.  The current implementation is a stub — Sixel decoder dispatch
 * will be wired here in a future step.
 *
 * @par Sequence format
 * @code
 *   ESC P <params> <intermediates> <final> <data> ESC \
 * @endcode
 *
 * @param params             Finalised DCS parameter accumulator (unused until decoder wired).
 * @param inter              Pointer to the intermediate byte buffer (unused until decoder wired).
 * @param interCount         Number of valid bytes in `inter` (unused until decoder wired).
 * @param finalByte          The DCS final byte (recorded in `dcsFinalByte` by caller).
 *
 * @note READER THREAD only.
 *
 * @see dcsUnhook()
 * @see appendToBuffer()
 */
void Parser::dcsHook (const CSI& /*params*/,
                      const uint8_t* /*inter*/,
                      uint8_t /*interCount*/,
                      uint8_t /*finalByte*/) noexcept
{
}

/**
 * @brief Called for each byte in the DCS passthrough data stream.
 *
 * Retained as an extension point.  DCS passthrough accumulation is now
 * performed directly in `performAction()` via `appendToBuffer (dcsBuffer, …)`.
 * This method is no longer called by the `put` action.
 *
 * @param byte  The DCS passthrough byte (unused).
 *
 * @note READER THREAD only.
 *
 * @see dcsHook()
 * @see dcsUnhook()
 */
void Parser::dcsPut (uint8_t /*byte*/) noexcept {}

/**
 * @brief Updates the physical cell dimensions used by the Sixel decoder.
 *
 * @param widthPx   Physical cell width in pixels.
 * @param heightPx  Physical cell height in pixels.
 * @note MESSAGE THREAD.
 */
void Parser::setPhysCellDimensions (int widthPx, int heightPx) noexcept
{
    physCellWidthAtomic.store  (widthPx,  std::memory_order_relaxed);
    physCellHeightAtomic.store (heightPx, std::memory_order_relaxed);
}

/**
 * @brief Called when a DCS sequence is terminated (ST received).
 *
 * When `dcsFinalByte == 'q'` and the buffer contains data, runs `SixelDecoder`
 * on the accumulated payload.  On success:
 * 1. Reserves an image ID via `Grid::Writer::reserveImageId()`.
 * 2. Writes image cells to the grid via `Grid::Writer::activeWriteImage()`.
 * 3. Stores the decoded image via `Grid::Writer::storeDecodedImage()`.
 *    The MESSAGE THREAD pulls it from Grid on first atlas encounter in
 *    `processCellForSnapshot()` and stages the RGBA pixels into `ImageAtlas`.
 *
 * Silently skips the decode path when:
 * - `dcsFinalByte != 'q'` (not a Sixel sequence)
 * - `physCellWidthAtomic` or `physCellHeightAtomic` is zero (Screen not yet
 *   calibrated — first frame startup edge case)
 * - Decoder returns an invalid image
 *
 * @note READER THREAD only.
 *
 * @see dcsHook()
 * @see appendToBuffer()
 * @see SixelDecoder
 * @see Grid::reserveImageId()
 * @see Grid::storeDecodedImage()
 */
void Parser::dcsUnhook() noexcept
{
    if (dcsFinalByte == 'q' and dcsBufferSize > 0)
    {
        const int cellW { physCellWidthAtomic.load  (std::memory_order_relaxed) };
        const int cellH { physCellHeightAtomic.load (std::memory_order_relaxed) };

        if (cellW > 0 and cellH > 0)
        {
            SixelDecoder decoder;
            DecodedImage image { decoder.decode (dcsBuffer.get(), static_cast<size_t> (dcsBufferSize)) };

            if (image.isValid())
            {
                const ActiveScreen scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
                const int cursorRow    { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
                const int cursorCol    { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };

                const uint32_t imageId { writer.reserveImageId() };

                writer.activeWriteImage (cursorRow, cursorCol, imageId,
                                         image.width, image.height, cellW, cellH);

                const int cellRows { (image.height + cellH - 1) / cellH };
                cursorMoveDown (scr, cellRows, effectiveClampBottom (scr));
                state.setCursorCol (scr, 0);

                PendingImage pending;
                pending.imageId = imageId;
                pending.rgba    = std::move (image.rgba);
                pending.width   = image.width;
                pending.height  = image.height;

                writer.storeDecodedImage (std::move (pending));
            }
        }
    }

    dcsBufferSize = 0;
}

/**
 * @brief Called when an APC sequence is terminated (BEL or ST received).
 *
 * Dispatches the accumulated `apcBuffer` payload to `KittyDecoder::process()`.
 * On a final-chunk transmit+display result, reserves an image ID, writes image
 * cells to the grid, and stores the decoded image in Grid for the MESSAGE THREAD
 * to pull on first atlas encounter.  Any non-empty response string is queued
 * via `sendResponse()` for delivery through `writeToHost` after `process()`.
 *
 * Silently skips when:
 * - `apcBufferSize == 0` (no APC data accumulated).
 * - Cell dimensions are not yet calibrated (zero values).
 * - Decoder returns `shouldDisplay=false` (query, delete, transmit-only, or
 *   mid-sequence chunk).
 * - Decoded image is invalid.
 *
 * @note READER THREAD only.
 *
 * @see KittyDecoder
 * @see appendToBuffer()
 * @see Grid::reserveImageId()
 * @see Grid::storeDecodedImage()
 */
void Parser::apcEnd() noexcept
{
    if (apcBufferSize > 0 and apcBuffer.get() != nullptr)
    {
        // apcBuffer starts with 'G' (the APC command identifier); KittyDecoder
        // expects data after 'G' (key=value pairs + payload).
        KittyDecoder::Result kittyResult { kittyDecoder.process (apcBuffer.get() + 1, apcBufferSize - 1) };

        if (kittyResult.shouldDisplay and kittyResult.image.isValid())
        {
            const int cellW { physCellWidthAtomic.load  (std::memory_order_relaxed) };
            const int cellH { physCellHeightAtomic.load (std::memory_order_relaxed) };

            if (cellW > 0 and cellH > 0)
            {
                const ActiveScreen scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
                const int cursorRow    { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
                const int cursorCol    { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };

                const uint32_t imageId { writer.reserveImageId() };

                state.setOverlayImageId (imageId);
                state.setOverlayRow (cursorRow);
                state.setOverlayCol (cursorCol);

                const int cellRows { (kittyResult.image.height + cellH - 1) / cellH };
                cursorMoveDown (scr, cellRows, effectiveClampBottom (scr));
                state.setCursorCol (scr, 0);

                PendingImage pending;
                pending.imageId = imageId;
                pending.rgba    = std::move (kittyResult.image.rgba);
                pending.width   = kittyResult.image.width;
                pending.height  = kittyResult.image.height;

                writer.storeDecodedImage (std::move (pending));
            }
        }
        else if (kittyResult.isVirtualPlacement and kittyResult.image.isValid())
        {
            // Virtual placement (U=1): store image for atlas consumption under the Kitty
            // image ID so that U+10EEEE placeholder cells can look it up by that ID.
            // No grid cells are written — the app writes U+10EEEE characters instead.
            PendingImage pending;
            pending.imageId              = kittyResult.kittyImageId;
            pending.rgba                 = std::move (kittyResult.image.rgba);
            pending.width                = kittyResult.image.width;
            pending.height               = kittyResult.image.height;
            pending.hasVirtualPlacement  = true;
            pending.placementCols        = kittyResult.placementCols;
            pending.placementRows        = kittyResult.placementRows;

            writer.storeDecodedImage (std::move (pending));
        }

        if (kittyResult.response.isNotEmpty())
        {
            sendResponse (kittyResult.response.toRawUTF8());
        }
    }

    apcBufferSize = 0;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
