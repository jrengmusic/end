# PLAN: Cell Type System — Pixel = juce, Cell = jam

**DEBT:** DEBT-20260529T020100, DEBT-20260529T020000 (Winsize subset)
**Date:** 2026-05-29
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE

## Overview

Formalize the cell/pixel type system. Eliminate `jam::Bounds`. Pixel domain uses juce types exclusively (`juce::Point<int>`, `juce::Rectangle<int>`). Cell domain uses jam types exclusively (`Cell::Point`, `Cell::Rectangle`). Both cell types mirror their juce counterparts' API. Conversion is `fromPixel()` / `toPixel()` with explicit `int cellWidth, int cellHeight` parameters. All unpaired `cell cols, cell rows` function arguments become `Cell::Rectangle`. All separate dimension Parameters become packed single-atomic values. `terminal::Winsize` packs cell dims + pixel dims into one uint64.

## Convention (LOCKED)

```
PIXEL DOMAIN (juce only)              CELL DOMAIN (jam only)
juce::Point<int>      position        Cell::Point        pack -> int32 (16+16)
juce::Rectangle<int>  region/size     Cell::Rectangle    pack -> int64 (16x4)
                                      cell (Cell::Unit)  scalar

Point     = position
Rectangle = dimension / region (position zeroed when dimension-only)
```

- `fromPixel(juce type, int cellWidth, int cellHeight)` — static factory, pixel -> cell
- `toPixel(int cellWidth, int cellHeight)` — member, cell -> pixel
- `fromJuce` / `toJuce` / `toLogical` / `totalPixels` — removed
- `jam::Bounds` — eliminated. Useful methods absorbed into `Cell::Rectangle`.
- `terminal::Winsize` — terminal-domain struct (mirrors POSIX `struct winsize`), packs cell dims (cols, rows) + pixel dims (w, h) into uint64. `Parameter<int64_t>`. Display sole author.

## Steps

### Step 1: Extend Cell::Point API (jam)

**Scope:** `jam/jam_graphics/detail/jam_cell_point.h`

**Action:** Mirror juce::Point<int> API. Add pack/unpack. Replace conversion methods.

**Add — mirrored from juce::Point<int>:**
- `isOrigin()` — true if (0, 0)
- `setX(Unit)`, `setY(Unit)`, `setXY(Unit, Unit)`, `addXY(Unit, Unit)` — mutable setters
- `withX(Unit)`, `withY(Unit)` — immutable builders
- `translated(Unit dx, Unit dy)` — offset copy
- `operator+=(Point)`, `operator-=(Point)` — in-place arithmetic
- `operator-()` — negation

**Add — new:**
- `pack() -> int` — `(x << 16) | (y & 0xFFFF)`, same bit layout as old Bounds
- `static unpack(int) -> Point`
- `static fromPixel(juce::Point<int> pixel, int cellWidth, int cellHeight) -> Point`
- `toPixel(int cellWidth, int cellHeight) -> juce::Point<int>`

**Remove:**
- Constructor `Point(const jam::Bounds& cellSize, juce::Point<ValueType> pixel)` — replaced by `fromPixel()`
- `toLogical(const jam::Bounds&)` — replaced by `toPixel()`
- `fromJuce(juce::Point<int>)` — replaced by direct construction `Point(cell(x), cell(y))`
- `toJuce()` — replaced by `juce::Point<int> { x, y }` at call sites
- `totalPixels()` — absorbed into `Cell::Rectangle::toPixel()`

**Not mirrored (float/geometry, inapplicable to int cell grid):**
- Distance methods, angle methods, rotation, circumference, dot product, AffineTransform, float conversions, isFinite, toString

**Validation:** Compiles with all downstream call sites updated in subsequent steps. API parity with juce::Point<int> for all integer-applicable methods.

### Step 2: Extend Cell::Rectangle API (jam)

**Scope:** `jam/jam_graphics/detail/jam_cell_rectangle.h`

