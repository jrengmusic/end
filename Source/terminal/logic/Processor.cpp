/**
 * @file Processor.cpp
 * @brief Implementation of the terminal pipeline orchestrator.
 *
 * Implements Processor — the pipeline half that owns State (the APVTS), Video
 * (the terminal state machine), and Parser, and references Grid owned by
 * Terminal::Session.  The PTY side (TTY + History) lives in Terminal::Session.
 *
 * ### Thread contexts used in this file
 * - **MESSAGE THREAD** — JUCE message loop; all public methods except `process()`.
 * - **READER THREAD**  — byte source (Terminal::Session onBytes or IPC); only `process()`.
 *
 * @see Processor.h
 */

#include "Processor.h"
#include "../../component/TerminalDisplay.h"
#include <chrono>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Constructs the Processor: binds Grid&, constructs State, Video, and Parser.
 *
 * Grid is owned by Terminal::Session and must outlive this Processor.
 * State is owned by this Processor (the APVTS).
 * Video is owned by this Processor and receives Grid& and events& references.
 * Parser is owned by this Processor and receives Video& reference directly.
 * UUID is provided by the caller — no internal generation.
 *
 * @param gridRef  Live cell buffer owned by Terminal::Session.
 * @param cols     Initial terminal column count.
 * @param rows     Initial terminal row count.
 * @param uuid     Stable UUID for this Processor — generated once by the caller.
 *
 * @note MESSAGE THREAD — must be constructed on the message thread.
 */
Processor::Processor (Grid& gridRef, TextBuffer& textBufferRef, int cols, int rows, const juce::String& uuid)
    : grid (gridRef)
    , textBuffer (textBufferRef)
    , state (textBufferRef)
    , video (grid, events)
    , skit (events)
    , uuid (uuid)
{
    state.setDimensions (cols, rows);
    video.setDimensions (cols, rows);
    registerEvents();
    parser = std::make_unique<Parser> (video);
    state.get().addListener (this);
}

/**
 * @brief Destroys the Processor.
 *
 * No explicit `removeListener()` needed — the ValueTree inside State is
 * destroyed alongside this Processor (member destruction order).
 *
 * @note MESSAGE THREAD.
 */
Processor::~Processor() = default;

// =============================================================================

/**
 * @brief Registers Processor-owned event handlers on the events map.
 *
 * Intercepts events fired by Video that require State access:
 *
 * - `"registerLink"` — receives the OSC 8 URI and params strings, registers the
 *   URI in State, and writes the returned ID back to Video via `setActiveLinkId()`.
 *   Synchronous (reader thread); no async dispatch needed because the event fires
 *   on the reader thread and `activeLinkId` is consumed immediately for cell stamping.
 *
 * - `"promptRow"` — OSC 133 A; converts screen-relative row to absolute and
 *   calls `State::setPromptRow()`.
 *
 * - `"outputBlockStart"` — OSC 133 C; converts screen-relative row and calls
 *   `State::setOutputBlockStart()`.
 *
 * - `"outputBlockEnd"` — OSC 133 D; converts screen-relative row and calls
 *   `State::setOutputBlockEnd()`.
 *
 * - `"extendOutputBlock"` — LF while scan-active; converts screen-relative row
 *   and calls `State::extendOutputBlock()`.
 *
 * - `"snapshotDirty"` — cell erase paths; calls `State::setSnapshotDirty()`.
 *
 * - `"title"` — OSC 0/2 window title; calls `State::setTitle()`.
 *
 * - `"cwd"` — OSC 7 working directory; calls `State::setCwd()`.
 *
 * - `"cursorColor"` — OSC 12 cursor colour set; calls `State::setCursorColor()`.
 *
 * - `"resetCursorColor"` — OSC 112 cursor colour reset; calls `State::resetCursorColor()`.
 *
 * - `"cursorShape"` — DECSCUSR; calls `State::setCursorShape()`.
 *
 * - `"pushKeyboardMode"` — CSI > u; calls `State::pushKeyboardMode()`.
 *
 * - `"popKeyboardMode"` — CSI < u; calls `State::popKeyboardMode()`.
 *
 * - `"syncOutput"` — DEC mode 2026 toggle; calls `State::setSyncOutput()`.
 *
 * - `"requestSyncResize"` — DEC mode 2026 set; calls `State::requestSyncResize()`.
 *
 * - `"bell"`, `"clipboardChanged"`, `"desktopNotification"` — placeholder registrations
 *   so `events.contains()` passes; Session registers the actual handlers after construction.
 *
 * - `"activeScreen"` / `"cursorRow"` / `"cursorCol"` / `"cursorVisible"` — Video::flushState()
 *   flush events; write the corresponding State atomics.
 *
 * - `"applicationCursor"` / `"bracketedPaste"` / ... — Video::flushState() mode flag flushes.
 *
 * - `"screenSwitch"` — Video::setScreen(); saves old cursor to State, loads new cursor from
 *   State atomics, calls `Video::loadScreenState()`.  Synchronous on reader thread.
 *
 * - `"dcsPayloadComplete"` — Video::applyDCSPayload(); delegates to Skit::processDCS()
 *   then Video::advanceCursorForImage().  Synchronous on reader thread.
 *
 * - `"apcPayloadComplete"` — Video::applyAPCPayload(); delegates to Skit::processAPC(),
 *   forwards any Kitty response via writeToHost, then Video::advanceCursorForImage().
 *   Synchronous on reader thread.
 *
 * @note READER THREAD — all handlers execute on the reader thread unless dispatched
 *       via `callAsync` (bell, clipboardChanged, desktopNotification land on the message thread).
 */
