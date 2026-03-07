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
 * The numeric command code and data payload are accumulated in `oscBuffer`
 * and dispatched by `oscDispatch()` when the terminator is received.
 *
 * @par DCS passthrough
 * DCS (Device Control String) sequences are accepted by the state machine
 * but currently passed through without processing.  `dcsHook()`, `dcsPut()`,
 * and `dcsUnhook()` are provided as extension points for future support
 * (e.g. DECRQSS — Request Status String).
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

namespace Terminal
{ /*____________________________________________________________________________*/

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
            if (not cursorGoToNextLine (scr, scrollBottom))
            {
                grid.scrollRegionUp (state.getScrollTop (scr), scrollBottom, 1);
            }
            break;
        }

        case 'E':
            state.setCursorCol (scr, 0);
            state.setWrapPending (scr, false);
            if (not cursorGoToNextLine (scr, scrollBottom))
            {
                grid.scrollRegionUp (state.getScrollTop (scr), scrollBottom, 1);
            }
            break;

        case 'H':
            setTabStop (scr);
            break;

        case 'M':
        {
            if (state.getCursorRow (scr) == state.getScrollTop (scr))
            {
                grid.scrollRegionDown (state.getScrollTop (scr), scrollBottom, 1);
            }
            else if (state.getCursorRow (scr) > 0)
            {
                state.setCursorRow (scr, state.getCursorRow (scr) - 1);
                state.setWrapPending (scr, false);
            }
            break;
        }

        case 'c':
            reset();
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
 * Currently only the G0 slot (`(`) is acted upon: `useLineDrawing` is set to
 * `true` when `finalByte == '0'` and `false` otherwise.
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
        useLineDrawing = (finalByte == '0');
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
        const int cols { grid.getCols() };
        const int visibleRows { grid.getVisibleRows() };

        for (int row { 0 }; row < visibleRows; ++row)
        {
            for (int col { 0 }; col < cols; ++col)
            {
                Cell st {};
                st.codepoint = 'E';
                st.style = 0;
                st.width = 1;
                st.layout = 0;
                grid.activeWriteCell (row, col, st);
                grid.activeEraseGrapheme (row, col);
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
    const auto scr { state.getScreen() };

    if (interCount == 0)
    {
        escDispatchNoIntermediate (scr, finalByte);
    }
    else if (interCount == 1 and inter[0] == '(')
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

namespace
{
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
        int commandNumber { 0 };   ///< Numeric OSC command code (e.g. 0, 2, 52).
        uint16_t dataStart { 0 };  ///< Byte offset of the first data byte after ';'.
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
    inline OscHeader parseOscHeader (const uint8_t* payload, uint16_t length) noexcept
    {
        OscHeader h;
        for (uint16_t i { 0 }; i < length; ++i)
        {
            if (payload[i] == ';')
            {
                h.dataStart = static_cast<uint16_t> (i + 1);
                break;
            }

            if (payload[i] >= '0' and payload[i] <= '9')
            {
                h.commandNumber = h.commandNumber * 10 + (payload[i] - '0');
            }
        }
        return h;
    }
}

/**
 * @brief Handles OSC 0 / OSC 2 — window title change.
 *
 * Extracts the title string from `data`, truncates it to `MAX_OSC_TITLE_LENGTH`
 * characters, and invokes `onTitleChanged` on the message thread via
 * `juce::MessageManager::callAsync`.
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
 * @note READER THREAD only.  The `onTitleChanged` callback is dispatched to
 *       the message thread.
 *
 * @see onTitleChanged
 * @see oscDispatch()
 * @see MAX_OSC_TITLE_LENGTH
 */
void Parser::handleOscTitle (const uint8_t* data, uint16_t dataLength) noexcept
{
    const int clampedLength { juce::jmin (static_cast<int> (dataLength), MAX_OSC_TITLE_LENGTH) };
    juce::String title (reinterpret_cast<const char*> (data), static_cast<size_t> (clampedLength));
    if (onTitleChanged)
        juce::MessageManager::callAsync ([this, title] { /* MESSAGE THREAD */ onTitleChanged (title); });
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
void Parser::handleOscClipboard (const uint8_t* data, uint16_t dataLength) noexcept
{
    if (dataLength > 2)
    {
        // Find the semicolon after the selection parameter (e.g., "c;" or "s0;")
        uint16_t payloadStart { 0 };

        for (uint16_t i { 0 }; i < dataLength; ++i)
        {
            if (*(data + i) == ';')
            {
                payloadStart = static_cast<uint16_t> (i + 1);
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
                const juce::String text (static_cast<const char*> (decoded.getData()),
                                         decoded.getSize());

                if (onClipboardChanged)
                    juce::MessageManager::callAsync ([this, text] { /* MESSAGE THREAD */ onClipboardChanged (text); });
            }
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
 * | Command | Name              | Handler                  |
 * |---------|-------------------|--------------------------|
 * | 0       | Icon + title      | `handleOscTitle()`       |
 * | 2       | Window title      | `handleOscTitle()`       |
 * | 52      | Clipboard write   | `handleOscClipboard()`   |
 * | Other   | —                 | silently ignored         |
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
void Parser::oscDispatch (const uint8_t* payload, uint16_t length) noexcept
{
    if (length >= 2)
    {
        const OscHeader h { parseOscHeader (payload, length) };

        if (h.dataStart > 0)
        {
            const uint16_t dataLength { static_cast<uint16_t> (length - h.dataStart) };
            const uint8_t* data { payload + h.dataStart };

            switch (h.commandNumber)
            {
                case 0:
                case 2:     handleOscTitle (data, dataLength);     break;
                case 52:    handleOscClipboard (data, dataLength); break;
                default:    break;
            }
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
 * received.  The current implementation is a no-op — DCS commands are not
 * processed.  Provided as an extension point for future support (e.g.
 * DECRQSS — Request Status String, or Sixel graphics).
 *
 * @par Sequence format
 * @code
 *   ESC P <params> <intermediates> <final> <data> ESC \
 * @endcode
 *
 * @param params             Finalised DCS parameter accumulator (unused).
 * @param inter              Pointer to the intermediate byte buffer (unused).
 * @param interCount         Number of valid bytes in `inter` (unused).
 * @param finalByte          The DCS final byte (unused).
 *
 * @note READER THREAD only.
 *
 * @see dcsPut()
 * @see dcsUnhook()
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
 * Invoked by the `put` action for every byte received while the parser is in
 * the `dcsPassthrough` state.  Currently a no-op — DCS data is discarded.
 * Provided as an extension point for future DCS command support.
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
 * @brief Called when a DCS sequence is terminated (ST received).
 *
 * Invoked by the `unhook` action when the String Terminator (`ESC \`) is
 * received while in the `dcsPassthrough` state.  Currently a no-op — no
 * cleanup is required since `dcsHook()` establishes no state.
 *
 * @note READER THREAD only.
 *
 * @see dcsHook()
 * @see dcsPut()
 */
void Parser::dcsUnhook() noexcept {}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