**Action:** Mirror juce::Rectangle<int> API. Add pack/unpack. Replace conversion methods. Absorb Bounds utilities.

**Add — mirrored from juce::Rectangle<int>:**

Position/center queries:
- `getPosition() -> Point` — top-left as Cell::Point
- `getCentre() -> Point`
- `getCentreX() -> Unit`, `getCentreY() -> Unit`
- `getTopLeft() -> Point`, `getTopRight() -> Point`, `getBottomLeft() -> Point`, `getBottomRight() -> Point`

Mutable setters:
- `setX(Unit)`, `setY(Unit)`, `setPosition(Unit, Unit)`, `setPosition(Point)`
- `setWidth(Unit)`, `setHeight(Unit)`, `setSize(Unit, Unit)`
- `setBounds(Unit x, Unit y, Unit w, Unit h)`
- `setLeft(Unit)`, `setTop(Unit)`, `setRight(Unit)`, `setBottom(Unit)` — edge setters that preserve opposite edge

Edge-adjusting immutable builders:
- `withZeroOrigin()` — copy with position (0, 0)
- `withLeft(Unit)`, `withTop(Unit)`, `withRight(Unit)`, `withBottom(Unit)` — edge builders preserving opposite edge
- `withRightX(Unit)`, `withBottomY(Unit)` — set right/bottom edge, adjust width/height

Expand/reduce:
- `expanded(Unit dx, Unit dy)` — opposite of reduced
- `expanded(Unit d)` — uniform expand
- `reduced(Unit d)` — uniform reduce (single-param overload, complements existing `reduced(Unit dx, Unit dy)`)

Set operations:
- `intersects(Rectangle)` — intersection test
- `getIntersection(Rectangle)` — intersection result
- `getUnion(Rectangle)` — union result
- `constrainedWithin(Rectangle)` — clamp to bounds

Translation operators:
- `operator+(Point)`, `operator-(Point)` — translate by point
- `operator+=(Point)`, `operator-=(Point)` — in-place translate

**Add — absorbed from jam::Bounds:**
- `isValid()` — already planned; both dims > 0
- `getArea()` — already planned; width * height
- `static getRelativeScale(juce::Rectangle<int> outer, juce::Rectangle<int> inner, float maxScale = 1.0f) -> float` — pixel inputs, returns scale factor

**Add — new:**
- `pack() -> int64` — `(int64(x) << 48) | (int64(y) << 32) | (int64(width) << 16) | (int64(height) & 0xFFFF)`, 16 bits per field
- `static unpack(int64) -> Rectangle`
- `static fromPixel(juce::Rectangle<int> pixel, int cellWidth, int cellHeight) -> Rectangle` — floor division
- `static fromPixelCeiling(juce::Rectangle<int> pixel, int cellWidth, int cellHeight) -> Rectangle` — ceiling division
- `toPixel(int cellWidth, int cellHeight) -> juce::Rectangle<int>` — multiply

**Remove:**
- Constructor `Rectangle(const jam::Bounds& cellSize, juce::Rectangle<int> pixel)` — replaced by `fromPixel()`
- Constructor `Rectangle(const jam::Bounds& cellSize, juce::Rectangle<int> pixel, bool ceiling)` — replaced by `fromPixelCeiling()`
- `toLogical(const jam::Bounds&)` — replaced by `toPixel()`
- `fromJuce(juce::Rectangle<int>)` — replaced by direct construction `Rectangle(cell(x), cell(y), cell(w), cell(h))`
- `toJuce()` — replaced by `juce::Rectangle<int> { x, y, width, height }` at call sites

**Not mirrored (float/geometry, inapplicable):**
- AffineTransform, proportional queries, aspect ratio, float scaling operators, Range accessors, Line intersection, type conversions, toString, findAreaContainingPoints, intersectRectangles (static), getRelativePoint, largestIntegerWithin/smallestIntegerContainer

**Validation:** Full juce::Rectangle<int> API parity for all integer-applicable methods. Compiles with downstream updates.

