/**
 * @file Processor.cpp
 * @brief Implementation of the terminal pipeline orchestrator.
 *
 * Implements Processor — the pipeline half that owns State (the APVTS), Video
 * (the terminal state machine), and Parser, and references Grid owned by
 * terminal::Session.  The PTY side (TTY + History) lives in terminal::Session.
 *
 * ### Thread contexts used in this file
 * - **MESSAGE THREAD** — JUCE message loop; all public methods except `process()`.
 * - **READER THREAD**  — byte source (terminal::Session onBytes or IPC); only `process()`.
 *
 * @see Processor.h
 */

#include "Processor.h"

namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @brief Constructs the Processor: binds Grid&, constructs State, Video, and Parser.
 *
 * Grid is owned by terminal::Session and must outlive this Processor.
 * State is owned by this Processor (the APVTS).
 * Video is owned by this Processor and receives Grid& and events& references.
 * Parser is owned by this Processor and receives Video& reference directly.
 * UUID is provided by the caller — no internal generation.
 *
 * @param gridRef        Live cell buffer owned by terminal::Session.
 * @param textBufferRef  Cross-thread string buffer owned by terminal::Session.
 * @param cols           Initial terminal column count.
 * @param rows           Initial terminal row count.
 * @param uuid           Stable UUID for this Processor — generated once by the caller.
 *
 * @note MESSAGE THREAD — must be constructed on the message thread.
 */
Processor::Processor (Grid& gridRef, TextBuffer& textBufferRef, cell cols, cell rows, const juce::String& uuid)
    : grid (gridRef)
    , textBuffer (textBufferRef)
    , state (textBufferRef)
    , video (grid, events)
    , skit (events)
    , gridResize (grid, video, state)
    , uuid (uuid)
{
    state.setDimensions (cols, rows);
    registerEvents();
    parser = std::make_unique<Parser> (video);
    state.get().addListener (this);
    scrollbackLines = lua::Engine::getContext()->nexus.terminal.scrollbackLines;
    gridResize.setScrollbackLines (scrollbackLines);
    gridResize.set (cols, rows);
    gridResize.apply();
}

/**
 * @brief Destroys the Processor.
 *
 * Nulls TTY callbacks before closing — prevents onData / onDrainComplete /
 * onShellExited from firing on the reader thread during the join.
 * No explicit `removeListener()` needed — the ValueTree inside State is
 * destroyed alongside this Processor (member destruction order).
 *
 * @note MESSAGE THREAD.
 */
