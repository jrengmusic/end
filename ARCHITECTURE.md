# END - Architecture

**Purpose:** Single source of truth for project structure, patterns, and contracts.

**Status:** STABLE

**Last Updated:** 2026-03-07

---

## Project Overview

### Purpose

END (Ephemeral Nexus Display) is a GPU-accelerated terminal emulator built with C++17 and JUCE. It renders terminal output through an OpenGL pipeline with FreeType/HarfBuzz text shaping and a glyph atlas cache.

### Architecture Philosophy

APVTS-inspired data flow. Reader thread writes atomics, timer flushes to ValueTree, UI pulls from ValueTree listeners. Render path bypasses the timer via VBlankAttachment polling a dirty atomic. No thread pushes to another — all communication is pull-based.

### Technology Stack

- **Language:** C++17
- **Framework:** JUCE 8
- **Text Rendering:** CoreText + HarfBuzz (macOS), FreeType + HarfBuzz (Linux/Windows)
- **GPU:** OpenGL (via JUCE OpenGLContext)
- **Config:** Lua (sol2)
- **Build System:** JUCE / CMake
- **Platform:** macOS (primary), Linux, Windows (in progress)

---

## Module Structure

### Module Map

```
Source/
  Main.cpp                          Application entry, owns Config + MainWindow
  MainComponent.h/cpp               Root component, owns TerminalComponent

  config/
    Config.h/cpp                    Lua config loader, Context<Config> pattern

  component/
    TerminalComponent.h/cpp         UI host, VBlankAttachment render loop
    CursorComponent.h               Cursor overlay, ValueTree-driven
    MessageOverlay.h                Transient overlay: grid size on resize, arbitrary messages

  fonts/
    DisplayMono-Book.ttf            Embedded base font (BinaryData)
    DisplayMono-Bold.ttf            Embedded bold variant (BinaryData)
    DisplayMono-Medium.ttf          Embedded medium variant (BinaryData)
    SymbolsNerdFont-Regular.ttf     Embedded NF icon font (BinaryData)

  terminal/
    data/                           Pure data types + state (no logic, no rendering)
      Cell.h                        16-byte trivially-copyable cell
      Color.h                       4-byte color (theme/palette/rgb)
      Pen.h (in Cell.h)             Current text attributes
      Grapheme.h (in Cell.h)        Multi-codepoint cluster
      CSI.h                         CSI parameter accumulator
      Charset.h                     Character set tables (G0/G1)
      Palette.h                     256-color palette (std::array)
      DispatchTable.h               VT state machine transition table
      Identifier.h                  ValueTree IDs + Identifier hash
      State.h/cpp                   APVTS-style atomic + timer + ValueTree
      StateFlush.cpp                Timer flush implementation
      ValueTreeUtilities.h          ValueTree traversal helpers
      Keyboard.h                    Keypress -> escape sequence mapping

    logic/                          Terminal emulation (parser, grid, session)
      Session.h/cpp                 Orchestrator: owns State, Grid, Parser, TTY
      Parser.h/cpp                  VT state machine + dispatch
      ParserVT.cpp                  Ground state: print, execute, LF
      ParserCSI.cpp                 CSI dispatch (cursor, erase, mode)
      ParserESC.cpp                 ESC dispatch (charset, OSC, DCS)
      ParserSGR.cpp                 SGR (text attributes, color)
      ParserEdit.cpp                Erase, scroll, screen switch
      ParserOps.cpp                 Cursor movement, tab, reset
      Grid.h/cpp                    Ring buffer, dual screen, dirty tracking
      GridScroll.cpp                Scroll region operations
      GridErase.cpp                 Erase operations
      GridReflow.cpp                Reflow on resize

    rendering/                      GPU pipeline (fonts, atlas, GL)
      Screen.h/cpp                  Render coordinator, cell cache, snapshot builder
      ScreenRender.cpp              populateFromGrid, buildSnapshot
      ScreenSnapshot.cpp            updateSnapshot, publish to mailbox
      ScreenSelection.h             Selection anchor/end, contains() hit test, inversion rendering
      Fonts.h                       Shared header (platform-agnostic API)
      Fonts.mm                      macOS: CoreText font loading, HarfBuzz shaping, CTFontDrawGlyphs rasterization
      Fonts.cpp                     Linux/Windows: FreeType font loading
      FontsMetrics.cpp              Cell metrics calculation
      FontsShaping.cpp              Text shaping (ASCII fast path, HarfBuzz, fallback) -- shared + FreeType path
      FontCollection.h              Codepoint-to-font-slot lookup API
      FontCollection.cpp            Flat int8_t[0x110000] dispatch table, up to 32 slots
      FontCollection.mm             macOS: platform font handle + hb_font_t construction
      GlyphConstraint.h             Per-codepoint NF icon scaling/alignment descriptor
      GlyphConstraintTable.cpp      Generated table: 10,470 codepoints, 88 switch arms
      BoxDrawing.h                  Procedural rasterizer: box drawing, block elements, braille
      GlyphAtlas.h/cpp              LRU glyph cache + atlas packer + staged upload
      AtlasPacker.h                 Shelf-based rectangle packer
      TerminalGLRenderer.cpp        OpenGL renderer (shaders, draw calls)
      TerminalGLDraw.cpp            Instance upload + draw
      GLShaderCompiler.h/cpp        Shader compilation
      GLVertexLayout.h/cpp          VAO/VBO layout
      shaders/                      GLSL vertex/fragment shaders

    tty/                            Platform TTY abstraction
      TTY.h/cpp                     Abstract base + reader thread
      UnixTTY.h/cpp                 macOS/Linux: forkpty
      WindowsTTY.h/cpp              Windows: ConPTY (in progress)
```