void Processor::registerEvents() noexcept
{
    // C5: OSC 8 hyperlink — link URI registration removed; hyperlink IDs are disabled pending replacement.
    events.add<const juce::String&, const juce::String&> (ID::registerLink,
        [this] (const juce::String& /*uri*/, const juce::String& /*params*/)
    {
        video.setActiveLinkId (0);
    });

    // C6: Shell integration — scrollback stub returns 0, so screen-relative == absolute.
    events.add<int> (ID::promptRow,
        [this] (int relativeRow)
    {
        state.setPromptRow (state.getScrollbackUsed() + relativeRow);
    });

    events.add<int> (ID::outputBlockStart,
        [this] (int relativeRow)
    {
        state.setOutputBlockStart (state.getScrollbackUsed() + relativeRow);
    });

    events.add<int> (ID::outputBlockEnd,
        [this] (int relativeRow)
    {
        state.setOutputBlockEnd (state.getScrollbackUsed() + relativeRow);
    });

    events.add<int> (ID::extendOutputBlock,
        [this] (int relativeRow)
    {
        state.extendOutputBlock (state.getScrollbackUsed() + relativeRow);
    });

    // OSC 1337 raw payload from Video — delegate to Skit, then advance cursor.
    events.add<const uint8_t*, int, int, int> (ID::osc1337Raw,
        [this] (const uint8_t* data, int length, int cursorRow, int cursorCol)
    {
        skit.processOSC1337 (data, length, cursorRow, cursorCol);
        video.advanceCursorForImage (skit.getLastImageRows());
    });

    // DCS payload complete — delegate to Skit, then advance cursor.
    events.add<const uint8_t*, int> (ID::dcsPayloadComplete,
        [this] (const uint8_t* data, int length)
    {
        skit.processDCS (video.getDcsFinalByte(), data, length,
                         video.getCursorRow(), video.getCursorCol());
        video.advanceCursorForImage (skit.getLastImageRows());
    });

    // APC payload complete — delegate to Skit, forward any Kitty response, then advance cursor.
    events.add<const uint8_t*, int> (ID::apcPayloadComplete,
        [this] (const uint8_t* data, int length)
    {
        skit.processAPC (data, length,
                         video.getCursorRow(), video.getCursorCol());

        const juce::String& response { skit.getLastResponse() };
        if (response.isNotEmpty() and events.contains (ID::writeToHost))
            events.get (ID::writeToHost, response.toRawUTF8(), int (response.getNumBytesAsUTF8()));

        video.advanceCursorForImage (skit.getLastImageRows());
    });

    // Cell erase — mark snapshot dirty so the renderer sees erased content.
    events.add (ID::snapshotDirty, [this] { state.setSnapshotDirty(); });

    // OSC 0/2 window title.
    events.add<const char*, int> (ID::title,
        [this] (const char* data, int length)
    {
        state.setTitle (data, length);
    });

    // OSC 7 current working directory.
    events.add<const char*, int> (ID::cwd,
        [this] (const char* data, int length)
    {
        state.setCwd (data, length);
    });

    // OSC 12 cursor colour set — packed ARGB from juce::Colour.
    events.add<int, juce::Colour> (ID::cursorColor,
        [this] (int screen, juce::Colour colour)
    {
        state.setCursorColor (screen, colour);
    });

    // OSC 112 cursor colour reset — revert to user config default.
    events.add<int> (ID::resetCursorColor,
        [this] (int screen)
    {
        state.resetCursorColor (screen);
    });

    // DECSCUSR cursor shape.
    events.add<int, int> (ID::cursorShape,
        [this] (int screen, int shape)
    {
        state.setCursorShape (screen, shape);
    });

    // CSI > u — push keyboard enhancement flags onto the per-screen stack.
    events.add<int, uint32_t> (ID::pushKeyboardMode,
        [this] (int screen, uint32_t flags)
    {
        state.pushKeyboardMode (screen, flags);
    });

    // CSI < u — pop keyboard enhancement flags from the per-screen stack.
    events.add<int, int> (ID::popKeyboardMode,
        [this] (int screen, int count)
    {
        state.popKeyboardMode (screen, count);
    });

    // DEC mode 2026 synchronized output toggle.
    events.add<bool> (ID::syncOutput,
        [this] (bool active)
    {
        state.setSyncOutput (active);
    });

    // DEC mode 2026 set — request same-size PTY resize on next drain completion.
    events.add (ID::requestSyncResize, [this] { state.requestSyncResize(); });

    // State delivery: activeScreen.
    events.add<int> (ID::activeScreen,
        [this] (int scr)
    {
        state.setScreen (scr);
    });

    // State delivery: cursor row for active screen.
    events.add<int, int> (ID::cursorRow,
        [this] (int scr, int row)
    {
        state.setCursorRow (scr, row);
    });

    // State delivery: cursor column for active screen.
    events.add<int, int> (ID::cursorCol,
        [this] (int scr, int col)
    {
        state.setCursorCol (scr, col);
    });

    // State delivery: cursor visibility for active screen.
    events.add<int, bool> (ID::cursorVisible,
        [this] (int scr, bool visible)
    {
        state.setCursorVisible (scr, visible);
    });

    // State delivery: mode flags.
    events.add<bool> (ID::applicationCursor,
        [this] (bool value) { state.setMode (ID::applicationCursor, value); });

    events.add<bool> (ID::bracketedPaste,
        [this] (bool value) { state.setMode (ID::bracketedPaste, value); });

    events.add<bool> (ID::mouseTracking,
        [this] (bool value) { state.setMode (ID::mouseTracking, value); });

    events.add<bool> (ID::mouseMotionTracking,
        [this] (bool value) { state.setMode (ID::mouseMotionTracking, value); });

    events.add<bool> (ID::mouseAllTracking,
        [this] (bool value) { state.setMode (ID::mouseAllTracking, value); });

    events.add<bool> (ID::focusEvents,
        [this] (bool value) { state.setMode (ID::focusEvents, value); });

    events.add<bool> (ID::win32InputMode,
        [this] (bool value) { state.setMode (ID::win32InputMode, value); });

    // Screen switch — fired by Video::setScreen() with the OLD screen's cursor values.
    // Saves old cursor to State, loads new cursor from State atomics, calls video.loadScreenState().
    // scrollTop/scrollBottom are always reset to 0 on screen switch (matches original setScreen behaviour).
    // wrapPending is always cleared on screen switch.
    events.add<int, int, int, bool, int, int, bool, uint32_t> (ID::screenSwitch,
        [this] (int newScreen,
                int oldRow, int oldCol, bool oldVisible,
                int /*oldScrollTop*/, int /*oldScrollBottom*/, bool /*oldWrapPending*/,
                uint32_t /*oldKeyboardFlags*/)
    {
        const int oldScreen { newScreen == Screen::Map::alternate ? Screen::Map::normal : Screen::Map::alternate };

        state.setCursorRow (oldScreen, oldRow);
        state.setCursorCol (oldScreen, oldCol);
        state.setCursorVisible (oldScreen, oldVisible);

        const int newRow          { state.loadCursorRow (newScreen) };
        const int newCol          { state.loadCursorCol (newScreen) };
        const bool newVisible     { state.loadCursorVisible (newScreen) };
        const uint32_t newKbFlags { state.loadKeyboardFlags (newScreen) };

        // scrollTop/scrollBottom reset to 0 (sentinel = full screen); wrapPending cleared.
        video.loadScreenState (newRow, newCol, newVisible, 0, 0, false, newKbFlags);
    });

    // BEL — placeholder so events.contains() passes; Session registers the actual handler.
    events.add (ID::bell, [] {});

    // OSC 52 clipboard write — placeholder; Session registers the actual handler.
    events.add<const juce::String&> (ID::clipboardChanged,
        [] (const juce::String& /*text*/) {});

    // OSC 9 / OSC 777 desktop notification — placeholder; Session registers the actual handler.
    events.add<const juce::String&, const juce::String&> (ID::desktopNotification,
        [] (const juce::String& /*title*/, const juce::String& /*body*/) {});
}

