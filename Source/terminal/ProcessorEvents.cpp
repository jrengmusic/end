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
 * - `"activeScreen"` / `"cursor"` — Video::flush() flush events; write the
 *   corresponding State atomics.  `"cursor"` carries packed CursorState (row+col+visible+kbFlags).
 *   `"cursor"` does not carry a screen param — handler reads activeScreen from State.
 *   `"writeHead"` is eliminated: flat buffer has no ring rotation, head is always 0.
 *
 * - `"shellExited"` — fired by TTY on reader thread on EOF; calls `State::setShellExited(true)`.
 *
 * - `"drainComplete"` — fired by TTY on reader thread after each full PTY drain;
 *   flushes parser responses, clears paste gate.
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

    // outputBlockStart fires when a command starts — set output block start, clear promptRow, query foreground process from TTY.
    events.add<int> (id::outputBlockStart,
                     [this] (int relativeRow)
                     {
                         state.setOutputBlockStart (cell (relativeRow));
                         state.setPromptRow (cell (-1));

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

    // Clear scrollback — zero all rows via Video, reset scrollOffset.
    // video.clearChannel zeros all rows through the active blocks (flat buffer, head always 0).
    // writeHead is always 0 for a flat buffer — no need to store it on clear.
    // CellFifo reset is dispatched to the message thread so it is safe from the reader thread.
    events.add<int> (id::clearBuffer,
                     [this] (int screen)
                     {
                         video.clearChannel (screen);

                         // Resize CellFifo on the message thread — SPSC: drain side must not race the resize.
                         juce::MessageManager::callAsync ([this]
                         {
                             const int scrollbackLines { AppModel::getContext()->getRawParameterValue<int> (app::id::scrollbackLines)->load() };
                             const int cols            { video.getCols().value };
                             const int visibleRows     { video.getVisibleRows().value };
                             cellFifo.setSize (scrollbackLines * cols, visibleRows * cols);
                         });
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

    // State delivery: activeScreen.
    events.add<int> (id::activeScreen,
                     [this] (int scr)
                     {
                         state.setScreen (scr);
                     });

    // Push path: pushLine — fired on the reader thread for each line departing to scrollback.
    // Args: int screen, int row — the physical row index of the departing row in the flat buffer.
    // Fires BEFORE the row is shifted up and cleared.
    // Alternate screen discards departing rows (zero scrollback).
    // Normal screen rows are copied into CellFifo unconditionally — no heap allocation, no callAsync.
    events.add<int, int> (
        id::pushLine,
        [this] (int screen, int row)
        {
            if (screen == Map::Screen::normal)
            {
                const jam::Buffer<jam::Row>& buf { video.getGrid() };
                const int numRingRows { buf.getNumRows() };
                const int numCols     { buf.getNumCols() };

                const jam::Block<jam::Row> block {
                    buf.getChannelPointer (screen),
                    0,
                    0,
                    numRingRows,
                    numCols,
                    buf.getRowStrideBytes(),
                    numRingRows
                };

                const jam::Row* const departingRow { block.getRowPointer (row) };
                const int usedCols { static_cast<int> (departingRow->usedCols) };

                uint8_t flags { 0 };

                if ((departingRow->flags & jam::Row::flexWrap) != 0)
                    flags |= CellFifo::isContinuedFlag;

                if ((departingRow->flags & jam::Row::justify) != 0)
                    flags |= CellFifo::isJustifiedFlag;

                // history: departing row enters scrollback history (I1).
                cellFifo.pushHistory (departingRow->cells, usedCols, flags);
            }
        });

    // State delivery: scrollUp — flat buffer, no ring rotation, no writeHead to update.
    // History depth is derived from textLineArrays.at(0).totalRows().
    events.add<int> (
        id::scrollUp,
        [this] (int /*count*/)
        {
            // Flat buffer: writeHead is always 0. No State update needed on scroll.
        });

    // State delivery: screenDirty — push live viewport rows [0..cursorRow] to the active ring
    // via pushActive(), then bump the monotonic counter.  The counter change propagates via
    // State::flush() to the ValueTree on the message thread, signalling Display to drain
    // CellFifo and update its CodeView.  Fired by Video::flush() on the reader thread;
    // Video's own buffer is safe to read here (same thread).  Content-bounded: only rows
    // [0..cursorRow] are pushed — the blank viewport tail below the cursor is never pushed.
    events.add (id::screenDirty,
                [this]
                {
                    const int activeScr { state.loadValue<int> (id::SESSION, id::activeScreen) };

                    const jam::Buffer<jam::Row>& buf { video.getGrid() };
                    const int numRingRows { buf.getNumRows() };
                    const int numCols     { buf.getNumCols() };

                    const jam::Block<jam::Row> block {
                        buf.getChannelPointer (activeScr),
                        0,
                        0,
                        numRingRows,
                        numCols,
                        buf.getRowStrideBytes(),
                        numRingRows
                    };

                    const int cursorRow { video.getCursorRow().value };

                    for (int r { 0 }; r <= cursorRow; ++r)
                    {
                        const jam::Row* const liveRow { block.getRowPointer (r) };
                        const int usedCols { static_cast<int> (liveRow->usedCols) };

                        uint8_t flags { 0 };

                        if ((liveRow->flags & jam::Row::flexWrap) != 0)
                            flags |= CellFifo::isContinuedFlag;

                        if ((liveRow->flags & jam::Row::justify) != 0)
                            flags |= CellFifo::isJustifiedFlag;

                        // active: live viewport row; row index is the zero-based screen row (I2).
                        cellFifo.pushActive (liveRow->cells, usedCols, flags);
                    }

                    const juce::Identifier dirtyScreenId { Map::Screen::getContext()->get (activeScr) };
                    const int current { state.loadValue<int> (dirtyScreenId, id::screenDirty) };
                    state.storeValue (dirtyScreenId, id::screenDirty, current + 1);
                });

    // State delivery: packed cursor (row + col + visible + kbFlags) for active screen.
    // Video fires cursor only for the active screen — no screen param needed.
    events.add<int> (id::cursor,
                     [this] (int packedCursor)
                     {
                         const juce::Identifier cursorScreenId { Map::Screen::getContext()->get (state.loadValue<int> (id::SESSION, id::activeScreen)) };
                         state.storeValue (cursorScreenId, id::cursor, packedCursor);
                         state.setSnapshotDirty();
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

    // Drain complete — flush parser responses, clear paste gate.
    events.add (id::drainComplete,
                [this]
                {
                    flushResponses();
                    state.clearPasteEchoGate();
                });
}

} // namespace terminal
