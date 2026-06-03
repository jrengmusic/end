# RFC — jam_terminal Module Extraction

Date: 2026-06-02
Status: Ready for COUNSELOR handoff

---

## Problem Statement

END (`~/Documents/Poems/dev/end/`) contains a production VT100/VT520 terminal
emulator engine under `Source/terminal/`. The same conceptual surface —
terminal cell grids, ANSI/VT protocol, cell rendering — also exists as scaffold
in two JAM locations:

- `jam_tui/` — a JUCE module (`jam::tui` namespace) providing **write-side** TUI
  rendering (emit ANSI to stdout): Writer, Graphics, Screen, Component, widgets.
- `jam_graphics/detail/` — the **foundation** types already shared by both
  sides: `jam::Char`, `jam::Cell`, `jam::Row`, `jam::Stamp`, `jam::Grapheme`.

The protocol and cell-handling knowledge is currently expressed up to **three
times** from different directions:

1. END `Source/terminal/` — VT **parse** side (`DispatchTable`, `Video`,
   `Identifier`/`Map`).
2. `jam_tui/ansi/jam_tui_escapes.h` — ANSI **write** constants.
3. END `Source/terminal/Identifier.h` + `Source/Map.h` — mode/screen vocabulary.

This violates the SSOT principle ARCHITECT stated for this objective: **write
once, use everywhere; anything TUI/terminal-related is SSOT.**

**Objective:** Extract END's production engine into a single new library module
`jam_terminal`, absorbing and **deleting** the `jam_tui` scaffold. END becomes a
consumer of its own extracted engine. The downstream TUI applications (TIT,
CAKE) are not properly started and will be rewritten against the new
structures — no backward-compatibility obligation. WHATDBG is a CLI DAP adapter,
not a TUI consumer, and is out of scope.

---

## Research Summary

All claims below are grounded in files read during this session. Citations are
`path:line`.

### A. END read-side engine inventory (`Source/terminal/`)

| Class / file | Role | External coupling |
|---|---|---|
| `Parser` (`Parser.h:94`) | VT100/VT520 DFA byte decoder. Holds `DispatchTable`, `CSI`, UTF-8 + OSC/DCS/APC accumulators. Dispatches decoded actions by calling `Video` methods directly. Holds `Video& video` (`Parser.h:150`). `process(data, length)` is reader-thread hot path (`Parser.h:139`). | None on END app. References only `Video`. |
| `DispatchTable` (`DispatchTable.h:140`) | O(1) `(ParserState, byte) → (nextState, ParserAction)` table. `ParserState` enum (`DispatchTable.h:51`), `ParserAction` enum (`DispatchTable.h:80`), `Transition` struct, 2 bytes, trivially copyable (`DispatchTable.h:107-114`). Immutable after construction. | None. |
| `CSI` (`CSI.h:81`) | Trivially-copyable CSI parameter accumulator. `static_assert(std::is_trivially_copyable_v<CSI>)` (`CSI.h:384`). MAX_PARAMS 24. | None. |
| `Video` (`Video.h:88`) | VT command processor. Owns `jam::Buffer<jam::Row> grid` (`Video.h:333`), 2-channel (normal+alternate) via `std::array<jam::Block<jam::Row>,2> blocks` (`Video.h:342`). Holds pen, cursor, modes, scroll region, tab stops, saved cursor, grapheme state. Constructed with `(jam::Cell::Rectangle dims, jam::Function::Map<juce::Identifier,void>& events)` (`Video.h:116-117`). Fires events through the events-map reference (`Video.h:363`). | **Events map keyed by `juce::Identifier`** — the single app-coupling point. |
| `CharProps` (`CharProps.h:65`) | Packed 32-bit Unicode property word. `charPropsFor(cp)` 3-level multistage lookup (`CharProps.h:316`). `width()` (`CharProps.h:87`). Grapheme segmentation: `graphemeSegmentationStep(state, props)` (`CharProps.h:416`), `graphemeSegmentationInit()` (`CharProps.h:451`), `GraphemeSegmentationResult` (`CharProps.h:360`). `static_assert(sizeof(CharProps)==4)` (`CharProps.h:277`). | None — read-only tables, any thread. |
| `CharPropsData` (`CharPropsData.h`) | Generated multistage lookup tables backing `CharProps`. | None. |
| `Charset` (`Charset.h`) | `DEC_LINE_DRAWING` constexpr table (`Charset.h:49`) + `translateCharset(codepoint, lineDrawing)` (`Charset.h:77`). Maps 0x60–0x7E to box-drawing glyphs when line-drawing active. | None. |
| `Palette` (`Palette.h`) | xterm-256 colour resolution. `ANSI_16` (`Palette.h:38`, mutable for hot-reload via `setAnsi16Colour` `Palette.h:67`), `COLOR_CUBE` (216), `GRAY_RAMP` (24), `palette256At(index)` (`Palette.h:230`). Uses `juce::Colour`. | None. |
| `CursorState` (`Identifier.h:34`) | Packed int32 cursor: row(12)+col(12)+visible(1)+kbFlags(5). `pack()`/`unpack()` (`Identifier.h:41-49`). | None. |
| `Winsize` (`Winsize.h:29`) | Packed int64 terminal dims: cols/rows/pixelWidth/pixelHeight, 16 bits each. `pack()`/`unpack()` (`Winsize.h:37-52`), `toCellRect()` (`Winsize.h:58`). | None. |
| `CellFifo` (`CellFifo.h:72`) | Two independent lock-free SPSC rings (history + active), each a `jam::BufferSPSC` (`CellFifo.h:573-574`). Seqlock per header slot, drop-oldest. Produces `jam::CodeLine` on drain. `pushHistory`/`pushActive` (reader), `drainHistory`/`drainActive` (message). | Only `jam::BufferSPSC`, `jam::Char`, `jam::CodeLine`. |
| `TextBuffer` / `TextSlot` (`TextBuffer.h:35`/`:9`) | Double-buffered cross-thread string slots, atomic index swap (`TextBuffer.h:17-27`). | None. |
| `Identifier.h` `id::` namespace (`Identifier.h:73`) | `juce::Identifier` ValueTree property keys + event-map keys. Mix of VT-universal and END-specific keys. | END-specific (ValueTree concern). |
| `Processor` (`Processor.h:108`) | Pipeline orchestrator/Controller. Owns `Parser` (`Processor.h:395`), `Video` (`Processor.h:375`), `CellFifo` (`Processor.h:346`), `Skit`, holds `Model&`. Owns the `events` map (`Processor.h:367`). `registerEvents()` (`Processor.h:415`) registers reader-thread handlers that translate Video-fired events into Model atomics / CellFifo pushes. Owns TTY. | END-specific. |
| `Session`, `Display`, `Model`, `Skit`, `Sixel/Kitty/ITerm2Decoder`, `LinkManager/LinkDetector/LinkSpan`, `action::*`, `tty::*`, `Input/Keyboard/Mouse`, `component/*` | END application layer. | END-specific. |

