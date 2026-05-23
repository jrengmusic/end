# RFC: `jam::Cell` Flex Layout Properties
**Status:** DRAFT  
**Component:** `jam_fonts/cell/jam_cell.h`  
**Author:** ARCHITECT  
**Scope:** `Cell.packed` bits 41–60 — no sizeof change, no Row change

---

## Motivation

END's reflow engine needs per-cell flex layout intent to drive physical line
construction on resize. Without it, the layout engine can only apply uniform
rules per row — insufficient for mixed-content rows (label + gap + value + unit).

All 5 flex properties fit in the 23 currently-reserved padding bits of
`Cell.packed`. `sizeof(Cell) == 8` is preserved. `Row` is unchanged.

---

## Bit Map — Before

```
Bit 63                                                    Bit 0
[ padding(23) | styleId(16) | wide(2) | contentTag(2) | codepoint(21) ]
  63-41          40-25         24-23     22-21            20-0
```

## Bit Map — After

```
Bit 63                                                                      Bit 0
[ free(3) | max(7) | min(7) | basis(2) | align(2) | wrap(2) | styleId(16) | wide(2) | tag(2) | cp(21) ]
  63-61     60-54    53-47    46-45       44-43      42-41      40-25         24-23     22-21    20-0
```

---

## New Fields

| Bits  | Width | Field       | Meaning                                      |
|-------|-------|-------------|----------------------------------------------|
| 42-41 |   2   | `flexWrap`  | wrap boundary intent at this cell            |
| 44-43 |   2   | `flexAlign` | self alignment within the cross axis         |
| 46-45 |   2   | `flexBasis` | preferred width mode before grow             |
| 53-47 |   7   | `flexMin`   | minimum cell width in columns, 0 = none      |
| 60-54 |   7   | `flexMax`   | maximum cell width in columns, 0x7F = none   |
| 63-61 |   3   | (reserved)  | always 0                                     |

**`grow`** has no dedicated bit — it is implicit in `contentTag == FLEX_GAP (2)`.
A `FLEX_GAP` cell carries no codepoint; it is pure elastic whitespace.

---

## `flexWrap` Values

| Value | Constant           | Meaning                                  |
|-------|--------------------|------------------------------------------|
| 0     | `FLEX_WRAP_WRAP`   | wrap permitted at this cell boundary     |
| 1     | `FLEX_WRAP_NOWRAP` | no wrap — cell stays on current line     |
| 2     | `FLEX_WRAP_REVERSE`| wrap in reverse direction                |
| 3     | (reserved)         |                                          |

**Distinct from `Row.flags.flexWrap`**, which is a VT terminal state flag set
by `Video::resolveWrapPending()`. That flag records how the row was physically
born. `Cell.flexWrap` is layout intent, authored, read by the reflow engine.

---

## `flexAlign` Values

| Value | Constant              | Meaning                   |
|-------|-----------------------|---------------------------|
| 0     | `FLEX_ALIGN_START`    | align to cross-axis start |
| 1     | `FLEX_ALIGN_CENTER`   | center on cross axis      |
| 2     | `FLEX_ALIGN_END`      | align to cross-axis end   |
| 3     | `FLEX_ALIGN_STRETCH`  | stretch to fill cross axis|

---

## `flexBasis` Values

| Value | Constant              | Meaning                                              |
|-------|-----------------------|------------------------------------------------------|
| 0     | `FLEX_BASIS_CONTENT`  | natural content width (`wcwidth` of codepoint)       |
| 1     | `FLEX_BASIS_FIXED`    | `codepoint` field holds fixed width value (cols)     |
| 2     | `FLEX_BASIS_FILL`     | fill all available space before grow distribution    |
| 3     | (reserved)            |                                                      |

`FLEX_BASIS_FIXED` repurposes the 21-bit codepoint field as a width integer
when `contentTag == FLEX_GAP`, since gap cells carry no renderable codepoint.

---

## `flexMin` / `flexMax`

- 7 bits each → range 0–127 columns per cell
- `flexMin == 0` — unconstrained minimum
- `flexMax == 0x7F` — unconstrained maximum (sentinel)
- Applied during grow distribution after basis resolution

---

## `contentTag` — Fourth Value

```cpp
static constexpr uint8_t CONTENT_FLEX { 3 };
```

Parser-originated cells always have `contentTag` 0 or 1. Authored cells with
active flex flags use `contentTag == CONTENT_FLEX (3)` for regular codepoints,
or `contentTag == FLEX_GAP (2)` for elastic whitespace. The layout engine gates
flex flag reads on `contentTag >= 2` — parser cells are never misread.

