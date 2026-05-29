# Handoff: SPSC Pipeline Sprint

**Date:** 2026-05-29
**Status:** Partially implemented, broken, needs continuation
**PLAN:** PLAN-spsc-pipeline.md (outdated — decisions evolved significantly during session)

---

## What Was Done (from HEAD)

### jam (shared framework)

1. **jam_glyph_arrangement_shape.cpp:355** — wrapping boundary `>=` changed to `>`. Confirmed correct by diagnostic logging.
2. **jam_text_line.h** — TextLine: `std::vector<Cell>` replaced with `juce::HeapBlock<Cell> cells` + `int cellCount`. All call sites updated.
3. **jam_text_line_array.h** — TextLineArray rewritten as generic document buffer: `add(TextLine&&)`, `remove(Range<int>)`, `set(int, TextLine&&)`, `insert(int, TextLine&&)`, `setCapacity(int)`, `clear()`, `operator[]`, `totalRows()`. No terminal semantics.
4. **jam_text_editor.h/.cpp** — `physicalViewWidth` member added. TextEditor computes rendering width locally (scrollbar-aware). Does NOT write viewportId to State. Display is sole author of viewportId.
5. **jam_parameter.h** — needs `Parameter<uint64_t>` specialization (not yet implemented).

### end (terminal)

1. **CellFifo.h** — new class. SPSC ring buffer (AbstractFifo + HeapBlock<Cell>). pushRow on reader thread, drainInto on message thread. Joins continued rows into logical TextLines.
2. **State.h/.cpp** — per-row flush-dirty flags (`std::unique_ptr<std::atomic<int>[]>`). setRowDirty/consumeRowDirty/rebuildRowDirtyFlags.
3. **Video.h/.cpp** — rowTouched tracking + fires rowDirty events in flush(). OSC 133 A/B/C/D handlers fire events only (no shadow state — promptRow, inPromptText, isCommandOutput all removed from Video).
4. **Video.cpp executeLineFeed** — fires pushLine unconditionally for non-scroll LF (no gate on Video side).
5. **ProcessorEvents.cpp** — pushLine handler: gates on `state.loadValue(id::SESSION, id::promptRow)` — rows >= promptRow suppressed (active prompt, not history). Writes to CellFifo. No callAsync.
6. **ProcessorEvents.cpp** — outputBlockStart handler: on OSC 133 C, reads promptRow from State atomic, commits prompt block [promptRow..relativeRow-1] to CellFifo, then sets promptRow to -1.
7. **ProcessorEvents.cpp** — requestSyncResize removed. Dead code cleaned (State methods, Identifiers, Parameters.xml).
8. **Processor.h/.cpp** — `prepare(cell cols, cell rows)` replaces resizeVideo + resizeTextLineArray + setWinsize. One method: CellFifo reset, Video resize, dirty flags rebuild, alternate TLA rebuild, SIGWINCH. Uses ScopedLock(callbackLock) internally.
9. **Processor.cpp screenDirty handler** — normal screen: remove mutable tail, drain CellFifo, add mutable rows from Video [promptRow..cursorRow] or [cursorRow] when promptRow < 0.
10. **Display.cpp** — sole author of viewportId. Computes cell dims from contentBounds + font metrics. Scrollbar doesn't trigger Video resize.
11. **Parameters.xml** — promptRow default changed to 0. syncResizePending removed.
12. **Session.cpp wireResizer** — start trigger: suspend + prepare + resume. Stop trigger: prepare (SIGWINCH included in prepare).

---

## What Is Broken

### Critical bugs (from diagnostic log)

1. **Content doubling** — C handler COMMIT_PROMPT pushes rows to CellFifo. Mutable tail re-adds same rows from Video. Overlap = doubled content.
2. **Stale promptRow** — flush ordering: screenDirty fires before promptRow Parameter flushes. Handler reads stale VT value. mutableRows computed from stale data overlaps CellFifo drain.
3. **mutableRows > totalRows** — guard `totalRows >= mutableRows` fails when stale promptRow gives oversized range. No REMOVE. Content accumulates.
4. **Init rendering** — first tick may see stale prompt=0 when cursor is elsewhere. promptRow default=0 in Parameters.xml helps but flush ordering can still produce wrong first frame.
5. **Screen caret** — historyRows=0 placeholder in Screen.cpp. Cursor position always wrong.
6. **DIAG logging** — jam::debug::Log::write calls throughout Video.cpp, VideoOSCExt.cpp, Processor.cpp, ProcessorEvents.cpp. Must be removed.

