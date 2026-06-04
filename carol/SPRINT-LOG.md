# SPRINT-LOG

---

## Sprint 1: Phase 1 — jam_terminal Extraction + SharedResource Redesign

**Date:** 2026-06-04
**Duration:** Full session

### Agents Participated
- COUNSELOR: Led extraction planning, PLAN authoring, identifier categorization, SharedResource redesign discussion, audit remediation. All decision gates with ARCHITECT.
- Pathfinder: Video event-firing site enumeration (33 sites, 28 distinct hooks across 7 files). Function::Map contract discovery.
- Engineer: Steps 1-9 implementation, SharedResource redesign, compile error fixes, pre-existing BLESSED violation fixes, doxygen remediation.
- Auditor: Step 1 audit (2 rounds), final sprint audit (38-item checklist, 12 findings).

### Files Modified — JAM (32 new, 11 modified)

**jam_core (2 modified)**
- `jam_core/utilities/jam_shared_resource.h` — Full redesign: split into `SharedResource` (non-template polymorphic entry base with virtual `operator==`/`hash()`) and `SharedResources<Derived>` (single-param CRTP container with `Owner<SharedResource>` storage). Methods use `const SharedResource&` params and `auto&` returns for CRTP deferred lookup.

**jam_graphics (11 modified, 3 new)**
- `jam_graphics/detail/jam_char.h` — Added 6 static methods (`fromCodepoint`, `width`, `isWordChar`, `isCombining`, `graphemeSegmentationStep`, `graphemeSegmentationInit`). Added private bit-layout constants (`BIT_WIDTH_SHIFTED`, `BIT_IS_COMBINING`, `BIT_IS_WORD_CHAR`, `BIT_GRAPHEME_SEG_PROPERTY`, `WIDTH_FIELD_BITS`, `GRAPHEME_SEG_PROPERTY_BITS`, `BOOL_FIELD_BITS`, `WIDTH_SHIFT`). Removed `GraphemeSegmentationResult` struct (moved to `Grapheme::SegmentationResult`).
- `jam_graphics/detail/jam_charset.cpp` — NEW. DEC line-drawing table + `Char::fromCodepoint` body.
- `jam_graphics/detail/jam_char_props.cpp` — NEW. charPropsT1/T2/T3 tables + `Char::charPropsFor`/`width`/`isWordChar`/`isCombining` bodies.
- `jam_graphics/detail/jam_grapheme_seg.cpp` — NEW. graphemeSegT1/T2/T3 tables + `Char::graphemeSegmentationStep` body.
- `jam_graphics/detail/jam_grapheme.h` — `Grapheme : SharedResources<Grapheme>`. Nested `Entry : SharedResource` (was top-level `GraphemeEntry`). Nested `SegmentationResult` (moved from `jam_char.h`). Added `@file` doxygen.
- `jam_graphics/detail/jam_stamp.h` — `Stamp : SharedResources<Stamp>`. Nested `Entry : SharedResource` (was top-level `StampEntry`). Widened `flags` `uint8_t`→`uint16_t`. Added `juce::Colour underline`, 3-bit underline style field, `OVERLINE`/`SUPERSCRIPT`/`SUBSCRIPT` bits. Removed old `UNDERLINE` single bit. Explicit constructor (virtual base breaks aggregate init).
- `jam_graphics/detail/jam_row.h` — `cells[]` → `chars[]` rename.
- `jam_graphics/jam_graphics.h` — Include order: `jam_grapheme.h` before `jam_char.h`.
- `jam_graphics/jam_graphics.cpp` — Aggregator: replaced `jam_char.cpp` with 3 split TUs.
- `jam_graphics/fonts/font/glyph/jam_glyph.cpp:31-32` — `0x01`/`0x02` → `Stamp::BOLD`/`Stamp::ITALIC`.
- `jam_graphics/fonts/font/glyph/jam_glyph_arrangement_shape.cpp:33-34` — Removed duplicate `sgrBold`/`sgrItalic` locals.
- `jam_graphics/fonts/font/glyph/jam_glyph_arrangement.h` — `uint8_t style` → `uint16_t style`.
- `jam_graphics/fonts/font/glyph/jam_glyph_graphics.h` — `const uint8_t* styles` → `const uint16_t* styles`.
- `jam_graphics/fonts/font/glyph/jam_glyph_graphics_cells.cpp` — `uint8_t` → `uint16_t` for style, `UNDERLINE_STYLE_MASK` test.
- `jam_graphics/fonts/typeface/jam_typeface.h` — `Typeface : SharedResource`. Added `hash()` override. `operator==` signature updated.
- `jam_graphics/fonts/typeface/jam_typeface_resources.h` — `SharedResources<TypefaceResources>`. Added `@file` doxygen.
- `jam_gui/code_editor/jam_caret_component.h:137` — Brace-init fix for widened `StampEntry`.