// =============================================================================

// =============================================================================

/**
 * @brief ValueTree::Listener — reacts to top-down dimension changes from Display.
 *
 * Fires on the message thread when Display writes new dimensions to State.
 * Mirrors `ProcessorChain::parameterChanged`: State changed → push to Video.
 * Also resizes Grid and fires `onResize` for PTY SIGWINCH.
 *
 * @note MESSAGE THREAD.
 */
void Processor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    // PARAM children flush via ID::value — check the id property to identify cols/visibleRows.
    if (property == ID::value and tree.getType() == jam::ValueTree::PARAM)
    {
        const juce::Identifier paramId { tree.getProperty (ID::id).toString() };

        if (paramId == ID::cols or paramId == ID::visibleRows)
        {
            const int numCols { state.getCols() };
            const int numRows { state.getVisibleRows() };

            if (numCols > 0 and numRows > 0)
                resized (numCols, numRows);
        }

        if (paramId == ID::outputBlockTop)
        {
            if (onCommandStarted != nullptr)
                onCommandStarted();
        }

        if (paramId == ID::promptRow)
        {
            if (onCommandEnded != nullptr)
                onCommandEnded();
        }
    }

    // TEXT parameters flush as direct properties on the SESSION root node.
    // When foregroundProcess or cwd change, recompute displayName.
    if (property == ID::foregroundProcess or property == ID::cwd)
    {
        const auto foreground { tree.getProperty (ID::foregroundProcess).toString() };
        const auto cwdPath    { tree.getProperty (ID::cwd).toString() };
        juce::String name;

        if (foreground.isNotEmpty())
        {
            name = foreground;
        }
        else if (cwdPath.isNotEmpty())
        {
            name = juce::File (cwdPath).getFileName();
        }

        if (name.isNotEmpty())
            state.get().setProperty (App::ID::displayName, name, nullptr);
    }
}

