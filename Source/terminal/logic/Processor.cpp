/**
 * @file Processor.cpp
 * @brief Implementation of the terminal pipeline orchestrator.
 *
 * Implements Processor — the pipeline half that owns State, Grid, and Parser.
 * The PTY side (TTY + History) lives in Terminal::Session.
 *
 * ### Thread contexts used in this file
 * - **MESSAGE THREAD** — JUCE message loop; all public methods except `process()`.
 * - **READER THREAD**  — byte source (Terminal::Session onBytes or IPC); only `process()`.
 *
 * @see Processor.h
 */

#include "Processor.h"
#include "../../component/TerminalDisplay.h"

#include "../data/Keyboard.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Constructs the Processor: State, Grid, Parser.
 *
 * UUID is provided by the caller — no internal generation.  The
 * `parser->writeToHost` callback is initially null; the owner (`Nexus`)
 * wires it to the appropriate sink before bytes start flowing.
 *
 * @param cols  Initial terminal column count.
 * @param rows  Initial terminal row count.
 * @param uuid  Stable UUID for this Processor — generated once by the caller.
 *
 * @note MESSAGE THREAD — must be constructed on the message thread.
 */
Processor::Processor (int cols, int rows, const juce::String& uuid)
    : grid (state)
    , uuid (uuid)
{
    state.setDimensions (cols, rows);
    grid.resize (cols, rows);

    parser = std::make_unique<Parser> (state, Grid::Writer { grid });

    parser->setScrollbackCallback ([this] (int count)
    {
        state.setScrollbackUsed (count);
    });

    parser->onClipboardChanged = [this] (const juce::String& c)
    {
        if (onClipboardChanged != nullptr)
            onClipboardChanged (c);
    };

    parser->onBell = [this]
    {
        if (onBell != nullptr)
            onBell();
    };

    parser->onDesktopNotification = [this] (const juce::String& title, const juce::String& body)
    {
        if (onDesktopNotification != nullptr)
            onDesktopNotification (title, body);
    };
}

// =============================================================================

/**
 * @brief Resizes the grid and notifies the parser.
 *
 * Does NOT touch any TTY — SIGWINCH is handled by Terminal::Session.
 * Called from Display when its component resizes.
 *
 * @param cols  New terminal width in character columns.
 * @param rows  New terminal height in character rows.
 * @note MESSAGE THREAD only.
 */
void Processor::resized (int cols, int rows) noexcept
{
    jassert (parser != nullptr);
    state.setDimensions (cols, rows);
    grid.resize (cols, rows);
    parser->resize (cols, rows);
}

// =============================================================================

/**
 * @brief Encodes a JUCE key press into a VT escape sequence.
 *
 * Const — no side effects, does NOT write to any PTY.
 *
 * @note MESSAGE THREAD only.
 */
juce::String Processor::encodeKeyPress (const juce::KeyPress& key) const noexcept
{
    juce::String seq;

#if JUCE_WINDOWS
    if (state.getMode (ID::win32InputMode))
    {
        seq = Keyboard::encodeWin32Input (key);
    }
    else
#endif
    {
        const bool applicationCursor { state.getMode (ID::applicationCursor) };
        const uint32_t keyboardFlags { state.getKeyboardFlags() };
        seq = Keyboard::map (key, applicationCursor, keyboardFlags);
    }

    return seq;
}

/**
 * @brief Encodes a bracketed-paste text block into a byte sequence.
 *
 * Const — no side effects, does NOT write to any PTY.
 *
 * @note MESSAGE THREAD only.
 */
juce::String Processor::encodePaste (const juce::String& text) const noexcept
{
    juce::String result;

    if (text.isNotEmpty())
    {
        const bool bracketed { state.getMode (ID::bracketedPaste) };

        if (bracketed)
        {
            static constexpr const char open[]  { "\x1b[200~" };
            static constexpr const char close[] { "\x1b[201~" };
            result = juce::String (open) + text + juce::String (close);
        }
        else
        {
            result = text;
        }
    }

    return result;
}

/**
 * @brief Encodes a focus-in or focus-out event into a VT byte sequence.
 *
 * Const — no side effects, does NOT write to any PTY.
 *
 * @note MESSAGE THREAD only.
 */
juce::String Processor::encodeFocusEvent (bool gained) const noexcept
{
    juce::String result;

    if (state.getMode (ID::focusEvents))
    {
        const char seq[4] { '\x1b', '[', gained ? 'I' : 'O', '\0' };
        result = juce::String (seq);
    }

    return result;
}

