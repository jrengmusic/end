# Handoff: Buffer-to-TextEditor Pipeline Sprint

**Date:** 2026-05-29 to 2026-05-30
**Status:** Partially working, resize content destruction unresolved (DEBT-20260530T100000)

---

## What Was Done

### jam (shared framework)

1. **jam_text_editor.h/.cpp** — TextEditor constructor takes `jam::ValueTree& parentState`. Owns `jam::ComponentAttachment` internally — grafts TEXT_EDITOR state node into parent VT on construction, ungrafts on destruction. RAII lifecycle. Removed: `graftState()`, `getProjectedRows()`, `getVisibleWidth()`, `getVisibleHeight()`. `setCaretPosition(jam::Cell::Point)` maps viewport-relative row to document space internally using `projectedRows` and viewport height. Added `setCaretShape(int)`.

### end (terminal)

1. **Screen.h/Screen.cpp** — DELETED. Session owns `jam::TextEditor textEditor` directly. Display absorbs caret positioning, ColourIds, state grafting.

2. **Session.h/.cpp** — `terminal::Screen screen` → `jam::TextEditor textEditor`. `getScreen()` → `getTextEditor()`. Constructor passes `state` (jam::ValueTree&) to TextEditor constructor. TextEditor grafts via ComponentAttachment at construction.

3. **Display.h/.cpp** — Absorbs Screen's ColourIds enum. vTPC decomposed: triggers on `screenDirty` (content update), `activeScreen` (screen switch), `cursor` (caret position). Calls `session.getTextEditor().setCaretPosition(Cell::Point)` with raw video cursor coords — no mapping in Display. `switchRenderer` removed (JUCE handles GL/CPU context transparently).

4. **Processor.h/.cpp** — vTPC decomposed into `jam::Function::Map<Identifier, void> parameters` dispatch (JFS pattern). Three handlers: `setCellSize()`, `setText()`, `setDisplayName()`. Destructor now removes VT listener (ComponentAttachment ungraft fires VT events during Session destruction). `prepare()` only: Video resize + dirty flags rebuild + SIGWINCH. No TLA touch, no CellFifo reset, no liveRows mutation.

5. **ProcessorEvents.cpp** — pushLine handler: promptRow gate removed, all departing rows go to CellFifo unconditionally. outputBlockStart handler: CellFifo commit block removed.

6. **Video.cpp** — executeLineFeed: non-scroll pushLine removed. Only scrollUpAndFill fires pushLine (actual departures).

7. **CellFifo.h** — `drainInto()` simplified: no insertIndex parameter, appends via `add()` instead of `insert()`. Fixes capacity-trim index drift crash on `seq 10M`.

8. **Identifier.h** — Dead identifiers removed: `historyCount`, `cursorRow`, `cursorCol`, `wrapPending`, `scrollTop`, `scrollBottom`. "Branked" → "Bracketed" typo fixed.

9. **Parameters.xml** — `historyCount` → `liveRows`. `promptRow` default: `0` → `-1` (sentinel).

10. **State.cpp** — `getCols()`/`getVisibleRows()`: int64_t truncation fix. Dead getters removed: `getRowDirtyCount`, `isSnapshotDirty`, `getShellExited`.

11. **PaneComponent.h** — `switchRenderer` pure virtual removed (JUCE handles GL/CPU transparently). Both Display and whelmed::Component overrides removed. Tabs::switchRenderer and MainComponent call removed.

12. **ARCHITECTURE.md** — Comprehensive sweep: ~30+ stale references updated (Screen → TextEditor, getScreen → getTextEditor, setWinsize → prepare, Buffer ownership, TextLine/TextLineArray glossary, module map, Session API table).

---

## What Is Broken

### Resize content destruction (DEBT-20260530T100000)

Resize destroys TLA history. Root cause: the full rebuild path (`drainCount > 0 or totalRows < contentRows`) removes old live zone entries and re-adds from Video. After resize, Video buffer is cleared but the rebuild copies zeroed rows into TLA before the shell redraws.