### Module Inventory

| Module | Location | Responsibility | Dependencies |
|--------|----------|----------------|--------------|
| Config | `config/` | Lua config load/save, Context-managed | sol2, jreng::Context |
| Component | `component/` | JUCE UI hosting, VBlank render trigger | Session, Screen, Config |
| Fonts | `fonts/` | Embedded TTF binaries (BinaryData) | — |
| Data | `terminal/data/` | Pure value types, state atomics, IDs | JUCE ValueTree |
| Logic | `terminal/logic/` | VT parsing, grid storage, session orchestration | Data |
| Rendering | `terminal/rendering/` | Font shaping, glyph atlas, GL draw | Data, FreeType, HarfBuzz, OpenGL |
| TTY | `terminal/tty/` | Platform PTY abstraction, reader thread | JUCE Thread |

---

## Layer Separation Rules

```
 Component (UI)          pulls from State via ValueTree listeners + VBlank
    |
    v
 Logic (Parser/Grid)     writes atomics on reader thread
    |
    v
 Data (State/Cell)       pure types, atomic storage, timer flush
    |
    v
 Rendering (Screen/GL)   reads from Grid + State, builds GPU snapshots
    |
    v
 TTY (platform)          reader thread feeds raw bytes to Parser
```

### Communication Contracts

**TTY -> Logic:**
- TTY reader thread calls `Parser::process(data, length)` directly
- Parser writes to Grid and State atomics on reader thread
- No allocation, no locks on this path

**Logic -> Data:**
- Parser calls `State::set*()` — stores to atomics, sets `needsFlush`
- Parser calls `Grid::markRowDirty()` — sets dirty bits, sets `snapshotDirty`
- All calls are `noexcept`, reader thread safe

**Data -> Component (timer path):**
- `State::timerCallback()` runs on message thread (60-120Hz)
- Flushes atomics to ValueTree via `flush()`
- ValueTree fires `valueTreePropertyChanged` -> CursorComponent updates

**Data -> Component (render path):**
- `VBlankAttachment` fires on every display vsync (CVDisplayLink)
- Calls `State::consumeSnapshotDirty()` — one atomic exchange
- If dirty: `Screen::render()` reads Grid + State, builds snapshot, publishes to Mailbox

**Component -> Rendering (GL path):**
- `Render::Mailbox::publish()` — message thread publishes snapshot pointer
- `Render::Mailbox::acquire()` — GL thread acquires snapshot pointer
- Lock-free: single `std::atomic<Snapshot*>` exchange

### Layer Violations (FORBIDDEN)

- Rendering must NEVER call Parser or Grid mutators
- TTY must NEVER call UI/Component code
- Parser must NEVER allocate on reader thread
- GL thread must NEVER write to Grid or State

---

## Threading Model

### Threads

