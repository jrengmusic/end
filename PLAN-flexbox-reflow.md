# PLAN: Flexbox Reflow Algorithm

**RFC:** RFC-flexbox-reflow.md
**Date:** 2026-05-23
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (reference implementation, no overrides)

## Overview

Rewrite the broken cell-streaming reflow algorithm with an HTML flexbox-inspired segment model. Cell-level FLEX_GAP metadata stamped at Video write time enables lossless gap identity across reflow cycles. The reflow function becomes a 4-phase pure transform: parse → collect → resolve → write.

## Language / Framework Constraints

C++ / JUCE — MANIFESTO enforced as written. No LANGUAGE.md overrides.

## Validation Gate

Each step validated against:
- MANIFESTO.md (BLESSED)
- NAMES.md (naming)
- JRENG-CODING-STANDARD.md (formatting, control flow, no early returns, brace init, `and`/`or`/`not`)
- Locked PLAN decisions (no deviation)

## Steps

### Step 1: Cell FLEX_GAP constant
**Scope:** `jam_fonts/cell/jam_cell.h`
**Action:** Add `static constexpr uint8_t FLEX_GAP { 2 };` alongside CONTENT_CODEPOINT (0) and CONTENT_GRAPHEME (1) at line ~77. No layout change — fits existing 2-bit contentTag field.
**Validation:** Constant exists, value 2, no Cell size change.

### Step 2: Row flag rename
**Scope:** `jam_fonts/cell/jam_row.h`, `Video.cpp:424`, `Screen.cpp:81,293`
**Action:** Rename `Row::wrapped` → `Row::flexWrap`, `Row::dead` → `Row::collapsed`. Update all 3 call sites in END. (`dead` has zero references — rename only in definition.)
**Validation:** No references to `Row::wrapped` or `Row::dead` remain. Grep both codebases.

### Step 3: Video FLEX_GAP stamping
**Scope:** `Source/terminal/Video.cpp` — print() function, after glyph write (~line 575)
**Action:** After writing a blank cell (codepoint 0x20) in print(), look back: if previous cell is also codepoint 0x20, stamp both as FLEX_GAP. Third+ consecutive: previous already FLEX_GAP, stamp current. One comparison per blank cell. NOT in erase paths (ECH/EL/ED write codepoint 0 via Cell::erase — confirmed).
**Validation:** Only print() path stamps FLEX_GAP. Erase paths untouched. No early returns.

### Step 4: Reflow rewrite — internal types
**Scope:** `Source/terminal/component/Screen.cpp` — file-scope static types
**Action:** Add `Segment` and `FlexLine` structs (file-scope `static`, not anonymous namespace) above reflow function. Per RFC scaffold. Stack-allocated arrays.
**Validation:** Types are file-local (`static`). No anonymous namespace. No heap allocation.

### Step 5: Reflow rewrite — Phase 1 (Parse)
**Scope:** `Screen.cpp` — static helper function
**Action:** Write parse function: walk logical line cells across flexWrap rows, classify into Segment array by FLEX_GAP tag. Leading FLEX_GAP → part of first item. Trailing cells beyond usedCols excluded. Source cells read via cursor pattern (row, col) — no flat copy.
**Validation:** Parse produces correct segments for: pure text, columnar output with gaps, prompt with left+right pinned content, indented lines.

### Step 6: Reflow rewrite — Phase 2 (Line Collection, §9.3)
**Scope:** `Screen.cpp` — static helper function
**Action:** Greedy single-pass bin-packing. Items pack into FlexLine at newCols width. Gap hypothetical = 1 cell for packing test. Item wider than newCols → character-level split (only case items break). Per RFC scaffold.
**Validation:** Collection handles: items fitting on one line, items wrapping across lines, single oversized item split.

### Step 7: Reflow rewrite — Phase 3 (Flex Resolution, §9.7)
**Scope:** `Screen.cpp` — static helper function
**Action:** Per FlexLine: items frozen (flex-shrink=0). Free space distributed across gaps proportionally using Bresenham integer distribution. Gap minimum = 1 cell. Per RFC scaffold.
**Validation:** Integer remainder distributed with zero loss. Wider gaps absorb proportionally more.

### Step 8: Reflow rewrite — Phase 4 (Write) + main function
**Scope:** `Screen.cpp` — rewrite `Screen::reflow()` body
**Action:** Delete entire current reflow body (lines 50–318). Replace with: content extent calculation, per-logical-line loop calling parse → collect → resolve → write. Write phase copies item cells from source, writes FLEX_GAP blanks at computed widths, sets usedCols and flexWrap. Alternate screen (channel 1) verbatim copy unchanged. Return newHistoryNormal.
**Validation:** Function signature unchanged. Pure transform — no State access, no Video access. Alternate screen handled. Wide char boundary handling (SPACER_HEAD/SPACER_TAIL).

### Step 9: DIAG removal
**Scope:** `Screen.cpp`, `Display.cpp`
**Action:** Remove all `// DIAG` lines. Screen.cpp: lines 68, 87–105, 232–237, 313–316, 382–387, 401–403, 411–413, 424–426. Display.cpp: lines 48–51, 250–253.
**Validation:** `grep "// DIAG"` returns zero results across END codebase.

## BLESSED Alignment

- **B (Bound):** FLEX_GAP bound to Cell — travels with it, no sidecar
- **L (Lean):** 4 phases, each a static helper under 30 lines. Segment/FlexLine are minimal POD
- **E (Explicit):** Cell tag makes gap classification explicit. Flexbox terminology in names. No early returns
- **S (SSOT):** Video sole stamping authority. Cell tag sole source of gap identity
- **S (Stateless):** Reflow is pure transform. No side effects, no State
- **E (Encapsulation):** Video stamps (writer). Parse classifies (reader). Reflow layouts (transform)
- **D (Deterministic):** Same input → same output. Integer arithmetic, Bresenham, no floats

## Risks / Open Questions

None. All design decisions resolved in RFC discussion.