### Step 3: Update jam::Font (jam)

**Scope:** `jam/jam_graphics/fonts/font/jam_font.h`, `jam/jam_graphics/fonts/font/jam_font.cpp`

**Action:** Replace `jam::Bounds bounds` member with `int cellWidth { 0 }` and `int cellHeight { 0 }`. Update `resolveMetrics()` to write the two ints. Update all read sites of `font.bounds.width` -> `font.cellWidth`, `font.bounds.height` -> `font.cellHeight`.

**Call sites in jam (from Pathfinder):**
- `jam_glyph_arrangement.cpp:99` — `cellSize = font.bounds` -> assign from font members
- `jam_glyph_arrangement.cpp:105,245` — `font.bounds.width`, `font.bounds.height` -> `font.cellWidth`, `font.cellHeight`

**Call sites in end (from Pathfinder):**
- `MainComponent.cpp:159` — `jam::Cell::Point::totalPixels<int>(cols, rows, font.bounds)` -> `Cell::Rectangle(cell(cols), cell(rows)).toPixel(font.cellWidth, font.cellHeight)` (returns juce::Rectangle, caller uses getWidth/getHeight)
- `Display.cpp:78-79` — `font.bounds.width`, `font.bounds.height` -> `font.cellWidth`, `font.cellHeight`
- `Display.cpp:83` — `mouse.setCellSize(font.bounds.width, font.bounds.height)` -> `mouse.setCellSize(font.cellWidth, font.cellHeight)`
- `Panes.cpp` — DPI-scaled: `font.bounds.width`, `font.bounds.height` -> `font.cellWidth`, `font.cellHeight`
- `AppState.cpp` — anywhere font.bounds is read

**Validation:** No `font.bounds` references remain. No `jam::Bounds` in Font.h.

### Step 4: Update GlyphArrangement and GlyphGraphics (jam)

**Scope:** `jam/jam_graphics/fonts/font/glyph/jam_glyph_arrangement.h`, `jam_glyph_arrangement.cpp`, `jam_glyph_graphics.h`, `jam_glyph_graphics.cpp`

**Action:**
- `jam_glyph_arrangement.h:465` — `jam::Bounds cellSize` member -> `int cellWidth { 0 }; int cellHeight { 0 };`
- `jam_glyph_arrangement.cpp:99,239` — `cellSize = font.bounds` -> assign from font.cellWidth/cellHeight
- `jam_glyph_graphics.h:66` — `push(Bounds viewport, ...)` -> `push(juce::Rectangle<int> viewport, ...)`. Winsize is pixel dims — juce type.
- `jam_glyph_graphics.h:163` — `drawGlyphs(..., Bounds cellSize, int baseline)` -> `drawGlyphs(..., int cellWidth, int cellHeight, int baseline)`
- Update all implementations and call sites within jam.

**Call site in end:**
- `jam_text_editor_content_view.cpp:39` — `owner.glyphGraphics.push(jam::Bounds { owner.getWidth(), owner.getHeight() }, clip)` -> `owner.glyphGraphics.push(juce::Rectangle<int> { 0, 0, owner.getWidth(), owner.getHeight() }, clip)`

**Validation:** No `jam::Bounds` in glyph headers or implementations.

### Step 5: Update Resizer (jam)

**Scope:** `jam/jam_core/buffer/jam_resizer.h`

**Action:** Resizer stores pending dimensions. These are cell dimensions (cols, rows) in terminal usage. Replace `jam::Bounds pendingBounds` with two plain ints (pendingWidth, pendingHeight) since Resizer is generic — it doesn't know if it's storing cell or pixel values. The variadic `set()` template and `timerCallback()` already pass width/height as separate args to triggers.

- Remove `jam::Bounds pendingBounds` -> `int pendingWidth { 0 }; int pendingHeight { 0 };`
- `set()`: `pendingWidth = first arg; pendingHeight = second arg;` (extract from variadic)
- `timerCallback()`: `triggers.get(stop, pendingWidth, pendingHeight);`
- Remove `getTargetValue()` -> `int getTargetWidth()`, `int getTargetHeight()` (or remove if unused — check call sites)