// =============================================================================

/**
 * @brief Stores pending dimensions for deferred application on the reader thread.
 *
 * Does NOT write to Video or Grid — those are reader-thread-only objects.
 * process() reads the pending values at batch start and applies them there.
 *
 * @param cols  New terminal width in character columns.
 * @param rows  New terminal height in character rows.
 * @note MESSAGE THREAD only.
 */
void Processor::resized (int cols, int rows) noexcept
{
    jassert (parser != nullptr);
    pendingCols.store (cols, std::memory_order_relaxed);
    pendingRows.store (rows, std::memory_order_relaxed);
    resizePending.store (true, std::memory_order_release);
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
            static constexpr const char open[] { "\x1b[200~" };
            static constexpr const char close[] { "\x1b[201~" };
            const juce::String normalized { text.replace ("\r\n", "\n").replace ("\r", "\n") };
            result = juce::String (open) + normalized + juce::String (close);
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
    return juce::String ("\x1b[<") + juce::String (button) + ";" + juce::String (col + 1) + ";" + juce::String (row + 1)
           + finalChar;
}

/**
 * @brief Processes raw bytes through the parser pipeline.
 *
 * Lock is held only during resize (buffer reallocation) — not for the entire
 * call.  Applies any pending resize or cell-size change at batch start (reader
 * thread), then feeds `Parser::process()`, flushes Video state, and flushes
 * responses.  Pending changes originate from message-thread calls to
 * resized() / setCellSize() and are transferred via atomics — all Video/Grid
 * writes happen here on the reader thread.
 *
 * @note READER THREAD only — called from the byte source (Terminal::Session
 *       onBytes callback or IPC dispatch in client mode).
 */
