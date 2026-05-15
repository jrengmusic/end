# RFC — Cell Transport: 8-byte Packed Cell with Shared Style and Grapheme Tables

Date: 2026-05-15
Status: Ready for COUNSELOR handoff

## Problem Statement

Cell+Grapheme transport from Processor (READER thread) to Screen (MESSAGE thread) is too heavy. Current `jam::Cell` is 16 bytes with inline `juce::Colour fg/bg` and `uint8_t style`. Parallel `jam::Buffer<Grapheme>` (29 bytes per entry) is memcpy'd every frame regardless of grapheme occupancy (<1% of cells). Total transport per 80-col row: 16×80 + 29×80 = 3,600 bytes. 120 visible rows = 432KB per frame — guaranteed L1 cache miss.

## Research Summary

### Survey of four terminal emulators

| Emulator | Cell size | Style storage | Grapheme storage |
|----------|-----------|---------------|------------------|
| Ghostty | 8B (packed u64) | Indirected via `style_id` → per-page `RefCountedSet`. Default 128, max u16. | Page-level `grapheme_map` (cell offset → u21 slice in bitmap allocator). Std 512 slots. |
| Kitty | 12B CPU + 20B GPU | Inline per-cell (`CellAttrs` u32 bitfield + 3×u32 colours). | Shared `TextCache`, indexed by u32. Starts 256, grows via realloc. Fatal at UINT32_MAX. |
| Alacritty | ≤24B | Inline per-cell (`Color fg/bg` + `Flags` u16). | `Arc<CellExtra>` per-cell, heap-allocated `Vec<char>`, lazy, unbounded. |
| WezTerm | 24B | Inline per-cell (`CellAttributes` 16B). | `TeenyString` u64: ≤7B UTF-8 inline, longer strings heap `Box`. |

Ghostty is the only emulator with a table-based style indirection. All others store attributes inline per-cell.
### END's existing architecture

- `jam::Cell` (16B, trivially copyable, `static_assert` enforced) — `jam_cell.h:79–321`.
- `jam::Grapheme` (29B, 7×u32 extras + u8 count) — `jam_grapheme.h:47–65`.
- Grid holds parallel `Buffer<Cell>` + `Buffer<Grapheme>`, ring-buffer with row-pointer swap — `Grid.h:225–228`.
- Screen holds parallel `Buffer<Cell> live` + `Buffer<Grapheme> liveGrapheme` — `Screen.h:90–91`.
- Transport: `memcpy` both buffers per row at vblank — `Screen.cpp:20–34`.
- Video writes `pen.fg`, `pen.bg`, `pen.style` inline into each Cell at `print()` — `Video.cpp:494–499`.
- SGR resolves colours immediately: `pen.fg = palette256At(code - 30)` — `VideoSGR.cpp:301`.
- `jam::Context<T>` is a process-lifetime singleton per type, ambient access via `T::getContext()`, no passing — `jam_context.h:43`.

### Renderer colour resolution (existing)

`buildArrangements` (`jam_glyph_arrangement_shape.cpp:284–285`):
```cpp
out.colour = pen.fg;
out.bg     = pen.bg;
```

`drawGlyphRuns` (`jam_text_editor_content_view.cpp:96–98`):
```cpp
const juce::Colour resolvedColour { run.colour.getAlpha() == 0
                                        ? owner.findColour (textColourId)
                                        : run.colour };
```

Two cases only. Alpha==0 → `findColour()` through LookAndFeel (Config-derived). Otherwise → use directly. This path is unchanged by this RFC.

### Config → LookAndFeel → Renderer chain

- `lua::Engine` (Config) holds theme colours, ANSI 16 overrides, font settings.
- `Terminal::LookAndFeel::setColours()` reads Config, sets JUCE colour IDs including `ansi0ColourId`–`ansi15ColourId` and `textColourId`/`backgroundColourId` — `LookAndFeel.cpp:34–99`.
- Screen extends `jam::TextEditor` (a JUCE component) — consumes LookAndFeel via `findColour()`.
- Config reload → `setColours()` updates colour IDs → next render picks up new colours via `findColour()`. No table rebuild needed.

## Principles and Rationale

### Why shrink Cell

Cell is the most copied type in the system. Every visible cell is memcpy'd from Grid to Screen every frame. Halving Cell size from 16B to 8B halves transport bandwidth. Eliminating the parallel Grapheme buffer removes 29B×cols of dead-weight transport (>99% of cells have no grapheme data).

80-col × 120-row frame: current 432KB → new 76.8KB. Borderlines L1 (64KB typical). Style and grapheme tables are shared memory — zero transport cost.

### Why table indirection

