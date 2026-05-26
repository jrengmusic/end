/**
 * @file Processor.cpp
 * @brief Implementation of the terminal pipeline orchestrator.
 *
 * Implements Processor — the pipeline half that owns State (the APVTS), Video
 * (the terminal state machine), and Parser,
 * and references Buffer<Row> owned by terminal::Session.
 * The PTY-side TTY is owned by Processor and created by startTTY().
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
 * @brief Constructs the Processor: binds State&, activeBlocksRef, and TextBuffer&; constructs Video and Parser.
 *
 * State is owned by terminal::Session and passed by reference.
 * activeBlocksRef is Screen's atomic active-blocks pointer — Video loads it via refreshBlocks().
 * Video is owned by this Processor and receives activeBlocksRef and events& references.
 * Parser is owned by this Processor and receives Video& reference directly.
 * UUID is provided by the caller — no internal generation.
 * Buffer resize is handled entirely by Session::valueChanged (winsize) — Processor does not touch Buffer.
 *
 * @param stateRef        Terminal parameter store owned by terminal::Session.
 * @param activeBlocksRef Reference to Screen's atomic active-blocks pointer.
 * @param textBufferRef   Cross-thread string buffer owned by terminal::Session.
 * @param uuid            Stable UUID for this Processor — generated once by the caller.
 *
 * @note MESSAGE THREAD — must be constructed on the message thread.
 */
Processor::Processor (State& stateRef,
                      std::atomic<jam::Block<jam::Row>*>& activeBlocksRef,
                      TextBuffer& textBufferRef,
                      const juce::String& uuid)
    : textBuffer (textBufferRef)
    , state (stateRef)
    , video (activeBlocksRef, events)
    , skit (events)
    , uuid (uuid)
{
    registerEvents();
    parser = std::make_unique<Parser> (video);
    state.get().addListener (this);
}

// =============================================================================

/**
 * @brief Destroys the Processor.
 *
 * Closes the TTY first — stops the reader thread before the events map is
 * destroyed by member destruction.  Reader thread joins inside close(), so
 * events.get(id::data) cannot fire after close() returns.
 * No explicit `removeListener()` needed — the ValueTree inside State is
 * destroyed alongside this Processor (member destruction order).
 *
 * @note MESSAGE THREAD.
 */