### Architectural gaps

1. **terminal::Viewport type** — not implemented. Cell Bounds + pixel Bounds should pack into single uint64_t Parameter. Display sole author. ~20 call sites to migrate.
2. **Parameter<uint64_t>** — specialization not implemented in jam.
3. **Value::map** — discussed extensively but not used anywhere. All coordinate mapping still uses manual arithmetic.
4. **Resizer stop trigger** — currently also calls `prepare` (redundant with start trigger). Should be cleaned up — prepare in start only, stop should only exist if there's stop-specific work.

---

## Key Architecture Decisions (LOCKED by ARCHITECT)

1. **Display is sole author of viewport cell dimensions.** Scrollbar is TextEditor rendering concern only.
2. **State is SSOT.** No shadow state on Video. READER reads/writes atomics. MESSAGE reads/writes ValueTree.
3. **OSC 133 lifecycle:** A=prompt start, B=prompt end (OMP doesn't fire B), C=command output start (resets promptRow to -1, commits prompt block), D=command output end.
4. **CellFifo**: ungated pushLine for all LFs. Gate in Processor handler via State atomic promptRow.
5. **Cursor row = mutable line.** Active prompt = [promptRow..cursorRow] from Video. Everything else committed via CellFifo.
6. **TextLineArray**: generic document buffer. No terminal semantics. Processor manages add/remove.
7. **Value::map** for ALL coordinate translation. No manual arithmetic.
8. **terminal::Viewport**: packed uint64_t (cell Bounds + pixel Bounds). Single Parameter<uint64_t>. Display sole author.
9. **Processor::prepare()**: single resize method. ScopedLock(callbackLock). Includes SIGWINCH.
10. **No requestSyncResize.** Mode 2026 is synchronized output, not resize.

---

## Files Modified (both repos)

### jam
- `jam_graphics/detail/jam_text_line.h`
- `jam_graphics/detail/jam_text_line_array.h`
- `jam_graphics/fonts/font/glyph/jam_glyph_arrangement_shape.cpp`
- `jam_graphics/fonts/font/glyph/jam_glyph_arrangement.h`
- `jam_graphics/fonts/font/glyph/jam_glyph_arrangement.cpp`
- `jam_graphics/detail/jam_ParagraphStorage.h` (call site update)
- `jam_gui/text_editor/jam_text_editor.h`
- `jam_gui/text_editor/jam_text_editor.cpp`

### end
- `Source/terminal/CellFifo.h` (new)
- `Source/terminal/Processor.h`
- `Source/terminal/Processor.cpp`
- `Source/terminal/ProcessorEvents.cpp`
- `Source/terminal/Video.h`
- `Source/terminal/Video.cpp`
- `Source/terminal/VideoOSCExt.cpp`
- `Source/terminal/VideoMode.cpp`
- `Source/terminal/VideoEdit.cpp`
- `Source/terminal/State.h`
- `Source/terminal/State.cpp`
- `Source/terminal/Session.cpp`
- `Source/terminal/Identifier.h`
- `Source/terminal/Parameters.xml`
- `Source/terminal/component/Display.cpp`
- `Source/terminal/component/Screen.cpp`
- `DEBT.md`
- `PLAN-spsc-pipeline.md` (outdated)

---

## Next Session Priority

1. Fix the three critical bugs (doubling, stale promptRow, mutableRows overflow) — root cause is flush ordering + manual arithmetic
2. Implement terminal::Viewport type + Parameter<uint64_t> specialization
3. Replace all manual arithmetic with Value::map
4. Fix Screen caret positioning
5. Remove all DIAG logging
6. Audit + clean sweep
