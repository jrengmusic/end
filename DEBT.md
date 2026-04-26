# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash. No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt. Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt is written). Survives `carol reset` — debts persist across protocol resets.

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
