# RFC — Missing Video Dispatch Completion (End-to-End)

Date: 2026-06-02
Status: Ready for COUNSELOR handoff

---

## Problem Statement

Full-handler reads of `terminal::Video` (SGR/OSC/DCS) confirmed a set of VT
escape sequences that reach the parser, dispatch to Video, and are then dropped
or mis-handled. Each is verified by reading the complete handler, not by grep.
This RFC completes them **end-to-end** — dispatch, storage, and rendering — so
the features are visible, not merely parsed.

Anchor case: undercurl. `ESC[4:3m` is presently stored as UNDERLINE+ITALIC
(`VideoSGR.cpp:171`), so nvim/LSP diagnostic squiggles render wrong.

Scope:
1. SGR underline **styles** — `4:2` double, `4:3` curly, `4:4` dotted, `4:5` dashed
2. SGR underline **color** — `58` / `59`
3. SGR **overline** (`53`/`55`) and **super/subscript** (`73`/`74`/`75`)
4. OSC **4** (palette set/query), **10**/**11** (default fg/bg set/query)
5. **DECRQSS** — DCS `$q` request-status-string

---

## Resolved Decisions (locked with ARCHITECT this session)

| # | Decision | Resolution |
|---|---|---|
| D1 | Renderer scope | **Full end-to-end** — dispatch + storage + render |
| D2 | Optional SGR | **Both in** — overline AND super/subscript |
| D3 | OSC 4 range | **Full 0–255 mutable** (256-slot override table) |
| D4 | Palette scope | **Global** — extend the existing `ANSI_16` global model to `PALETTE_256` |
| D5 | Storage layout | **Widen `jam::StampEntry`, not `jam::Char`** — BLESSED Bounds (Char stays 8 bytes, `jam_char.h:177`); the SharedResource design carries new attributes at zero per-cell cost |
| D6 | Names | Approved, convention-conformant (see Scaffold). Derived from NAMES.md + `jam_stamp.h`/`jam_char.h`/`Identifier.h`/`Palette.h` established patterns |

No open questions remain.

---

## Research Summary

Grounded in complete-handler and render-path reads. Citations `path:line`.

### A. SGR dispatch — `VideoSGR.cpp` (full read)
- `handleSGR()` (`:210`) iterates `params.values` **independently**; never consults
  `CSI::isSubSeparator()` (`CSI.h:371`). `ESC[4:3m` → values `[4,3]`, subSeparator
  bit 1 set → read as `4`(UNDERLINE) then `3`(ITALIC). **Curly renders as
  underline+italic.**
- `parseExtendedColor()` (`:118`) already handles `;` and `:` forms for 38/48 —
  reusable for 58.
- No 53/55/58/59/73/74/75 arms.

### B. SGR storage — `jam_stamp.h` (full read)
- `StampEntry { juce::Colour fg; juce::Colour bg; uint8_t flags; }` (`:6-10`);
  flags 7/8 bits used (`:42-48`). One `UNDERLINE` bit — no style field, no
  underline-color field.
- `styleId` is the interned index (`jam_char.h:134`). Widening `StampEntry` does
  not change `jam::Char` (8-byte invariant `jam_char.h:177` holds).

### C. OSC dispatch — `VideoOSC.cpp` (full read)
- `applyOSC()` switch (`:338-358`): 0/2, 7, 8, 9, 12, 52, 112, 133, 777, 1337.
  **No 4, 10, 11.** OSC 12 fires `id::cursorColor` (`Identifier.h:282`) — the
  app-resolution event model to mirror.

### D. DCS dispatch — `VideoDCS.cpp` (full read)
- `applyDCSPayload()` (`:91`) fires `id::dcsPayloadComplete` unconditionally →
  `Skit` image. **No DECRQSS.**
- Video already owns the device-response path: `responseBuf` (`Video.h:562`),
  `sendResponse()` (`Video.h:1654`), `flushResponses()` (`Video.h:258`),
  `reportCursorPosition`/`reportDeviceAttributes` (`Video.h:1471,1486`).
  `storeDCSHeader` keeps only `dcsFinalByte` (`VideoDCS.cpp:74`) — the intermediate
  is discarded; DECRQSS needs it.

### E. Render path — `jam_glyph_graphics_cells.cpp` (full read)
- Decoration is a **CPU BGRA pixel pass** (`:128-199`). Reads `styles[i]`
  (`uint8_t` per-cell flags). `Stamp::UNDERLINE` → solid horizontal run at
  `positions[i].y + 1`, thickness `max(1, physCellHeight/14)` (`:133,144-170`);
  `Stamp::STRIKE` → solid run at `positions[i].y - physBaseline/3` (`:173-199`).
  Color is the glyph fg `colour` (`:129-132`).
- `styles[i]` is `uint8_t` — widening `StampEntry::flags` to `uint16_t` ripples to
  this per-cell render array.
- Underline color: the pass currently has only fg `colour`; SGR 58 requires the
  per-cell `underline` colour to be threaded here.
- Super/subscript is **not** a decoration concern — it requires scaling and
  repositioning the **glyph quad** in the composite pass (`:117-124`,
  `compositeMonoGlyph`), upstream of decoration. This is the larger renderer change.
- Metrics available: `underlinePosition/Thickness`, `strikeoutPosition/Thickness`
  (`jam_typeface_metrics.cpp:174-200`).

---

## Principles and Rationale

### P1. Storage widens `StampEntry`, never `Char` (D5)
BLESSED **Bounds** mandates the 8-byte `jam::Char` invariant
(`static_assert jam_char.h:177`). Underline style (3 bits), underline color
(a `juce::Colour`), overline, super/subscript all live in the **interned**
`StampEntry`; the cell still stores a 16-bit `styleId`. This is the SharedResource
design working as intended — not a decision to debate, a contract to honor.
Rejected: packing into Char padding (breaks the value-type/size discipline) and a
parallel decoration table (reinvents `Stamp` — the jam_tui-sidecar anti-pattern).

### P2. Sub-parameter disambiguation via existing `CSI::isSubSeparator` (BLESSED Explicit)
`handleSGR` must read `CSI::isSubSeparator()` to separate `4:3` (curly, one SGR)
from `4;3` (underline+italic, two SGRs). The bit data already exists in `CSI`
(`CSI.h:116,371`); only `handleSGR` ignores it.

### P3. DECRQSS reuses the existing response path (BLESSED Lean/SSOT)
`reportStatusString` builds the reply from current Video state (pen→SGR,
`cursorShape`→DECSCUSR, `scrollTop`/`scrollBottom`→DECSTBM) and emits via
`sendResponse()` — the same buffer CPR/DA already use. No new transport.

### P4. OSC query/response via the event pattern (BLESSED Encapsulation)
Set forms mutate palette/theme state; query forms (`OSC 4;n;?`, `OSC 10;?`,
`OSC 11;?`) need the resolved color, owned by `Palette`/`AppModel`, not Video.
Video fires an event (as OSC 12 does) → app resolves → `writeToHost` reply.

### P5. Global palette extension (D4)
`ANSI_16` is already global (`Palette.h:38`). OSC 4 set extends it to a global
256-slot `PALETTE_256` seeded with the current algorithmic cube/gray defaults.
Consistent with the existing model; cross-pane bleed is accepted per D4.

### P6. Render: extend the decoration pass; super/subscript in the glyph pass
Underline styles, overline, and underline color are added to the existing CPU
decoration pass (P-local, mechanical). Super/subscript is a glyph-quad scale +
vertical offset in the composite pass — the one invasive renderer change, scoped
explicitly.

---

## Scaffold

All names below are ARCHITECT-approved and convention-conformant.

### S1. `jam_stamp.h`

```cpp
struct StampEntry
{
    juce::Colour fg;
    juce::Colour bg;
    juce::Colour underline;          // SGR 58/59; alpha==0 => follow fg
    uint16_t     flags { 0 };         // widened from uint8_t

    bool operator== (const StampEntry& o) const noexcept
    {
        return fg == o.fg and bg == o.bg and underline == o.underline and flags == o.flags;
    }
    struct Hash { size_t operator() (const StampEntry&) const noexcept; }; // include underline + uint16 flags
};

struct Stamp : SharedResource<Stamp, StampEntry>
{
    using Entry = StampEntry;

    static constexpr uint16_t BOLD        { 0x0001 };
    static constexpr uint16_t DIM         { 0x0002 };
    static constexpr uint16_t ITALIC      { 0x0004 };
    static constexpr uint16_t STRIKE      { 0x0008 };
    static constexpr uint16_t BLINK       { 0x0010 };
    static constexpr uint16_t INVERSE     { 0x0020 };
    static constexpr uint16_t OVERLINE    { 0x0040 };
    static constexpr uint16_t SUPERSCRIPT { 0x0080 };
    static constexpr uint16_t SUBSCRIPT   { 0x0100 };

    // 3-bit underline-style field, bits [11:9]; replaces the old single UNDERLINE bit
    static constexpr int      underlineStyleShift { 9 };
    static constexpr uint16_t underlineStyleMask  { uint16_t (0x7) << 9 };

    static constexpr uint16_t UNDERLINE_NONE   { 0 };
    static constexpr uint16_t UNDERLINE_SINGLE { 1 };
    static constexpr uint16_t UNDERLINE_DOUBLE { 2 };
    static constexpr uint16_t UNDERLINE_CURLY  { 3 };
    static constexpr uint16_t UNDERLINE_DOTTED { 4 };
    static constexpr uint16_t UNDERLINE_DASHED { 5 };
};
```

The old `UNDERLINE 0x04` single-bit constant is removed. All flag read sites
migrate (see Handoff).

### S2. `Video.h` — pen state
```cpp
juce::Colour penUnderline {};        // SGR 58 underline color
uint16_t     penFlags { 0 };          // widened from uint8_t
uint8_t      dcsIntermediateByte { 0 };  // retained by storeDCSHeader for DECRQSS
```
`currentStyleId()` (`Video.h:459`) interns `{ penFg, penBg, penUnderline, penFlags }`.

### S3. `VideoSGR.cpp` — dispatch
```cpp
else if (code == 4)
{
    if (i + 1 < params.count and params.isSubSeparator (i + 1))
    { penFlags = setUnderlineStyle (penFlags, mapUnderlineSub (params.values.at (i + 1))); ++i; }
    else
    { penFlags = setUnderlineStyle (penFlags, jam::Stamp::UNDERLINE_SINGLE); }
    penStyleDirty = true;
}
else if (code == 24) { penFlags = setUnderlineStyle (penFlags, jam::Stamp::UNDERLINE_NONE); penStyleDirty = true; }
else if (code == 53) { penFlags |= jam::Stamp::OVERLINE;  penStyleDirty = true; }
else if (code == 55) { penFlags &= ~jam::Stamp::OVERLINE; penStyleDirty = true; }
else if (code == 58) { penUnderline = parseExtendedColor (params, i); penStyleDirty = true; }
else if (code == 59) { penUnderline = {};                              penStyleDirty = true; }
else if (code == 73) { penFlags |= jam::Stamp::SUPERSCRIPT; penFlags &= ~jam::Stamp::SUBSCRIPT;   penStyleDirty = true; }
else if (code == 74) { penFlags |= jam::Stamp::SUBSCRIPT;   penFlags &= ~jam::Stamp::SUPERSCRIPT; penStyleDirty = true; }
else if (code == 75) { penFlags &= ~(jam::Stamp::SUPERSCRIPT | jam::Stamp::SUBSCRIPT); penStyleDirty = true; }
```
File-static helpers:
```cpp
static uint16_t mapUnderlineSub (uint16_t n) noexcept
{ return n <= jam::Stamp::UNDERLINE_DASHED ? n : jam::Stamp::UNDERLINE_SINGLE; }

static uint16_t setUnderlineStyle (uint16_t flags, uint16_t style) noexcept
{
    return (flags & ~jam::Stamp::underlineStyleMask)
         | ((style << jam::Stamp::underlineStyleShift) & jam::Stamp::underlineStyleMask);
}
```
`applySGRStyle`/`resetSGRStyle` drop their old `case 4`/`case 24` arms and take
`uint16_t&`.

### S4. `VideoOSC.cpp` — dispatch
```cpp
case 4:   handleOscPalette (data, dataLength);          break;
case 10:  handleOscForegroundColor (data, dataLength);  break;
case 11:  handleOscBackgroundColor (data, dataLength);  break;
```
Handlers parse `rgb:RR/GG/BB` / `#RRGGBB` for set; `?` fires
`id::paletteColor` / `id::foregroundColor` / `id::backgroundColor` for app
resolution + `writeToHost` reply (mirrors `handleOscCursorColor` → `id::cursorColor`).
OSC 4 set writes `PALETTE_256` via `setPaletteColour (index, colour)`.

### S5. `VideoDCS.cpp` — DECRQSS split
```cpp
void Video::storeDCSHeader (const CSI&, const uint8_t* inter, uint8_t interCount, uint8_t finalByte) noexcept
{
    dcsFinalByte        = finalByte;
    dcsIntermediateByte = (interCount > 0) ? inter[0] : 0;
}

void Video::applyDCSPayload (const uint8_t* data, int length) noexcept
{
    if (dcsIntermediateByte == '$' and dcsFinalByte == 'q') { reportStatusString (data, length); return; }
    if (events.contains (id::dcsPayloadComplete)) events.get (id::dcsPayloadComplete, data, length);
}

void Video::reportStatusString (const uint8_t* data, int length) noexcept
{
    char buf[64];
    if      (length == 1 and data[0] == 'm')                       formatSgrReport (buf, sizeof buf);
    else if (length == 1 and data[0] == 'r')
        std::snprintf (buf, sizeof buf, "\x1bP1$r%d;%dr\x1b\\", scrollTop.value + 1,
                       effectiveScrollBottom (visibleRows).value + 1);
    else if (length == 2 and data[0] == ' ' and data[1] == 'q')
        std::snprintf (buf, sizeof buf, "\x1bP1$r%d q\x1b\\", cursorShapeParam());
    else
        std::snprintf (buf, sizeof buf, "\x1bP0$r\x1b\\");
    sendResponse (buf);
}
```

### S6. `Palette.h` — 256-slot global override
```cpp
inline std::array<juce::Colour, 256> PALETTE_256 { /* seeded: ANSI_16 + cube + gray defaults */ };
inline void         setPaletteColour (int index, juce::Colour c) noexcept { PALETTE_256[(size_t) index] = c; }
inline juce::Colour palette256At     (int index) noexcept { return PALETTE_256[(size_t) index]; } // now table-backed
```
Seed `PALETTE_256` at init from the existing `ANSI_16`/`COLOR_CUBE`/`GRAY_RAMP`
formulas (`Palette.h:38,117,191`); `setAnsi16Colour` becomes `setPaletteColour`
for 0–15.

### S7. Render — decoration pass (`jam_glyph_graphics_cells.cpp`)
`styles` array widened to `uint16_t`. Replace the single underline branch
(`:144-171`) with a style switch; add overline; select underline color.
```cpp
const uint16_t style { styles[i] };
const uint16_t ulStyle { uint16_t ((style & Stamp::underlineStyleMask) >> Stamp::underlineStyleShift) };