/**
 * @brief Encodes a mouse event into a VT SGR byte sequence.
 *
 * Const — no side effects, does NOT write to any PTY.
 *
 * @note MESSAGE THREAD only.
 */
juce::String Processor::encodeMouseEvent (int button, int col, int row, bool press) const noexcept
{
    const char finalChar { press ? 'M' : 'm' };
    return juce::String ("\x1b[<")
           + juce::String (button) + ";"
           + juce::String (col + 1) + ";"
           + juce::String (row + 1)
           + finalChar;
}

/**
 * @brief Processes raw bytes through the parser pipeline.
 *
 * Feeds `Parser::process()` then fires `sendChangeMessage()` to notify Display.
 *
 * @note READER THREAD only — called from the byte source (Terminal::Session
 *       onBytes callback or IPC dispatch in client mode).
 */
void Processor::process (const char* data, int length) noexcept
{
    jassert (parser != nullptr);
    parser->process (reinterpret_cast<const uint8_t*> (data), static_cast<size_t> (length));
    state.consumePasteEcho (length);
    sendChangeMessage();
}

State& Processor::getState() noexcept       { return state; }
const State& Processor::getState() const noexcept { return state; }

Grid& Processor::getGrid() noexcept        { return grid; }
const Grid& Processor::getGrid() const noexcept { return grid; }

const juce::String& Processor::getUuid() const noexcept { return uuid; }

/**
 * @brief Sets the scroll offset, clamped to [0, scrollbackUsed].
 *
 * Acquires the Grid resize lock, clamps @p newOffset, and writes to State
 * only when the value changes.
 *
 * @note MESSAGE THREAD.
 */
void Processor::setScrollOffsetClamped (int newOffset) noexcept
{
    const juce::ScopedLock lock (grid.getResizeLock());
    const int maxOffset { grid.getScrollbackUsed() };
    const int current { state.getScrollOffset() };
    const int clamped { juce::jlimit (0, maxOffset, newOffset) };

    if (clamped != current)
        state.setScrollOffset (clamped);
}

/**
 * @brief Returns a const reference to the VT parser.
 * @note Asserts if parser is null (should never be in this new design).
 */
const Parser& Processor::getParser() const noexcept
{
    jassert (parser != nullptr);
    return *parser;
}

Parser& Processor::getParser() noexcept
{
    jassert (parser != nullptr);
    return *parser;
}

/**
 * @brief Acquires the grid resize lock and calls process().
 *
 * Convenience for reader-thread callers (e.g. the daemon's `onBytes` callback) that must hold
 * the resize lock for the duration of the parse.
 *
 * @note READER THREAD only.
 */
void Processor::processWithLock (const char* data, int len) noexcept
{
    const juce::ScopedLock lock (grid.getResizeLock());
    process (data, len);
}

/**
 * @brief Wires parser.writeToHost to the given callback.
 *
 * @note MESSAGE THREAD — call before the first process() invocation.
 */
void Processor::setHostWriter (std::function<void (const char*, int)> writer) noexcept
{
    jassert (parser != nullptr);
    parser->writeToHost = std::move (writer);
}

/**
 * @brief Creates and returns a Display for this Processor.
 *
 * @param font  Font instance providing metrics, shaping, and rasterisation.
 * @return Unique pointer to the newly created Display.
 * @note MESSAGE THREAD.
 */
std::unique_ptr<Display> Processor::createDisplay (jreng::Typeface& font)
{
    auto display { std::make_unique<Display> (*this, font) };
    display->setComponentID (uuid);
    return display;
}

// =============================================================================

/**
 * @brief Serializes Grid and State into a portable snapshot for session restore.
 *
 * Binary format:
 *   [Header]
 *     uint32_t version = 1
 *     int32_t  cols
 *     int32_t  visibleRows
 *     int32_t  activeScreen (0=normal, 1=alternate)
 *
 *   [Grid section — two buffers, delegated to Grid::getStateInformation]
 *
 *   [State section]
 *     Per screen (normal then alternate):
 *       int32_t cursorRow
 *       int32_t cursorCol
 *       int32_t cursorVisible (0 or 1)
 *       int32_t cursorShape
 *       int32_t scrollTop
 *       int32_t scrollBottom
 *       int32_t wrapPending (0 or 1)
 *     Mode flags (13 × int32_t, 0 or 1):
 *       originMode, autoWrap, applicationCursor, bracketedPaste, insertMode,
 *       mouseTracking, mouseMotionTracking, mouseAllTracking, mouseSgr,
 *       focusEvents, applicationKeypad, reverseVideo, win32InputMode
 *
 * @note MESSAGE THREAD.
 */
