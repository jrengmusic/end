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

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Constructs the Processor: binds Grid&, constructs State, Video, and Parser.
 *
 * Grid is owned by Terminal::Session and must outlive this Processor.
 * State is owned by this Processor (the APVTS).
 * Video is owned by this Processor and receives State&, Grid&, and events& references.
 * Parser is owned by this Processor and receives commands& reference.
 * UUID is provided by the caller — no internal generation.
 *
 * @param gridRef  Live cell buffer owned by Terminal::Session.
 * @param cols     Initial terminal column count.
 * @param rows     Initial terminal row count.
 * @param uuid     Stable UUID for this Processor — generated once by the caller.
 *
 * @note MESSAGE THREAD — must be constructed on the message thread.
 */
Processor::Processor (Grid& gridRef, int cols, int rows, const juce::String& uuid)
    : grid (gridRef)
    , video (grid, events)
    , skit (events)
    , uuid (uuid)
{
    state.setDimensions (cols, rows);
    video.setDimensions (cols, rows);
    registerCommands();
    registerEvents();
    parser = std::make_unique<Parser> (commands);
    state.get().addListener (this);
}

/**
 * @brief Destroys the Processor — removes the ValueTree listener before teardown.
 *
 * @note MESSAGE THREAD — must be destroyed on the message thread.
 */
Processor::~Processor() = default;

// =============================================================================

/**
 * @brief Registers all command handlers that forward Parser actions to Video.
 *
 * Each handler is keyed by `Command::Type` and calls the corresponding
 * `Video` action method.  All commands are registered unconditionally —
 * no `contains()` check is needed at dispatch time.
 *
 * @note MESSAGE THREAD — called once from the constructor.
 */
void Processor::registerCommands() noexcept
{
    using CT = Command::Type;

    commands.add<uint32_t> (CT::print, [this] (uint32_t codepoint)
    {
        video.print (codepoint);
        syncVideoToState();
    });

    commands.add<uint8_t> (CT::applyControlCode, [this] (uint8_t controlByte)
    {
        video.applyControlCode (controlByte);
        syncVideoToState();
    });

    commands.add<const CSI&, const uint8_t*, uint8_t, uint8_t> (CT::applyCSI,
        [this] (const CSI& csi, const uint8_t* inter, uint8_t interCount, uint8_t finalByte)
    {
        video.applyCSI (csi, inter, interCount, finalByte);
        syncVideoToState();
    });

    commands.add<const uint8_t*, uint8_t, uint8_t> (CT::applyESC,
        [this] (const uint8_t* inter, uint8_t interCount, uint8_t finalByte)
    {
        video.applyESC (inter, interCount, finalByte);
        syncVideoToState();
    });

    commands.add<const uint8_t*, int> (CT::applyOSC,
        [this] (const uint8_t* payload, int length)
    {
        video.applyOSC (payload, length);
        syncVideoToState();
    });

    commands.add<const CSI&, const uint8_t*, uint8_t, uint8_t> (CT::storeDCSHeader,
        [this] (const CSI& csi, const uint8_t* inter, uint8_t interCount, uint8_t finalByte)
    {
        video.storeDCSHeader (csi, inter, interCount, finalByte);
        syncVideoToState();
    });

    commands.add<const uint8_t*, int> (CT::applyDCSPayload,
        [this] (const uint8_t* data, int length)
    {
        video.applyDCSPayload (data, length);

        const ActiveScreen scr { video.getActiveScreen() };
        skit.processDCS (video.getDcsFinalByte(), data, length,
                         video.getCursorRow (scr), video.getCursorCol (scr));
        video.advanceCursorForImage (skit.getLastImageRows());

        syncVideoToState();
    });

    commands.add<const uint8_t*, int> (CT::applyAPCPayload,
        [this] (const uint8_t* data, int length)
    {
        video.applyAPCPayload (data, length);

        const ActiveScreen scr { video.getActiveScreen() };
        skit.processAPC (data, length,
                         video.getCursorRow (scr), video.getCursorCol (scr));

        const juce::String& response { skit.getLastResponse() };
        if (response.isNotEmpty() and events.contains (ID::writeToHost))
            events.get (ID::writeToHost, response.toRawUTF8(), int (response.getNumBytesAsUTF8()));

        video.advanceCursorForImage (skit.getLastImageRows());

        syncVideoToState();
    });
}

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
 * - `"queueImageErase"` — inline image erase region; calls `State::queueImageErase()`.
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
 * @note READER THREAD — all handlers execute on the reader thread unless dispatched
 *       via `callAsync` (bell, clipboardChanged, desktopNotification land on the message thread).
 */