| Thread | QoS | Owns | Reads | Writes |
|--------|-----|------|-------|--------|
| **Reader** (TTY) | high | TTY fd | raw bytes | State atomics, Grid cells, dirty bits |
| **Timer** (JUCE) | default | — | `needsFlush` atomic | ValueTree properties |
| **Message** (main) | user-interactive | Component, Screen | ValueTree, `snapshotDirty` atomic, Grid cells | Snapshot, hotCells cache |
| **GL** (OpenGL) | user-interactive | OpenGL context | Snapshot (via Mailbox), staged bitmaps | GPU textures, framebuffer |

### Data Flow: Keystroke to Pixel

```
Keystroke -> Message Thread -> TTY::write()
         -> Reader Thread reads response -> Parser::process()
         -> Grid cells written, State atomics set, snapshotDirty = true
         -> VBlank fires on Message Thread -> consumeSnapshotDirty()
         -> Screen::render() reads Grid -> builds Snapshot -> Mailbox::publish()
         -> GL Thread -> Mailbox::acquire() -> uploadStagedBitmaps() -> draw
```

### Synchronization Primitives

| Mechanism | Between | Purpose |
|-----------|---------|---------|
| `std::atomic<float>` | Reader -> Timer/Message | State parameter transport |
| `std::atomic<bool> snapshotDirty` | Reader -> Message (VBlank) | Render trigger |
| `std::atomic<bool> needsFlush` | Reader -> Timer | ValueTree flush trigger |
| `std::atomic<Snapshot*>` (Mailbox) | Message -> GL | Snapshot handoff |
| `std::mutex` (uploadMutex) | Message -> GL | Staged bitmap queue |
| `juce::CriticalSection` (resizeLock) | Message <-> Reader | Grid resize safety |

---

## Design Patterns in Use

### Pattern: APVTS-Style State

**Used for:** Cross-thread state synchronization without locks on the hot path.

**Implementation:** `State.h/cpp`, `StateFlush.cpp`

Reader thread writes to `std::atomic<float>` via `storeAndFlush()`. Timer polls `needsFlush` and copies atomics to ValueTree. UI reads from ValueTree listeners. Render path bypasses timer via `snapshotDirty` atomic polled by VBlankAttachment.

### Pattern: Double-Buffered Snapshot Mailbox

**Used for:** Lock-free handoff of render data from message thread to GL thread.

**Implementation:** `Screen.h` — `Render::Mailbox`

Message thread builds into `snapshotA` or `snapshotB`, publishes via atomic exchange, gets back the old one for reuse. GL thread acquires via atomic exchange. Zero allocation, zero locking.

### Pattern: Context<T> (Responsible Global)

**Used for:** Config access without Meyer's singleton.

**Implementation:** `Config.h` inherits `jreng::Context<Config>`

Lifetime owned by `ENDApplication` (Main.cpp). Access via `Config::getContext()->`. Fail-fast jassert if accessed before construction.

### Pattern: Shelf-Based Atlas Packing

**Used for:** Packing variable-size glyphs into a fixed 4096x4096 texture atlas.

**Implementation:** `AtlasPacker.h`

Horizontal shelves, best-fit allocation. Separate packers for mono and emoji. LRU eviction when cache is full.

### Pattern: File Decomposition by Concern

**Used for:** Keeping files under 300 lines while maintaining logical cohesion.

Parser.cpp -> ParserCSI, ParserESC, ParserSGR, ParserVT, ParserEdit, ParserOps
Grid.cpp -> GridScroll, GridErase, GridReflow
State.cpp -> StateFlush
Screen.cpp -> ScreenRender, ScreenSnapshot
Fonts.cpp -> FontsMetrics, FontsShaping
TerminalGLRenderer.cpp -> TerminalGLDraw

All split files define member functions of the parent class. No separate classes needed.

---

## Key Data Types

### Cell (16 bytes, trivially copyable)

```
| codepoint (4B) | style (1B) | layout (1B) | width (1B) | reserved (1B) | fg (4B) | bg (4B) |
```

Style bits: BOLD, ITALIC, UNDERLINE, STRIKE, BLINK, INVERSE
Layout bits: wide continuation, emoji, has grapheme

### Color (4 bytes, trivially copyable)

```
| red/paletteIndex (1B) | green (1B) | blue (1B) | mode (1B) |
```

Mode: theme (0), palette (1), rgb (2)
Access: `setRGB()`, `setPalette()`, `setTheme()`, `paletteIndex()`