### B. jam_tui write-side inventory

Module declaration (`jam_tui/jam_tui.h:1-11`): namespace `jam::tui`, dependencies
`jam_core, jam_graphics, jam_markdown, juce_*`.

**Engine core (`ansi/`, `metrics/`, `input/`, `graphics/`):**

| Class / file | Role |
|---|---|
| `Writer` (`jam_tui_writer.h:16`) | ANSI escape accumulator → single stdout flush per frame. `beginFrame()`/`endFrame()` (`:19`/`:23`), `setFg`/`setBg` true-colour (`:43`/`:47`), cursor/clear/attr emission. |
| `Graphics` (`jam_tui_graphics.h:31`) | Cell framebuffer. Owns `juce::HeapBlock<jam::Char> cells` (`:78`) **and a parallel `juce::HeapBlock<jam::Stamp::Entry> styles` sidecar** (`:79`). Pen is a `jam::Stamp::Entry` (`:86`). `clip()` returns child borrowing parent grid (`:53`). `getLines()` serializes to ANSI per row (`:59`). |
| `Screen` (`jam_tui_screen.h:73`) | Root TUI component (`: public Component`). Owns render loop, 3-strategy diff (first/full/differential), `Writer&` (`:115`), overlays with RAII `OverlayHandle`. |
| `Component` (`jam_tui_component.h:25`) | `: public juce::Component`. `paint(Graphics&)` pure virtual (`:29`), cell-bounds helpers. Doc names "CAROLINE TUI components" (`:10`). |
| `escapes` (`jam_tui_escapes.h`) | `namespace ANSI` (global, `:3`). Write constants. `CURSOR_MARKER = "\x1b_caroline:cursor\x07"` (`:26`). |
| `Metrics` (`jam_tui_metrics.h:34`) | Platform terminal-size query `getBounds()` via ioctl/`GetConsoleScreenBufferInfo` (`:52`). Doc references broken path `s/cell/...` (`:8`,`:16`). |
| `Input`, `KeyPress` (`input/`) | Raw stdin handling. |
| `Point` (`graphics/jam_tui_point.h`) | `Cell::Point` alias. |

**Widget layer (`component/`, `braille/`, `lookandfeel/`, `markdown/`):**
Label, Menu, ListPane, SplitPane, Dialog, Console, TextPane (+diff/+paint),
Spinner, ThemeResolver, Braille grid, LookAndFeel, MarkdownRenderer (pulls
`jam_markdown`).

### C. jam foundation (stays in jam_graphics / jam_core)

