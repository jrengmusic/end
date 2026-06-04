# SPRINT-LOG

---

## Sprint 0: END Rewrite — SPEC + ARCHITECTURE

**Date:** 2026-06-04
**Duration:** Full session

### Agents Participated
- COUNSELOR: Led SPEC writing through dialogue with ARCHITECT. Read all RFCs, old ARCHITECTURE.md, old DEBT.md, PLAN-WHELMED.md, jam source (PaneManager, Typeface, glyph pipeline, Markdown parser, SpectrumProcessor, TETRIS.md, identifier system, button::Group). Verified CONTRACT alignment.
- Pathfinder: Initial codebase survey (old END — 155 files, 46K LOC, 7 subsystems, 6 debts, last 20 commits all refactoring).
- Librarian: JUCE focus system research (getCurrentlyFocusedComponent, hasKeyboardFocus, focusOfChildComponentChanged, FocusChangeListener, focus on hide/remove/overlay — all from JUCE source with line citations).

### Files Modified (2 total)
- `SPEC.md` — Complete rewrite specification v0.0.1. 15 phases, full architecture, META-MVC, APVTS-analog, anti-mental-model, coordinate spaces, performance targets, incremental Model tracking per phase.
- `ARCHITECTURE.md` — Architectural contracts and mental model. Pre-implementation — no file details, contracts only.

### Alignment Check
- [x] BLESSED principles followed (each section references specific BLESSED pillars)
- [x] NAMES.md adhered (all new names discussed and approved: end::Model/View, config::Model, terminal::Controller/Model/View/Processor, PaneView, CodeView::Selection)
- [x] MANIFESTO.md principles applied (lock-free, unidirectional, SSOT, no shadow state, TETRIS contract for CodeView)

### Decisions Locked
1. **Priority order:** JUCE GUI app first, VT emulator second, niceties third.
2. **APVTS analog:** Spectrum analyzer pattern — reader pushes, message thread paints.
3. **META-MVC:** Recursive MVC layers, not three god objects.
4. **Plugin mapping:** Nexus=Host, Controller=AudioProcessor, Model=APVTS, View=PluginEditor, CellFifo=SpectrumFIFO, CodeModel=outputDB.
5. **Two independent trees:** config::Model (config constants) + end::Model (runtime state). Never mixed.
6. **Config SSOT:** Lua files on disk. config::Model is derived state. Init and reload are the same code path. No referTo.
7. **CodeView TETRIS contract:** Dumb widget, cell-space API, NOT jam::ValueTree::Component. Selection TYPE on TABS, selection COORDS transient in CodeView.
8. **Three coordinate spaces:** Video-grid, Document, Screen/pixel. jam::Cell::Point::fromPixel/toPixel is the ONLY converter. Manual arithmetic forbidden.
9. **Keyboard centralized at end::View** (KeyListener), mouse per-PaneView (JUCE delivery).
10. **activePaneID on TABS** authored by end::View (FocusChangeListener). Async delivery guarded.
11. **DisplayCallbacks eliminated.** Lua actions dispatch through action::Registry. Parameterized dispatch required.
12. **TTY moved to jam_terminal.** Constructor takes config path, no app coupling.
13. **Identifiers:** IDENTIFIER_TERMINAL X-macro in jam_terminal, expanded into jam::ID. Video event keys eliminated (virtual hooks replace string-keyed dispatch). END-specific identifiers in AppIdentifier.h.
14. **PaneManager resizer bar fix:** RAII-bound lifetime in Phase 2 (jam_gui).
15. **Namespace structure:** end:: (app), config:: (config), terminal:: (terminal), whelmed:: (markdown). Main.cpp not namespaced.
16. **Nexus ownership:** Nexus owns Controllers. Controllers survive View destruction (daemon mode). Minimal working Nexus in Phase 3.
17. **action::Registry functional in Phase 3** with prefix key state machine and keys.lua parsing.
18. **Whelmed two-pass pipeline:** jam::Markdown::Parser (proven, unchanged) → ParsedDocument IR → style resolution → jam::String with PROPORTIONAL Char → CodeModel → CodeView (edit) / TextView (read).
19. **Font/atlas GL-thread binding:** UNRESOLVED open seam. Must be designed before Phase 4.
20. **Anti-mental-model:** Explicit negation of terminal scanline model. Buffer<Row> is scratch, NOT document. CodeModel is SSOT. Width enters once at projection. Reader NEVER touches CodeModel.

### Problems Solved
- Identified root cause of old END's architectural rot: terminal-first mental model fighting JUCE.
- Identified font/atlas GL-thread race condition (use-after-free on reload) as principal blocker of old END.
- Identified PaneManager resizer bar lifecycle bug (RAII violation in remove()).
- Identified DisplayCallbacks as layer violation (config parser holding UI closures).
- Identified CodeView as jam::ValueTree::Component as layer violation (generic widget coupled to END state schema).

### Debts Paid
- None (Sprint 0 — specification only, no code)

### Debts Deferred
- None