### Grid Ring Buffer

Dual buffers (normal + alternate). Each buffer is a flat `HeapBlock<Cell>` with ring-buffer row indexing. `head` tracks the logical top row. Dirty tracking via `std::atomic<uint64_t> dirtyRows[4]` (256-bit bitmask).

### GlyphConstraint

Per-codepoint scaling and alignment descriptor for Nerd Font icons. Applied at rasterization time in GlyphAtlas.

Fields:
- `ScaleMode` — none / fit / cover / adaptiveScale / stretch
- `AlignH` / `AlignV` — horizontal and vertical alignment within cell
- `HeightRef` — cell height or icon natural height as reference
- `padding` — inset from cell edges
- position/size overrides — pixel-level nudge for specific icons
- `maxAspectRatio` — clamp for very wide icons
- `maxCellSpan` — maximum columns an icon may occupy

Coverage: 10,470 codepoints across 88 switch arms, generated from NF patcher v3.4.0 data.

### FontCollection

O(1) codepoint-to-font-slot lookup. Flat `int8_t[0x110000]` dispatch table covering the full Unicode range. Up to 32 font slots.

Sentinel values: `UNRESOLVED` (-1) — not yet queried; `NOT_FOUND` (-2) — no font covers this codepoint.

Each slot holds: platform font handle + `hb_font_t*` + `hasColorGlyphs` flag.

NF icon font loaded from BinaryData, not the system font manager.

### BoxDrawing

Procedural rasterizer for three Unicode ranges — no font lookup for these codepoints:
- U+2500-U+257F — box drawing characters
- U+2580-U+259F — block elements
- U+2800-U+28FF — braille patterns

Uses SDF for rounded corners and anti-aliased diagonals. Produces pixel-perfect alignment at any cell size.

### ScreenSelection

Anchor + end `Point<int>` pair. `contains()` for per-cell hit testing. Renders via transparent background overlay using `colours.selection` config color.

### MessageOverlay

Transient overlay component. Shows grid dimensions (columns x rows) during resize. Accepts arbitrary string messages on demand. Fade-in/out animation driven by `jreng::Animator`. Replaces the earlier GridSizeOverlay.

### StagedBitmap

Cross-thread upload packet passed from the message thread to the GL thread via the staged bitmap queue. Contains: pixel data, target atlas region, and kind (mono / emoji).

### AtlasGlyph

Cached rasterization result stored in the LRU map. Contains: texture UV rect, pixel dimensions, and bearing (horizontal + vertical offset from cell origin).

### LRUGlyphCache

Frame-stamped LRU map. Each entry records the last frame it was accessed. When capacity is exceeded, the oldest 10% of entries are evicted and their atlas regions returned to the packer.

Capacities: mono 19,000 glyphs; emoji 4,000 glyphs.

---

## Key Design Decisions

### Decision: VBlankAttachment for Render Trigger

**Context:** Timer-based render trigger (60-120Hz) suffered latency under CPU contention because macOS deprioritizes the JUCE timer thread.

**Decision:** Replace timer-driven render with CVDisplayLink-driven polling via `juce::VBlankAttachment`. State stays pure (timer + atomics only). TerminalComponent polls `consumeSnapshotDirty()` on every vsync.

**Rationale:** CVDisplayLink runs at display-driver priority, immune to timer QoS coalescing. Worst-case latency is one frame (8-16ms), imperceptible for terminal text. State remains a pure data layer with no UI knowledge.

### Decision: Context<T> over Meyer's Singleton for Config

**Context:** Config was a Meyer's singleton (`static Config& get()`). This caused static initialization order issues and hid lifetime management.

**Decision:** Config inherits `jreng::Context<Config>`. Owned by `ENDApplication`. Accessed via `Config::getContext()->`.

**Rationale:** Explicit lifetime, fail-fast on misuse, no static init ordering problems.

### Decision: Unified Cell (16B) over HotCell/ColdCell Split

**Context:** SPEC originally proposed 8B HotCell + 20B ColdCell SoA layout for cache optimization.

**Decision:** Unified 16-byte Cell with inline fg/bg Color. Grapheme stored separately in a sparse map.

**Rationale:** Simpler code, still fits 2 cells per cache line. The HotCell/ColdCell split added complexity for marginal cache benefit given typical terminal workloads. 95%+ of cells have no grapheme, so the sparse map handles the rare case efficiently.

