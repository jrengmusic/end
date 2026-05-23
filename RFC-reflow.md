# RFC: Reflow — Content Projection at Width W

**Date:** 2026-05-23
**Status:** ACTIVE
**Replaces:** RFC-flexbox-reflow.md, RFC-cell-flex.md, RFC-reflow-layout-engine.md, PLAN-flexbox-reflow.md

---

## Problem

No published terminal emulator preserves content on resize. All destroy history above the active prompt. Root cause: every implementation uses scanline character-grid arithmetic — flat byte streams with no structural awareness of items vs whitespace. Downsize "removes space" by truncating or destructively wrapping. Upsize cannot recover what was lost.

END is different. The renderer is `jam::TextEditor`, a `juce::Component` with a `juce::Viewport` whose ContentView height grows. END does not have a fixed grid — it has a scrollable document. This changes the problem fundamentally.

---

## Core Insight

**Downsize does not remove space.** ContentView grows in height — more physical rows, same content, viewport scrolls. Nothing is lost, nothing is removed.

**There is no upsize vs downsize.** There is one operation: project logical lines to physical rows at width W. W changes, row count changes, content is invariant. One function, any W.

Two code paths (EXPAND/CONTRACT) with different semantics is the architectural bug that broke past attempts.

---

## Mental Model

**Items = signal.** Non-whitespace cell runs. Never altered, never split (except single items wider than W — character-level split only). The content.

**Gaps = interpolation.** Elastic whitespace (`FLEX_GAP`-tagged cells). Contracts and expands to fill available space. The spacing.

Resize is lossless for signal. Only interpolation changes.

---

## Gap Identity

When gaps contract to 1 cell, they become indistinguishable from single-space word separators. A single downsize-upsize cycle destroys layout structure — items merge, gaps are lost.

**Solution:** Cell-level `FLEX_GAP` tag (contentTag value 2). Video stamps elastic whitespace at write time — the only point where application-emitted spaces (`print(0x20)`) are distinguishable from erase-cleared cells (codepoint 0). Gap identity survives all reflow cycles regardless of width.

**Already implemented:** `jam::Cell::FLEX_GAP { 2 }` in jam_cell.h, Video look-back stamping in Video.cpp:577–589.

---

## Projection Architecture

```
source Buffer<Row> (live, read-only) → reflow() → dest Buffer<Row> (scratch) → Block(dest) → setText()
```

- **Source is never mutated.** Reflow reads source, writes dest. Pure transform.
- **Dest is scratch storage** owned by Screen. Physical rows at new width W.
- **Block is a non-owning view** over dest — what TextEditor::setText() consumes.
- **No DST.** No transition animation during iteration.
- **No buffer writeback.** Live buffer stays pristine.
- **No SIGWINCH.** Shell resize signal is a separate problem, solved after projection works.

---

## Logical Line

One or more contiguous source rows joined by `Row::flexWrap` flag.

```
Row[n].flags & Row::flexWrap != 0  →  Row[n+1] continues same logical line
Row[n].flags & Row::flexWrap == 0  →  Row[n] terminates its logical line
```

`Row::flexWrap` is the sole logical line boundary marker.

---

## Gap Distribution

Free space within a physical row is distributed across gaps proportionally using `jam::Value::map` in cumulative form:

```cpp
for (int i { 0 }; i < gapCount; ++i)
{
    gapSoFar += gap[i].currentWidth;
    const int target { Value::map (gapSoFar, 0, totalGapWidth, 0, freeSpace) };
    gap[i].width = target - distributed;
    distributed = target;
}
```

Integer arithmetic. Zero remainder loss. Battle-tested SSOT API.

---

## Cursor Stability (deferred)

Physical position is ephemeral — valid only for current W. Logical address (logical row + flat cell offset within logical line) is the stable invariant. After reflow, physical position is re-derived from logical address.

Not implemented in first iteration. Required when SIGWINCH is wired.

---

## Iteration Plan

1. **Upsize first.** Join wrapped rows, lay out items at new W, distribute free space across gaps. Verify rendering.
2. **Downsize second.** Contract gaps, wrap items that overflow to new rows. ContentView grows taller. Different failure modes, separate iteration.
3. **SIGWINCH + buffer writeback.** Wire resize signal, write projection back to live buffer, cursor stability. Separate problem after projection is robust.

---

## Constraints

- One projection function, any W. No EXPAND/CONTRACT split.
- `Cell::FLEX_GAP` is the sole gap identity mechanism.
- `Value::map` for all proportional distribution.
- `Row::flexWrap` is the sole logical line boundary.
- Reflow is a pure transform — no State, no Video, no side effects.
- C++ / JUCE. JRENG-CODING-STANDARD enforced.
- No anonymous namespaces. Static linkage for file-scope helpers.
- No early returns. Positive checks, jassert at preconditions.

---

## What Is Deliberately Absent

- CSS flexbox terminology and algorithm (cognitive overhead without payoff in integer cell domain)
- Cell flex properties beyond FLEX_GAP (flexWrap/Align/Basis per cell, CONTENT_FLEX — premature)
- flexMin/flexMax — add when projection works and constraints are needed
- DST lifecycle, transition animation
- Alternate screen reflow (apps redraw on SIGWINCH)
- Knuth-Plass line breaking (global optimization has no surface in integer cell domain with greedy packing)