| Type | Definition | Key facts |
|---|---|---|
| `jam::Char` (`jam_graphics/detail/jam_char.h:63`) | 8-byte packed u64. Fields: codepoint(21) + contentTag(2) + wide(2) + styleId(16) + padding(23). `static_assert(sizeof(Char)==8)` and trivially-copyable (`:177-178`). `styleId()` is index into `jam::Stamp` (`:134-138`). `make(cp,tag,wide,sid)` (`:152`), `erase(sid)` (`:171`). contentTag `CONTENT_GRAPHEME` (`:72`) → codepoint field indexes `jam::Grapheme`. wide hints NARROW/WIDE/SPACER_TAIL/PROPORTIONAL (`:81-90`). |
| `jam::Row` (`jam_graphics/detail/jam_row.h:35`) | FAM row. `usedCols` (`:39`), `flags` (`:40`) with flexWrap/collapsed/justify (`:42-44`), `FlexType = Char` (`:37`), `Char cells[]` FAM (`:46`). Trivially copyable. Width-free storage. |
| `jam::Stamp` (`jam_graphics/detail/jam_stamp.h:38`) | `: SharedResource<Stamp, StampEntry>`. `StampEntry = {juce::Colour fg, juce::Colour bg, uint8_t flags}` with `Hash` (`:6-36`). Style flags BOLD..DIM (`:42-48`). |
| `jam::Grapheme` (`jam_graphics/detail/jam_grapheme.h:30`) | `: SharedResource<Grapheme, GraphemeEntry>`. `GraphemeEntry = {std::array<char32_t,8> codepoints, uint8_t count}` with `Hash` (`:6-28`). |
| `jam::SharedResource<Derived,Entry>` (`jam_core/utilities/jam_shared_resource.h:20`) | `: Context<Derived>` (`:21`). Interning table: `addIfNotAlreadyThere(entry) → int index` dedups via `find` (`:27-35`), `get(index) → Entry&` (`:42-50`). Owns `Owner<Entry> entries` (`:58`). |
| `jam::Context<T>` (`jam_core/context/jam_context.h:42`) | CRTP global-access contract. STANDALONE branch: single global pointer (`:47-86`). PLUGIN branch: thread-local LIFO chain (`:87-127`). `getContext()` returns active instance. Copy/move deleted. |
| `jam::Buffer<T>` / `jam::Block<T>` | Multi-channel SIMD-aligned 2D storage; detects FAM via `FlexType`. `Block<Row>::getWritePointer(row,col)` returns `Row*` (used `Video.cpp:415`). |
| `jam::CodeLine` | Logical line: `chars` (`HeapBlock<jam::Char>`), `cellCount`, `isContinued`, `isJustified` (per `CellFifo.h` drain usage `:556-564`). |

### D. Ownership model (grounded, application-owned Context singletons)

END's `MainApplication` owns the SharedResource tables as members:
`jam::Stamp stampContext;` (`Source/Main.h:118`) and
`jam::Grapheme graphemeContext;` (`Source/Main.h:121`). They self-register as
`Context` on construction. Any code reaches them via `jam::Stamp::getContext()`
/ `jam::Grapheme::getContext()`. `Video` interns the pen via
`jam::Stamp::getContext()->addIfNotAlreadyThere({penFg,penBg,penFlags})`
(`Video.cpp:491`) and packs the returned styleId into the `jam::Char`.

### E. The Video event-firing usage (the coupling to resolve)

`Video` fires events through `jam::Function::Map<juce::Identifier,void>& events`
(`Video.h:363`), keyed by END's `id::` identifiers (`Identifier.h:267-376`).
`Processor::registerEvents()` (`Processor.h:415`) installs reader-thread handlers
that need `Model` access Video does not hold: link-ID assignment, OSC 133
screen-relative→absolute row conversion, pushHistory/pushActive into CellFifo,
mode-flag flushes. Event keys enumerated at `Processor.h:353-366` and
`Identifier.h:270-376`.

### F. The `print()` cell-construction path (the absorption target)

`Video::print(codepoint)` (`Video.cpp:405`) performs, per codepoint:
1. `charPropsFor(codepoint)` (`Video.cpp:408`) — Unicode props.
2. `graphemeSegmentationStep(graphemeState, props)` (`Video.cpp:409`) — running
   cluster state machine; `graphemeState` is a `Video` member.
3. On cluster extension: interns into `jam::Grapheme::getContext()`
   (`Video.cpp:436`), repacks base cell as `CONTENT_GRAPHEME`.
4. Else: `props.width()` → wide hint; `translateCharset(codepoint, useLineDrawing)`
   (`Video.cpp:483`); intern style into `jam::Stamp` (`Video.cpp:491`);
   `jam::Char::make(cp, CONTENT_CODEPOINT, wideHint, sid)` (`Video.cpp:498`).

`translateCharset`, `charPropsFor`, and `graphemeSegmentationStep` are free
functions external to `jam::Char`.

---

## Principles and Rationale

Every decision below was locked by ARCHITECT during this session. Rationale and
BLESSED pillar mapping are recorded. "Considered and rejected" entries note
alternatives ARCHITECT declined.

### P1. Single module `jam_terminal`, namespace `jam::terminal::`

One module holds both sides. Subdivision:

