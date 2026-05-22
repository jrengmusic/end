# PLAN: Lossless Terminal Reflow

**RFC:** RFC-lossless-reflow.md
**Date:** 2026-05-22
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / jam framework

## Overview

Restore `jam::Row` FAM struct, migrate `Buffer<Cell>` to `Buffer<Row>`, move DST from Processor to Screen, implement lossless reflow algorithm (wrap/unwrap/pad), wire Display as the resize orchestrator via DST events. Content survives any resize cycle.

## Locked Decisions

1. **DST ownership:** Screen (end-level), not TextEditor (jam-level). Screen already holds `Buffer<Row>&` — no new dependency. Terminal reflow is END-specific; jam::TextEditor stays generic.
2. **TextEditor content type:** Keep both `Block<Cell>` and `Block<Row>` overloads. Additive. Existing Cell consumers unaffected.
3. **liveRows source:** `numRows + visibleRows` per screen. Capture all content (history + viewport). Reflow operates on history rows only; viewport rows written back as-is.

## Language / Framework Constraints

- C++17: `if constexpr` for FlexType trait dispatch, `std::void_t` for SFINAE detection
- JUCE: `juce::Timer` (DST), `juce::Viewport` (TextEditor scroll), `juce::ValueTree::Listener` (reactive state)
- jam: FAM structs require trivially-copyable ElementType constraint relaxation for Row (Row contains C99 FAM — `sizeof(Row)` is header only). Buffer's `static_assert(std::is_trivially_copyable)` must hold for Row's fixed fields.
- No early returns. Positive checks only. No magic numbers.

## Validation Gate

Each step validated against:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md
- Locked PLAN decisions

## Steps

### Step 1: jam::Row — Create FAM struct
**Scope:** `jam_fonts/cell/jam_row.h`, `jam_fonts/jam_fonts.h` (module header)
**Action:**
- Create `jam_fonts/cell/jam_row.h` with: `FlexType = Cell`, `uint16_t usedCols`, `uint8_t flags`, static constexpr `wrapped`/`dead` bit masks, `Cell cells[]` FAM
- Register in `jam_fonts.h` module header (include after jam_cell.h — Row depends on Cell)
**Validation:** Struct layout matches RFC Section 1. FAM is last member. No constructors (FAM forbids them). Bit flags are named constants.

### Step 2: Buffer FlexType stride
**Scope:** `jam_core/buffer/jam_buffer.h`
**Action:**
- Add `has_flex_type` SFINAE trait and `hasFlexType<T>` variable template (namespace-level, before Buffer class)
- In `setSize()`: `if constexpr (hasFlexType<ElementType>)` branch computes stride as `sizeof(ElementType) + alignedCols * sizeof(typename ElementType::FlexType)`, else existing computation
- All downstream APIs (`getWritePointer`, `getReadPointer`, `clear`, `copyFrom`) unchanged — they operate on `rowStrideBytes` which is now correctly computed for FAM types
- `getWritePointer` returns `ElementType*` — for `Buffer<Row>`, returns `Row*`. Caller accesses `row->cells[col]`.
**Validation:** Stride computation matches RFC Section 2. No API signature changes. `hasFlexType<Cell>` is false, `hasFlexType<Row>` is true.

### Step 3: Arrangement + TextEditor Row support
**Scope:** `jam_fonts/jam_font/glyph/jam_glyph_arrangement.h`, `jam_glyph_arrangement_shape.cpp`, `jam_gui/text_editor/jam_text_editor.h`, `jam_text_editor.cpp`, `jam_text_editor_content_view.cpp`
**Action:**
- Add `shape(const jam::Block<jam::Row>&, ...)` public overload to `glyph::Arrangement` — instantiates existing `shapeImpl` with Row-aware extractor (reads `row->cells[]`, stops at `row->usedCols`)
- Add `setText(jam::Block<jam::Row> block)` overload to `TextEditor` — stores as `Block<Row>` content member
- `ContentView::shapeVisibleContent` — add Row path (shape calls the new overload)
- TextEditor needs both `Block<Cell>` and `Block<Row>` content paths — or unify under Row-only. RFC says Row-only: "one buffer row = one display row"
**Decision surface:** TextEditor content member type — `Block<Cell>` → `Block<Row>` or dual? RFC implies Row-only.
**Validation:** shape(Block<Row>) stops at `usedCols`. setText(Block<Row>) compiles. Existing Block<Cell> path still compiles (jam is shared with other projects).

### Step 4: Session + Video type migration
**Scope:** `Source/terminal/Session.h`, `Source/terminal/Video.h`, `Source/terminal/Video.cpp`, `Source/terminal/VideoEdit.cpp`, `Source/terminal/VideoCSI.cpp`, `Source/terminal/VideoESC.cpp`
**Action:**
- Session: `jam::Buffer<jam::Cell>` → `jam::Buffer<jam::Row>`
- Video: `jam::Buffer<jam::Cell>&` → `jam::Buffer<jam::Row>&`
- Video `print()`: `buffer.getWritePointer(scr, row)[col]` → `auto* row = buffer.getWritePointer(scr, writeRow); row->cells[writeCol] = glyph; row->usedCols = max(row->usedCols, writeCol + charWidth)`
- Video `resolveWrapPending()`: add `row->flags |= jam::Row::wrapped` before cursor advance
- VideoEdit.cpp: all ED/EL/shiftLines cell access → `row->cells[]`
- VideoCSI.cpp: scroll down row copy → Row* access
- VideoESC.cpp: DECALN fill → Row* access
- `buffer.clear(scr, row)` zeroes entire Row stride — no change needed for erase-by-clear paths
- Erase paths filling with `Cell::erase()` write to `row->cells[col]` and reset `row->usedCols = 0`, `row->flags = 0`
**Validation:** All cell access paths use `row->cells[]`. `usedCols` updated in print(). `wrapped` set only in resolveWrapPending(). No direct Cell* arithmetic on buffer pointers.