`fg` and `bg` are 4 bytes each — 8 of Cell's 16 bytes are colours that are shared across many cells (most cells in a terminal share the same fg/bg). Moving them to a deduped table trades one L1-resident array lookup per cell for 8 fewer bytes per cell transported. Net win: less memory bandwidth dominates over one cache-friendly indirection.

### Why shared `jam::Context`

Both threads need access. Video (READER) inserts entries. Renderer (MESSAGE) reads entries. Entries are immutable once inserted. The vblank Cell memcpy provides the ordering guarantee — by the time the renderer sees a `styleId`, that entry already exists in the table. No locking needed. Same thread-safety pattern as `lua::Engine::getContext()`.

### Why dynamic vector (not fixed array)

24-bit RGB SGR sequences (`ESC[38;2;R;G;Bm`) produce arbitrary colours specified by applications — unique combination count is unbounded. Ghostty starts at 128 and grows on exhaustion. No fixed capacity is deterministic. `std::vector` with `getOrInsert` grows on miss.

## Scaffold

### Cell — packed u64

```
Bit 63                                                    Bit 0
[  padding (23)  |  styleId (16)  |  wide (2)  |  contentTag (2)  |  codepoint (21)  ]
```

| Field | Bits | Range | Purpose |
|-------|------|-------|---------|
| `codepoint` | 21 | U+0000–U+10FFFF | Unicode scalar, or Grapheme vector index when `contentTag == 1` |
| `contentTag` | 2 | 0–3 | 0 = codepoint, 1 = grapheme index, 2–3 reserved |
| `wide` | 2 | 0–3 | 0 = narrow, 1 = wide, 2 = spacerTail, 3 = spacerHead |
| `styleId` | 16 | 0–65535 | Index into `Terminal::Stamp` vector |
| padding | 23 | — | Reserved for future fields |

8 bytes. Trivially copyable. `static_assert` enforced.

### Terminal::Stamp — `jam::Context<Stamp>`

Shared deduped style table. App-lifetime, owned by Main.

```cpp
struct Stamp : jam::Context<Stamp>
{
    struct Entry
    {
        juce::Colour fg;   // 4B — alpha==0 = theme default (resolved by renderer via findColour)
        juce::Colour bg;   // 4B — alpha==0 = theme default
        uint8_t flags;     // 1B — BOLD|ITALIC|UNDERLINE|STRIKE|BLINK|INVERSE|DIM

        bool operator== (const Entry& other) const noexcept
        {
            return fg == other.fg and bg == other.bg and flags == other.flags;
        }
    };

    std::vector<Entry> entries;

    uint16_t getOrInsert (const Entry& e) noexcept
    {
        const int count { static_cast<int> (entries.size()) };

        for (int i { 0 }; i < count; ++i)
        {
            if (entries[static_cast<size_t> (i)] == e)
                return static_cast<uint16_t> (i);
        }

        entries.push_back (e);
        return static_cast<uint16_t> (count);
    }

    const Entry& get (uint16_t id) const noexcept { return entries[id]; }
};
```

### Terminal::Grapheme — `jam::Context<Grapheme>`

Shared deduped grapheme cluster table. App-lifetime, owned by Main.

```cpp
struct Grapheme : jam::Context<Grapheme>
{
    struct Entry
    {
        std::array<char32_t, 8> codepoints {};  // full cluster including base
        uint8_t count { 0 };                     // valid entries (1–8)

        bool operator== (const Entry& other) const noexcept
        {
            if (count != other.count) return false;

            for (uint8_t i { 0 }; i < count; ++i)
            {
                if (codepoints[i] != other.codepoints[i]) return false;
            }

            return true;
        }
    };

    std::vector<Entry> entries;

    uint32_t getOrInsert (const Entry& e) noexcept
    {
        const int count { static_cast<int> (entries.size()) };

        for (int i { 0 }; i < count; ++i)
        {
            if (entries[static_cast<size_t> (i)] == e)
                return static_cast<uint32_t> (i);
        }

        entries.push_back (e);
        return static_cast<uint32_t> (count);
    }

    const Entry& get (uint32_t id) const noexcept { return entries[id]; }
};
```

### Impact on Video

**pen** changes from `jam::Cell` to internal tracking of colour specs and flags:

- `handleSGR()` continues to resolve colours the same way (`palette256At`, `parseExtendedColor`, `juce::Colour{}` for default). Stores resolved `juce::Colour fg/bg` and `uint8_t flags` in pen members.
- `print()` at write time: `Stamp::getContext()->getOrInsert({pen.fg, pen.bg, pen.flags})` → gets `styleId` → writes into packed Cell.
- `stamp.bg` for erase/scroll fill: resolved from `Stamp::getContext()->get(stampId).bg`.