void Processor::getStateInformation (juce::MemoryBlock& destData) const
{
    const uint32_t version      { 1 };
    const int32_t  cols         { static_cast<int32_t> (state.getCols()) };
    const int32_t  visibleRows  { static_cast<int32_t> (state.getVisibleRows()) };
    const int32_t  activeScreen { static_cast<int32_t> (state.getActiveScreen()) };

    destData.append (&version,      sizeof (uint32_t));
    destData.append (&cols,         sizeof (int32_t));
    destData.append (&visibleRows,  sizeof (int32_t));
    destData.append (&activeScreen, sizeof (int32_t));

    grid.getStateInformation (destData);

    const ActiveScreen screens[2] { normal, alternate };

    for (const ActiveScreen screen : screens)
    {
        const int32_t cursorRow     { static_cast<int32_t> (state.getRawValue<int> (state.screenKey (screen, ID::cursorRow))) };
        const int32_t cursorCol     { static_cast<int32_t> (state.getRawValue<int> (state.screenKey (screen, ID::cursorCol))) };
        const int32_t cursorVisible { state.getRawValue<bool> (state.screenKey (screen, ID::cursorVisible)) ? 1 : 0 };
        const int32_t cursorShape   { static_cast<int32_t> (state.getRawValue<int> (state.screenKey (screen, ID::cursorShape))) };
        const int32_t scrollTop     { static_cast<int32_t> (state.getRawValue<int> (state.screenKey (screen, ID::scrollTop))) };
        const int32_t scrollBottom  { static_cast<int32_t> (state.getRawValue<int> (state.screenKey (screen, ID::scrollBottom))) };
        const int32_t wrapPending   { state.getRawValue<bool> (state.screenKey (screen, ID::wrapPending)) ? 1 : 0 };

        destData.append (&cursorRow,     sizeof (int32_t));
        destData.append (&cursorCol,     sizeof (int32_t));
        destData.append (&cursorVisible, sizeof (int32_t));
        destData.append (&cursorShape,   sizeof (int32_t));
        destData.append (&scrollTop,     sizeof (int32_t));
        destData.append (&scrollBottom,  sizeof (int32_t));
        destData.append (&wrapPending,   sizeof (int32_t));
    }

    const juce::Identifier modeIds[13]
    {
        ID::originMode, ID::autoWrap, ID::applicationCursor, ID::bracketedPaste,
        ID::insertMode, ID::mouseTracking, ID::mouseMotionTracking, ID::mouseAllTracking,
        ID::mouseSgr, ID::focusEvents, ID::applicationKeypad, ID::reverseVideo,
        ID::win32InputMode
    };

    for (const juce::Identifier& modeId : modeIds)
    {
        const int32_t flag { state.getRawValue<bool> (state.modeKey (modeId)) ? 1 : 0 };
        destData.append (&flag, sizeof (int32_t));
    }
}

/**
 * @brief Restores Grid and State from a snapshot produced by getStateInformation.
 *
 * Reads the header, delegates grid buffer restore to Grid::setStateInformation,
 * then reads per-screen and mode State fields. Uses a single forward cursor
 * that parses the grid section manually to locate the State section boundary.
 *
 * @note MESSAGE THREAD.
 */