Processor::~Processor()
{
    if (tty != nullptr)
        tty->close();
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
 * - `"activeScreen"` / `"cursor"` / `"writeHead"` — Video::flush() flush events; write the
 *   corresponding State atomics.  `"cursor"` carries packed CursorState (row+col+visible+kbFlags).
 *   `"writeHead"` carries the raw ring write position; handler preserves historyRows from State.
 *
 * - `"shellExited"` — fired by TTY on reader thread on EOF; calls `State::setShellExited(true)`.
 *
 * - `"drainComplete"` — fired by TTY on reader thread after each full PTY drain;
 *   flushes parser responses, clears paste gate, handles sync resize.
 *
 * - `"applicationCursor"` / `"bracketedPaste"` / ... — Video::flush() mode flag flushes.
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

    // Clear scrollback — zero all rows via Video, reset writeHead, and clear scroll offset for the given screen.
    // video.clearChannel zeros all rows through the active blocks.
    // video.getWritePosition returns the current ring position — preserved after clear (ring head is unchanged).
    // historyRows is reset to 0 — no history remains after clear.
    events.add<int> (id::clearBuffer,
                     [this] (int screen)
                     {
                         video.clearChannel (screen);
                         const juce::Identifier clearScreenId { Map::Screen::getContext()->get (screen) };
                         const jam::WriteHead clearedWH { video.getWritePosition (screen), 0 };
                         state.storeValue (clearScreenId, id::writeHead, clearedWH.pack());
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

    // State delivery: scrollUp — pack new WriteHead (ring position + history rows) capped at scrollbackLines,
    // then adjust selection anchors. Excess rows beyond scrollbackLines are overwritten naturally by ring rotation.
    events.add<int, int, int> (
        id::scrollUp,
        [this] (int screen, int count, int newWritePosition)
        {
            const int scrollbackLines { AppState::getContext()->getRawParameterValue<int> (app::id::scrollbackLines)->load() };
            const juce::Identifier scrollScreenId { Map::Screen::getContext()->get (screen) };
            const jam::WriteHead currentWH { jam::WriteHead::unpack (state.loadValue (scrollScreenId, id::writeHead)) };
            const int newHistoryRows { juce::jmin (currentWH.historyRows + count, scrollbackLines) };
            const jam::WriteHead newWH { newWritePosition, newHistoryRows };
            state.storeValue (scrollScreenId, id::writeHead, newWH.pack());

            const int selType { state.loadValue (jam::TextEditor::properties.at (jam::TextEditor::textEditorId),
                                                 jam::TextEditor::properties.at (jam::TextEditor::selectionTypeId)) };

            if (selType != static_cast<int> (terminal::SelectionType::none))
            {
                const int anchorRow { state.loadValue (jam::TextEditor::properties.at (jam::TextEditor::textEditorId),
                                                       jam::TextEditor::properties.at (jam::TextEditor::selectionAnchorRowId)) - count };
                const int selCursorRow { state.loadValue (jam::TextEditor::properties.at (jam::TextEditor::textEditorId),
                                                          jam::TextEditor::properties.at (jam::TextEditor::selectionCursorRowId)) - count };

                if (anchorRow < 0 or selCursorRow < 0)
                {
                    state.storeValue (jam::TextEditor::properties.at (jam::TextEditor::textEditorId),
                                      jam::TextEditor::properties.at (jam::TextEditor::selectionTypeId),
                                      static_cast<int> (terminal::SelectionType::none));
                }
                else
                {
                    state.storeValue (jam::TextEditor::properties.at (jam::TextEditor::textEditorId),
                                      jam::TextEditor::properties.at (jam::TextEditor::selectionAnchorRowId), anchorRow);
                    state.storeValue (jam::TextEditor::properties.at (jam::TextEditor::textEditorId),
                                      jam::TextEditor::properties.at (jam::TextEditor::selectionCursorRowId), selCursorRow);
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

    // State delivery: packed cursor (row + col + visible + kbFlags) for active screen.
    events.add<int, int> (id::cursor,
                          [this] (int scr, int packedCursor)
                          {
                              const juce::Identifier cursorScreenId { Map::Screen::getContext()->get (scr) };
                              state.storeValue (cursorScreenId, id::cursor, packedCursor);
                              state.setSnapshotDirty();
                          });

    // State delivery: writeHead position per screen — preserves historyRows set by scrollUp.
    events.add<int, int> (id::writeHead,
                          [this] (int screen, int position)
                          {
                              const juce::Identifier whScreenId { Map::Screen::getContext()->get (screen) };
                              const jam::WriteHead currentWH { jam::WriteHead::unpack (state.loadValue (whScreenId, id::writeHead)) };
                              const jam::WriteHead newWH { position, currentWH.historyRows };
                              state.storeValue (whScreenId, id::writeHead, newWH.pack());
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

    // Byte data from TTY reader thread — optional IPC broadcast then process under callbackLock.
    events.add<const char*, int> (id::data,
                                  [this] (const char* bytes, int len)
                                  {
                                      if (events.contains (id::bytesReceived))
                                          events.get (id::bytesReceived, bytes, len);

                                      const juce::ScopedLock sl (callbackLock);

                                      if (not suspended)
                                          process (bytes, len);
                                  });

    // Shell exited — TTY fires on EOF (reader thread). Write State so flush timer delivers to message thread.
    events.add (id::shellExited,
                [this]
                {
                    state.setShellExited (true);
                });

    // Drain complete — flush parser responses, clear paste gate, handle sync resize.
    events.add (id::drainComplete,
                [this]
                {
                    flushResponses();
                    state.clearPasteEchoGate();

                    if (state.consumeSyncResize())
                        setWinsize (state.getCols(), state.getVisibleRows());
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
 * promptRow → clear foreground), cell pixel changes (cellWidth, cellHeight)
 * applied directly to Video, and displayName recomputation from foregroundProcess / cwd.
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
        }

        if (paramId == id::promptRow)
        {
            state.get().setProperty (id::foregroundProcess, juce::String(), nullptr);
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
 * Clean pipeline — lock and suspend check are the caller's responsibility
 * (host wrapper pattern, matching the JUCE VST wrapper contract).
 *
 * @note READER THREAD only — called from the byte source (terminal::Session
 *       onBytes callback or IPC dispatch in client mode).
 */
void Processor::process (const char* data, int length) noexcept
{
    jassert (parser != nullptr);
    video.refreshBlocks();

    if (video.getCols() > cell (0) and video.getVisibleRows() > cell (0))
    {
        parser->process (reinterpret_cast<const uint8_t*> (data), static_cast<size_t> (length));
        video.flush();
        video.flushResponses();
    }

    state.consumePasteEcho (length);
}

/**
 * @brief Suspends or unsuspends processing.
 *
 * Acquires callbackLock before writing the flag — blocks until any in-progress
 * process() call completes.  Same implementation as AudioProcessor::suspendProcessing.
 *
 * @note MESSAGE THREAD — blocks on callbackLock.
 */
void Processor::suspendProcessing (bool shouldBeSuspended) noexcept
{
    const juce::ScopedLock sl (callbackLock);
    suspended = shouldBeSuspended;
}

State& Processor::getState() noexcept { return state; }
const State& Processor::getState() const noexcept { return state; }

const juce::String& Processor::getUuid() const noexcept { return uuid; }

void Processor::flushResponses() noexcept { video.flushResponses(); }

/**
 * @brief Creates the platform TTY, adds shell env vars, wires callbacks, and opens the PTY.
 *
 * Creates UnixTTY or WindowsTTY with the shared events map, adds all shell integration
 * env vars, wires writeInput/writeToHost events for PTY stdin/response routing
 * (id::data is already registered in registerEvents()), then opens the TTY (starts the reader thread).
 *
 * @param shell   Shell program path.
 * @param args    Shell arguments string.  Empty = none.
 * @param cwd     Initial working directory.  Empty = inherit.
 * @param env     Shell integration environment variable pairs.
 * @param cols    Initial terminal width in character columns.
 * @param rows    Initial terminal height in character rows.
 * @note MESSAGE THREAD — called from Session::start().
 */
void Processor::startTTY (const juce::String& shell,
                           const juce::String& args,
                           const juce::String& cwd,
                           const juce::StringPairArray& env,
                           cell cols,
                           cell rows) noexcept
{
#if JUCE_MAC || JUCE_LINUX
    tty = std::make_unique<UnixTTY> (events);
#elif JUCE_WINDOWS
    tty = std::make_unique<WindowsTTY> (events);
#endif

    const juce::StringArray& keys { env.getAllKeys() };

    for (const auto& key : keys)
        tty->addShellEnv (key, env[key]);

    // writeToHost — parser responses (DSR, DA, CPR) → PTY stdin.
    events.add<const char*, int> (id::writeToHost,
                                  [this] (const char* data, int len)
                                  {
                                      if (tty != nullptr)
                                          tty->write (data, len);
                                  });

    // writeInput — keyboard/mouse input from Display → PTY stdin.
    events.add<const char*, int> (id::writeInput,
                                  [this] (const char* data, int len)
                                  {
                                      tty->write (data, len);
                                  });

    tty->open (cols, rows, shell, args, cwd);
}

/**
 * @brief Single resize API — tells Video the new dimensions and sends SIGWINCH to shell.
 *
 * Called from Processor vTPC (viewport change), Session::start() (initial sizing),
 * and the drainComplete event handler (sync-resize path).
 *
 * @param cols        New column count.
 * @param rows        New row count.
 * @param pixelWidth  Total viewport width in physical pixels (0 if unknown).
 * @param pixelHeight Total viewport height in physical pixels (0 if unknown).
 */
void Processor::setWinsize (cell cols, cell rows, int pixelWidth, int pixelHeight) noexcept
{
    video.setWinsize (cols, rows);

    if (tty != nullptr and tty->isThreadRunning())
        tty->setWinsize (cols, rows, pixelWidth, pixelHeight);
}


/**
 * @brief Writes raw input bytes to the PTY via the writeInput event handler.
 *
 * No-op when writeInput is not registered (remote sessions with no TTY before IPC wiring).
 *
 * @note MESSAGE THREAD.
 */
void Processor::writeInput (const char* data, int len) noexcept
{
    if (events.contains (id::writeInput))
        events.get (id::writeInput, data, len);
}

/**
 * @brief Registers the writeInput event handler.
 *
 * Called by Link for remote (IPC-connected) sessions to route input through IPC.
 * startTTY() calls this internally for PTY sessions.
 *
 * @note MESSAGE THREAD.
 */
void Processor::setInputWriter (std::function<void (const char*, int)> handler) noexcept
{
    events.add<const char*, int> (id::writeInput, std::move (handler));
}

/**
 * @brief Registers an IPC byte broadcast observer on the events map.
 *
 * The registered handler fires on the reader thread inside the id::data handler,
 * before callbackLock is acquired.  Used by nexus::Daemon::wireOnBytes() in daemon mode.
 *
 * @note MESSAGE THREAD — call before the first bytes arrive.
 */
void Processor::setBytesObserver (std::function<void (const char*, int)> handler) noexcept
{
    events.add<const char*, int> (id::bytesReceived, std::move (handler));
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
