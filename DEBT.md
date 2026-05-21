# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

---

---

## DEBT-20260521T120000

**Observation:** entering tmux inside END, garbage bytes echoed to terminal: `^[[?62;4c%` and `/62;4c_`. Need to type random input until gaining full control of active prompt.
**Divergence:** entering tmux should start cleanly with no garbage byte echo — the CSI response bytes should be consumed by tmux, not displayed
**Expectation:** tmux session starts with clean prompt, no leaked escape sequence fragments visible to the user


---

## DEBT-20260501T193217

**Observation:** split third V/H from action list did not split viewport symetrically
**Divergence:** split third with shortcut is correct, why different than action triggered by Action List
**Expectation:** any split  action from any trigger should be identical

---

## DEBT-20260428T215350

**Observation:** need lua api for pane resizing
**Divergence:** resizing pane only available with mouse
**Expectation:** can assign keyboard shorcut for resizing pane vertically/horizontally

---


## DEBT-20260428T213146

**Observation:** codebase is messy with violation:
**Divergence:** constant naming with polish notation, variable names especially args with underscore, naked pointers
**Expectation:** all must adhere to NAMES.md JRENG-CODING-STANDARD.md MANIFESTO.md windows pointers must be wrapped with RAII

---

## DEBT-20260426T122105

**Observation:** when screen resolution changes, end didnt resized
**Divergence:** screen resolution (also include desktop scaling) changes, end stays at starting resolution, terminal rendered small, while window size is correct
**Expectation:** end should reactively conform to screen resolution/desktop scaling changes and adjust terminal render dims accordingly

---

## DEBT-20260426T121503

**Observation:** update nexus.lua hot reload
**Divergence:** changes on nexus.lua saved, file watcher triggered, config reloaded, no message at all
**Expectation:** changes on any lua files included in end.lua should trigger "RELOAD" message overlay

---

## DEBT-20260426T120444

**Observation:** https://developer.apple.com/documentation/AppKit/NSGlassEffectView
**Divergence:** jam style window and menu that using "modern" API fallback with NSVisualEffect should be gone
**Expectation:** mac glassmorphism pre-Tahoe must be using CGSBackgroundBlur as default, Tahoe+ use the latest API with NSGlassEffectView for all Window, Menu, and Sheet (might need to fork from kuassa lib)

---



## DEBT-20260411T100058

**Observation:** mermaid rendering is broken
**Divergence:** nothing render at all, just stuck
**Expectation:** loader overlay when mermaid still loading async, render correctly in given space

---