### Decision: Procedural Box Drawing over Font Glyphs

**Context:** Box drawing characters (U+2500-U+257F) could come from the font or be drawn procedurally.

**Decision:** All box drawing, block elements, and braille rendered procedurally in `BoxDrawing.h`. No font lookup for these ranges.

**Rationale:** Pixel-perfect alignment at any cell size. Font glyphs often have inconsistent metrics causing visible gaps at cell boundaries. Kitty, Ghostty, and WezTerm all use procedural rendering for these ranges.

### Decision: Glyph-Based Cursor over Geometric Shapes

**Context:** SPEC proposed block/underline/bar cursor styles as geometric quads.

**Decision:** Cursor renders any Unicode codepoint via `Fonts::rasterizeToImage()`. Default: U+2588 (full block). Configurable via `cursor.char`.

**Rationale:** More flexible — user can use any character. Color emoji cursors supported. Consistent with the font rendering pipeline.

### Decision: Direct Parser Callback over SPSC FIFO

**Context:** SPEC proposed 2x SPSC ring buffers (`juce::AbstractFifo`) between PTY and message thread.

**Decision:** TTY reader thread calls `Parser::process()` directly. Parser writes to Grid cells and State atomics on the reader thread.

**Rationale:** Simpler, lower latency. The FIFO added a drain step on the message thread that was unnecessary — the parser is fast enough to run on the reader thread without blocking. Grid access is protected by `resizeLock` CriticalSection only during resize.

### Decision: NF Glyph Constraint System

**Context:** Nerd Font icons have wildly varying aspect ratios and need per-glyph positioning to look correct in a monospace grid.

**Decision:** Generated constraint table (10,470 codepoints, 88 switch arms) from NF patcher v3.4.0 data. Applied at rasterization time in GlyphAtlas.

**Rationale:** Matches NF patcher's own scaling logic. Icons render identically to how they appear in patched fonts, but with runtime flexibility for any cell size.

---

## Font Architecture

### Display Monolithic — Embedded Font

Single embedded TTF covering all modern terminal rendering needs. No exotic scripts — those delegate to OS system fonts at runtime.

**Build Pipeline (strict order):**

```
1. Display Mono (base)          -- 98 glyphs + 12 ligatures, advance width uniform
2. + Noto Sans Symbols 2        -- geometric, dingbats, block elements, braille,
                                   legacy computing (U+1FB00), miscellaneous symbols
3. + Noto Emoji (non-color)     -- monotone pictographs U+1F300-U+1F5FF
4. -> NF patcher                -- --complete --mono --careful
                                   careful = never overwrite existing glyphs
```

`fonttools merge` step 1-3 with Display Mono first (first-file-wins on conflicts). NF patch runs last on the merged result.

**What Display Monolithic covers:**
- All ASCII + extended Latin
- Developer ligatures
- Nerd Font icons (complete)
- Powerline symbols
- Geometric shapes, dingbats, arrows, misc symbols
- Block elements, braille, legacy computing sextants
- Monotone emoji/pictographs

**What END handles internally (not in font):**
- Box drawing U+2500-U+257F — synthesized via BoxDrawing procedural rasterizer
- Block elements U+2580-U+259F — synthesized via BoxDrawing
- Braille U+2800-U+28FF — synthesized via BoxDrawing
- Color emoji — delegated to system font (Apple Color Emoji / Segoe UI Emoji / Noto Color Emoji)
- CJK and exotic scripts — delegated to OS system fonts via CoreText (Mac) / DirectWrite (Windows) / Fontconfig (Linux)

**Donor font licenses:**
- Noto Sans Symbols 2 — OFL
- Noto Emoji — OFL
- Symbols Nerd Font — OFL (NF patcher source)
- Display Mono — proprietary (JRENG), embedded binary only

### FontCollection Subsystem

`FontCollection` owns the flat `int8_t[0x110000]` codepoint dispatch table. On first access for a codepoint, it queries the font stack and caches the result. Subsequent lookups are a single array read.

The NF icon font (`SymbolsNerdFont-Regular.ttf`) is loaded from BinaryData, not the system font manager. This ensures consistent icon rendering regardless of what fonts the user has installed.

### GlyphConstraint Subsystem

