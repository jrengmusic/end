# PLAN: Viewport Redesign

**RFC:** RFC-viewport-redesign.md
**Date:** 2026-05-09
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE
**Supersedes:** Previous PLAN-viewport-redesign.md (framebuffer model — incorrect)

## Overview

Decouple UI (spatial) from data (protocol). Screen is a growing document with spatial ownership. Parser is a stateless byte decoder. Commands are semantic (no coordinates). State is passive SSOT. Mutation is unidirectional.

## Topology

```
PTY → Parser (stateless decoder) → Grid FIFO → Display (drain) → Screen (spatial owner, append)
         ↕ atomic r/w                            ↕ tell              ↓ writes resolved cursor
                       State (SSOT: dims, modes, cursor, SGR)
                         ↑
                    Display::resized() writes dims
```

- **Parser**: bytes → Commands. Reads/writes State atomics (raw cursor from VT sequences, SGR). No spatial knowledge. No viewport dimensions. No wrap. No scroll.
- **State**: passive SSOT. Dimensions (from GUI). Cursor position (raw from Parser, resolved from Screen). Modes, SGR.
- **Display**: orchestrator. Drains FIFO → tells Screen. Writes dimensions to State on resize. Reads cursor visibility/shape from State → tells Screen.
- **Screen**: content + spatial owner. Appends cells (growing document). Wraps at cols. Tracks write position. Handles erase/insert/delete spatially. Reads raw cursor from State, resolves (clamp, wrap), writes resolved position back. Viewport auto-scrolls to bottom.

## Content Model

**Normal screen (index 0):** Growing document. LineFeed = append new row. Content accumulates. Viewport scrolls to show latest. Scrollbar gives history. Resize changes viewport window, not content. Scrollback trimmed at capacity (oldest rows removed).

**Alternate screen (index 1):** Fixed framebuffer (`visibleRows × cols`). LineFeed = memmove scroll up. Used by full-screen apps (vim, htop). Resize reallocates.

**TextEditor IS the substrate.** Cells grows like a text document. TextEditor's Viewport handles scrolling. No hacks, no workarounds. The rendered image IS the visual storage.

## Command Model

Commands are semantic. No row/col. No spatial payload.

```cpp
struct Command
{
    enum class Type : uint8_t
    {
        Print,            ///< Append cell at Screen's write position.
        LineFeed,         ///< New row (normal: append, alternate: scroll).
        CarriageReturn,   ///< Write position to start of current row.
        Tab,              ///< Advance to next tab stop.
        Backspace,        ///< Move write position back one.
        EraseInLine,      ///< param: 0=cursor-to-end, 1=start-to-cursor, 2=whole line.
        EraseInDisplay,   ///< param: 0=cursor-to-end, 1=start-to-cursor, 2=whole, 3=scrollback.
        InsertLines,      ///< param: count. At current row.
        DeleteLines,      ///< param: count. At current row.
        InsertChars,      ///< param: count. At current position.
        DeleteChars,      ///< param: count. At current position.
        EraseChars,       ///< param: count. At current position.
        SetScreen,        ///< param: 0=normal, 1=alternate.
        ClearScrollback,
        SetTabStop,
        ClearTabStop,
        ClearAllTabStops,
    };

    Type type { Type::Print };
    jam::Cell cell {};           ///< Cell data (Print only).
    juce::Colour fillBg {};      ///< Fill colour for erase operations.
    int param { 0 };             ///< Count or mode parameter.
};
```

`row` and `col` removed. Screen resolves all positions from its own cursor state.

## Cursor Authority

- **Parser** writes raw cursor values to State from VT sequences (CSI H, CSI A/B/C/D). Arithmetic only — no clamping, no spatial validation.
- **Screen** reads raw cursor from State, resolves spatially (clamp to bounds, wrap), applies to write position, writes resolved position back to State.
- **Display** reads resolved cursor from State → tells Screen caret position for rendering.
- **CPR** (cursor position report): Parser reads resolved cursor from State, responds to PTY.

## Resolved Decisions

| Q | Decision |
|---|----------|
| Q1 Alt screen | `jam::Owner<Cells>` on TextEditor. Screen adds normal (0) and alternate (1). |
| Q2 Content | Normal = growing document (append). Alternate = fixed framebuffer. |
| Q3 Spatial | Screen owns all spatial logic. Parser is coordinate-free. |
| Q4 Cursor | Parser writes raw, Screen resolves. State is passive. |
| Q5 Dimensions | Display writes to State on resize. PTY reads for TIOCGWINSZ. |
| Q6 Wrap | Screen handles. Parser does not know cols. |
| Q7 Scroll | Normal: append (no destruction). Alternate: memmove. |

## Architecture Constraints (ARCHITECT-stated)

- Fully event-driven — Display listens, orchestrates
- Always tell, never ask — no poking internals
- No manual boolean, no manual lambda, no manual state tracking, no shadow state
- No naked non-owning pointers — `jam::Owner` for storage ownership
- Parser has ZERO spatial knowledge — no viewport dimensions, no wrap, no scroll
- Lock-free — no mutex, no CriticalSection
- Resize never mutates content (normal screen) — viewport window changes
- State is passive SSOT — dimensions, modes, cursor
- Mutation is unidirectional — UI is UI, data is data

## Validation Gate

Each step validated by @Auditor against MANIFESTO.md (BLESSED), NAMES.md, JRENG-CODING-STANDARD.md, and locked PLAN decisions.