```
jam_terminal/
  cell/        Palette
  parser/      Parser, DispatchTable, CSI
  video/       Video (base), CursorState, Winsize
  transport/   CellFifo, TextBuffer
  ui/
    core/      Writer, Graphics, Screen, Component, escapes, Metrics, Input, KeyPress, Point
    widgets/   Label, Menu, ListPane, SplitPane, Dialog, Console, TextPane,
               Spinner, ThemeResolver, Braille, MarkdownRenderer, LookAndFeel
```

- **Rationale:** ARCHITECT directed a single `jam_terminal` over a split
  `jam_vt`/`jam_tui`. Read-side and write-side both converge on the same
  foundation types (`jam::Char`, `jam::Row`, `jam::Stamp`), so one module avoids
  an artificial inter-module boundary.
- **Considered and rejected:** split `jam_vt` (read) + `jam_tui` (write) sharing
  foundation types. ARCHITECT chose single module.
- **BLESSED:** SSOT (one home for all terminal/TUI code), Encapsulation
  (subdivisions express concern boundaries).

### P2. Read-side coupling resolved by inheritance + virtual hooks

`jam::terminal::Video` is the **independent base** engine: it interprets VT
commands and writes `jam::Char` cells into `jam::Buffer<jam::Row>`. It holds
**zero** knowledge of `juce::Identifier`, `jam::Function::Map`, ValueTree, or
END's `id::` keys. Where the base must signal the application, it calls a
**protected virtual hook**. END's `terminal::Video : public jam::terminal::Video`
overrides the hooks and adds the event-firing methods as needed.

- **Rationale:** ARCHITECT: "make independent module first. END's terminal::video
  inherit and add method as needed." The events map and `id::` keys are a
  ValueTree/application concern (`Identifier.h`); they must not live in the
  library. Virtual hooks invert the dependency: the base defines extension
  points, the subclass supplies application glue.
- **Mechanics:** `Parser` takes a `jam::terminal::Video&` (`Parser.h:150`
  analogue). END instantiates `Parser` with its `terminal::Video` subclass; the
  base reference dispatches to overrides polymorphically. END's `Processor`
  retains its `events` map, `registerEvents()`, and Model wiring unchanged —
  they move from "Video fires into a map" to "Video subclass overrides fire into
  the map."
- **Considered and rejected:** (a) `vt::Event` enum replacing Identifier keys;
  (b) abstract `EventSink` interface with typed methods. ARCHITECT chose
  inheritance over both.
- **BLESSED:** Encapsulation (base hides nothing app-specific), Lean (library
  carries no ValueTree baggage), Explicit (hooks are named virtual methods).

### P3. Pure types move to `jam::terminal::` directly, used as-is

`CSI`, `CellFifo`, `TextBuffer`, `Winsize`, `CursorState`, `Palette` have zero
END-application coupling (Research §A). They become `jam::terminal::` types with
no subclassing; END uses them directly.

- **BLESSED:** SSOT, Lean.

### P4. `CharProps` + `Charset` + `CharPropsData` absorbed into `jam::Char`