void Processor::registerEvents() noexcept
{
    // C5: OSC 8 hyperlink — register URI, push ID back to Video.
    events.add<const juce::String&, const juce::String&> (ID::registerLink,
        [this] (const juce::String& uri, const juce::String& /*params*/)
    {
        const uint16_t id { state.registerLinkUri (uri.toRawUTF8(), static_cast<int> (uri.getNumBytesAsUTF8())) };
        video.setActiveLinkId (id);
    });

    // C6: Shell integration — screen-relative → absolute row before State write.
    events.add<int> (ID::promptRow,
        [this] (int relativeRow)
    {
        state.setPromptRow (state.getRawValue<int> (ID::scrollbackUsed) + relativeRow);
    });

    events.add<int> (ID::outputBlockStart,
        [this] (int relativeRow)
    {
        state.setOutputBlockStart (state.getRawValue<int> (ID::scrollbackUsed) + relativeRow);
    });

    events.add<int> (ID::outputBlockEnd,
        [this] (int relativeRow)
    {
        state.setOutputBlockEnd (state.getRawValue<int> (ID::scrollbackUsed) + relativeRow);
    });

    events.add<int> (ID::extendOutputBlock,
        [this] (int relativeRow)
    {
        state.extendOutputBlock (state.getRawValue<int> (ID::scrollbackUsed) + relativeRow);
    });

    // OSC 1337 raw payload from Video — delegate to Skit, then advance cursor.
    events.add<const uint8_t*, int, int, int> (ID::osc1337Raw,
        [this] (const uint8_t* data, int length, int cursorRow, int cursorCol)
    {
        skit.processOSC1337 (data, length, cursorRow, cursorCol);
        video.advanceCursorForImage (skit.getLastImageRows());
        syncVideoToState();
    });

    // Cell erase — mark snapshot dirty so the renderer sees erased content.
    events.add (ID::snapshotDirty, [this] { state.setSnapshotDirty(); });

    // Inline image erase — accumulate bounding box for image reclamation on the message thread.
    events.add<int, int, int, int> (ID::queueImageErase,
        [this] (int topRow, int leftCol, int bottomRow, int rightCol)
    {
        state.queueImageErase (topRow, leftCol, bottomRow, rightCol);
    });

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

    // OSC 12 cursor colour set — extract r/g/b from juce::Colour.
    events.add<int, juce::Colour> (ID::cursorColor,
        [this] (int screen, juce::Colour colour)
    {
        state.setCursorColor (static_cast<ActiveScreen> (screen),
                              int (colour.getRed()),
                              int (colour.getGreen()),
                              int (colour.getBlue()));
    });

    // OSC 112 cursor colour reset — revert to user config default.
    events.add<int> (ID::resetCursorColor,
        [this] (int screen)
    {
        state.resetCursorColor (static_cast<ActiveScreen> (screen));
    });

    // DECSCUSR cursor shape.
    events.add<int, int> (ID::cursorShape,
        [this] (int screen, int shape)
    {
        state.setCursorShape (static_cast<ActiveScreen> (screen), shape);
    });

    // CSI > u — push keyboard enhancement flags onto the per-screen stack.
    events.add<int, uint32_t> (ID::pushKeyboardMode,
        [this] (int screen, uint32_t flags)
    {
        state.pushKeyboardMode (static_cast<ActiveScreen> (screen), flags);
    });

    // CSI < u — pop keyboard enhancement flags from the per-screen stack.
    events.add<int, int> (ID::popKeyboardMode,
        [this] (int screen, int count)
    {
        state.popKeyboardMode (static_cast<ActiveScreen> (screen), count);
    });

    // DEC mode 2026 synchronized output toggle.
    events.add<bool> (ID::syncOutput,
        [this] (bool active)
    {
        state.setSyncOutput (active);
    });

    // DEC mode 2026 set — request same-size PTY resize on next drain completion.
    events.add (ID::requestSyncResize, [this] { state.requestSyncResize(); });

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

/**
 * @brief Reads all Video public getters and writes State atomics (value delivery).
 *
 * Called after every command handler dispatch.  Unconditional sync — negligible
 * cost at terminal frame rates.  Mirrors the ProcessorChain pattern: reads DSP
 * core state, writes APVTS atomics for the timer-driven ValueTree flush.
 *
 * @note READER THREAD — State setters are atomic, safe from any thread.
 */
void Processor::syncVideoToState() noexcept
{
    const ActiveScreen scr { video.getActiveScreen() };
    state.setScreen (scr);
    state.setCursorRow (scr, video.getCursorRow (scr));
    state.setCursorCol (scr, video.getCursorCol (scr));
    state.setWrapPending (scr, video.isWrapPending (scr));
    state.setScrollTop (scr, video.getScrollTop (scr));
    state.setScrollBottom (scr, video.getScrollBottom (scr));
    state.setCursorVisible (scr, video.isCursorVisible (scr));
    state.setMode (ID::autoWrap,             video.getMode (ID::autoWrap));
    state.setMode (ID::originMode,           video.getMode (ID::originMode));
    state.setMode (ID::applicationCursor,    video.getMode (ID::applicationCursor));
    state.setMode (ID::bracketedPaste,       video.getMode (ID::bracketedPaste));
    state.setMode (ID::insertMode,           video.getMode (ID::insertMode));
    state.setMode (ID::mouseTracking,        video.getMode (ID::mouseTracking));
    state.setMode (ID::mouseMotionTracking,  video.getMode (ID::mouseMotionTracking));
    state.setMode (ID::mouseAllTracking,     video.getMode (ID::mouseAllTracking));
    state.setMode (ID::mouseSgr,             video.getMode (ID::mouseSgr));
    state.setMode (ID::focusEvents,          video.getMode (ID::focusEvents));
    state.setMode (ID::applicationKeypad,    video.getMode (ID::applicationKeypad));
    state.setMode (ID::reverseVideo,         video.getMode (ID::reverseVideo));
}

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
void Processor::valueTreePropertyChanged (juce::ValueTree& /*tree*/, const juce::Identifier& property)
{
    if (property == ID::cols or property == ID::visibleRows)
    {
        const int numCols { state.getCols() };
        const int numRows { state.getVisibleRows() };
        resized (numCols, numRows);
    }
}

// =============================================================================

/**
 * @brief Resizes the grid and notifies Video.
 *
 * Called from `valueTreePropertyChanged` when State dimensions change.
 * Does NOT touch any TTY — SIGWINCH is handled by Terminal::Session via `onResize`.
 *
 * @param cols  New terminal width in character columns.
 * @param rows  New terminal height in character rows.
 * @note MESSAGE THREAD only.
 */
void Processor::resized (int cols, int rows) noexcept
{
    jassert (parser != nullptr);
    grid.setSize (rows, cols, true, true, true);
    video.setDimensions (cols, rows);
    video.resize (cols, rows);
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
 * Feeds `Parser::process()` then flushes any queued Video responses via
 * `video.flushResponses()`.  Display is notified via ValueTree::Listener
 * when State::flush() propagates atomic values to the ValueTree on the timer tick.
 *
 * @note READER THREAD only — called from the byte source (Terminal::Session
 *       onBytes callback or IPC dispatch in client mode).
 */
void Processor::process (const char* data, int length) noexcept
{
    jassert (parser != nullptr);
    parser->process (reinterpret_cast<const uint8_t*> (data), static_cast<size_t> (length));
    video.flushResponses();
    state.consumePasteEcho (length);
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
 * @brief Delivers cell pixel dimensions to Skit and Video.
 *
 * Forwards `widthPx` and `heightPx` to `Skit::setCellSize()` (image decode)
 * and `Video::setCellSize()` (CSI `t` pixel dimension reports).
 *
 * @param widthPx   Cell width in pixels.
 * @param heightPx  Cell height in pixels.
 * @note MESSAGE THREAD.
 */
void Processor::setCellSize (int widthPx, int heightPx) noexcept
{
    skit.setCellSize (widthPx, heightPx);
    video.setCellSize (widthPx, heightPx);
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
