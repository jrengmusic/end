# PLAN: Fix Zoom

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-04-12
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE (no overrides — reference implementation)

## Overview

Fix zoom determinism: zoom in + zoom out must produce identical terminal state. Root cause is `Grid::reflow()` truncating hard lines on shrink, compounded by 4 SSOT violations in the zoom path and a missing link rescan.

## Language / Framework Constraints

C++ / JUCE — reference implementation. All MANIFESTO principles enforced as written.

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: Fix reflow truncation (D violation)

**Scope:** `Source/terminal/logic/GridReflow.cpp`
**Action:** Remove the `(runLen == 1)` special case that caps `effectiveLen` at `newCols` for hard lines. Both `countOutputRows()` (line 575) and `writeReflowedContent()` (line 619) have the same truncation. Change both to use `flatLen` unconditionally — hard lines wrap at the new width (marked as soft-wrapped), unwrap when cols increase. Round-trip determinism restored.
**Validation:** @Auditor verifies: (1) both sites changed identically, (2) no other `effectiveLen` truncation exists, (3) BLESSED D: zoom in + zoom out produces same grid state.

### Step 2: Extract zoom step constant (S/SSOT violation)

**Scope:** `Source/config/Config.h`
**Action:** Add `static constexpr float zoomStep { 0.25f };` next to existing `zoomMin` / `zoomMax` in Config. Six hardcoded `0.25f` sites will reference this constant.
**Validation:** @Auditor verifies: (1) constant exists in Config, (2) zero remaining hardcoded `0.25f` in zoom code paths, (3) NAMES.md compliance for the constant name.

### Step 3: Remove dead Display zoom triad (S/SSOT + L violation)

**Scope:** `Source/component/TerminalDisplay.h`, `Source/component/TerminalDisplay.cpp`
**Action:** Delete `Display::increaseZoom()`, `Display::decreaseZoom()`, `Display::resetZoom()` — declarations and implementations. These are dead code; all callers route through `Tabs`. `Display::applyZoom(float)` stays (called by `Tabs`).
**Validation:** @Auditor verifies: (1) no remaining callers of removed methods, (2) `applyZoom(float)` still exists and is called by Tabs, (3) grep confirms zero references to removed methods.

### Step 4: Consolidate Tabs zoom to use Config constants (S/SSOT violation)

**Scope:** `Source/component/Tabs.cpp`
**Action:** Replace hardcoded `0.25f` with `Config::zoomStep` in `Tabs::increaseZoom()` and `Tabs::decreaseZoom()`. Fix `resetZoom()` to use the configured `window.zoom` default from `Config::Key::windowZoom` instead of `Config::zoomMin`.
**Validation:** @Auditor verifies: (1) `Config::zoomStep` used in both methods, (2) `resetZoom` resets to configured default, (3) `NativeScaleFactorNotifier` in MainComponent.h calls `tabs->resetZoom()` — now correctly resets to configured default.

### Step 5: Trigger link rescan after zoom

**Scope:** `Source/component/TerminalDisplay.cpp`
**Action:** In `Display::applyZoom()`, after `resized()`, trigger a `LinkManager` rescan. The existing `LinkManager::scan()` requires a `cwd` and `outputRowsOnly` flag. Use the same pattern as the ValueTree listener path — read cwd from processor state, call `linkManager->scan(cwd, false)` to refresh `clickableLinks` for the new grid geometry.
**Validation:** @Auditor verifies: (1) `applyZoom` calls `linkManager->scan()` after `resized()`, (2) pattern matches existing scan call sites, (3) no stale link positions after zoom.

## BLESSED Alignment

- **B (Bound):** No ownership changes. All objects keep existing owners.
- **L (Lean):** Dead code removed (3 methods). Named constant replaces 6 hardcoded sites.
- **E (Explicit):** `Config::zoomStep` makes the step value visible and named. `resetZoom` uses the configured default, not a magic constant.
- **S (SSOT):** Single zoom path (Tabs → Display::applyZoom). Single constant for step. Single source for reset default.
- **S (Stateless):** No new state introduced.
- **E (Encapsulation):** Layer flow unchanged. Config owns constants, Tabs owns zoom orchestration, Display owns rendering.
- **D (Deterministic):** Reflow preserves content on shrink (wrap) and grow (unwrap). Zoom round-trip is identity.

## Risks / Open Questions

- **Reflow behavior change (Step 1):** Wrapping hard lines on resize changes visible behavior — `ls` output that previously truncated will now wrap to multiple rows when the terminal shrinks. This matches kitty's behavior and is correct per BLESSED D, but is a deliberate semantic change. ARCHITECT has approved this direction.
- **Link rescan performance (Step 5):** `scan()` is O(rows * cols). At zoom changes this runs once synchronously on the message thread. Should be negligible but worth noting.