The three free functions that turn a codepoint into stored-cell state move onto
`jam::Char` as **static methods**, with their lookup tables as
**translation-unit-static data** in `jam::Char`'s `.cpp`. There must be **no
stray free function that produces or interprets `Char`** (ARCHITECT: "absorb into
Char, there shouldn't be any stray Char* object").

- `charPropsFor` / `width` → `jam::Char` static query methods.
- `translateCharset` (DEC line drawing) → internal to the `jam::Char` factory.
- `CharPropsData` multistage tables + `DEC_LINE_DRAWING` → private statics in the
  `.cpp`.
- The factory becomes the single Char producer, e.g.
  `jam::Char::fromCodepoint(codepoint, styleId, lineDrawing)` — it translates the
  charset, looks up width, sets the wide hint, and packs. Callers stop touching
  Unicode tables or DEC charsets.

**`jam::Char` stays exactly 8 bytes, stateless, trivially copyable.** Adding
static methods and TU-static tables costs **zero** per-instance bytes; the
existing `static_assert(sizeof(Char)==8)` and trivially-copyable assertions
(`jam_char.h:177-178`) hold unchanged. The lookup tables are shared const data,
queried by all instances, owned by none.

- **Charset translation is stateful** (depends on `useLineDrawing`, toggled by
  `ESC ( 0` / SO / SI). That state lives in the caller (Video). The factory
  receives it as a parameter; `jam::Char` itself stays stateless.
- **Grapheme segmentation running state stays in the caller.**
  `GraphemeSegmentationResult` carries across successive codepoints
  (`Video.cpp:409-411`). It cannot live in `jam::Char` without making Char
  stateful and breaking the 8-byte/trivial guarantee. The per-codepoint
  property lookup is Char's; the cluster-assembly state belongs to whoever
  consumes a stream.
- **Location:** `jam::Char` is in `jam_graphics`; therefore the absorbed tables
  move into `jam_graphics`. `jam_terminal` depends on `jam_graphics`.
- **Rationale:** ARCHITECT: Char is the terminal/TUI cell atom and must own all
  codepoint→cell knowledge. Removing the free functions removes the duplicate
  surface (CharProps existed only to feed `Char::make`).
- **BLESSED:** SSOT (one authority for codepoint→cell), Stateless (Char remains
  pure value), Encapsulation (tables hidden in TU), Bounds (8-byte invariant
  enforced by static_assert).

### P5. Grid unified on `jam::Buffer<jam::Row>` for both sides

The read-side grid is `jam::Buffer<jam::Row>` (`Video.h:333`). The write-side
`Graphics` currently uses a flat `HeapBlock<jam::Char>` plus a **parallel
`HeapBlock<jam::Stamp::Entry> styles` sidecar** (`jam_tui_graphics.h:78-79`).

The sidecar is a **reinvention of `jam::Stamp`**. `jam::Stamp` is the existing
SSOT interning table (`SharedResource<Stamp,StampEntry>` + `Context`,
`jam_stamp.h:38`, `jam_shared_resource.h:20-21`); `jam::Char`'s `styleId` field
**is** the index into it. The write-side must drop the sidecar and intern via
`jam::Stamp::getContext()->addIfNotAlreadyThere(...)` exactly as Video does
(`Video.cpp:491`), packing the styleId into the Char. Then the write-side grid is
pure `jam::Char` — i.e. `jam::Buffer<jam::Row>`.

Access maps cleanly:

| Graphics need | `Buffer<Row>` provision |
|---|---|
| `cellAt(col,row)` | `Block<Row>::getWritePointer(row,col)->cells[col]` (same as Video, `Video.cpp:415`) |
| `clip()` sub-region | `Block<Row>` view with row/col offset |
| `getLines()` row serialize | `Row::usedCols` (`jam_row.h:39`) gives authoritative trailing-blank boundary |
| per-frame rebuild | flat single-channel Buffer, cleared each frame |

- **SSOT is at the type/code level:** one `Buffer<Row>` grid type, one `Char`,
  one `Stamp` pattern — written once, used by both the emulator (END, read-side)
  and TUI apps (TIT/CAKE, write-side). Read-side and write-side run as **separate
  processes**; each process owns its own `Stamp`/`Grapheme` Context per the
  application-owned model (`Main.h:118,121`). No runtime sharing across
  processes, no new pattern.
- **Considered and rejected:** keeping two distinct grid storage models. Research
  showed the only divergence was the sidecar, which is itself redundant with
  `Stamp`; ARCHITECT directed unification on `Buffer<Row>`.
- **BLESSED:** SSOT (one grid type, one style table), Lean (sidecar deleted),
  Encapsulation (style state lives in `Stamp`, not duplicated per-grid).

### P6. `jam_tui` fully deleted; everything absorbed

The engine core moves to `jam_terminal/ui/core/`; the widget layer moves to
`jam_terminal/ui/widgets/`. `jam_tui` is removed entirely. `jam_terminal`
inherits `jam_tui`'s dependency on `jam_markdown` (via MarkdownRenderer).

- **Rationale:** ARCHITECT: "Absorb into jam_terminal." TIT/CAKE are not properly
  started and rewrite against the new structures; no API-preservation obligation.
- **BLESSED:** SSOT (no parallel TUI module), Lean.

### P7. Foundation stays in `jam_graphics`

`jam::Char`, `jam::Cell`, `jam::Row`, `jam::Stamp`, `jam::Grapheme` remain in
`jam_graphics` (their current home). `jam_terminal` depends on `jam_graphics`.
`cell/` in `jam_terminal` therefore holds only `Palette` (colour resolution, not
a codepoint or cell concern); Charset and CharProps were absorbed into
`jam::Char` (P4) and do not appear as standalone files.

- **BLESSED:** SSOT, Encapsulation.

### P8. Protocol-vocabulary SSOT

The VT/ANSI protocol vocabulary (DEC mode numbers — e.g. 2026 sync, 1049 alt,
2004 bracketed paste; OSC codes; CSI final bytes) is presently hardcoded
independently in three places (Problem Statement). Writer (emit-complete) and
Parser (recognize-partial) cannot share one representation, but the **vocabulary
constants** can be defined once in `parser/` and referenced by both sides.

- **Status:** direction follows from "write once"; exact shape is design work
  delegated to COUNSELOR/PLAN (Open Questions Q1).
- **BLESSED:** SSOT, Explicit (no magic numbers).

---

## Scaffold

### S1. Class disposition (complete)

| Source | → Destination | Transform |
|---|---|---|
| `Source/terminal/Parser.h/.cpp`, `ParserAction.cpp` | `jam_terminal/parser/` | Namespace → `jam::terminal`. Holds `jam::terminal::Video&`. |
| `Source/terminal/DispatchTable.h` | `jam_terminal/parser/` | Namespace only. |
| `Source/terminal/CSI.h` | `jam_terminal/parser/` | Namespace only. Trivially-copyable preserved. |
| `Source/terminal/Video.h/.cpp` + `VideoCSI/ESC/SGR/Mode/Edit/OSC/OSCExt/DCS/Ops.cpp` | `jam_terminal/video/` (base) | Strip events-map/Identifier. Replace event-firing sites with protected virtual hooks. END `terminal::Video` subclass overrides. |
| `Source/terminal/CursorState` (`Identifier.h:34`) | `jam_terminal/video/` | Extract from Identifier.h into own header. |
| `Source/terminal/Winsize.h` | `jam_terminal/video/` | Namespace only. |
| `Source/terminal/CellFifo.h` | `jam_terminal/transport/` | Namespace only. |
| `Source/terminal/TextBuffer.h` | `jam_terminal/transport/` | Namespace only. |
| `Source/terminal/Palette.h` | `jam_terminal/cell/` | Namespace only. |
| `Source/terminal/CharProps.h`, `CharPropsData.h`, `Charset.h` | **absorbed into `jam_graphics/detail/jam_char`** | Free functions → `jam::Char` static methods; tables → TU-static in `.cpp`. Files deleted. (P4) |
| `jam_tui/ansi/jam_tui_writer.*` | `jam_terminal/ui/core/` | Namespace `jam::tui` → `jam::terminal`. |
| `jam_tui/ansi/jam_tui_graphics.*`, `*_serialize.cpp` | `jam_terminal/ui/core/` | Delete `styles` sidecar; intern via `jam::Stamp`; back grid with `jam::Buffer<jam::Row>`. (P5) |
| `jam_tui/ansi/jam_tui_screen.*` | `jam_terminal/ui/core/` | Namespace. |
| `jam_tui/ansi/jam_tui_component.h` | `jam_terminal/ui/core/` | Namespace; strip "CAROLINE" doc (`:10`). |
| `jam_tui/ansi/jam_tui_escapes.h` | `jam_terminal/ui/core/` | `namespace ANSI` → `jam::terminal`; remove `caroline` literal in `CURSOR_MARKER` (`:26`). |
| `jam_tui/ansi/jam_tui_textbox.*` | `jam_terminal/ui/core/` | Namespace. |
| `jam_tui/metrics/jam_tui_metrics.h` | `jam_terminal/ui/core/` | Namespace; fix `s/cell/` doc rot (`:8`,`:16`). |
| `jam_tui/input/*`, `graphics/jam_tui_point.h` | `jam_terminal/ui/core/` | Namespace. |
| `jam_tui/component/*`, `braille/*`, `lookandfeel/*`, `markdown/*` | `jam_terminal/ui/widgets/` | Namespace. |
| `jam_tui/jam_tui.h/.cpp` | **deleted** | Replaced by `jam_terminal/jam_terminal.h/.cpp` module header. |
| END `Source/terminal/Processor`, `Session`, `Model`, `Display`, `Skit`, `Sixel/Kitty/ITerm2Decoder`, `LinkManager*`, `action/*`, `tty/*`, `Input/Keyboard/Mouse`, `component/*`, `Identifier.h` `id::`, `Source/Map.h` | **stay in END** (`terminal::` / `app::`) | END consumes `jam::terminal::`; `terminal::Video` becomes a subclass. |

### S2. Inheritance model (read-side)

Base (`jam_terminal/video/`), illustrative shape — protected virtual hooks
replace each current event-firing site; final hook set is enumerated in PLAN
(Open Questions Q2):

```cpp
namespace jam::terminal
{
class Video
{
public:
    explicit Video (jam::Cell::Rectangle dims) noexcept;   // no events-map param

    void print (uint32_t codepoint) noexcept;
    void applyControlCode (uint8_t) noexcept;
    void applyCSI (const CSI&, const uint8_t*, uint8_t, uint8_t) noexcept;
    void applyESC (const uint8_t*, uint8_t, uint8_t) noexcept;
    void applyOSC (const uint8_t*, int) noexcept;
    void applyDCSPayload (const uint8_t*, int) noexcept;
    void applyAPCPayload (const uint8_t*, int) noexcept;
    void setWinsize (jam::Cell::Rectangle) noexcept;
    const jam::Buffer<jam::Row>& getGrid() const noexcept;
    // ... existing public VT surface, minus the events map ...

protected:
    // Extension points — base calls, subclass fires application events.
    // Names/granularity finalized in PLAN (Q2). Examples:
    virtual void onLineDeparted (int screen) {}             // was id::pushLine
    virtual void onScreenDirty (int screen) {}              // was id::screenDirty
    virtual void onScrollUp (int screen, int count) {}      // was id::scrollUp
    virtual void onWriteToHost (const char*, int) {}        // was id::writeToHost
    virtual void onBell () {}                                // was id::bell
    virtual void onTitle (const uint8_t*, int) {}           // was id::title
    virtual void onCwd (const uint8_t*, int) {}             // was id::cwd
    virtual void onClipboard (const juce::String&) {}        // was id::clipboardChanged
    virtual void onNotification (const juce::String&, const juce::String&) {}
    virtual uint16_t onRegisterLink (const juce::String& uri,
                                     const juce::String& params) { return 0; } // OSC 8
    virtual void onCursorFlush (int screen, int packedCursorState) {}
    virtual void onModeFlush (/* mode id, bool */) {}
    virtual void onOutputBlockStart (int relRow) {}          // OSC 133
    virtual void onOutputBlockEnd (int relRow) {}
    virtual void onExtendOutputBlock (int relRow) {}
    virtual void onPromptRow (int relRow) {}
    virtual void onDcsPayloadComplete (const uint8_t*, int) {}
    virtual void onApcPayloadComplete (const uint8_t*, int) {}
    virtual void onOsc1337Raw (const uint8_t*, int, int, int) {}
    virtual void onClearBuffer (int screen) {}
};
} // namespace jam::terminal
```

END (`Source/terminal/Video.h`):

```cpp
namespace terminal
{
class Video : public jam::terminal::Video
{
public:
    Video (jam::Cell::Rectangle dims,
           jam::Function::Map<juce::Identifier, void>& events) noexcept;

protected:
    void onLineDeparted (int screen) override;   // → events.fire(id::pushLine, screen)
    void onScreenDirty (int screen) override;    // → events.fire(id::screenDirty, screen)
    uint16_t onRegisterLink (const juce::String& uri,
                             const juce::String& params) override; // → Model link-id
    // ... overrides for the events END's Processor::registerEvents() handles ...

private:
    jam::Function::Map<juce::Identifier, void>& events;
};
} // namespace terminal
```

`Processor` (`Processor.h:127`) constructs `terminal::Video` with its events map
exactly as today; `registerEvents()` (`Processor.h:415`) is unchanged. The only
change is *where* the firing originates — subclass override instead of inline
call within the base.

### S3. Char absorption (P4) — factory shape

```cpp
namespace jam
{
struct Char
{
    // ... existing 8-byte storage + accessors + make()/erase() unchanged ...

    // Single producer from a raw codepoint. Charset state passed by caller.
    static Char fromCodepoint (uint32_t codepoint,
                               uint16_t styleId,
                               bool lineDrawing) noexcept;   // translate + width + pack

    // Stateless per-codepoint queries (back tables in jam_char.cpp).
    static int  width (uint32_t codepoint) noexcept;
    static bool isWordChar (uint32_t codepoint) noexcept;
    static bool isCombining (uint32_t codepoint) noexcept;
    // ... only the queries actually consumed by Video/selection ...
};
} // namespace jam
```

Grapheme cluster detection (running state machine) is offered as a stateless
step the caller threads — placement (`jam::Char` static vs `jam::Grapheme`
static) is Open Question Q3. The running `GraphemeSegmentationResult` stays a
caller-held value (Video member), never inside `Char`.

`Video::print()` collapses from three external helpers + manual packing to the
factory call; the `useLineDrawing` member supplies the charset state.

### S4. Grid unification (P5) — Graphics change

- Delete `juce::HeapBlock<jam::Stamp::Entry> styles` (`jam_tui_graphics.h:79`).
- Back the framebuffer with `jam::Buffer<jam::Row>` (single channel).
- `setColour`/`setBold`/`setItalic`/`setUnderline` update a `jam::Stamp::Entry`
  pen; on cell write, intern via `jam::Stamp::getContext()->addIfNotAlreadyThere`
  and pack the returned styleId into the `jam::Char` (mirrors `Video.cpp:491`).
- `clip()` returns a `Block<Row>` sub-view (offset/clamped) instead of a raw
  pointer+stride child.
- `getLines()` reads `Row::usedCols` for trailing-blank trimming.

### S5. Cleanup register (file:line)

| Item | Location | Action |
|---|---|---|
| CAROLINE literal in cursor marker | `jam_tui_escapes.h:26` | Replace `caroline` token; parameterize or rename neutrally. |
| "CAROLINE TUI components" doc | `jam_tui_component.h:10` | Neutral wording. |
| Global `namespace ANSI` | `jam_tui_escapes.h:3` | → `jam::terminal` (P1). |
| Doc rot `s/cell/...` | `jam_tui_metrics.h:8,16` | Fix path references. |
| Namespace `jam::tui` | all `jam_tui/*` | → `jam::terminal`. |
| Protocol vocabulary triplication | `jam_tui_escapes.h`, END `DispatchTable.h`/`Video`, END `Identifier.h`/`Map.h` | Single vocabulary in `parser/` (P8, Q1). |
| Style sidecar | `jam_tui_graphics.h:79` | Delete; use `jam::Stamp` (P5). |

### S6. Dependency graph

```
jam_terminal  →  jam_core, jam_graphics, jam_markdown, juce_*
jam_graphics  →  (gains absorbed Char Unicode/charset tables; otherwise unchanged)
END           →  jam_terminal (+ existing jam deps)
```

---

## BLESSED Compliance Checklist

- [x] **Bounds** — `jam::Char` 8-byte invariant preserved by `static_assert`
  (`jam_char.h:177`); `CSI` trivial-copyability preserved (`CSI.h:384`);
  `CellFifo` drop-oldest bounded rings unchanged.
- [x] **Lean** — `jam_tui` deleted; `Graphics` style sidecar deleted;
  `CharProps` standalone surface deleted (absorbed); library `Video` carries no
  ValueTree/Identifier baggage.
- [x] **Explicit** — virtual hooks are named methods (S2); protocol vocabulary
  becomes named constants (P8); no magic mode numbers after Q1.
- [x] **SSOT** — one grid type (`Buffer<Row>`), one style table (`jam::Stamp`),
  one cluster table (`jam::Grapheme`), one codepoint→cell authority (`jam::Char`),
  one terminal/TUI module (`jam_terminal`).
- [x] **Stateless** — `jam::Char` stays pure value metadata; absorbed tables are
  shared const TU-static; grapheme running state stays in the caller, not Char.
- [x] **Encapsulation** — base `Video` exposes only VT surface + protected hooks;
  END app-glue confined to the subclass; tables hidden in `.cpp`.
- [x] **Deterministic** — `DispatchTable` O(1) immutable lookup; `charPropsFor`
  branchless multistage lookup absorbed unchanged; SPSC seqlock semantics
  unchanged.

---

## Open Questions

These are design-level items delegated to COUNSELOR/PLAN. They do **not** require
further ARCHITECT input to begin planning; each has a locked direction.

- **Q1 — Protocol-vocabulary shape.** Direction locked (single vocabulary in
  `parser/`, referenced by Writer and Parser; P8). COUNSELOR/PLAN decides the
  concrete container (enum, `constexpr` table, or namespaced constants) and
  migration order for the three current sites.
- **Q2 — Virtual hook granularity.** Direction locked (protected virtual hooks,
  P2/S2). PLAN enumerates the exact hook set 1:1 against the current event keys
  (`Processor.h:353-366`, `Identifier.h:270-376`) and names them per NAMES.md.
  ARCHITECT guidance was "add method as needed" — granularity finalizes during
  implementation.
- **Q3 — Grapheme-step placement.** The stateless segmentation step (currently
  `graphemeSegmentationStep`, `CharProps.h:416`) lands as a static on `jam::Char`
  or `jam::Grapheme`. Both keep the running state in the caller (P4). PLAN
  selects placement; the principle (codepoint property = Char; cluster assembly =
  Grapheme) is the deciding rule.

No questions block the RFC. All architectural decisions are locked by ARCHITECT
this session (Principles P1–P8).

---

## Handoff Notes

For COUNSELOR consuming this RFC into PLAN:

1. **Refactor-Rewrite Discipline applies.** This is an extraction/rewrite.
   Per CAROL §10: delete-first where a class moves. The `jam_tui` deletion and
   the `Graphics` sidecar removal are step-1 deletions, not post-hoc cleanup.
   Compiler errors after deletion are the ground of truth for what remains to
   wire.

2. **Build/validation constraint.** ARCHITECT-only builds (MEMORY.md). Do not run
   cmake/ninja/make/xcodebuild. Validation is CONTRACT-adherence + ARCHITECT
   build runs.

3. **Sequencing dependency.** P4 (Char absorption) modifies `jam_graphics` and is
   a prerequisite for P5 (grid unification) and for `Video::print` simplification.
   `jam_graphics` change lands before `jam_terminal` consumers compile.

4. **Inheritance preserves END's Processor wiring.** `Processor` (`Processor.h`),
   `registerEvents()`, the `events` map, and `Model` flush remain END-side and
   essentially unchanged — only the *origin* of event firing moves from inline
   base calls to subclass overrides (S2). This bounds the END-side blast radius.

5. **Two separate runtime processes.** Read-side (END emulator) and write-side
   (TUI apps) never share a `Stamp`/`Grapheme` instance at runtime; SSOT here is
   code-level reuse, each process owning its own application-scoped Context
   (`Main.h:118,121`). Do not introduce cross-process table sharing.

6. **Doxygen.** Per CAROL Doxygen Protocol, regenerate library doxygen after the
   move: `jam` index at `~/Documents/Poems/dev/jam/doxygen/xml/index.xml`. New
   module compounds (`jam::terminal::*`) must index before END-side audits rely
   on `<references>`/`<referencedby>`.

7. **Naming gate.** New names introduced here — `jam::terminal` namespace, the
   `Video` virtual hook method names, `jam::Char::fromCodepoint` and query
   methods — are Decision-Gate items against NAMES.md. ARCHITECT approved
   `jam::terminal::Video` as the base name this session; remaining new names
   (hooks, factory) require NAMES.md alignment before introduction.