void Processor::setStateInformation (const void* data, int size)
{
    const char* cursor { static_cast<const char*> (data) };
    const char* const end { cursor + size };

    static constexpr int headerBytes { static_cast<int> (sizeof (uint32_t)) + 3 * static_cast<int> (sizeof (int32_t)) };

    if ((cursor + headerBytes) <= end)
    {
        uint32_t version     { 0 };
        int32_t  cols        { 0 };
        int32_t  visibleRows { 0 };
        int32_t  activeScr   { 0 };

        std::memcpy (&version,     cursor, sizeof (uint32_t)); cursor += sizeof (uint32_t);
        std::memcpy (&cols,        cursor, sizeof (int32_t));  cursor += sizeof (int32_t);
        std::memcpy (&visibleRows, cursor, sizeof (int32_t));  cursor += sizeof (int32_t);
        std::memcpy (&activeScr,   cursor, sizeof (int32_t));  cursor += sizeof (int32_t);

        if (version == 1)
        {
            state.setDimensions (static_cast<int> (cols), static_cast<int> (visibleRows));

            // Delegate grid section to Grid (cursor points to start of grid section)
            const int gridRemaining { static_cast<int> (end - cursor) };
            grid.setStateInformation (cursor, gridRemaining);

            // Walk cursor past the grid section (2 buffers × [5 scalars + bulk arrays])
            bool gridWalkOk { true };

            for (int screenIndex { 0 }; screenIndex < 2 and gridWalkOk; ++screenIndex)
            {
                static constexpr int scalarBytes { 5 * static_cast<int> (sizeof (int32_t)) };

                if ((cursor + scalarBytes) <= end)
                {
                    int32_t sTotalRows { 0 };
                    int32_t sCols      { 0 };

                    cursor += sizeof (int32_t); // head
                    cursor += sizeof (int32_t); // scrollbackUsed
                    std::memcpy (&sTotalRows, cursor, sizeof (int32_t)); cursor += sizeof (int32_t);
                    std::memcpy (&sCols,      cursor, sizeof (int32_t)); cursor += sizeof (int32_t);
                    cursor += sizeof (int32_t); // allocatedVisibleRows

                    const size_t cellCount { static_cast<size_t> (sTotalRows) * static_cast<size_t> (sCols) };
                    const ptrdiff_t bulkBytes { static_cast<ptrdiff_t> (cellCount * sizeof (Cell)
                                              + cellCount * sizeof (Grapheme)
                                              + static_cast<size_t> (sTotalRows) * sizeof (RowState)) };

                    if ((cursor + bulkBytes) <= end)
                        cursor += bulkBytes;
                    else
                        gridWalkOk = false;
                }
                else
                {
                    gridWalkOk = false;
                }
            }

            // cursor now points to the State section
            static constexpr int perScreenBytes { 7 * static_cast<int> (sizeof (int32_t)) };
            static constexpr int modeCount      { 13 };
            static constexpr int stateBytes     { 2 * perScreenBytes + modeCount * static_cast<int> (sizeof (int32_t)) };

            if (gridWalkOk and (cursor + stateBytes) <= end)
            {
                const ActiveScreen screens[2] { normal, alternate };

                for (const ActiveScreen screen : screens)
                {
                    int32_t cursorRow     { 0 };
                    int32_t cursorCol     { 0 };
                    int32_t cursorVisible { 1 };
                    int32_t cursorShape   { 0 };
                    int32_t scrollTop     { 0 };
                    int32_t scrollBottom  { 0 };
                    int32_t wrapPending   { 0 };

                    std::memcpy (&cursorRow,     cursor, sizeof (int32_t)); cursor += sizeof (int32_t);
                    std::memcpy (&cursorCol,     cursor, sizeof (int32_t)); cursor += sizeof (int32_t);
                    std::memcpy (&cursorVisible, cursor, sizeof (int32_t)); cursor += sizeof (int32_t);
                    std::memcpy (&cursorShape,   cursor, sizeof (int32_t)); cursor += sizeof (int32_t);
                    std::memcpy (&scrollTop,     cursor, sizeof (int32_t)); cursor += sizeof (int32_t);
                    std::memcpy (&scrollBottom,  cursor, sizeof (int32_t)); cursor += sizeof (int32_t);
                    std::memcpy (&wrapPending,   cursor, sizeof (int32_t)); cursor += sizeof (int32_t);

                    state.setCursorRow     (screen, static_cast<int> (cursorRow));
                    state.setCursorCol     (screen, static_cast<int> (cursorCol));
                    state.setCursorVisible (screen, cursorVisible != 0);
                    state.setCursorShape   (screen, static_cast<int> (cursorShape));
                    state.setScrollTop     (screen, static_cast<int> (scrollTop));
                    state.setScrollBottom  (screen, static_cast<int> (scrollBottom));
                    state.setWrapPending   (screen, wrapPending != 0);
                }

                const juce::Identifier modeIds[modeCount]
                {
                    ID::originMode, ID::autoWrap, ID::applicationCursor, ID::bracketedPaste,
                    ID::insertMode, ID::mouseTracking, ID::mouseMotionTracking, ID::mouseAllTracking,
                    ID::mouseSgr, ID::focusEvents, ID::applicationKeypad, ID::reverseVideo,
                    ID::win32InputMode
                };

                for (const juce::Identifier& modeId : modeIds)
                {
                    int32_t flag { 0 };
                    std::memcpy (&flag, cursor, sizeof (int32_t));
                    cursor += sizeof (int32_t);
                    state.setMode (modeId, flag != 0);
                }
            }

            state.setScreen (static_cast<ActiveScreen> (activeScr));
            state.refresh();
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