Processor::~Processor()
{
    if (tty != nullptr)
    {
        tty->onShellExited = nullptr;
        tty->onData = nullptr;
        tty->onDrainComplete = nullptr;
        tty->close();
    }
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
 * - `"activeScreen"` / `"cursorRow"` / `"cursorCol"` / `"cursorVisible"` — Video::flush()
 *   flush events; write the corresponding State atomics.
 *
 * - `"applicationCursor"` / `"bracketedPaste"` / ... — Video::flush() mode flag flushes.
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
    events.add<const juce::String&, const juce::String&> (
        id::registerLink,
        [this] (const juce::String& /*uri*/, const juce::String& /*params*/)
        {
            video.setActiveLinkId (0);
        });

    // C6: Shell integration — no scrollback, screen-relative == absolute.
    events.add<int> (id::promptRow,
                     [this] (int relativeRow)
                     {
                         state.setPromptRow (cell (relativeRow));
                     });

    events.add<int> (id::outputBlockStart,
                     [this] (int relativeRow)
                     {
                         state.setOutputBlockStart (cell (relativeRow));
                     });

    events.add<int> (id::outputBlockEnd,
                     [this] (int relativeRow)
                     {
                         state.setOutputBlockEnd (cell (relativeRow));
                     });

    events.add<int> (id::extendOutputBlock,
                     [this] (int relativeRow)
                     {
                         state.extendOutputBlock (cell (relativeRow));
                     });

    // OSC 1337 raw payload from Video — delegate to Skit, then advance cursor.
    events.add<const uint8_t*, int, int, int> (id::osc1337Raw,
                                               [this] (const uint8_t* data, int length, int cursorRow, int cursorCol)
                                               {
                                                   skit.processOSC1337 (data, length, cursorRow, cursorCol);
                                                   video.advanceCursorForImage (skit.getLastImageRows().value);
                                               });

    // DCS payload complete — delegate to Skit, then advance cursor.
    events.add<const uint8_t*, int> (
        id::dcsPayloadComplete,
        [this] (const uint8_t* data, int length)
        {
            skit.processDCS (video.getDcsFinalByte(), data, length, video.getCursorRow().value, video.getCursorCol().value);
            video.advanceCursorForImage (skit.getLastImageRows().value);
        });

    // APC payload complete — delegate to Skit, forward any Kitty response, then advance cursor.
    events.add<const uint8_t*, int> (
        id::apcPayloadComplete,
        [this] (const uint8_t* data, int length)
        {
            skit.processAPC (data, length, video.getCursorRow().value, video.getCursorCol().value);

            const juce::String& response { skit.getLastResponse() };
            if (response.isNotEmpty() and events.contains (id::writeToHost))
                events.get (id::writeToHost, response.toRawUTF8(), int (response.getNumBytesAsUTF8()));

            video.advanceCursorForImage (skit.getLastImageRows().value);
        });

    // Cell erase — mark snapshot dirty so the renderer sees erased content.
    events.add (id::snapshotDirty,
                [this]
                {
                    state.setSnapshotDirty();
                });

    // OSC 0/2 window title.
    events.add<const char*, int> (id::title,
                                  [this] (const char* data, int length)
                                  {
                                      state.setTitle (data, length);
                                  });

    // OSC 7 current working directory.
    events.add<const char*, int> (id::cwd,
                                  [this] (const char* data, int length)
                                  {
                                      state.setCwd (data, length);
                                  });

    // OSC 12 cursor colour set — packed ARGB from juce::Colour.
    events.add<int, juce::Colour> (id::cursorColor,
                                   [this] (int screen, juce::Colour colour)
                                   {
                                       state.setCursorColor (screen, colour);
                                   });

    // OSC 112 cursor colour reset — revert to user config default.
    events.add<int> (id::resetCursorColor,
                     [this] (int screen)
                     {
                         state.resetCursorColor (screen);
                     });

    // DECSCUSR cursor shape.
    events.add<int, int> (id::cursorShape,
                          [this] (int screen, int shape)
                          {
                              state.setCursorShape (screen, shape);
                          });

    // CSI > u — push keyboard enhancement flags onto the per-screen stack.
    events.add<int, uint32_t> (id::pushKeyboardMode,
                               [this] (int screen, uint32_t flags)
                               {
                                   state.pushKeyboardMode (screen, flags);
                               });

    // CSI < u — pop keyboard enhancement flags from the per-screen stack.
    events.add<int, int> (id::popKeyboardMode,
                          [this] (int screen, int count)
                          {
                              state.popKeyboardMode (screen, count);
                          });

    // DEC mode 2026 synchronized output toggle.
    events.add<bool> (id::syncOutput,
                      [this] (bool active)
                      {
                          state.setSyncOutput (active);
                      });

    // DEC mode 2026 set — request same-size PTY resize on next drain completion.
    events.add (id::requestSyncResize,
                [this]
                {
                    state.requestSyncResize();
                });

    // State delivery: activeScreen.
    events.add<int> (id::activeScreen,
                     [this] (int scr)
                     {
                         state.setScreen (scr);
                     });

    // State delivery: scrollUp — increment history row count (capped at scrollbackLines), sync Grid and State.
    events.add<int, int> (id::scrollUp,
                          [this] (int screen, int count)
                          {
                              numRows.at (screen) = juce::jmin (numRows.at (screen) + count, scrollbackLines);
                              grid.setNumRows (screen, numRows.at (screen));
                              state.setNumRows (screen, numRows.at (screen));
                          });

    // State delivery: screenDirty — increment monotonic counter so Screen detects new cell data.
    events.add<int> (id::screenDirty,
                     [this] (int screen)
                     {
                         state.setScreenDirty (screen);
                     });

    // State delivery: cursor row for active screen.
    events.add<int, int> (id::cursorRow,
                          [this] (int scr, int row)
                          {
                              state.setCursorRow (scr, cell (row));
                          });

    // State delivery: cursor column for active screen.
    events.add<int, int> (id::cursorCol,
                          [this] (int scr, int col)
                          {
                              state.setCursorCol (scr, cell (col));
                          });

    // State delivery: cursor visibility for active screen.
    events.add<int, bool> (id::cursorVisible,
                           [this] (int scr, bool visible)
                           {
                               state.setCursorVisible (scr, visible);
                           });

    // State delivery: mode flags.
    events.add<bool> (id::applicationCursor,
                      [this] (bool value)
                      {
                          state.setMode (id::applicationCursor, value);
                      });

    events.add<bool> (id::bracketedPaste,
                      [this] (bool value)
                      {
                          state.setMode (id::bracketedPaste, value);
                      });

    events.add<bool> (id::mouseTracking,
                      [this] (bool value)
                      {
                          state.setMode (id::mouseTracking, value);
                      });

    events.add<bool> (id::mouseMotionTracking,
                      [this] (bool value)
                      {
                          state.setMode (id::mouseMotionTracking, value);
                      });

    events.add<bool> (id::mouseAllTracking,
                      [this] (bool value)
                      {
                          state.setMode (id::mouseAllTracking, value);
                      });

    events.add<bool> (id::focusEvents,
                      [this] (bool value)
                      {
                          state.setMode (id::focusEvents, value);
                      });

    events.add<bool> (id::win32InputMode,
                      [this] (bool value)
                      {
                          state.setMode (id::win32InputMode, value);
                      });

    // Screen switch — fired by Video::setScreen() with the OLD screen's cursor values.
    // Saves old cursor to State, loads new cursor from State atomics, calls video.loadScreenState().
    // scrollTop/scrollBottom are always reset to 0 on screen switch (matches original setScreen behaviour).
    // wrapPending is always cleared on screen switch.
    events.add<int, int, int, bool, int, int, bool, uint32_t> (
        id::screenSwitch,
        [this] (int newScreen,
                int oldRow,
                int oldCol,
                bool oldVisible,
                int /*oldScrollTop*/,
                int /*oldScrollBottom*/,
                bool /*oldWrapPending*/,
                uint32_t /*oldKeyboardFlags*/)
        {
            const int oldScreen { newScreen == ScreenMap::alternate ? ScreenMap::normal : ScreenMap::alternate };

            state.setCursorRow (oldScreen, cell (oldRow));
            state.setCursorCol (oldScreen, cell (oldCol));
            state.setCursorVisible (oldScreen, oldVisible);

            const int newRow { state.loadCursorRow (newScreen) };
            const int newCol { state.loadCursorCol (newScreen) };
            const bool newVisible { state.loadCursorVisible (newScreen) };
            const uint32_t newKbFlags { state.loadKeyboardFlags (newScreen) };

            // scrollTop/scrollBottom reset to 0 (sentinel = full screen); wrapPending cleared.
            video.loadScreenState (newRow, newCol, newVisible, 0, 0, false, newKbFlags);
        });

    // BEL — placeholder so events.contains() passes; Session registers the actual handler.
    events.add (id::bell,
                []
                {
                });

    // OSC 52 clipboard write — placeholder; Session registers the actual handler.
              events.add<const juce::String&> (id::clipboardChanged,
                                               [] (const juce::String& /*text*/)
                                               {
                                               });

    // OSC 9 / OSC 777 desktop notification — placeholder; Session registers the actual handler.
    events.add<const juce::String&, const juce::String&> (
        id::desktopNotification,
        [] (const juce::String& /*title*/, const juce::String& /*body*/)
        {
        });
}