| contentTag | Origin  | Flex flags active |
|------------|---------|-------------------|
| 0          | parser  | no — defaults apply |
| 1          | parser  | no — defaults apply |
| 2          | authored| yes — FLEX_GAP (grow) |
| 3          | authored| yes — CONTENT_FLEX |

---

## New Mask/Shift Constants

```cpp
static constexpr int      flexWrapShift  { 41 };
static constexpr uint64_t flexWrapMask   { uint64_t(0x3)  << 41 };

static constexpr int      flexAlignShift { 43 };
static constexpr uint64_t flexAlignMask  { uint64_t(0x3)  << 43 };

static constexpr int      flexBasisShift { 45 };
static constexpr uint64_t flexBasisMask  { uint64_t(0x3)  << 45 };

static constexpr int      flexMinShift   { 47 };
static constexpr uint64_t flexMinMask    { uint64_t(0x7F) << 47 };

static constexpr int      flexMaxShift   { 54 };
static constexpr uint64_t flexMaxMask    { uint64_t(0x7F) << 54 };
```

---

## New Accessors

```cpp
uint8_t flexWrap()  const noexcept
{
    return static_cast<uint8_t>((packed & flexWrapMask)  >> flexWrapShift);
}

uint8_t flexAlign() const noexcept
{
    return static_cast<uint8_t>((packed & flexAlignMask) >> flexAlignShift);
}

uint8_t flexBasis() const noexcept
{
    return static_cast<uint8_t>((packed & flexBasisMask) >> flexBasisShift);
}

uint8_t flexMin() const noexcept
{
    return static_cast<uint8_t>((packed & flexMinMask) >> flexMinShift);
}

uint8_t flexMax() const noexcept
{
    return static_cast<uint8_t>((packed & flexMaxMask) >> flexMaxShift);
}
```

---

## Updated `make`

```cpp
static Cell make (uint32_t cp, uint8_t tag, uint8_t wideHint, uint16_t sid,
                  uint8_t fWrap  = FLEX_WRAP_WRAP,
                  uint8_t fAlign = FLEX_ALIGN_START,
                  uint8_t fBasis = FLEX_BASIS_CONTENT,
                  uint8_t fMin   = 0,
                  uint8_t fMax   = 0x7F) noexcept
{
    Cell c;
    c.packed = (static_cast<uint64_t>(cp)              & codepointMask)
             | (static_cast<uint64_t>(tag)             << contentTagShift)
             | (static_cast<uint64_t>(wideHint)        << wideShift)
             | (static_cast<uint64_t>(sid)             << styleIdShift)
             | (static_cast<uint64_t>(fWrap  & 0x3)   << flexWrapShift)
             | (static_cast<uint64_t>(fAlign & 0x3)   << flexAlignShift)
             | (static_cast<uint64_t>(fBasis & 0x3)   << flexBasisShift)
             | (static_cast<uint64_t>(fMin   & 0x7F)  << flexMinShift)
             | (static_cast<uint64_t>(fMax   & 0x7F)  << flexMaxShift);
    return c;
}
```

## New `makeFlexGap`

```cpp
static Cell makeFlexGap (uint16_t sid,
                          uint8_t fAlign = FLEX_ALIGN_START,
                          uint8_t fMin   = 0,
                          uint8_t fMax   = 0x7F) noexcept
{
    return make (0, FLEX_GAP, NARROW, sid,
                 FLEX_WRAP_WRAP, fAlign, FLEX_BASIS_FILL, fMin, fMax);
}
```

---

## Invariants Preserved

- `sizeof(Cell) == 8` ✓
- `std::is_trivially_copyable_v<Cell>` ✓
- `Row` — no changes ✓
- `Buffer<Row>` stride math — unaffected ✓
- Parser path — `contentTag` always 0 or 1, flex bits always 0, defaults resolve
  to `wrap / start / content / unconstrained` ✓
- `erase()` — unaffected, flex bits default to 0 ✓

---

## What Is Not Addressed

- **Layout engine** — `reflow()` / `resolveFlexLine()` implementation is a
  separate sprint. This RFC covers only the data model.
- **`Row.rowWrap`** — row-level wrap mode (WRAP / NOWRAP / WRAP_REVERSE) is
  deferred. `Row.flags` has bits 2–7 free when needed.
- **Serialisation** — `Cell.packed` is not serialised across versions; bit layout
  changes are binary-incompatible by design.