void Processor::process (const char* data, int length) noexcept
{
    jassert (parser != nullptr);

    static int64_t callCount { 0 };
    static double lockMs { 0.0 };
    static double parseMs { 0.0 };
    static double flushStateMs { 0.0 };
    static double restMs { 0.0 };
    static int64_t totalBytes { 0 };

    const auto t0 { std::chrono::steady_clock::now() };
    const auto t1 { t0 };  // No lock acquisition — lockMs will be 0.

    if (resizePending.exchange (false, std::memory_order_acquire))
    {
        const int cols { pendingCols.load (std::memory_order_relaxed) };
        const int rows { pendingRows.load (std::memory_order_relaxed) };
        grid.setSize (rows, cols);
        video.setDimensions (cols, rows);
        video.resize (cols, rows);
    }

    if (cellSizePending.exchange (false, std::memory_order_acquire))
    {
        const int w { pendingCellWidth.load (std::memory_order_relaxed) };
        const int h { pendingCellHeight.load (std::memory_order_relaxed) };
        video.setCellSize (w, h);
        skit.setCellSize (w, h);
    }

    parser->process (reinterpret_cast<const uint8_t*> (data), static_cast<size_t> (length));

    const auto t2 { std::chrono::steady_clock::now() };

    video.flushState();

    const auto t3 { std::chrono::steady_clock::now() };

    video.flushResponses();
    state.consumePasteEcho (length);

    const auto t4 { std::chrono::steady_clock::now() };

    lockMs       += std::chrono::duration<double, std::milli> (t1 - t0).count();
    parseMs      += std::chrono::duration<double, std::milli> (t2 - t1).count();
    flushStateMs += std::chrono::duration<double, std::milli> (t3 - t2).count();
    restMs       += std::chrono::duration<double, std::milli> (t4 - t3).count();
    totalBytes   += length;
    ++callCount;

    if (callCount % 2000 == 0)
    {
        jam::debug::Log::write (
            juce::String ("PROC: calls=") + juce::String (callCount)
            + " bytes=" + juce::String (totalBytes)
            + " lockMs=" + juce::String (lockMs, 1)
            + " parseMs=" + juce::String (parseMs, 1)
            + " flushStateMs=" + juce::String (flushStateMs, 1)
            + " restMs=" + juce::String (restMs, 1));

        lockMs       = 0.0;
        parseMs      = 0.0;
        flushStateMs = 0.0;
        restMs       = 0.0;
        totalBytes   = 0;
    }
}

State& Processor::getState() noexcept { return state; }
const State& Processor::getState() const noexcept { return state; }

Grid& Processor::getGrid() noexcept { return grid; }
const Grid& Processor::getGrid() const noexcept { return grid; }

const juce::String& Processor::getUuid() const noexcept { return uuid; }

void Processor::flushResponses() noexcept
{
    video.flushResponses();
}

/**
 * @brief Registers the `writeToHost` event handler in the events map.
 *
 * @note MESSAGE THREAD — call before the first process() invocation.
 */
void Processor::setHostWriter (std::function<void (const char*, int)> writer) noexcept
{
    events.add<const char*, int> (ID::writeToHost, std::move (writer));
}

/**
 * @brief Stores pending cell pixel dimensions for deferred application on the reader thread.
 *
 * Does NOT write to Video or Skit directly — those are reader-thread-only objects.
 * process() reads the pending values at batch start and applies them there.
 *
 * @param widthPx   Cell width in pixels.
 * @param heightPx  Cell height in pixels.
 * @note MESSAGE THREAD.
 */
void Processor::setCellSize (int widthPx, int heightPx) noexcept
{
    pendingCellWidth.store (widthPx, std::memory_order_relaxed);
    pendingCellHeight.store (heightPx, std::memory_order_relaxed);
    cellSizePending.store (true, std::memory_order_release);
}

/**
 * @brief Creates and returns a Display for this Processor.
 *
 * @return Unique pointer to the newly created Display.
 * @note MESSAGE THREAD.
 */
std::unique_ptr<Display> Processor::createDisplay()
{
    auto display { std::make_unique<Display> (*this) };
    display->setComponentID (uuid);
    return display;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