**Validation:** No `jam::Bounds` in Resizer.

### Step 6: Remove jam_bounds.h (jam)

**Scope:** `jam/jam_core/jam_bounds.h`, `jam_core` module header that includes it

**Action:** Delete `jam_bounds.h`. Remove its include from the jam_core module header (likely `jam_core.h` or `jam_core.cpp`). Verify zero remaining references to `jam::Bounds` across both jam and end codebases.

**Validation:** `grep -r "jam::Bounds\|jam_bounds" jam/ end/` returns zero hits.

### Step 7: Implement terminal::Winsize (end)

**Scope:** New file `Source/terminal/Winsize.h`

**Action:** Create `terminal::Winsize` — purpose-built struct packing cell dims + pixel dims into single uint64. Mirrors POSIX `struct winsize`.

```cpp
struct Winsize
{
    int cols        { 0 };  ///< Terminal width in cell columns.
    int rows        { 0 };  ///< Terminal height in cell rows.
    int pixelWidth  { 0 };  ///< Viewport width in physical pixels.
    int pixelHeight { 0 };  ///< Viewport height in physical pixels.

    int64_t pack() const noexcept;         // 4 x 16 bits
    static Winsize unpack (int64_t v) noexcept;
    bool isValid() const noexcept;         // cols > 0 and rows > 0

    Cell::Rectangle toCellRect() const noexcept;  // Rectangle(cell(cols), cell(rows))
    cell getCols() const noexcept;
    cell getRows() const noexcept;
};
```

Display constructs Winsize from contentBounds + font metrics, packs into single Parameter<int64_t>. All consumers unpack and read what they need (cell dims or pixel dims or both).

**Validation:** Trivially copyable. 16-bit fields support up to 65535 (sufficient for terminal: max ~500 cols, ~200 rows, ~8K pixels).

### Step 8: Implement Parameter<int64_t> (jam)

**Scope:** `jam/jam_core/parameter/` (wherever Parameter specializations live)

**Action:** Add `Parameter<int64_t>` specialization following the existing `Parameter<int>` pattern. Uses `std::atomic<int64_t>` for lock-free cross-thread transfer. Flushes to `juce::var(int64)` on ValueTree property.

**Validation:** `std::atomic<int64_t>::is_always_lock_free` — verify on target platforms (true on x86-64 and ARM64).

### Step 9: Migrate end function signatures (end)

**Scope:** All files with unpaired `cell cols, cell rows` arguments.

**Action:** Replace `cell cols, cell rows` parameter pairs with `Cell::Rectangle` (dimension-only, using `(Unit w, Unit h)` constructor). Callers construct `Cell::Rectangle(cols, rows)`.

**Signatures to change (14 total):**
1. `Processor::Processor(State&, cell cols, cell rows, ...)` -> `Processor(State&, Cell::Rectangle dims, ...)`
2. `Processor::prepare(cell cols, cell rows)` -> `prepare(Cell::Rectangle dims)`
3. `Video::Video(cell cols, cell rows, events)` -> `Video(Cell::Rectangle dims, events)`
4. `Video::setWinsize(cell cols, cell rows)` -> `setWinsize(Cell::Rectangle dims)`
5. `TTY::open(cell cols, cell rows, ...)` -> `open(Cell::Rectangle dims, ...)`
6. `TTY::setWinsize(cell cols, cell rows, int pw, int ph)` -> `setWinsize(terminal::Winsize vp)` (carries both cell and pixel dims)
7. `UnixTTY::setWinsize` — override matches base
8. `WindowsTTY::setWinsize` — override matches base
9. `Processor::startTTY(..., cell cols, cell rows)` -> `startTTY(..., Cell::Rectangle dims)`
10. `Session::create(cwd, cell cols, cell rows, ...)` -> `create(cwd, Cell::Rectangle dims, ...)`
11. `Session::create(cell cols, cell rows, cwd, ...)` -> `create(Cell::Rectangle dims, cwd, ...)`
12. `Session constructors` — both overloads
13. `Tabs::addNewTab(cell cols, cell rows)` -> `addNewTab(Cell::Rectangle dims)`
14. `MessageOverlay::showResize(cell cols, cell rows, ...)` -> `showResize(Cell::Rectangle dims, ...)`