**jam_terminal (32 new)**
- `jam_terminal/jam_terminal.h` — Module header. Deps: `jam_core`, `jam_graphics`, `jam_data_structures`, `juce_core`, `juce_data_structures`, `juce_gui_basics`.
- `jam_terminal/jam_terminal.cpp` — Aggregator: 16 sub-TU includes.
- `jam_terminal/identifier/jam_identifier_terminal.h` — `IDENTIFIER_TERMINAL(X)` X-macro, 53 identifiers in `jam::terminal::ID::`.
- `jam_terminal/cell/jam_palette.h` — 256-slot mutable palette with `setPaletteColour`/`palette256At`.
- `jam_terminal/video/jam_screen.h` — `Screen : jam::Map::Instance<Screen>` CRTP.
- `jam_terminal/video/jam_winsize.h` — Terminal dimensions.
- `jam_terminal/video/jam_video.h` — Video base class (1743 lines). Ctor preserved: `Video(dims, events)`. 33+ fire sites renamed `id::` → `jam::terminal::ID::`.
- `jam_terminal/video/jam_video.cpp` — Core Video: constructor, flush, scroll, print, reset.
- `jam_terminal/video/jam_video_csi.cpp` — CSI dispatch + DECRQSS/DECRQM.
- `jam_terminal/video/jam_video_esc.cpp` — ESC dispatch.
- `jam_terminal/video/jam_video_sgr.cpp` — SGR dispatch. RFC-missing: underline styles/color, overline, super/subscript.
- `jam_terminal/video/jam_video_mode.cpp` — DEC mode handling.
- `jam_terminal/video/jam_video_edit.cpp` — Screen edit ops.
- `jam_terminal/video/jam_video_osc.cpp` — OSC dispatch. RFC-missing: OSC 4/10/11.
- `jam_terminal/video/jam_video_oscext.cpp` — OSC 8/133/1337.
- `jam_terminal/video/jam_video_dcs.cpp` — DCS/APC payload.
- `jam_terminal/video/jam_video_ops.cpp` — Cursor primitives, tab stops.
- `jam_terminal/parser/jam_csi.h` — CSI parameter accumulator.
- `jam_terminal/parser/jam_dispatch_table.h` — VT state machine dispatch table.
- `jam_terminal/parser/jam_parser.h` — Parser DFA.
- `jam_terminal/parser/jam_parser.cpp` — Process loop.
- `jam_terminal/parser/jam_parser_action.cpp` — Action dispatch.
- `jam_terminal/transport/jam_cell_fifo.h` — Lock-free cell transport. Bail-out guards refactored to positive nesting.
- `jam_terminal/keyboard/jam_keyboard.h` — Keyboard encoding.
- `jam_terminal/keyboard/jam_keyboard.cpp` — Windows keyboard encoder.
- `jam_terminal/tty/jam_tty.h` — TTY base class.
- `jam_terminal/tty/jam_tty.cpp` — TTY drain loop.
- `jam_terminal/tty/jam_unix_tty.h` — Unix PTY.
- `jam_terminal/tty/jam_unix_tty.cpp` — Unix PTY implementation.
- `jam_terminal/tty/jam_windows_tty.h` — Windows ConPTY. Decoupled from `lua::Engine` (ctor takes `const juce::File& conptyDir`).
- `jam_terminal/tty/jam_windows_tty.cpp` — Windows ConPTY implementation.
- `jam_terminal/protocol/jam_vt_vocabulary.h` — 166 named VT protocol constants.

**jam CMakeLists**
- `jam/CMakeLists.txt:25` — `jam_tui` → `jam_terminal`.

### Files Modified — END (4 total)
- `CMakeLists.txt:113` — `jam_tui` → `jam_terminal` in `JAM_MODULES`.
- `Source/Main.h` — `ENDApplication` with `public:` access specifier.
- `Source/Main.cpp` — `ENDApplication` fixes (semicolon, class name).
- `PLAN-jam-terminal-extraction.md` — Written and updated through session. Locked decisions, SharedResource redesign notes, `static_assert` drop.

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Extracted jam_terminal as reusable JUCE module (32 files, ~13K lines) — VT engine decoupled from END.
- Redesigned SharedResource into 2-type architecture: `SharedResource` (polymorphic entry base) + `SharedResources<Derived>` (single-param CRTP container). Entry types nest inside their owner. No top-level `GraphemeEntry`/`StampEntry`.
- Resolved C++ CRTP chicken-and-egg: method signatures use `SharedResource&`/`auto&`, bodies use deferred `Derived::Entry` lookup. `Owner<SharedResource>` polymorphic storage with virtual dispatch for hash/equality.
- Absorbed CharProps/Charset/CharPropsData into `jam::Char` static methods with TU-static lookup tables split into 3 files by table family.
- Widened `StampEntry::flags` to `uint16_t` with underline color, 3-bit underline style, overline/super/subscript.
- Consolidated 166 scattered VT protocol magic numbers into named `constexpr` constants in `jam_vt_vocabulary.h`.
- Decoupled WindowsTTY from `lua::Engine` (ctor takes `const juce::File& conptyDir`).
- Fixed 4 pre-existing BLESSED violations (magic numbers in jam_glyph.cpp, duplicate sgrBold/sgrItalic, CellFifo bail-out guards, Font::styleFlags investigation).

### Debts Paid
- None

### Debts Deferred
- None

### Known Residual (ARCHITECT-visible, not deferred)
- F7: Forward declaration `class Video;` in `jam_parser.h:78` — JRENG standard forbids forward decls; submodule zero-include rule prevents the alternative. Structural tension, PLAN-acknowledged.
- F8: `jam_screen.h:46` uses plain anonymous `enum` (Map::Instance convention) — JRENG requires `enum class`. Pre-existing pattern tension.
- F10: Multiple files exceed L (Lean) 300-line limit — data-dense/protocol-faithful files (Video.h 1743, WindowsTTY.cpp 1718, Keyboard.h 911, etc.).

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