`GlyphConstraintTable.cpp` is a generated file. It maps NF icon codepoints to `GlyphConstraint` descriptors that replicate the scaling decisions made by the NF patcher. GlyphAtlas applies the constraint before writing pixels to the atlas, so icons are positioned and scaled identically to how they appear in a patched font — but at any runtime cell size.

### BoxDrawing Subsystem

`BoxDrawing.h` intercepts codepoints in the box drawing, block element, and braille ranges before any font lookup occurs. It rasterizes directly to a pixel buffer using:
- Integer arithmetic for straight lines and solid blocks
- SDF for rounded box corners
- Anti-aliased Bresenham for diagonal lines

This eliminates the font-metric inconsistencies that cause visible seams between adjacent box drawing characters.

### Embolden

Bold variants use platform-native stroke widening rather than a separate bold font file:
- macOS: `kCGTextFillStroke` with stroke width proportional to cell size
- Linux: `FT_Outline_Embolden` applied to the FreeType outline before rasterization

### Font Stack at Runtime

```
Display Monolithic -> OS system fonts (CJK/exotic) -> OS color emoji
```

### Platform Font Dispatch

Same header (`Fonts.h`), different implementations. Caller site identical.

| Platform | Font Loading | Text Shaping | Emoji Shaping | Glyph Rasterization |
|----------|-------------|--------------|---------------|---------------------|
| macOS | CoreText (`CTFontCreateWithName`) | HarfBuzz (`hb_coretext_font_create`) | HarfBuzz (`emojiHbFont`) | CoreText (`CTFontDrawGlyphs`) |
| Linux/Win | FreeType (`FT_New_Face`) | HarfBuzz (`hb_ft_font_create`) | TBD | FreeType (`FT_Render_Glyph`) |

HarfBuzz is JUCE's bundled version (10.1.0, `HAVE_CORETEXT=1` on macOS).

### Font Resize (Zoom)

`Fonts::setSize()` resizes all font handles when zoom changes (Cmd+/-/0):

**macOS:** `CTFontCreateCopyWithAttributes` on each font (mainFont, emojiFont, identityFont, nerdFont). Destroys and recreates all HarfBuzz shaping fonts. Clears the `fallbackFontCache` (releases all cached CTFontRefs). Updates FontCollection entry pointers (slot 0 = identityFont, slot 1 = nerdFont).

**Linux:** `FT_Set_Char_Size` on all FT_Face handles (4 style faces, emojiFace, nfFace). Destroys and recreates `nerdShapingFont`. Updates FontCollection slot 1.

Zoom state is persisted in `~/.config/end/state.lua`, not in `end.lua` config.

---

## Glossary

| Term | Definition |
|------|------------|
| AtlasGlyph | Cached rasterization result: texture UV rect, pixel dimensions, bearing |
| BoxDrawing | Procedural rasterizer for box drawing, block elements, and braille — no font lookup |
| Cell | 16-byte struct representing one terminal character position |
| Embolden | Platform stroke-widening for bold: kCGTextFillStroke (macOS), FT_Outline_Embolden (Linux) |
| FontCollection | Flat int8_t[0x110000] codepoint-to-font-slot dispatch table, O(1) lookup |
| GlyphConstraint | Per-codepoint NF icon scaling/alignment descriptor applied at rasterization time |
| Grapheme | Multi-codepoint character cluster (e.g., flag emoji, combining marks) |
| Grid | Ring-buffer storage for terminal cells, dual-screen (normal/alternate) |
| LRUGlyphCache | Frame-stamped LRU map; evicts oldest 10% when over capacity |
| Mailbox | Lock-free atomic pointer exchange for snapshot handoff between threads |
| MessageOverlay | Transient overlay showing grid size on resize or arbitrary messages on demand |
| Pen | Current text attributes (style + fg/bg color) applied to new cells |
| ScreenSelection | Anchor + end Point<int> pair for text selection; contains() for hit testing |
| Snapshot | Pre-built GPU instance data (glyphs + backgrounds) for one frame |
| StagedBitmap | Cross-thread upload packet: pixel data + atlas region + mono/emoji kind |
| State | APVTS-style atomic + ValueTree bridge for cross-thread terminal state |
| VBlank | Display vertical blank — CVDisplayLink callback synced to monitor refresh |
| Atlas | 4096x4096 texture containing rasterized glyphs, shelf-packed |

---

*This document reflects the codebase as implemented. If code diverges, update this document.*