if (ulStyle != Stamp::UNDERLINE_NONE)
    drawUnderline (targetData, ulStyle, underlineColourFor (i),   // penUnderline-or-fg per cell
                   cellX, cellW, ulY, decorationThickness, physCellHeight);
if ((style & Stamp::OVERLINE) != 0)
    drawSolidRun  (targetData, colour, cellX, cellW, /*olY=*/cellTopY, decorationThickness);
if ((style & Stamp::STRIKE) != 0)
    drawSolidRun  (targetData, colour, cellX, cellW, stY, decorationThickness);
```
`drawUnderline` dispatches on style:
- SINGLE → one solid run (current behavior)
- DOUBLE → two solid runs, gap `decorationThickness`
- CURLY  → per-x sine offset, period `physCellWidth/2`, amplitude `decorationThickness`
- DOTTED → solid run, emit `dx % 2 == 0`
- DASHED → solid run, emit `(dx / (physCellWidth/3)) % 2 == 0`

Per-cell underline colour: the render input must carry `underline` (resolved from
`styleId → StampEntry.underline`, falling back to fg when alpha==0).

### S8. Render — super/subscript (glyph composite pass, `:117-124`)
When `SUPERSCRIPT`/`SUBSCRIPT` set, scale the glyph quad by `subScale` (≈ 0.65)
and offset vertically: superscript toward ascent, subscript toward baseline.
Applied where `quad` is built before `compositeMonoGlyph`/`compositeEmojiGlyph`.
Requires the per-cell flags (already in `styles[]`) at composite time.

---

## BLESSED Compliance Checklist
- [x] **Bounds** — `jam::Char` 8 bytes preserved (`jam_char.h:177`); style growth confined to interned `StampEntry`; DECRQSS uses fixed `responseBuf`.
- [x] **Lean** — no new transport (DECRQSS reuses `sendResponse`); no parallel style table; render reuses the existing decoration pass.
- [x] **Explicit** — `4:3` vs `4;3` via `CSI::isSubSeparator`; named underline-style constants; no magic SGR codes.
- [x] **SSOT** — one style record (`StampEntry`), one palette table (`PALETTE_256`), one response buffer.
- [x] **Stateless** — `jam::Char` unchanged value type; new pen fields are reader-thread working state.
- [x] **Encapsulation** — Video fires events for app-owned color resolution; renderer reads style via `styleId`, not back-channels.
- [x] **Deterministic** — SGR parse is branch logic; DECRQSS output is a pure function of Video state; decoration draw is deterministic per style.

---

## Handoff Notes

1. **Sequence (foundation → app → render):** `jam_stamp.h` (widen StampEntry) →
   `Palette.h` (PALETTE_256) → `Video` (SGR/OSC/DCS dispatch) →
   `jam_glyph_graphics_cells.cpp` + glyph composite (render). The Stamp widening
   is the prerequisite; do it first, fix compiler breaks (CAROL §10).
2. **`uint8_t flags` → `uint16_t` ripples.** Every read of Stamp flags migrates:
   the removed single `UNDERLINE 0x04` bit becomes `underlineStyleMask`; the
   per-cell render `styles[]` array (`jam_glyph_graphics_cells.cpp:137`) widens.
   Compiler surfaces the sites.
3. **Per-cell underline colour must reach the renderer.** Today the decoration
   pass has only fg `colour`. Thread `StampEntry.underline` (resolved via
   `styleId`) into the render input alongside `styles[]`.
4. **Super/subscript is the one heavy item (S8).** It is glyph-quad scaling +
   offset in the composite pass, not a decoration line. Size it as the larger
   sub-task; the other four SGR features are mechanical.
5. **Librarian before Engineer** (CAROL delegation): confirm JUCE's color parsing
   for OSC `rgb:`/`#RRGGBB` (`juce::Colour::fromString` vs manual hex) before
   hand-rolling in S4.
6. **Extraction alignment** (RFC-jam-terminal-extraction): the OSC 4/10/11 query
   events become protected virtual hooks on `jam::terminal::Video`; SGR/DCS logic
   belongs in the base. If this RFC lands first, use the current `Function::Map`
   events — migration is mechanical.
7. **Build/validation** — ARCHITECT-only builds (MEMORY.md). No cmake/ninja/make/xcodebuild.