### Step 5: Screen + Processor type alignment
**Scope:** `Source/terminal/component/Screen.h`, `Screen.cpp`, `Source/terminal/Processor.h`, `Processor.cpp`
**Action:**
- Screen: `jam::Buffer<jam::Cell>&` → `jam::Buffer<jam::Row>&`, constructor parameter updated
- Screen `valueTreePropertyChanged`: `Block<jam::Cell>` → `Block<jam::Row>`
- Processor.h: `DiscreteStateTransition<jam::Cell>` → `DiscreteStateTransition<jam::Row>` (temporary — will be removed in Step 7)
- Processor.cpp: DST trigger lambda — update any Cell-specific access to Row
**Validation:** Compiles with Buffer<Row>. Block<Row> constructed correctly in Screen. DST type matches Buffer.

### Step 6: DST move — Processor to Screen
**Scope:** `Source/terminal/component/Screen.h`, `Screen.cpp`, `Source/terminal/component/Display.h`, `Display.cpp`, `Source/terminal/Processor.h`, `Processor.cpp`
**Action:**
- Screen: add `jam::DiscreteStateTransition<jam::Row> transitioner` member, constructed from buffer reference (Screen already holds `Buffer<Row>&`)
- Screen: expose DST's events API (public transitioner access or forwarding methods)
- Display constructor: wire trigger handler (lambda capturing processor ref) and onStop handler on screen's transitioner
- Display `resized()`: after computing gridRect, call `screen.transitioner.set(id::resizeStart, newCols, newRows)` instead of `state.setDimensions()`
- Processor: remove `transitioner` member, `addTrigger`, `onStop`, `prepare()`, `liveRows` zeroing
- Processor `valueTreePropertyChanged`: remove cols/visibleRows dimension-change detection that called `transitioner.set()`
- Processor: remove `resizeHeight()` static function (replaced by reflow in Step 7)
- Processor: remove `id::resizeEnd` event handler (platformResize moves to Display's onStop)
**Validation:** DST owned by Screen. Display is sole resize trigger. Processor has no DST, no resize path. Unidirectional: Display → Screen DST → (trigger) → reflow → (onStop) → SIGWINCH.

### Step 7: Reflow algorithm
**Scope:** `Source/terminal/Processor.cpp` (static function), potentially new `Source/terminal/Reflow.cpp`
**Action:**
- Implement `static void reflow(jam::Buffer<jam::Row>&, terminal::State&, terminal::Video&, int scrollbackLines, cell oldCols, cell newCols, cell newRows)` per RFC Section 4
- Downsize: join wrapped rows into logical lines, re-split at newCols, mark consumed rows dead, handle wide char at wrap boundary (SPACER_HEAD)
- Upsize: join wrapped rows, write as single row if fits, re-wrap if exceeds newCols, pad full-width terminates (interior whitespace proportional distribution)
- Active viewport rows: not reflowed (PTY owns them, shell redraws after SIGWINCH)
- Alternate screen: not reflowed
- Working area: DST's `previous` buffer — captureSnapshot() before setSize() clears live buffer
- Fix `liveRows`: Display's trigger lambda must set `liveRows` to actual content row counts before `set()` call, so `captureSnapshot()` preserves content
- Wire reflow into Display's DST trigger handler (from Step 6): captureSnapshot → reflow from `previous` → buffer.setSize() → write reflowed content back → video.setDimensions/resize
**Validation:** Reflow is a pure function — same input produces same output. Wide char boundary handling correct (SPACER_HEAD). Padding algorithm: interior whitespace only, proportional distribution, remainder left-to-right. No content loss on any resize cycle.

### Step 8: Cleanup
**Scope:** `Source/terminal/Identifier.h`, `Source/terminal/Processor.cpp`, `ARCHITECTURE.md`, git status
**Action:**
- Remove stale resize identifiers if unused after DST move (id::resizeTick if dead)
- Remove GridSizeTransition ghost entries from git: `git clean` or equivalent (ARCHITECT runs git)
- Update ARCHITECTURE.md: Buffer<Row>, DST ownership, reflow, resize sequence, layer separation
- Verify all Processor `valueTreePropertyChanged` listeners are correct (preserve: outputBlockTop, promptRow, foregroundProcess, cwd, cellWidth, cellHeight)
**Validation:** No dead code. ARCHITECTURE.md matches codebase. Identifier.h has no stale entries.

## BLESSED Alignment

- **B (Bound):** Buffer<Row> owned by Session. DST owned by Screen. Row owns cells via FAM. Reflow scoped to DST trigger. No floating resources.
- **L (Lean):** Row: 3 fields + FAM. Reflow: one static function. No speculative abstractions.
- **E (Explicit):** `Row::wrapped` set in ONE place. `Row::dead` named tombstone. `Row::usedCols` named boundary. All reflow parameters in signature.
- **S (SSOT):** Buffer<Row> is THE content store. No parallel buffer. No shadow state.
- **S (Stateless):** Reflow is pure function from current state. TextEditor stateless renderer. DST transient only.
- **E (Encapsulation):** Buffer doesn't know reflow. TextEditor doesn't know Processor. Video doesn't know display. Display bridges via events.
- **D (Deterministic):** Same buffer + same dimensions = same reflow. Wrap/unwrap reversible. Padding arithmetic from cell content.

## Risks / Open Questions

None. All decisions locked.
