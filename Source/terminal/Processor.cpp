/**
 * @file Processor.cpp
 * @brief Implementation of the terminal pipeline orchestrator.
 *
 * Implements Processor — the pipeline half that owns State (the APVTS), Video
 * (the terminal state machine), DiscreteStateTransition transitioner, and Parser,
 * and references Buffer<Cell> owned by terminal::Session.
 * The PTY side (TTY + History) lives in terminal::Session.
 *
 * ### Thread contexts used in this file
 * - **MESSAGE THREAD** — JUCE message loop; all public methods except `process()`.
 * - **READER THREAD**  — byte source (terminal::Session onBytes or IPC); only `process()`.
 *
 * @see Processor.h
 */

#include "Processor.h"

/** @brief Height-only viewport resize — adjusts ring head and numRows per screen.
 *
 *  Relocated from Grid::resizeHeight. Same algorithm:
 *  - Shrink: eat empty rows below cursor (screen 0), push remainder to scrollback.
 *  - Grow: pull from scrollback history, fill remaining with blanks.
 *
 *  @param buffer          Live cell buffer.
 *  @param state           Terminal parameter store (numRows read/write via setValue).
 *  @param video           Video state machine (cursor row read).
 *  @param scrollbackLines Maximum history row count from config.
 *  @param currentRows     Current viewport row count.
 *  @param newRows         Target viewport row count.
 */
static void resizeHeight (jam::Buffer<jam::Cell>& buffer,
                          terminal::State& state,
                          terminal::Video& video,
                          int scrollbackLines,
                          cell currentRows,
                          cell newRows) noexcept
{
    if (newRows.value != currentRows.value)
    {
        for (int screen { 0 }; screen < Map::Screen::count; ++screen)
        {
            const juce::Identifier screenId { Map::Screen::getContext()->get (screen) };
            int screenNumRows { state.loadValue (screenId, terminal::id::numRows) };

            if (newRows.value < currentRows.value)
            {
                // Shrink: eat empty from bottom, push remainder to scrollback.
                int needed { currentRows.value - newRows.value };

                // Eat empty rows below cursor (normal screen only).
                if (screen == Map::Screen::normal)
                {
                    const int belowCursor { currentRows.value - 1 - video.getCursorRow().value };
                    const int available { juce::jmin (belowCursor, needed) };
                    needed -= available;
                }

                // Push remaining from top into scrollback by advancing head.
                if (needed > 0)
                {
                    buffer.advanceHead (screen, needed);
                    screenNumRows = juce::jmin (screenNumRows + needed, scrollbackLines);

                    if (screen == Map::Screen::normal)
                    {
                        const int cursorRow { video.getCursorRow().value - needed };
                        state.setValue (screenId, terminal::id::cursorRow, cursorRow);
                    }
                }
            }
            else
            {
                // Grow: pull from scrollback, fill remaining with blanks.
                int needed { newRows.value - currentRows.value };

                // Pull from scrollback.
                const int available { juce::jmin (screenNumRows, needed) };

                if (available > 0)
                {
                    screenNumRows -= available;
                    buffer.reverseHead (screen, available);

                    if (screen == Map::Screen::normal)
                    {
                        const int cursorRow { video.getCursorRow().value + available };
                        state.setValue (screenId, terminal::id::cursorRow, cursorRow);
                    }
                }

                needed -= available;

                // Fill remaining with blanks at bottom of new viewport.
                for (int r { currentRows.value + available }; r < newRows.value; ++r)
                    buffer.clear (screen, r);
            }

            state.setValue (screenId, terminal::id::numRows, screenNumRows);
        }
    }
}

namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @brief Constructs the Processor: binds Buffer<Cell>&, constructs State, Video, DiscreteStateTransition, and Parser.
 *
 * Buffer<Cell> is owned by terminal::Session and must outlive this Processor.
 * State is owned by this Processor (the APVTS).
 * Video is owned by this Processor and receives Buffer<Cell>& and events& references.
 * Parser is owned by this Processor and receives Video& reference directly.
 * UUID is provided by the caller — no internal generation.
 *
 * @param bufferRef      Live cell buffer owned by terminal::Session.
 * @param textBufferRef  Cross-thread string buffer owned by terminal::Session.
 * @param cols           Initial terminal column count.
 * @param rows           Initial terminal row count.
 * @param uuid           Stable UUID for this Processor — generated once by the caller.
 *
 * @note MESSAGE THREAD — must be constructed on the message thread.
 */
Processor::Processor (jam::Buffer<jam::Cell>& bufferRef,
                      TextBuffer& textBufferRef,
                      cell cols,
                      cell rows,
                      const juce::String& uuid)
    : buffer (bufferRef)
    , textBuffer (textBufferRef)
    , state (textBufferRef)
    , video (buffer, events)
    , skit (events)
    , transitioner (buffer)
    , uuid (uuid)
{
    state.setDimensions (cols, rows);
    registerEvents();
    parser = std::make_unique<Parser> (video);
    state.get().addListener (this);

    // DST trigger: resize — height adjustment + column change + video sync.
    transitioner.addTrigger<cell, cell> (
        id::resizeStart,
        [this] (cell targetCols, cell targetRows)
        {
            const int scrollbackLines { AppState::getContext()->getRawParameterValue<int> (app::id::scrollbackLines)->load() };
            const cell currentRows { video.getVisibleRows() };

            // Height adjustment (same logic as the former Grid::resizeHeight).
            resizeHeight (buffer, state, video, scrollbackLines, currentRows, targetRows);

            // Column change — destructive setSize. Content preserved in DST snapshot;
            // shell redraws after SIGWINCH.
            if (targetCols.value != buffer.getNumCols())
            {
                const int ringSize { juce::nextPowerOfTwo (scrollbackLines + targetRows.value) };
                buffer.setSize (2, ringSize, targetCols.value);
            }
            else if (targetRows.value != currentRows.value)
            {
                // Height-only: reallocate ring if needed (larger viewport).
                const int needed { juce::nextPowerOfTwo (scrollbackLines + targetRows.value) };

                if (needed > buffer.getNumRows())
                    buffer.setSize (2, needed, buffer.getNumCols());
            }

            // Video sync — same calls as former GridSizeTransition::applyChange.
            video.setDimensions (targetCols, targetRows);

            const int activeScr { state.getActiveScreen() };
            const juce::Identifier activeScreenId { Map::Screen::getContext()->get (activeScr) };
            const int cursorRow { video.getCursorRow().value };
            const int cursorCol { video.getCursorCol().value };

            video.loadScreenState (cell (cursorRow), cell (cursorCol), true, cell (0), cell (0), false, 0);
            video.resize (targetCols, targetRows);

            // State sync — direct ValueTree writes (message thread).
            const juce::Identifier normalScreenId { Map::Screen::getContext()->get (Map::Screen::normal) };
            const juce::Identifier alternateScreenId { Map::Screen::getContext()->get (Map::Screen::alternate) };
            state.setValue (normalScreenId, id::numRows, state.loadValue (normalScreenId, id::numRows));
            state.setValue (alternateScreenId, id::numRows, state.loadValue (alternateScreenId, id::numRows));
            state.setValue (activeScreenId, id::scrollOffset, 0);
            state.setValue (activeScreenId, id::cursorRow, cursorRow);
            state.setValue (activeScreenId, id::cursorCol, cursorCol);
        });

    // SIGWINCH delivery on settle.
    transitioner.onStop = [this]
    {
        events.get (id::resizeEnd);
    };

    transitioner.prepare();

    // Cold start allocation — same as former GridSizeTransition::allocate().
    const int scrollbackLines { AppState::getContext()->getRawParameterValue<int> (app::id::scrollbackLines)->load() };
    const int ringSize { juce::nextPowerOfTwo (scrollbackLines + rows.value) };
    buffer.setSize (2, ringSize, cols.value);
    video.setDimensions (cols, rows);
    video.resize (cols, rows);
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
 * - `"bell"` — BEL (0x07); writes `\a` to stderr for system alert sound.
 *   Fired by Video via `callAsync` on the message thread.
 *
 * - `"clipboardChanged"` — OSC 52 clipboard write; calls
 *   `juce::SystemClipboard::copyTextToClipboard()`.
 *   Fired by Video via `callAsync` on the message thread.
 *
 * - `"desktopNotification"` — OSC 9 / OSC 777 desktop notification; calls
 *   `terminal::showNotification()`.
 *   Fired by Video via `callAsync` on the message thread.
 *
 * @note READER THREAD — all handlers execute on the reader thread except bell,
 *       clipboardChanged, and desktopNotification, which fire on the message thread
 *       (Video dispatches them via callAsync).
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
                                                   skit.processOSC1337 (
                                                       data, length, cell (cursorRow), cell (cursorCol));
                                                   video.advanceCursorForImage (skit.getLastImageRows());
                                               });

    // DCS payload complete — delegate to Skit, then advance cursor.
    events.add<const uint8_t*, int> (
        id::dcsPayloadComplete,
        [this] (const uint8_t* data, int length)
        {
            skit.processDCS (video.getDcsFinalByte(), data, length, video.getCursorRow(), video.getCursorCol());
            video.advanceCursorForImage (skit.getLastImageRows());
        });

    // APC payload complete — delegate to Skit, forward any Kitty response, then advance cursor.
    events.add<const uint8_t*, int> (
        id::apcPayloadComplete,
        [this] (const uint8_t* data, int length)
        {
            skit.processAPC (data, length, video.getCursorRow(), video.getCursorCol());

            const juce::String& response { skit.getLastResponse() };
            if (response.isNotEmpty() and events.contains (id::writeToHost))
                events.get (id::writeToHost, response.toRawUTF8(), int (response.getNumBytesAsUTF8()));

            video.advanceCursorForImage (skit.getLastImageRows());
        });

    // Cell erase — mark snapshot dirty so the renderer sees erased content.
    events.add (id::snapshotDirty,
                [this]
                {
                    state.setSnapshotDirty();
                });

    // Clear scrollback — clear Buffer cells, reset numRows, and clear scroll offset for the given screen.
    events.add<int> (id::clearBuffer,
                     [this] (int screen)
                     {
                         buffer.clear (screen);
                         const juce::Identifier clearScreenId { Map::Screen::getContext()->get (screen) };
                         state.storeValue (clearScreenId, id::numRows, 0);
                         state.storeValue (clearScreenId, id::scrollOffset, 0);
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
                                       const juce::Identifier colorScreenId { Map::Screen::getContext()->get (screen) };
                                       state.storeValue (
                                           colorScreenId, id::cursorColor, static_cast<int> (colour.getARGB()));
                                   });

    // OSC 112 cursor colour reset — revert to user config default.
    events.add<int> (id::resetCursorColor,
                     [this] (int screen)
                     {
                         const juce::Identifier resetColorScreenId { Map::Screen::getContext()->get (screen) };
                         state.storeValue (resetColorScreenId, id::cursorColor, -1);
                     });

    // DECSCUSR cursor shape.
    events.add<int, int> (id::cursorShape,
                          [this] (int screen, int shape)
                          {
                              const juce::Identifier shapeScreenId { Map::Screen::getContext()->get (screen) };
                              state.storeValue (shapeScreenId, id::cursorShape, shape);
                          });

    // CSI > u — push keyboard enhancement flags onto the per-screen stack.
    events.add<int, uint32_t> (id::pushKeyboardMode,
                               [this] (int screen, uint32_t flags)
                               {
                                   pushKeyboardMode (screen, flags);
                               });

    // CSI < u — pop keyboard enhancement flags from the per-screen stack.
    events.add<int, int> (id::popKeyboardMode,
                          [this] (int screen, int count)
                          {
                              popKeyboardMode (screen, count);
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

    // State delivery: scrollUp — increment numRows (history count) capped at scrollbackLines, then adjust selection.
    events.add<int, int> (
        id::scrollUp,
        [this] (int screen, int count)
        {
            const int scrollbackLines { AppState::getContext()->getRawParameterValue<int> (app::id::scrollbackLines)->load() };
            const juce::Identifier scrollScreenId { Map::Screen::getContext()->get (screen) };
            const int current { state.loadValue (scrollScreenId, id::numRows) };
            const int excess { current + count - scrollbackLines };

            if (excess > 0)
            {
                for (int i { 0 }; i < excess; ++i)
                    buffer.clear (screen, buffer.getNumRows() - scrollbackLines - excess + i);
            }

            state.storeValue (scrollScreenId, id::numRows, juce::jmin (current + count, scrollbackLines));
            using TE = jam::TextEditor;
            const auto& teId { TE::properties.at (TE::textEditorId) };
            const auto& selTypeId { TE::properties.at (TE::selectionTypeId) };
            const auto& anchorRowId { TE::properties.at (TE::selectionAnchorRowId) };
            const auto& cursorRowId { TE::properties.at (TE::selectionCursorRowId) };

            const int selType { state.loadValue (teId, selTypeId) };

            if (selType != static_cast<int> (terminal::SelectionType::none))
            {
                const int anchorRow { state.loadValue (teId, anchorRowId) + (-count) };
                const int cursorRow { state.loadValue (teId, cursorRowId) + (-count) };

                if (anchorRow < 0 or cursorRow < 0)
                {
                    state.storeValue (teId, selTypeId, static_cast<int> (terminal::SelectionType::none));
                }
                else
                {
                    state.storeValue (teId, anchorRowId, anchorRow);
                    state.storeValue (teId, cursorRowId, cursorRow);
                }
            }
        });

    // State delivery: screenDirty — increment monotonic counter so Screen detects new cell data.
    events.add<int> (id::screenDirty,
                     [this] (int screen)
                     {
                         const juce::Identifier dirtyScreenId { Map::Screen::getContext()->get (screen) };
                         const int current { state.loadValue (dirtyScreenId, id::screenDirty) };
                         state.storeValue (dirtyScreenId, id::screenDirty, current + 1);
                     });

    // State delivery: cursor row for active screen.
    events.add<int, cell> (id::cursorRow,
                           [this] (int scr, cell row)
                           {
                               const juce::Identifier rowScreenId { Map::Screen::getContext()->get (scr) };
                               state.storeValue (rowScreenId, id::cursorRow, row.value);
                               state.setSnapshotDirty();
                           });

    // State delivery: cursor column for active screen.
    events.add<int, cell> (id::cursorCol,
                           [this] (int scr, cell col)
                           {
                               const juce::Identifier colScreenId { Map::Screen::getContext()->get (scr) };
                               state.storeValue (colScreenId, id::cursorCol, col.value);
                               state.setSnapshotDirty();
                           });

    // State delivery: cursor visibility for active screen.
    events.add<int, bool> (id::cursorVisible,
                           [this] (int scr, bool visible)
                           {
                               const juce::Identifier visScreenId { Map::Screen::getContext()->get (scr) };
                               state.storeValue (visScreenId, id::cursorVisible, visible ? 1 : 0);
                           });

    // State delivery: mode flags.
    for (const auto& modeId : { id::applicationCursor,
                                id::bracketedPaste,
                                id::mouseTracking,
                                id::mouseMotionTracking,
                                id::mouseAllTracking,
                                id::focusEvents,
                                id::win32InputMode })
    {
        events.add<bool> (modeId,
                          [this, modeId] (bool value)
                          {
                              state.setMode (modeId, value);
                          });
    }

    // BEL (0x07) — write ASCII BEL to stderr for system alert sound.
    events.add (id::bell,
                []
                {
                    std::fwrite ("\a", 1, 1, stderr);
                });

    // OSC 52 — clipboard write.
    events.add<const juce::String&> (id::clipboardChanged,
                                     [] (const juce::String& text)
                                     {
                                         juce::SystemClipboard::copyTextToClipboard (text);
                                     });

    // OSC 9 / OSC 777 — desktop notification.
    events.add<const juce::String&, const juce::String&> (id::desktopNotification,
                                                          [] (const juce::String& title, const juce::String& body)
                                                          {
                                                              terminal::showNotification (title, body);
                                                          });

    // Screen switch — fired by Video::setScreen() with the OLD screen's cursor values.
    // Saves old cursor to State, loads new cursor from State atomics, calls video.loadScreenState().
    events.add<int, int, int, bool> (
        id::screenSwitch,
        [this] (int newScreen, int oldRow, int oldCol, bool oldVisible)
        {
            const int oldScreen { newScreen == Map::Screen::alternate ? Map::Screen::normal : Map::Screen::alternate };
            const juce::Identifier oldScreenId { Map::Screen::getContext()->get (oldScreen) };
            state.storeValue (oldScreenId, id::cursorRow, oldRow);
            state.storeValue (oldScreenId, id::cursorCol, oldCol);
            state.storeValue (oldScreenId, id::cursorVisible, oldVisible ? 1 : 0);

            const juce::Identifier newScreenId { Map::Screen::getContext()->get (newScreen) };
            const int newRow { state.loadValue (newScreenId, id::cursorRow) };
            const int newCol { state.loadValue (newScreenId, id::cursorCol) };
            const bool newVisible { state.loadValue (newScreenId, id::cursorVisible) != 0 };
            const uint32_t newKbFlags { static_cast<uint32_t> (state.loadValue (newScreenId, id::keyboardFlags)) };

            // scrollTop/scrollBottom reset to 0 (sentinel = full screen); wrapPending cleared.
            video.loadScreenState (cell (newRow), cell (newCol), newVisible, cell { 0 }, cell { 0 }, false, newKbFlags);
        });

    // DiscreteStateTransition SIGWINCH debounce — fired when resize gesture settles.
    events.add<> (id::resizeEnd,
                  [this]
                  {
                      if (tty != nullptr and tty->isThreadRunning())
                      {
                          const cell cols { state.getCols() };
                          const cell rows { state.getVisibleRows() };
                          const int pixelWidth { static_cast<int> (
                              jam::ValueTree::getValueFromChildWithID (state.get(), jam::ID::width).getValue()) };
                          const int pixelHeight { static_cast<int> (
                              jam::ValueTree::getValueFromChildWithID (state.get(), jam::ID::height).getValue()) };
                          tty->platformResize (cols, rows, pixelWidth, pixelHeight);
                      }
                  });
}

// =============================================================================

// =============================================================================
// Keyboard mode stack — per-screen progressive enhancement flags (CSI u protocol).
// Moved from State to Processor so keyboard protocol state lives alongside the
// event handlers that manage it (reader thread).
// =============================================================================

void Processor::pushKeyboardMode (int s, uint32_t flags) noexcept
{
    jassert (s >= 0 and s < 2);
    const int base { s * maxKeyboardStackDepth };
    auto& size { keyboardModeStackSize.at (static_cast<size_t> (s)) };

    if (size >= maxKeyboardStackDepth)
    {
        for (int i { 0 }; i < maxKeyboardStackDepth - 1; ++i)
        {
            jassert (base + i + 1 < 2 * maxKeyboardStackDepth);
            keyboardModeStack.at (static_cast<size_t> (base + i)) =
                keyboardModeStack.at (static_cast<size_t> (base + i + 1));
        }

        --size;
    }

    jassert (base + size < 2 * maxKeyboardStackDepth);
    keyboardModeStack.at (static_cast<size_t> (base + size)) = flags;
    ++size;
    const juce::Identifier pushScreenId { Map::Screen::getContext()->get (s) };
    state.storeValue (pushScreenId, id::keyboardFlags, static_cast<int> (flags));
}

void Processor::popKeyboardMode (int s, int count) noexcept
{
    jassert (s >= 0 and s < 2);
    auto& size { keyboardModeStackSize.at (static_cast<size_t> (s)) };
    const int toPop { std::min (count, size) };
    size -= toPop;

    const int base { s * maxKeyboardStackDepth };
    jassert (size <= 0 or base + size - 1 < 2 * maxKeyboardStackDepth);
    const uint32_t current { size > 0 ? keyboardModeStack.at (static_cast<size_t> (base + size - 1)) : 0u };
    const juce::Identifier popScreenId { Map::Screen::getContext()->get (s) };
    state.storeValue (popScreenId, id::keyboardFlags, static_cast<int> (current));
}

void Processor::setKeyboardMode (int s, uint32_t flags, int mode) noexcept
{
    jassert (s >= 0 and s < 2);
    const int base { s * maxKeyboardStackDepth };
    auto& size { keyboardModeStackSize.at (static_cast<size_t> (s)) };

    if (size == 0)
    {
        jassert (base < 2 * maxKeyboardStackDepth);
        keyboardModeStack.at (static_cast<size_t> (base)) = 0u;
        size = 1;
    }

    jassert (base + size - 1 < 2 * maxKeyboardStackDepth);
    auto& top { keyboardModeStack.at (static_cast<size_t> (base + size - 1)) };

    static constexpr int kbModeSet { 1 };// XTMODKEYS: assign flags verbatim
    static constexpr int kbModeOr { 2 };// XTMODKEYS: enable bits (bitwise OR)
    static constexpr int kbModeAndNot { 3 };// XTMODKEYS: disable bits (bitwise AND NOT)

    if (mode == kbModeSet)
    {
        top = flags;
    }
    else if (mode == kbModeOr)
    {
        top |= flags;
    }
    else if (mode == kbModeAndNot)
    {
        top &= ~flags;
    }

    const juce::Identifier setScreenId { Map::Screen::getContext()->get (s) };
    state.storeValue (setScreenId, id::keyboardFlags, static_cast<int> (top));
}

void Processor::resetKeyboardMode (int s) noexcept
{
    jassert (s >= 0 and s < 2);
    keyboardModeStackSize.at (static_cast<size_t> (s)) = 0;
    const juce::Identifier resetScreenId { Map::Screen::getContext()->get (s) };
    state.storeValue (resetScreenId, id::keyboardFlags, 0);
}

// =============================================================================

/**
 * @brief ValueTree::Listener — reacts to top-down property changes from Display.
 *
 * Fires on the message thread when State's ValueTree properties change.
 * Handles shell integration callbacks (outputBlockTop → command process query,
 * promptRow → clear foreground), dimension changes (cols, visibleRows) via
 * transitioner.set() or direct cold-start allocation, cell pixel changes (cellWidth,
 * cellHeight) applied directly to Video, and displayName recomputation
 * from foregroundProcess / cwd.
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
                        state.get().setProperty (id::foregroundProcess, juce::String(), nullptr);
                    }
                    else
                    {
                        static constexpr int fgNameBufSize { 256 };
                        char fgNameBuf[fgNameBufSize] {};
                        const int fgNameLen { tty->getProcessName (fgPid, fgNameBuf, fgNameBufSize) };

                        if (fgNameLen > 0)
                            state.get().setProperty (
                                id::foregroundProcess, juce::String::fromUTF8 (fgNameBuf, fgNameLen), nullptr);
                    }
                }
            }

            if (onStateFlush != nullptr)
                onStateFlush();
        }

        if (paramId == id::promptRow)
        {
            state.get().setProperty (id::foregroundProcess, juce::String(), nullptr);

            if (onStateFlush != nullptr)
                onStateFlush();
        }

        if (paramId == id::cols or paramId == id::visibleRows)
        {
            const cell newCols { state.getCols() };
            const cell newRows { state.getVisibleRows() };

            if (newCols.value > 0 and newRows.value > 0)
            {
                if (buffer.getNumRows() == 0)
                {
                    // Cold start — allocate directly (no transition).
                    const int scrollbackLines { AppState::getContext()->getRawParameterValue<int> (app::id::scrollbackLines)->load() };
                    const int ringSize { juce::nextPowerOfTwo (scrollbackLines + newRows.value) };
                    buffer.setSize (2, ringSize, newCols.value);
                    video.setDimensions (newCols, newRows);
                    video.resize (newCols, newRows);
                }
                else if (newCols.value != video.getCols().value or newRows.value != video.getVisibleRows().value)
                {
                    transitioner.liveRows = { 0, 0 };
                    transitioner.set (id::resizeStart, newCols, newRows);
                }
            }
        }

        if (paramId == id::cellWidth or paramId == id::cellHeight)
        {
            const juce::Identifier displayId { id::DISPLAY };
            auto displayNode { state.get().getChildWithName (displayId) };
            const int cellW { static_cast<int> (
                jam::ValueTree::getValueFromChildWithID (displayNode, id::cellWidth).getValue()) };
            const int cellH { static_cast<int> (
                jam::ValueTree::getValueFromChildWithID (displayNode, id::cellHeight).getValue()) };
            video.setCellSize (cellW, cellH);
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
        const int activeScr { state.getActiveScreen() };
        const juce::Identifier kbScreenId { Map::Screen::getContext()->get (activeScr) };
        auto kbScreenNode { state.get().getChildWithName (kbScreenId) };
        const uint32_t keyboardFlags { static_cast<uint32_t> (
            static_cast<int> (jam::ValueTree::getValueFromChildWithID (kbScreenNode, id::keyboardFlags).getValue())) };
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
juce::String Processor::encodeMouseEvent (int button, cell col, cell row, bool press) const noexcept
{
    const char finalChar { press ? 'M' : 'm' };
    return juce::String ("\x1b[<") + juce::String (button) + ";" + juce::String (col.value + 1) + ";"
           + juce::String (row.value + 1) + finalChar;
}

/**
 * @brief Processes raw bytes through the parser pipeline.
 *
 * Pure bytes-to-Buffer pipeline: forwards to Parser::process(), flushes Video,
 * and consumes the paste echo gate.  No lock, no suspended check, no cell-size
 * detection — all resize work is handled exclusively by DiscreteStateTransition on the
 * message thread.
 *
 * @note READER THREAD only — called from the byte source (terminal::Session
 *       onBytes callback or IPC dispatch in client mode).
 */
void Processor::process (const char* data, int length) noexcept
{
    jassert (parser != nullptr);

    if (video.getCols() > cell (0) and video.getVisibleRows() > cell (0))
    {
        parser->process (reinterpret_cast<const uint8_t*> (data), static_cast<size_t> (length));
        video.flush();
        video.flushResponses();
    }

    state.consumePasteEcho (length);
}

State& Processor::getState() noexcept { return state; }
const State& Processor::getState() const noexcept { return state; }

jam::Buffer<jam::Cell>& Processor::getBuffer() noexcept { return buffer; }
const jam::Buffer<jam::Cell>& Processor::getBuffer() const noexcept { return buffer; }

const juce::String& Processor::getUuid() const noexcept { return uuid; }

void Processor::flushResponses() noexcept { video.flushResponses(); }

/**
 * @brief Transfers TTY ownership to this Processor.
 *
 * @note MESSAGE THREAD.
 */
void Processor::setTTY (std::unique_ptr<TTY> ttyToOwn) noexcept { tty = std::move (ttyToOwn); }

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
}// namespace terminal
