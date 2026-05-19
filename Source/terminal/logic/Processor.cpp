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
 * Video is owned by this Processor and receives Grid& and events& references.
 * Parser is owned by this Processor and receives Video& reference directly.
 * UUID is provided by the caller — no internal generation.
 *
 * @param gridRef        Live cell buffer owned by Terminal::Session.
 * @param textBufferRef  Cross-thread string buffer owned by Terminal::Session.
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
    , uuid (uuid)
{
    state.setDimensions (cols, rows);
    registerEvents();
    parser = std::make_unique<Parser> (video);
    state.get().addListener (this);
    prepare (rows, cols, lua::Engine::getContext()->nexus.terminal.scrollbackLines);
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
 * @brief Prepares the terminal pipeline for the given dimensions.
 *
 * Sets Grid size, Video dimensions, and stores the scrollback limit for use
 * by scroll event handlers.  Analogous to `AudioProcessor::prepareToPlay()`.
 * Called by Session on construction and on resize.  Never called from process().
 *
 * @param viewportRows     Visible row count.
 * @param numCols          Column count.
 * @param scrollbackLines  Maximum history row count (from config).
 */
void Processor::prepare (cell viewportRows, cell numCols, int scrollbackLines) noexcept
{
    this->scrollbackLines = scrollbackLines;

    if (grid.isAllocated())
    {
        int cursorRow { video.getCursorRow().value };
        int cursorCol { video.getCursorCol().value };
        const auto reflowedNumRows { grid.reflow (viewportRows.value, numCols.value, scrollbackLines, cursorRow, cursorCol) };

        numRows.at (0) = reflowedNumRows.at (0);
        numRows.at (1) = reflowedNumRows.at (1);
        grid.setNumRows (0, numRows.at (0));
        grid.setNumRows (1, numRows.at (1));
        state.setNumRows (0, numRows.at (0));
        state.setNumRows (1, numRows.at (1));

        const int activeScreen { state.getActiveScreen() };
        const bool visible { state.loadCursorVisible (activeScreen) };

        video.setDimensions (numCols, viewportRows);
        video.loadScreenState (cursorRow, cursorCol, visible, 0, 0, false, state.getKeyboardFlags());
        video.resize (numCols, viewportRows);
    }
    else
    {
        numRows.at (0) = 0;
        numRows.at (1) = 0;
        grid.setSize (viewportRows.value, numCols.value, scrollbackLines);
        video.setDimensions (numCols, viewportRows);
        video.resize (numCols, viewportRows);
    }

    if (tty != nullptr and tty->isThreadRunning())
    {
        const int width { state.loadWidth() };
        const int height { state.loadHeight() };
        tty->platformResize (numCols, viewportRows, width, height);
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
        ID::registerLink,
        [this] (const juce::String& /*uri*/, const juce::String& /*params*/)
        {
            video.setActiveLinkId (0);
        });

    // C6: Shell integration — no scrollback, screen-relative == absolute.
    events.add<int> (ID::promptRow,
                     [this] (int relativeRow)
                     {
                         state.setPromptRow (cell (relativeRow));
                     });

    events.add<int> (ID::outputBlockStart,
                     [this] (int relativeRow)
                     {
                         state.setOutputBlockStart (cell (relativeRow));
                     });

    events.add<int> (ID::outputBlockEnd,
                     [this] (int relativeRow)
                     {
                         state.setOutputBlockEnd (cell (relativeRow));
                     });

    events.add<int> (ID::extendOutputBlock,
                     [this] (int relativeRow)
                     {
                         state.extendOutputBlock (cell (relativeRow));
                     });

    // OSC 1337 raw payload from Video — delegate to Skit, then advance cursor.
    events.add<const uint8_t*, int, int, int> (ID::osc1337Raw,
                                               [this] (const uint8_t* data, int length, int cursorRow, int cursorCol)
                                               {
                                                   skit.processOSC1337 (data, length, cursorRow, cursorCol);
                                                   video.advanceCursorForImage (skit.getLastImageRows().value);
                                               });

    // DCS payload complete — delegate to Skit, then advance cursor.
    events.add<const uint8_t*, int> (
        ID::dcsPayloadComplete,
        [this] (const uint8_t* data, int length)
        {
            skit.processDCS (video.getDcsFinalByte(), data, length, video.getCursorRow().value, video.getCursorCol().value);
            video.advanceCursorForImage (skit.getLastImageRows().value);
        });

    // APC payload complete — delegate to Skit, forward any Kitty response, then advance cursor.
    events.add<const uint8_t*, int> (
        ID::apcPayloadComplete,
        [this] (const uint8_t* data, int length)
        {
            skit.processAPC (data, length, video.getCursorRow().value, video.getCursorCol().value);

            const juce::String& response { skit.getLastResponse() };
            if (response.isNotEmpty() and events.contains (ID::writeToHost))
                events.get (ID::writeToHost, response.toRawUTF8(), int (response.getNumBytesAsUTF8()));

            video.advanceCursorForImage (skit.getLastImageRows().value);
        });

    // Cell erase — mark snapshot dirty so the renderer sees erased content.
    events.add (ID::snapshotDirty,
                [this]
                {
                    state.setSnapshotDirty();
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
    events.add (ID::requestSyncResize,
                [this]
                {
                    state.requestSyncResize();
                });

    // State delivery: activeScreen.
    events.add<int> (ID::activeScreen,
                     [this] (int scr)
                     {
                         state.setScreen (scr);
                     });

    // State delivery: scrollUp — increment history row count (capped at scrollbackLines), sync Grid and State.
    events.add<int, int> (ID::scrollUp,
                          [this] (int screen, int count)
                          {
                              numRows.at (screen) = juce::jmin (numRows.at (screen) + count, scrollbackLines);
                              grid.setNumRows (screen, numRows.at (screen));
                              state.setNumRows (screen, numRows.at (screen));
                          });

    // State delivery: screenDirty — increment monotonic counter so Screen detects new cell data.
    events.add<int> (ID::screenDirty,
                     [this] (int screen)
                     {
                         state.setScreenDirty (screen);
                     });

    // State delivery: cursor row for active screen.
    events.add<int, int> (ID::cursorRow,
                          [this] (int scr, int row)
                          {
                              state.setCursorRow (scr, cell (row));
                          });

    // State delivery: cursor column for active screen.
    events.add<int, int> (ID::cursorCol,
                          [this] (int scr, int col)
                          {
                              state.setCursorCol (scr, cell (col));
                          });

    // State delivery: cursor visibility for active screen.
    events.add<int, bool> (ID::cursorVisible,
                           [this] (int scr, bool visible)
                           {
                               state.setCursorVisible (scr, visible);
                           });

    // State delivery: mode flags.
    events.add<bool> (ID::applicationCursor,
                      [this] (bool value)
                      {
                          state.setMode (ID::applicationCursor, value);
                      });

    events.add<bool> (ID::bracketedPaste,
                      [this] (bool value)
                      {
                          state.setMode (ID::bracketedPaste, value);
                      });

    events.add<bool> (ID::mouseTracking,
                      [this] (bool value)
                      {
                          state.setMode (ID::mouseTracking, value);
                      });

    events.add<bool> (ID::mouseMotionTracking,
                      [this] (bool value)
                      {
                          state.setMode (ID::mouseMotionTracking, value);
                      });

    events.add<bool> (ID::mouseAllTracking,
                      [this] (bool value)
                      {
                          state.setMode (ID::mouseAllTracking, value);
                      });

    events.add<bool> (ID::focusEvents,
                      [this] (bool value)
                      {
                          state.setMode (ID::focusEvents, value);
                      });

    events.add<bool> (ID::win32InputMode,
                      [this] (bool value)
                      {
                          state.setMode (ID::win32InputMode, value);
                      });

    // Screen switch — fired by Video::setScreen() with the OLD screen's cursor values.
    // Saves old cursor to State, loads new cursor from State atomics, calls video.loadScreenState().
    // scrollTop/scrollBottom are always reset to 0 on screen switch (matches original setScreen behaviour).
    // wrapPending is always cleared on screen switch.
    events.add<int, int, int, bool, int, int, bool, uint32_t> (
        ID::screenSwitch,
        [this] (int newScreen,
                int oldRow,
                int oldCol,
                bool oldVisible,
                int /*oldScrollTop*/,
                int /*oldScrollBottom*/,
                bool /*oldWrapPending*/,
                uint32_t /*oldKeyboardFlags*/)
        {
            const int oldScreen { newScreen == Screen::Map::alternate ? Screen::Map::normal : Screen::Map::alternate };

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
    events.add (ID::bell,
                []
                {
                });

    // OSC 52 clipboard write — placeholder; Session registers the actual handler.
              events.add<const juce::String&> (ID::clipboardChanged,
                                               [] (const juce::String& /*text*/)
                                               {
                                               });

    // OSC 9 / OSC 777 desktop notification — placeholder; Session registers the actual handler.
    events.add<const juce::String&, const juce::String&> (
        ID::desktopNotification,
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
 * prepare() (which calls tty->platformResize() directly), and displayName
 * recomputation from foregroundProcess / cwd.
 *
 * @note MESSAGE THREAD.
 */
void Processor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    // PARAM children flush via ID::value — check the id property to identify shell integration params.
    if (property == ID::value and tree.getType() == jam::ValueTree::PARAM)
    {
        const juce::Identifier paramId { tree.getProperty (ID::id).toString() };

        if (paramId == ID::outputBlockTop)
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

        if (paramId == ID::promptRow)
        {
            state.setForegroundProcess ("", 0);

            if (onStateFlush != nullptr)
                onStateFlush();
        }

        if (paramId == ID::cols or paramId == ID::visibleRows)
        {
            const cell newCols { state.loadCols() };
            const cell newRows { state.loadVisibleRows() };

            if (newCols.value > 0 and newRows.value > 0)
            {
                if (newCols != video.getCols() or newRows != video.getVisibleRows())
                {
                    suspendProcessing (true);
                    prepare (newRows, newCols, lua::Engine::getContext()->nexus.terminal.scrollbackLines);
                    suspendProcessing (false);
                }
            }
        }

    }

    // TEXT parameters flush as direct properties on the SESSION root node.
    // When foregroundProcess or cwd change, recompute displayName.
    if (property == ID::foregroundProcess or property == ID::cwd)
    {
        const auto foreground { tree.getProperty (ID::foregroundProcess).toString() };
        const auto cwdPath { tree.getProperty (ID::cwd).toString() };
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
 * Reads cellWidth and cellHeight from State atomics at batch start and applies
 * any changes to Video and Skit on the reader thread.  Grid resize is handled
 * by Processor::prepare() — never in process().  Guards on video dimensions
 * prevent processing before prepare() is called.
 *
 * @note READER THREAD only — called from the byte source (Terminal::Session
 *       onBytes callback or IPC dispatch in client mode).
 */
void Processor::process (const char* data, int length) noexcept
{
    const juce::ScopedLock sl (callbackLock);

    jassert (parser != nullptr);

    if (not suspended)
    {
        if (displayReady)
        {
            const int stateCellW { state.loadCellWidth() };
            const int stateCellH { state.loadCellHeight() };

            if (stateCellW != video.getCellWidth() or stateCellH != video.getCellHeight())
            {
                video.setCellSize (stateCellW, stateCellH);
                skit.setCellSize (stateCellW, stateCellH);
            }
        }

        if (video.getCols().value > 0 and video.getVisibleRows().value > 0)
        {
            parser->process (reinterpret_cast<const uint8_t*> (data), static_cast<size_t> (length));
            video.flush();
            video.flushResponses();
        }

        state.consumePasteEcho (length);
    }
}

void Processor::suspendProcessing (bool shouldBeSuspended) noexcept
{
    const juce::ScopedLock sl (callbackLock);
    suspended = shouldBeSuspended;
}

bool Processor::isSuspended() const noexcept { return suspended; }

State& Processor::getState() noexcept { return state; }
const State& Processor::getState() const noexcept { return state; }

Grid& Processor::getGrid() noexcept { return grid; }
const Grid& Processor::getGrid() const noexcept { return grid; }

const juce::String& Processor::getUuid() const noexcept { return uuid; }

void Processor::flushResponses() noexcept { video.flushResponses(); }

/**
 * @brief Transfers TTY ownership to this Processor.
 *
 * @note MESSAGE THREAD.
 */
void Processor::setTTY (std::unique_ptr<TTY> ttyToOwn) noexcept
{
    tty = std::move (ttyToOwn);
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
    events.add<const char*, int> (ID::writeToHost, std::move (writer));
}

/**
 * @brief Writes raw input bytes to the PTY via the registered writeInput handler.
 *
 * @note MESSAGE THREAD.
 */
void Processor::writeInput (const char* data, int len) noexcept
{
    if (events.contains (ID::writeInput))
        events.get (ID::writeInput, data, len);
}

/**
 * @brief Registers the handler invoked by writeInput().
 *
 * @note MESSAGE THREAD — call before the first writeInput() invocation.
 */
void Processor::setInputWriter (std::function<void (const char*, int)> handler) noexcept
{
    events.add<const char*, int> (ID::writeInput, std::move (handler));
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
    displayReady = true;
    return display;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