**Nexus signatures (separate int cols/rows):**
15. `Link::sendResize(int cols, int rows)` -> `sendResize(Cell::Rectangle dims)`
16. `Link::sendCreateSession(...)` — similar
17. `Daemon::attachSession(...)` — similar
18. `Nexus::create(...)` — similar

**Internal member access:** Where implementations need raw values, use `dims.getWidth()` (cols) and `dims.getHeight()` (rows). Where int is needed: `dims.getWidth().value`.

**Validation:** No remaining `cell cols, cell rows` paired parameters in any function signature.

### Step 10: Migrate end conversion call sites (end)

**Scope:** All manual pixel<->cell arithmetic and Bounds usage in end.

**Action:**

**Mouse.cpp (~12 call sites):**
- `jam::Cell::Point(jam::Bounds { physCellWidth, physCellHeight }, juce::Point<int> { event.x, event.y })` -> `Cell::Point::fromPixel(juce::Point<int> { event.x, event.y }, physCellWidth, physCellHeight)`

**Display.cpp:**
- Line 170-172: manual `contentBounds.getWidth() / cellW` division -> construct `terminal::Winsize` from contentBounds + font metrics
- Line 159-160: separate `state.setValue(jam::ID::width, ...)` / `state.setValue(jam::ID::height, ...)` -> single Winsize Parameter write
- Line 172-179: `jam::Bounds cellDims { cols, rows }; cellDims.pack()` -> Winsize.pack() written to single Parameter

**VideoCSI.cpp:**
- Line 204: `jam::Cell::Point::totalPixels<int>(cols, visibleRows, jam::Bounds{cellWidth, cellHeight})` -> `Cell::Rectangle(cols, visibleRows).toPixel(cellWidth, cellHeight)` then extract width/height

**Skit.cpp (~3 call sites):**
- `jam::Cell::Rectangle(jam::Bounds { cellW, cellH }, juce::Rectangle<int> { ... }, true)` -> `Cell::Rectangle::fromPixelCeiling(juce::Rectangle<int> { ... }, cellW, cellH)`
- `jam::Bounds::getRelativeScale(...)` -> `Cell::Rectangle::getRelativeScale(outer, inner, maxScale)`

**MainComponent.cpp:**
- Line 526: `jam::Cell::Rectangle(font.bounds, content)` -> `Cell::Rectangle::fromPixel(content, font.cellWidth, font.cellHeight)`
- Line 159: `totalPixels` -> `Cell::Rectangle(...).toPixel(...)` as in Step 3

**Panes.cpp:**
- Line 71: `jam::Cell::Rectangle(jam::Bounds { physCellW, physCellH }, juce::Rectangle<int> { ... })` -> `Cell::Rectangle::fromPixel(juce::Rectangle<int> { ... }, physCellW, physCellH)`

**State.cpp:**
- `getCols()` / `getVisibleRows()` — currently unpack from Bounds. Replace with Winsize unpack or keep as convenience getters that read the Winsize Parameter.

**Input.cpp:**
- Lines 129-130: paired `getCols()`/`getVisibleRows()` reads -> single Winsize unpack

**Validation:** No remaining `jam::Bounds` references in end codebase. No manual division/multiplication for pixel<->cell conversion. All dimension reads from packed atomic values.

### Step 11: Merge separate dimension Parameters (end)

**Scope:** `Source/terminal/Parameters.xml`, `Source/terminal/State.h`, `Source/terminal/State.cpp`, `Source/terminal/Identifier.h`, `Source/terminal/component/Display.cpp`