// =============================================================================

// =============================================================================

/**
 * @brief ValueTree::Listener — reacts to top-down property changes from Display.
 *
 * Fires on the message thread when State's ValueTree properties change.
 * Handles shell integration callbacks (outputBlockTop → command process query,
 * promptRow → clear foreground), dimension changes (cols, visibleRows) via
 * gridResize.set(), cell pixel changes (cellWidth, cellHeight) via
 * gridResize.setCellSize(), and displayName recomputation from foregroundProcess / cwd.
 *
 * @note MESSAGE THREAD.
 */
void Processor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    // PARAM children flush via id::value — check the id property to identify shell integration params.
    if (property == id::value and tree.getType() == jam::ValueTree::PARAM)
    {
        const juce::Identifier paramId { tree.getProperty (id::id).toString() };

        if (paramId == id::outputBlockTop)
        {
            if (tty != nullptr)
            {
                const int fgPid { tty->getForegroundPid() };
                const int shellPid { tty->getShellPid() };

                if (fgPid > 0)
                {
                    if (fgPid == shellPid)
                    {
                        state.setForegroundProcess ("", 0);
                    }
                    else
                    {
                        static constexpr int fgNameBufSize { 256 };
                        char fgNameBuf[fgNameBufSize] {};
                        const int fgNameLen { tty->getProcessName (fgPid, fgNameBuf, fgNameBufSize) };

                        if (fgNameLen > 0)
                            state.setForegroundProcess (fgNameBuf, fgNameLen);
                    }

                    if (shouldTrackCwdFromOs)
                    {
                        static constexpr int cwdBufSize { 4096 };
                        char cwdBuf[cwdBufSize] {};
                        const int cwdLen { tty->getCwd (fgPid, cwdBuf, cwdBufSize) };

                        if (cwdLen > 0)
                            state.setCwd (cwdBuf, cwdLen);
                    }
                }
            }

            if (onStateFlush != nullptr)
                onStateFlush();
        }

        if (paramId == id::promptRow)
        {
            state.setForegroundProcess ("", 0);

            if (onStateFlush != nullptr)
                onStateFlush();
        }

        if (paramId == id::cols or paramId == id::visibleRows)
        {
            gridResize.set (state.loadCols(), state.loadVisibleRows());
        }

        if (paramId == id::cellWidth or paramId == id::cellHeight)
        {
            gridResize.setCellSize (state.loadCellWidth(), state.loadCellHeight());
        }

    }

    // TEXT parameters flush as direct properties on the SESSION root node.
    // When foregroundProcess or cwd change, recompute displayName.
    if (property == id::foregroundProcess or property == id::cwd)
    {
        const auto foreground { tree.getProperty (id::foregroundProcess).toString() };
        const auto cwdPath { tree.getProperty (id::cwd).toString() };
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
            state.get().setProperty (app::id::displayName, name, nullptr);
    }
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
    if (state.getMode (id::win32InputMode))
    {
        seq = Keyboard::encodeWin32Input (key);
    }
    else