The `totalRows < contentRows` gate also fires on resize (cursor clamped to new dimensions → contentRows changes → TLA size mismatches). This triggers the full rebuild from the empty Video buffer.

The fundamental issue: TLA content is still coupled to Buffer<Row> lifecycle through the full rebuild path. The dirty-row in-place path is correct (only overwrites what Video actually wrote), but the full rebuild path bypasses dirty checks.

### Known working

- `cd end`, `ls`, `time seq 150` — renders correctly
- Cursor visible from init
- Alternate screen enter/exit — content preserved (screen switch triggers via `id::activeScreen`)
- `seq 1M` — no crash (CellFifo drain via `add()` instead of `insert()`)

---

## Key Architecture Decisions (locked)

1. **Session owns jam::TextEditor directly** — no Screen class. TextEditor's ComponentAttachment grafts state node into terminal VT at construction.
2. **Display orchestrates** — caret positioning, ColourIds, content triggers (screenDirty + activeScreen + cursor). Tell pattern — Display tells TextEditor "where", TextEditor maps internally.
3. **Processor vTPC = JFS parameters pattern** — `jam::Function::Map` dispatch on paramId. `setCellSize()`, `setText()`, `setDisplayName()`.
4. **prepare() decoupled from TLA** — only Video resize + dirty flags + SIGWINCH. No TLA, no CellFifo, no liveRows.
5. **pushLine unconditional** — no promptRow gate. Only scrollUpAndFill fires pushLine (actual departures from Video row 0).
6. **Value::map for coordinate projection** — dirty-row in-place path uses Value::map from Video row space to TLA index space.
7. **liveRows is a per-screen Parameter** — not a Processor member. Stored via storeValue, read via loadValue.

---

## Files Modified

### jam
- `jam_gui/text_editor/jam_text_editor.h`
- `jam_gui/text_editor/jam_text_editor.cpp`

### end
- `Source/terminal/Processor.h`
- `Source/terminal/Processor.cpp`
- `Source/terminal/ProcessorEvents.cpp`
- `Source/terminal/Video.cpp`
- `Source/terminal/Video.h` (doxygen)
- `Source/terminal/CellFifo.h`
- `Source/terminal/State.h`
- `Source/terminal/State.cpp`
- `Source/terminal/Session.h`
- `Source/terminal/Session.cpp`
- `Source/terminal/Identifier.h`
- `Source/terminal/Parameters.xml`
- `Source/terminal/Input.h` (doxygen)
- `Source/terminal/component/Display.h`
- `Source/terminal/component/Display.cpp`
- `Source/terminal/component/PaneComponent.h`
- `Source/terminal/component/LookAndFeel.h`
- `Source/terminal/component/LookAndFeel.cpp`
- `Source/terminal/component/Tabs.h`
- `Source/terminal/component/TabsActions.cpp`
- `Source/whelmed/component/Component.h`
- `Source/whelmed/component/Component.cpp`
- `Source/MainComponent.cpp`
- `Source/Main.h`
- `ARCHITECTURE.md`
- `DEBT.md`

### Deleted
- `Source/terminal/component/Screen.h`
- `Source/terminal/component/Screen.cpp`
- `PLAN-buffer-texteditor.md`
- `PLAN-cell-type-system.md`

---

## Next Session Priority

1. Fix resize content destruction (DEBT-20260530T100000): the full rebuild path copies from Video before Video has real content. The dirty-row path is correct but can't handle TLA growth (cursor advancing to new row). Need a way to grow TLA without copying garbage from Video.
2. The `totalRows < contentRows` condition in the rebuild gate conflates cold start (TLA needs growth) with resize (TLA has stale entries). These are different scenarios needing different handling.
3. Consider: should the dirty path handle growth by adding new entries for dirty rows beyond current TLA size? This would eliminate the full rebuild path entirely for cursor advancement.
