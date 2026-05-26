/**
 * @file ProcessorEvents.cpp
 * @brief Event handler registration for Processor — all events fired by Video and TTY.
 *
 * This translation unit implements `Processor::registerEvents()`.
 * Separated from Processor.cpp following the VideoCSI.cpp / VideoEdit.cpp pattern:
 * member function definitions in dedicated translation units for readability.
 */

#include "Processor.h"

namespace terminal
{

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
 *   `"writeHead"` carries the raw ring write position for the active screen; handler preserves historyRows from State.
 *   Neither `"cursor"` nor `"writeHead"` carry a screen param — handlers read activeScreen from State.
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
    // promptRow fires when shell is at prompt — clear foreground process (shell owns the fg slot).
    events.add<int> (id::promptRow,
                     [this] (int relativeRow)
                     {
                         state.setPromptRow (cell (relativeRow));
                         state.setForegroundProcess ("", 0);
                     });

    // outputBlockStart fires when a command starts — query foreground process from TTY.
    events.add<int> (id::outputBlockStart,
                     [this] (int relativeRow)
                     {
                         state.setOutputBlockStart (cell (relativeRow));

                         if (tty != nullptr)
                         {
                             const int fgPid { tty->getForegroundPid() };
                             const int shellPid { tty->getShellPid() };

                             if (fgPid > 0 and fgPid != shellPid)
                             {
                                 static constexpr int fgNameBufSize { 256 };
                                 char fgNameBuf[fgNameBufSize] {};
                                 const int fgNameLen { tty->getProcessName (fgPid, fgNameBuf, fgNameBufSize) };

                                 if (fgNameLen > 0)
                                     state.setForegroundProcess (fgNameBuf, fgNameLen);
                             }
                         }
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

    // State delivery: scrollUp — pack new WriteHead (ring position + history rows) capped at scrollbackLines.
    // Selection adjustment is Screen's domain — handled in Screen::valueTreePropertyChanged on the message thread.
    // Excess rows beyond scrollbackLines are overwritten naturally by ring rotation.
    events.add<int, int> (
        id::scrollUp,
        [this] (int count, int newWritePosition)
        {
            const int scrollbackLines { AppState::getContext()->getRawParameterValue<int> (app::id::scrollbackLines)->load() };
            const juce::Identifier scrollScreenId { Map::Screen::getContext()->get (state.loadValue (id::SESSION, id::activeScreen)) };
            const jam::WriteHead currentWH { jam::WriteHead::unpack (state.loadValue (scrollScreenId, id::writeHead)) };
            const int newHistoryRows { juce::jmin (currentWH.historyRows + count, scrollbackLines) };
            const jam::WriteHead newWH { newWritePosition, newHistoryRows };
            state.storeValue (scrollScreenId, id::writeHead, newWH.pack());
        });

    // State delivery: screenDirty — increment monotonic counter so Screen detects new cell data.
    // Active screen is the only screen Video writes cells to; screen param is redundant.
    events.add (id::screenDirty,
                [this]
                {
                    const juce::Identifier dirtyScreenId { Map::Screen::getContext()->get (state.loadValue (id::SESSION, id::activeScreen)) };
                    const int current { state.loadValue (dirtyScreenId, id::screenDirty) };
                    state.storeValue (dirtyScreenId, id::screenDirty, current + 1);
                });

    // State delivery: packed cursor (row + col + visible + kbFlags) for active screen.
    // Video fires cursor only for the active screen — no screen param needed.
    events.add<int> (id::cursor,
                     [this] (int packedCursor)
                     {
                         const juce::Identifier cursorScreenId { Map::Screen::getContext()->get (state.loadValue (id::SESSION, id::activeScreen)) };
                         state.storeValue (cursorScreenId, id::cursor, packedCursor);
                         state.setSnapshotDirty();
                     });

    // State delivery: writeHead position for active screen — preserves historyRows set by scrollUp.
    // Video fires writeHead only for the active screen; handler reads activeScreen from State.
    events.add<int> (id::writeHead,
                     [this] (int position)
                     {
                         const juce::Identifier whScreenId { Map::Screen::getContext()->get (state.loadValue (id::SESSION, id::activeScreen)) };
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

} // namespace terminal