## Completed Steps

### Step 1: Grid → FIFO ✓
Grid rewritten as AbstractFifo<Command>. Parser pushes Commands.

### Step R1: jam::TextEditor — Owner<Cells> ✓
`cellsContent` replaced with `jam::Owner<Cells> cells` (protected). `setActiveCells(int)`. `reshapeCellsContent()` protected.

### Step R2: jam::CaretComponent ✓
DECSCUSR shapes. Full cell-width bounds. LookAndFeel wired.

## Remaining Steps

### Step A1: Command — strip coordinates

**Scope:** Command.h

**Action:**
- Remove `int row` and `int col` fields from Command struct.
- Keep: type, cell, fillBg, param.
- Update `static_assert` for trivially_copyable.

**Validation:** Compiles with updated Parser emission sites (Step A2).

### Step A2: Parser — coordinate-free emission

**Scope:** ParserVT.cpp, ParserEdit.cpp, ParserCSI.cpp, ParserESC.cpp

**Action:**
- Remove all `cmd.row = ...` and `cmd.col = ...` assignments at every `grid.push()` call.
- Parser still writes raw cursor to State atomics from VT cursor sequences (CSI H, cursor moves).
- Parser still reads cursor from State for relative moves (arithmetic, no clamping).
- Parser stops computing spatial layout (no wrap detection, no scroll trigger).
- Keep: decoder FSM, CSI accumulator, UTF-8, OSC/DCS/APC buffers, Pen (SGR), SavedCursor, grapheme state, charset state.

**Validation:** Compiles. Commands carry no coordinates.

### Step A3: Screen — append model (normal screen)

**Scope:** Screen.h, Screen.cpp

**Action:**
- Screen tracks its own write position (row, col within the growing document).
- `makeLayout` processes Commands spatially:
  - Print: write cell at write position, advance. Wrap at cols if needed (append new row on wrap).
  - LineFeed: append new row to Cells. Write position to col 0 of new row.
  - CarriageReturn: write position to col 0 of current row.
  - Tab: advance write position to next tab stop.
  - Backspace: move write position back one.
  - EraseInLine: clear cells from/to write position within current row. Screen knows bounds.
  - EraseInDisplay: clear cells. Screen knows total content.
  - InsertLines/DeleteLines: shift rows within visible area.
  - InsertChars/DeleteChars/EraseChars: shift/clear cells within current row.
- LineFeed = `cells.resize(totalCells + cols)` — append, not memmove.
- Viewport auto-scrolls to bottom after content change.
- Screen reads raw cursor from State. Resolves (clamp to document bounds). Writes resolved position back to State.
- Scrollback: Cells grows up to capacity. Oldest rows trimmed (memmove front, infrequent).
- Remove '\n' marker hack — ShapedText wraps at `cols` naturally.

**Validation:** Terminal renders. Content grows. Scroll works. Resize preserves content.

### Step A4: Screen — alternate screen

**Scope:** Screen.cpp (alternate buffer handling within makeLayout)

**Action:**
- Alternate Cells is fixed-size (`visibleRows × cols`).
- LineFeed on alternate = memmove scroll (existing behavior for framebuffer).
- Print/Erase/Insert/Delete use cursor position within fixed buffer.
- SetScreen command switches active screen. Display calls `screen.setActive()`.

**Validation:** vim/htop render correctly. Screen switch works.

### Step A5: Display — dimensions to State

**Scope:** TerminalDisplay.cpp, State.h/State.cpp (if setter needed)

**Action:**
- `Display::resized()` writes grid dimensions (cols, rows) to State.
- Remove `Processor::resized()` as dimension conduit if dimensions go directly to State.
- PTY layer reads dimensions from State for TIOCGWINSZ.
- Display reads cursor visibility/shape from State → tells Screen (caret rendering).

**Validation:** SIGWINCH fires on resize. Programs see correct dimensions.

### Step A6: Cleanup

- Remove dead spatial code from Parser (wrap helpers, scroll helpers, row mapping remnants).
- Delete completed PLAN files.
- Update ARCHITECTURE.md.

## BLESSED Alignment

- **B:** TextEditor owns Cells via Owner. Grid FIFO owns HeapBlock. No cross-thread calls. Lifecycle clear.
- **L:** Command = 4 fields. Screen append = simple. No row ring, no intermediate buffers.
- **E:** Command is semantic — no hidden spatial context. Parser writes what VT says. Screen resolves.
- **S(SSOT):** State = SSOT for cursor, modes, dimensions. Cells = SSOT for content. No duplication.
- **S(Stateless):** Parser is stateless decoder (FSM + SGR are calculation inputs). Grid has no memory after drain.
- **E(Encap):** Parser → Grid → Display → Screen. Unidirectional. Each layer ignorant of others. Screen owns spatial.
- **D:** Same bytes → same Commands → same appended Cells → same rendered image.

## Risks

1. **Alternate screen** — framebuffer model within the append architecture. Needs clear separation in Screen.
2. **Parser spatial shed** — large refactor across Parser*.cpp files. Must not break VT compliance.
3. **Cursor resolution** — Screen reads raw cursor from State, resolves, writes back. Timing with Parser writes needs care (atomic read-modify-write on State).
4. **Scrollback trimming** — memmove front of Cells. Acceptable for infrequent operation at capacity limit.