**Grapheme path** changes:
- Currently: sets `LAYOUT_GRAPHEME` on Cell, writes extras into parallel `Buffer<Grapheme>` at same (row, col).
- New: builds full cluster (base + extras), calls `Grapheme::getContext()->getOrInsert(cluster)` → gets grapheme index → writes index into Cell's `codepoint` field with `contentTag = 1`.

### Impact on Grid

- `Buffer<Cell>` retained (cell size changes from 16B to 8B).
- `Buffer<Grapheme>` deleted — `graphemes` and `scrollOffGraphemes` members removed.
- `getGraphemeWritePointer()` / `getGraphemeReadPointer()` removed.
- Scroll-off FIFO carries 8B cells only.

### Impact on Screen

- `Buffer<Cell> live` retained (8B cells).
- `Buffer<Grapheme> liveGrapheme` deleted.
- `updateVisibleRow()`: single `memcpy` of Cell row only. No grapheme memcpy.
- `append()`: no grapheme row parameter.
- `setLiveDimensions()`: no `liveGrapheme.setSize()`.

### Impact on buildArrangements

Currently (`jam_glyph_arrangement_shape.cpp:284–285, 331`):
```cpp
out.colour = pen.fg;
out.bg     = pen.bg;
out.style  = pen.style;
```

New: extract from Stamp:
```cpp
const auto& stamp { Stamp::getContext()->get (cell.styleId()) };
out.colour = stamp.fg;
out.bg     = stamp.bg;
out.style  = stamp.flags;
```

Grapheme resolution (currently line 236–257): instead of reading parallel `graphemes[i]`, reads `Grapheme::getContext()->get(cell.codepoint())` when `cell.contentTag() == 1`.

### Impact on drawGlyphRuns

None. `run.colour` alpha==0 → `findColour(textColourId)` path unchanged. Config → LookAndFeel → `findColour()` chain untouched.

## BLESSED Compliance Checklist

- [x] **Bounds** — Cell is exactly 8 bytes, static_assert enforced. Vector indices bounded by bit width (16-bit styleId, 21-bit grapheme index).
- [x] **Lean** — Cell carries identity only. Visual attributes and grapheme data externalized to shared tables. Transport halved.
- [x] **Explicit** — `contentTag` discriminates codepoint vs grapheme index. `wide` enum replaces implicit `layout` + `width` fields. No sentinels except existing alpha==0 convention (unchanged).
- [x] **SSOT** — Each unique style combination stored once in Stamp. Each unique grapheme cluster stored once in Grapheme. No duplication across cells or sessions.
- [x] **Stateless** — Tables are append-only, entries immutable. No mutable shared state between threads. Ordering guaranteed by existing vblank memcpy barrier.
- [x] **Encapsulation** — Cell exposes accessors for packed fields (codepoint, contentTag, wide, styleId). Stamp and Grapheme expose `getOrInsert`/`get` API. Internal storage is private.
- [x] **Deterministic** — `getOrInsert` returns the same index for the same input every time. Linear scan dedup is deterministic. No hash ordering dependency.

## Open Questions

None. All decisions resolved during session.

## Handoff Notes

- Stamp::Entry stores resolved `juce::Colour` values — same types the renderer already consumes. The alpha==0 sentinel convention for theme-default colours is preserved and resolved by the existing `findColour()` path in `drawGlyphRuns`. No new resolution mechanism needed.
- Config → LookAndFeel → `findColour()` chain is completely untouched. Config reload updates ANSI 16 colours via `setAnsi16Colour()` and JUCE colour IDs via `setColours()`. New cells written after reload will get new colours via `palette256At()` (which reads the mutable `ANSI_16` array). Existing cells retain their baked colours in Stamp entries — same behavior as today with inline Cell colours.
- `Cell::erase(bg)` factory method needs adaptation — currently takes a `juce::Colour bg` and returns a Cell with inline bg. New version needs a `styleId` for the erase fill style. Video's `scrollUpAndFill`/`scrollDownAndFill` already track `stamp.bg` — will need a corresponding `stampId` for the erase cell.
- Grapheme entry holds full cluster (base + extras, up to 8 codepoints). Current `jam::Grapheme` holds only extras (7 max) with base in `Cell::codepoint`. The new design stores base in the Grapheme entry because `Cell::codepoint` becomes the grapheme index when `contentTag == 1`.
- Per-cell grapheme codepoint limit increases from 8 (1 base + 7 extras) to 8 (all in entry). Kitty allows 24 — consider expanding `Grapheme::Entry::codepoints` array if needed, but 8 covers the vast majority of real-world clusters per `jam_grapheme.h` documentation.
- `SavedCursor` in Video stores `jam::Cell pen` for DECSC/DECRC. Needs adaptation to store the new pen representation (colour specs + flags, or a stampId).