#endif
    {
        const bool applicationCursor { state.getMode (id::applicationCursor) };
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
        const bool bracketed { state.getMode (id::bracketedPaste) };

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

    if (state.getMode (id::focusEvents))
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
 * Pure bytes-to-Grid pipeline: forwards to Parser::process(), flushes Video,
 * and consumes the paste echo gate.  No lock, no suspended check, no cell-size
 * detection — all resize work is handled exclusively by GridResize on the
 * message thread.
 *
 * @note READER THREAD only — called from the byte source (terminal::Session
 *       onBytes callback or IPC dispatch in client mode).
 */
void Processor::process (const char* data, int length) noexcept
{
    jassert (parser != nullptr);

    if (video.getCols().value > 0 and video.getVisibleRows().value > 0)
    {
        parser->process (reinterpret_cast<const uint8_t*> (data), static_cast<size_t> (length));
        video.flush();
        video.flushResponses();
    }

    state.consumePasteEcho (length);
}

GridResize& Processor::getGridResize() noexcept { return gridResize; }

State& Processor::getState() noexcept { return state; }
const State& Processor::getState() const noexcept { return state; }

Grid& Processor::getGrid() noexcept { return grid; }
const Grid& Processor::getGrid() const noexcept { return grid; }

const juce::String& Processor::getUuid() const noexcept { return uuid; }

void Processor::flushResponses() noexcept { video.flushResponses(); }

/**
 * @brief Transfers TTY ownership to this Processor and wires GridResize for SIGWINCH delivery.
 *
 * @note MESSAGE THREAD.
 */
void Processor::setTTY (std::unique_ptr<TTY> ttyToOwn) noexcept
{
    tty = std::move (ttyToOwn);
    gridResize.setTTY (tty.get());
}

/**
 * @brief Performs the OS-level PTY resize (SIGWINCH to shell).
 *
 * Called from Session::onDrainComplete for the sync-resize path.  No-op if
 * no TTY is owned or its reader thread is not running.
 *
 * @note READER THREAD.
 */
void Processor::platformResize (cell cols, cell rows, int pixelWidth, int pixelHeight) noexcept
{
    if (tty != nullptr and tty->isThreadRunning())
        tty->platformResize (cols, rows, pixelWidth, pixelHeight);
}

/**
 * @brief Registers the `writeToHost` event handler in the events map.
 *
 * @note MESSAGE THREAD — call before the first process() invocation.
 */
void Processor::setHostWriter (std::function<void (const char*, int)> writer) noexcept
{
    events.add<const char*, int> (id::writeToHost, std::move (writer));
}

/**
 * @brief Writes raw input bytes to the PTY via the registered writeInput handler.
 *
 * @note MESSAGE THREAD.
 */
void Processor::writeInput (const char* data, int len) noexcept
{
    if (events.contains (id::writeInput))
        events.get (id::writeInput, data, len);
}

/**
 * @brief Registers the handler invoked by writeInput().
 *
 * @note MESSAGE THREAD — call before the first writeInput() invocation.
 */
void Processor::setInputWriter (std::function<void (const char*, int)> handler) noexcept
{
    events.add<const char*, int> (id::writeInput, std::move (handler));
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