**Action:**

**Parameters.xml:**
- Remove separate `width` and `height` root params
- Add `viewport` param (type int64, default 0) — stores terminal::Winsize.pack()

**Identifier.h:**
- Add `static const juce::Identifier viewport { "viewport" };`
- Remove separate pixel dim identifiers if they existed as Parameters

**State.h/State.cpp:**
- Add `terminal::Winsize getWinsize() const noexcept` — unpacks from Parameter<int64_t>
- Keep convenience: `cell getCols()`, `cell getVisibleRows()` — delegate to getWinsize()
- Remove separate width/height getters/setters that read pixel dims independently

**Display.cpp (DISPLAY attachment):**
- Merge `cellWidth` + `cellHeight` attachment properties into single packed int if feasible, or keep as separate attachment properties (they're for font metrics, not viewport dims — Display writes them from Font, Processor reads them). These are pixel values written to DISPLAY node, separate from the Winsize parameter.
- Winsize Parameter: Display computes `terminal::Winsize { cols, rows, pixelW, pixelH }`, packs to int64, stores as single Parameter on SESSION node.

**Processor.cpp (vTPC):**
- screenDirty handler reads Winsize from Parameter<int64_t> — single atomic read gives both cell and pixel dims

**Session.cpp (wireResizer):**
- `winsize.referTo(...)` — bind to Winsize Parameter instead of viewportId
- `valueChanged` — unpack Winsize, fire resizer with cell dims

**Validation:** Single atomic Parameter carries both cell and pixel viewport dimensions. Display writes once. All consumers read once.

### Step 12: Remove DIAG logging + clean sweep (end)

**Scope:** All files listed in HANDOFF-spsc-pipeline.md bug #6.

**Action:** Remove all `jam::debug::Log::write(...)` calls from:
- `Video.cpp` (SCROLL_PUSH, LF)
- `Processor.cpp` (TICK, REMOVE, DRAIN, ADD_MUTABLE, RESULT)
- `VideoOSCExt.cpp` (OSC133_A/B/C/D)
- `ProcessorEvents.cpp` (COMMIT_PROMPT, PUSHLINE)

**Validation:** `grep -r "jam::debug::Log" Source/terminal/` returns zero hits.

## BLESSED Alignment

- **B (Bound):** Cell::Point and Cell::Rectangle own their coordinate data. Winsize owns its packed state. RAII lifecycle.
- **L (Lean):** No new types in jam — only new methods on existing types. Bounds eliminated (net reduction). Winsize is terminal-domain only.
- **E (Explicit):** Pixel vs cell enforced by type system, not convention. `fromPixel`/`toPixel` with explicit `int cellWidth, int cellHeight` — no ambiguous wrapper type. No `toLogical`/`fromJuce`/`toJuce` naming confusion.
- **S (SSOT):** One pack format per type. One atomic Parameter per dimension pair. Display sole author of Winsize.
- **S (Stateless):** Cell::Point and Cell::Rectangle are value types. No internal mutable state.
- **E (Encapsulation):** Conversion logic lives on the cell types. Pixel domain stays pure juce. No leaking jam types into pixel operations.
- **D (Deterministic):** Same pixel input + same cellSize = same cell output. Pack/unpack are bijective.

## Risks

- **Parameter<int64_t> lock-free guarantee:** `std::atomic<int64_t>::is_always_lock_free` is true on x86-64 and ARM64 (both target platforms). Verify at compile time with `static_assert`.
- **16-bit field overflow:** Cell::Rectangle pack uses 16 bits per field. Max value 65535. Terminal cols/rows never exceed ~500/200. Pixel dims could reach ~8K (8192 < 16384 < 65535). Safe.
- **Resizer variadic template:** Resizer::set() uses variadic args that construct Bounds. Replacing with two plain ints requires careful template adjustment — verify timerCallback trigger args match addTrigger registration.
