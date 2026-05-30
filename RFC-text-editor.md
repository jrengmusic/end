# RFC: Universal Text Editor Foundation — jam Module Restructure and END Terminal Content Preservation

**Author:** ARCHITECT
**Status:** Draft — Pending COUNSELOR
**Replaces:** All prior RFC and PLAN documents relating to text rendering, reflow,
scrollback, buffer architecture, and module naming in jam and END.
Those documents are deleted. They encoded wrong mental models and wrong names.
There is no legacy. There is no backward compatibility.

---

## 1. Mental Model

### 1.1 What This Is NOT

END's text rendering is NOT a terminal scanline model.

The scanline model treats the screen as a volatile grid of cells sized to the
viewport, replaced on every frame. Scrollback is an afterthought bolted onto the
same grid. SIGWINCH resizes the grid and corrupts or loses content. This is the
model every existing terminal emulator uses. It is fundamentally wrong for
lossless content preservation.

END does not use this model. Agents that reason from this model will produce
wrong implementations.

### 1.2 What This IS

END's text rendering IS the same design used by both Neovim and JUCE — a
universal text editor model where:

- External sources (VT parser, markdown renderer, user keyboard) commit text
  INTO the editor.
- The editor IS the SSOT for all content it renders.
- Width enters exactly once, at projection time.
- Storage knows nothing about width, pixel dimensions, or viewport geometry.
- SIGWINCH changes the projection width only — storage is untouched.

This is not "the Neovim model" as opposed to "the JUCE model." They are the
same model. The source of text is irrelevant — keyboard input, VT bytes, or
markdown are all external sources committing into the same storage. The editor
owns what it renders.

### 1.3 Verified Source Mapping

All decisions in this RFC trace to verified source in these files:

**Neovim (`src/nvim/terminal.c`):**
- `ScrollbackLine { size_t cols; VTermScreenCell cells[]; }` — lines stored at
  original width, no geometry in storage.
- `term_sb_push` / `refresh_scrollback()` — commit path: departed rows go into
  `sb_buffer`, then into `buf_T` via `ml_append_buf`.
- `refresh_screen()` — flush path: `ml_replace_buf(buf, linenr, textbuf)` per
  dirty visible row.
- `sb_popline` / `fetch_row()` — NOT reflow. Lines are read from `sb_buffer` at
  their original `cols` and padded or truncated to current width. No
  line-splitting, no line-joining. Dimension adjustment only.

**Neovim (`src/nvim/memline.c`, `memline_defs.h`):**
- `memline_T` = B-tree of lines. Zero geometry. Zero width knowledge. Lines are
  UTF-8 strings. `ml_get(linenr)` returns the raw line.
- `ml_append_buf(buf, linenr, text)` — insert one line at position.
- `ml_replace_buf(buf, linenr, text)` — replace one line in place.
- `ml_delete_buf(buf, linenr)` — delete one line.

**JUCE (`juce_gui_basics/widgets/juce_TextEditorModel.cpp`):**
- `ParagraphStorage` owns `String text` + `optional<ShapedText> shapedText` +
  `optional<float> height` + `optional<int64> numGlyphs`. Lazy shaping:
  `getShapedText()` shapes on first call, `clearShapedText()` invalidates.
- `ParagraphsModel` owns `vector<ParagraphStorage>` + `Ranges ranges` (character-
  position index). Operations: `set(Range<int64>, String)`.
- `TextEditorStorage` owns `ParagraphsModel` + `RangedValues<Font>` +
  `RangedValues<Colour>`. The extra layer exists because JUCE's `String` carries
  no intrinsic style — style lives in external attribute ranges.

**jam (`jam_graphics/fonts/font/glyph/jam_glyph_arrangement.h`, `.cpp`):**
- `glyph::Arrangement` — the render cache. Two-pass: `buildArrangements`
  (shaped Entry buffer, cell-space coordinates) → `buildDrawRuns` (pixel-
  positioned Run objects, GPU-ready).
- `Arrangement::shape(lines, font, wrapColumns, lineOffset)` — full re-shape
  of all content. Called by `TextEditor::setText()`.

**jam (`jam_gui/text_editor/jam_text_editor.cpp`):**
- `setText(const TextLineArray& lines)` — iterates lines, sums
  `getWrappedLines(viewWidth)` into `projectedRows`, calls
  `paragraphsModel.build(lines)` and `arrangement.shape(lines, ...)`.
  Does NOT store the reference. `Arrangement` IS the internal storage.
- `paragraphsModel` is built in `setText()` but never consumed in rendering.
  `ContentView::paint` reads only from `arrangement`. This scaffold is INCOMPLETE
  and must be completed by this RFC.

---

## 2. Why We Do Not Fork JUCE TextEditorStorage

JUCE's `TextEditorStorage` is an extra layer that owns `RangedValues<Font>` and
`RangedValues<Colour>` because `juce::String` is plain UTF-8 with no intrinsic
style. Per-run fonts and colours must be stored externally and resolved during
shaping via `getShapedTextOptions(range)`.

`jam::String` (defined in §4.4) stores `jam::Char` atoms. Each `jam::Char`
carries `styleId` → `jam::Stamp` → `{fg, bg, flags}`. Style is intrinsic to the
atom. No external attribute ranges are needed. `TextEditorStorage` has no
equivalent in jam.

`jam::TextEditor` owns `ParagraphsModel` directly. No intermediate storage
layer.

---

## 3. Why ParagraphsModel Uses Neovim's API, Not JUCE's

JUCE's `ParagraphsModel::set(Range<int64>, String)` uses `int64` character
positions across the entire document. Finding paragraph N requires the `Ranges`
index to translate a codepoint offset to a paragraph index. This machinery
exists because JUCE's edit operations target arbitrary codepoint positions — a
user can place the cursor at character 4872 of a document and type.

Our atoms are `jam::String` lines — complete rows. Operations are always line-
indexed:

- New terminal history line: insert at a line number.
- Live zone update: replace at a line number.
- WHELMED paragraph: insert/replace at a line number.

We never ask "which paragraph owns codepoint 4872." The `Ranges` machinery is
unnecessary complexity.

`jam::ParagraphsModel` uses Neovim's API: `insert(int lineIndex, ...)`,
`remove(juce::Range<int>)`, `set(int lineIndex, ...)`. Identical to
`ml_append_buf`, `ml_delete_buf`, `ml_replace_buf`. Line-indexed. No character
offset arithmetic.

---

## 4. JAM Module Restructure (PRIORITY)

This section is the primary deliverable. All other sections depend on it.

### 4.1 File Renames

| Old path | New path | Reason |
|---|---|---|
| `jam_graphics/detail/jam_cell.h` | `jam_graphics/detail/jam_cell.h` | Keeps coordinate types only — see §4.2 |
| `jam_graphics/detail/jam_cell.h` (character content) | `jam_graphics/detail/jam_char.h` | Character atom moves to new file |
| `jam_graphics/detail/jam_text_line.h` | `jam_graphics/detail/jam_string.h` | Renamed type |
| `jam_graphics/detail/jam_text_line_array.h` | `jam_graphics/detail/jam_string_array.h` | Renamed type |

`jam_graphics.h` include order updated accordingly. All submodule headers include
nothing — includes live at the topmost module header only, per coding standard.

### 4.2 jam::Cell — Coordinate Unit

`jam::Cell` retains the name but sheds the character content. It IS now the
grid coordinate unit — what was `Cell::Unit`. The character content moves to
`jam::Char` (§4.3).

**What remains in `jam_cell.h`:**

```cpp
// jam_cell.h — coordinate system only, zero includes
namespace jam
{

struct Cell
{
    int value { 0 };

    constexpr explicit Cell (int v) noexcept : value { v } {}

    // Arithmetic
    constexpr Cell operator+  (Cell other)  const noexcept;
    constexpr Cell operator-  (Cell other)  const noexcept;
    constexpr Cell operator*  (int factor)  const noexcept;
    constexpr Cell operator/  (int divisor) const noexcept;
    constexpr Cell& operator+= (Cell other) noexcept;
    constexpr Cell& operator-= (Cell other) noexcept;
    constexpr Cell& operator++ ()    noexcept;
    constexpr Cell  operator++ (int) noexcept;
    constexpr Cell& operator-- ()    noexcept;
    constexpr Cell  operator-- (int) noexcept;
    constexpr Cell operator%  (Cell other)  const noexcept;
    constexpr Cell operator%  (int divisor) const noexcept;

    // Comparison
    constexpr bool operator== (Cell other) const noexcept;
    constexpr bool operator!= (Cell other) const noexcept;
    constexpr bool operator<  (Cell other) const noexcept;
    constexpr bool operator<= (Cell other) const noexcept;
    constexpr bool operator>  (Cell other) const noexcept;
    constexpr bool operator>= (Cell other) const noexcept;

    // Forward declarations — defined in jam_cell_point.h and jam_cell_rectangle.h
    struct Point;
    struct Rectangle;
};

} // namespace jam

namespace jam::literals
{
constexpr jam::Cell operator"" _cell (unsigned long long v) noexcept
{
    return jam::Cell { static_cast<int> (v) };
}
} // namespace jam::literals

using namespace jam::literals;
using cell = jam::Cell; // alias unchanged at all call sites
```

`Cell::Point` and `Cell::Rectangle` stay as nested coordinate types of
`jam::Cell`. `jam_cell_point.h` and `jam_cell_rectangle.h` are unchanged except
for the `Unit` → `Cell` reference update throughout.

**Dead code deleted from `jam_cell.h`:**

- `Cell::RowState` — verified dead across entire jam and END codebase. Never
  referenced in any .h or .cpp file. `jam::Row` owns its own `uint8_t flags`
  with `flexWrap`, `collapsed`, `justify`. Deleted.
- `Cell::getKey(int row, int col)` — doxygen claims "used as the key into
  grapheme or hyperlink side-tables." No such tables exist in the codebase.
  Grapheme uses `jam::Grapheme` SharedResource indexed by integer ID. OSC 8
  hyperlinks use `activeLinkId` stamped into Cell via Stamp. Verified dead.
  Deleted.

### 4.3 jam::Char — Attributed Character Atom

New file: `jam_graphics/detail/jam_char.h`. Contains the packed character type
formerly called `jam::Cell` (the character content, not the coordinate).

`jam::Char` is an 8-byte, trivially-copyable packed `uint64_t`. Same bit layout
as the former `Cell` character content. One semantic change: `SPACER_HEAD` (value
3, formerly "reserved for future use") is renamed `PROPORTIONAL`.

**Packed layout (unchanged):**

```
Bit 63                                                    Bit 0
[  padding (23)  |  styleId (16)  |  wide (2)  |  contentTag (2)  |  codepoint (21)  ]
```

**contentTag values (unchanged):**

```cpp
static constexpr uint8_t CONTENT_CODEPOINT { 0 };
static constexpr uint8_t CONTENT_GRAPHEME  { 1 };
static constexpr uint8_t FLEX_GAP          { 2 };
```

**wide values (one rename):**

```cpp
static constexpr uint8_t NARROW       { 0 }; // monospace: 1 column
static constexpr uint8_t WIDE         { 1 }; // monospace: 2 columns (CJK)
static constexpr uint8_t SPACER_TAIL  { 2 }; // monospace: skip (right half of wide char)
static constexpr uint8_t PROPORTIONAL { 3 }; // proportional: derive advance from HarfBuzz
```

`PROPORTIONAL` semantics: the shaper (`buildArrangements`) ignores the cell-unit
advance for this character and uses the HarfBuzz glyph advance metric instead.
Terminal content never sets this value — Video only writes NARROW, WIDE, or
SPACER_TAIL. WHELMED sets this for proportional font rendering.

`SPACER_TAIL` (value 2) is unchanged. `buildArrangements` already filters it:
```cpp
if (pen.wide() != jam::Char::SPACER_TAIL and pen.wide() != jam::Char::PROPORTIONAL)
```
For `PROPORTIONAL`, the shaper uses HarfBuzz advance instead of cell-unit
multiplication. This requires a targeted change to `buildArrangements` and
`fillRunArrays` — out of scope for terminal RFC, noted here for WHELMED work.

`jam::Char` preserves `make()`, `erase()`, all accessors. `static_assert` on
size and trivial copyability unchanged.

### 4.4 jam::String — Attributed String Line

Renamed from `jam::TextLine`. File: `jam_graphics/detail/jam_string.h`.

```cpp
// jam_string.h — zero includes
namespace jam
{

struct String
{
    juce::HeapBlock<Char> chars;  // renamed from cells
    int cellCount { 0 };          // authoritative content width in cell units
    bool isContinued { false };   // DECAWM soft-wrap — content continues on next line
    bool isJustified { false };   // contains Char::FLEX_GAP — distribute free space

    int getWrappedLines (int viewWidth) const noexcept
    {
        if (viewWidth <= 0 or cellCount <= 0)
            return 1;

        return (cellCount + viewWidth - 1) / viewWidth;
    }
};

} // namespace jam
```

`getWrappedLines(viewWidth)` is the exact translation of Neovim's
`plines_win_nofold`. Verified implementation: `(cellCount + viewWidth - 1) /
viewWidth` — ceiling division. `cellCount` IS `usedCols` at commit time. No
geometry beyond the content.

`juce::HeapBlock<Char>` not `std::vector<Char>` — HeapBlock is trivially
relocatable, no destructor overhead, dense allocation. `jam::String` is not
stored in `jam::Buffer<T>` (which requires trivially copyable elements and uses
`memcpy`). It lives in `jam::ParagraphsModel` only.

### 4.5 jam::StringArray — Bounded String Deque

Renamed from `jam::TextLineArray`. File: `jam_graphics/detail/jam_string_array.h`.

`jam::StringArray` is a generic bounded deque. It carries no committed/live
semantics, no terminal knowledge, no screen channel knowledge. It is pure
attributed string storage.

`juce::StringArray` already exists in the JUCE namespace. `using namespace juce`
is forbidden by coding standard. `jam::StringArray` and `juce::StringArray` are
always explicitly namespaced — no ambiguity possible by contract.

### 4.6 jam::Row — Cascade Rename

`jam::Row` retains its structure. The FAM element type changes from `jam::Cell`
(character) to `jam::Char`:

```cpp
struct Row
{
    using FlexType = Char;   // was Cell

    uint16_t usedCols { 0 };
    uint8_t flags { 0 };

    static constexpr uint8_t flexWrap  { 1 << 0 };
    static constexpr uint8_t collapsed { 1 << 1 };
    static constexpr uint8_t justify   { 1 << 2 };

    Char chars[];  // renamed from cells[]
};
```

All call sites that reference `row->cells[col]` become `row->chars[col]`.
`Buffer<Row>`, `Block<Row>`, and all Video write paths update accordingly.

### 4.7 Factually Wrong Doxygen — Corrections Required

These doxygen comments are currently factually false. They must be corrected as
part of this RFC's implementation:

**`jam_cell.h` (before split):**
- `Cell::getKey()` — "Used as the key into grapheme or hyperlink side-tables."
  **FALSE.** No such tables exist. Function is dead. Corrected by deletion.
- `Cell::RowState` — "Stored alongside each row in a terminal screen buffer."
  **FALSE.** Never referenced anywhere. Dead code. Corrected by deletion.
- `Cell::SPACER_HEAD` — "reserved for future use." **OBSOLETE.** Renamed to
  `PROPORTIONAL` with correct semantics.

**`jam_ParagraphStorage.h`:**
- File-level: "JUCE's ParagraphStorage owns a String; jam's owns a row span into
  Buffer<Row>." **FALSE.** Current `jam::ParagraphStorage` is `{ int startRow;
  int rowCount; }` — two integers. Owns nothing. No Buffer reference.
- `ParagraphStorage` struct: "One logical line — a row span in Buffer<Row>."
  **FALSE.** Two integers. Not a row span.
- `ParagraphsModel::append()` — "Called by Video on each line write." **FALSE.**
  Video never calls `ParagraphsModel::append()`. Not wired anywhere.

---

## 5. jam::ParagraphStorage — Completed

The scaffold in `jam_ParagraphStorage.h` is structurally incomplete — it mirrors
the JUCE name but not the JUCE content. This RFC completes it.

`jam::ParagraphStorage` mirrors `juce::TextEditor::ParagraphStorage` in
structure: owns one attributed string line and its lazy shaped cache.

```cpp
// jam_ParagraphStorage.h — zero includes
namespace jam
{

struct ParagraphStorage
{
    explicit ParagraphStorage (jam::String s) noexcept
        : text { std::move (s) }
    {}

    const jam::String& getText() const noexcept { return text; }

    // Lazy height — calls text.getWrappedLines(viewWidth), caches result.
    // viewWidth is the physicalViewWidth from TextEditor at projection time.
    int getHeight (int viewWidth) noexcept
    {
        if (not cachedHeight.has_value())
            cachedHeight = text.getWrappedLines (viewWidth);

        return *cachedHeight;
    }

    // Invalidates shaped cache and height cache. Called on set(), SIGWINCH
    // (width change), and when the paragraph scrolls out of view.
    void clearShapedText() noexcept
    {
        shapedEntries.reset();
        cachedHeight.reset();
    }

    // True when shaped entries are available for rendering.
    bool isShapedValid() const noexcept { return shapedEntries.has_value(); }

private:
    jam::String text;
    std::optional<int> cachedHeight;

    // Lazy shaped cache — Entry buffer from buildArrangements, cell-space
    // coordinates. Populated on first paint of visible paragraph.
    // Freed when paragraph scrolls out of view.
    std::optional<juce::HeapBlock<glyph::Arrangement::Entry>> shapedEntries;

    friend class ParagraphsModel;
};

} // namespace jam
```

Differences from JUCE's `ParagraphStorage`:
- Owns `jam::String` (attributed via `jam::Char`) not `juce::String` (plain UTF-8).
- No `Range<int64> range` — we are line-indexed, not character-indexed. No
  character position mapping needed.
- No back-reference to storage for font resolution — style is intrinsic to
  `jam::Char` via `styleId`. Shaping uses `jam::Stamp::getContext()` directly.
- Lazy shaped cache is `HeapBlock<glyph::Arrangement::Entry>` — cell-space glyph
  entries from the first shaping pass, before pixel position computation. These
  are rebuildable into draw runs at any zoom or font change.

---

## 6. jam::ParagraphsModel — Completed

`jam::ParagraphsModel` uses Neovim's line-indexed API over a bounded deque,
not JUCE's character-range API over a `Ranges` index tree.

```cpp
// jam_ParagraphsModel.h — zero includes (declared alongside ParagraphStorage)
namespace jam
{

class ParagraphsModel
{
public:
    explicit ParagraphsModel (int capacity) noexcept
        : capacity { capacity }
    {}

    // Insert one attributed string at lineIndex.
    // Equivalent to ml_append_buf(buf, lineIndex, text).
    // If size exceeds capacity after insert, drops from the front (FIFO).
    void insert (int lineIndex, jam::String&& string) noexcept;

    // Remove lines in lineRange [start, end).
    // Equivalent to ml_delete_buf(buf, linenr) per line in range.
    void remove (juce::Range<int> lineRange) noexcept;

    // Replace one line at lineIndex in place.
    // Equivalent to ml_replace_buf(buf, lineIndex, text).
    // Clears that paragraph's shaped cache.
    void set (int lineIndex, jam::String&& string) noexcept;

    // Clear all paragraphs.
    void clear() noexcept;

    // Resize capacity. Called on scrollback option change.
    void setCapacity (int newCapacity) noexcept;

    const ParagraphStorage& operator[] (int lineIndex) const noexcept;
    ParagraphStorage& operator[] (int lineIndex) noexcept;

    int totalRows() const noexcept;
    bool isEmpty() const noexcept;

    // Invalidate height caches on all paragraphs. Called on SIGWINCH.
    void clearHeightCaches() noexcept;

private:
    std::deque<ParagraphStorage> paragraphs;
    int capacity { 0 };
};

} // namespace jam
```

`std::deque<ParagraphStorage>` — O(1) `push_back`, O(1) `pop_front`, stable
references on growth. Correct for variable-size attributed string elements.
`jam::Buffer<T>` is wrong here — it uses `memcpy`/`memset` requiring trivially
copyable elements; `jam::String` contains `juce::HeapBlock` which is not trivially
copyable.

FIFO behavior: when `insert()` causes `paragraphs.size() > capacity`, `pop_front()`
drops the oldest paragraph. Equivalent to Neovim's `adjust_scrollback()` calling
`ml_delete_buf` from the front when `sb_current > scbk`.

---

## 7. jam::TextEditor — Updated Architecture

`jam::TextEditor` owns **two** `ParagraphsModel` instances — one per screen
channel. TextEditor IS the SSOT for all content it renders.

```
jam::TextEditor owns:
    std::array<ParagraphsModel, 2> screens
        screens[0]  — normal screen (history lines + active lines)
        screens[1]  — alternate screen (active lines only, no history)
    int activeScreen { 0 }
    glyph::Arrangement arrangement  — render cache, derived from active model
    glyph::Graphics glyphGraphics
    ParagraphsModel is the SSOT. Arrangement is the projection.
```

### 7.1 Editing API

Mirrors `ml_append_buf`, `ml_replace_buf`, `ml_delete_buf`:

```cpp
// Insert one attributed string at lineIndex in the specified screen.
// Clears shaped cache for that paragraph.
void insert (int screen, int lineIndex, jam::String&& string) noexcept;

// Remove lines in lineRange from the specified screen.
void remove (int screen, juce::Range<int> lineRange) noexcept;

// Replace one line at lineIndex in the specified screen.
// Clears that paragraph's shaped cache.
void set (int screen, int lineIndex, jam::String&& string) noexcept;

// Replace all content in the specified screen.
void setText (int screen, jam::String&& string) noexcept;

// Switch active screen channel. Arrangement rebuilds from new active model.
void setActiveScreen (int screen) noexcept;
```

No `replaceAtTail`. No invented patterns. These four operations cover every
terminal and WHELMED use case.

### 7.2 calc() — ContentView Height

`calc()` computes ContentView pixel height from the active `ParagraphsModel`:

```cpp
void TextEditor::calc() noexcept
{
    glyphGraphics.clear();

    const int viewWidth { physicalViewWidth };
    int totalScreenRows { 0 };

    const ParagraphsModel& model { screens.at (static_cast<size_t> (activeScreen)) };
    const int totalLines { model.totalRows() };

    for (int i { 0 }; i < totalLines; ++i)
        totalScreenRows += model[i].getHeight (viewWidth);  // lazy, cached

    // ... ContentView pixel height, auto-scroll logic, repaint
}
```

This is the direct translation of Neovim's `comp_botline`: summing
`plines_win_nofold` over all buffer lines to compute window height. `getHeight`
calls `getWrappedLines(viewWidth)` on first call and caches the result.
SIGWINCH → `clearHeightCaches()` → all heights recomputed at new width on next
`calc()`.

### 7.3 Lazy Rendering

On paint, `ContentView::drawGlyphRuns` iterates only visible paragraphs
(determined by Viewport clip rectangle). For each visible paragraph:

1. If `ParagraphStorage::isShapedValid()` — use cached shaped entries directly.
2. Otherwise — call `buildArrangements(paragraph.getText().chars, ...)` to shape
   into the paragraph's entry buffer. Mark valid.

Paragraphs that scroll out of view have `clearShapedText()` called — their
shaped cache is freed. Equivalent to Neovim only drawing visible buffer lines,
with our persistence between frames reducing redundant HarfBuzz calls.

`ParagraphsModel` as a whole is NOT fully re-shaped on every `setText()`. Only
modified or newly visible paragraphs are shaped. This is the JUCE pattern:
`ParagraphStorage::clearShapedText()` per affected paragraph on edit, lazy
re-shape on next render.

### 7.4 SIGWINCH Safety

On SIGWINCH (viewport resize):

1. `physicalViewWidth` recomputed from new visible pixel width.
2. `screens[activeScreen].clearHeightCaches()` — all `cachedHeight` invalidated.
   All `shapedEntries` caches remain valid — cell-space coordinates are width-
   invariant. Only height (wrapped line count) changes with width.
3. `calc()` — recomputes ContentView pixel height by summing `getHeight(newWidth)`
   per paragraph.
4. ContentView resizes. JUCE Viewport repositions.

History paragraphs (`screens[0]`, normal screen history) are untouched. Their
`jam::String::cellCount` records content width at commit time. `getWrappedLines`
recomputes their screen height at the new `viewWidth`. Buffer not mutated.

This is exactly Neovim's behavior: `ScrollbackLine` stores lines at original
`cols`. `plines_win_nofold` recomputes screen rows at `w_view_width` on every
draw. Buffer untouched.

### 7.5 Screen Switch

Normal → Alternate:
```cpp
textEditor.setActiveScreen (1);  // screens[1] renders, screens[0] preserved
```

Alternate → Normal:
```cpp
textEditor.setActiveScreen (0);  // screens[0] renders, screens[1] preserved
```

Both `ParagraphsModel` instances remain in memory. Shaped caches for normal
screen history survive during alternate screen — no re-shaping cost on return.

---

## 8. END — Processor Write Paths

Processor holds no `StringArray`. No shadow state. Processor calls TextEditor's
editing API directly. TextEditor IS the SSOT.

### 8.1 Commit Path (Reader Thread → Message Thread)

When `Video::scrollUpAndFill(top=0)` fires — a row is departing the visible
surface:

1. Reader thread: `id::pushLine` event fires with the departing row index.
2. `CellFifo` (SPSC ring) — reader thread writes the row. Message thread reads.
   CellFifo is the sole cross-thread bridge. Video NEVER touches TextEditor.
3. Message thread: drains `CellFifo`. For each drained row, constructs
   `jam::String { chars[0..usedCols-1], flags }` and calls:
   ```cpp
   textEditor.insert (0, liveStart, std::move (newString));
   ```
   where `liveStart = screens[0].totalRows() - previousActiveCount`. Inserts
   before the live zone, pushing active lines down by one. Equivalent to
   Neovim's `ml_append_buf(buf, committedCount-1, text)`.
4. `textEditor.calc()` — ContentView height recomputes.

`previousActiveCount` is a plain `int` member of Processor. Updated on every
flush. NOT a State parameter. NOT stored in ValueTree. The `liveRows` State
parameter that caused resize content corruption is eliminated.

### 8.2 Flush Path (Message Thread, 60/120Hz)

On every State timer tick:

1. For each dirty row `r` in `Video::Buffer<Row>` (tracked by `rowTouched[]`):
   - Serialize `row->chars[0..usedCols-1]`, `row->flags` into `jam::String`.
   - Call `textEditor.set (activeScreen, liveStart + r, std::move (string))`.
2. `textEditor.calc()` — ContentView repaints.

`set()` replaces one paragraph in place and calls `clearShapedText()` on that
paragraph. Only the changed paragraph reshapes on next paint. Unchanged history
paragraphs are untouched. Equivalent to Neovim's `ml_replace_buf` in
`refresh_screen()`.

### 8.3 prepare() — Decoupled

`prepare()` resizes `Video::Buffer<Row>` and rebuilds `rowTouched[]` dirty flags.
It does NOT touch TextEditor. It does NOT reset `CellFifo`. In-flight committed
lines survive resize and are committed to `screens[0]` normally.

### 8.4 Screen Switch (Normal ↔ Alternate)

Normal → Alternate (`?1049h`):
1. Video switches active Buffer channel to 1.
2. Processor calls `textEditor.setActiveScreen(1)`.
3. `screens[1].clear()` — alternate screen starts blank.
4. Shell redraws. Flush path populates `screens[1]` via `set()`.

Alternate → Normal (`?1049l`):
1. Video switches active Buffer channel to 0.
2. Processor calls `textEditor.setActiveScreen(0)`.
3. `screens[0]` is unchanged — history and previous active content intact.
4. Shell redraws active zone. Flush path updates `screens[0]` tail via `set()`.

### 8.5 ED 3 — Clear Scrollback

`id::clearBuffer` event (from `eraseInDisplay(mode=3)`):
```cpp
textEditor.screens[0].clear();
textEditor.screens[1].clear();
textEditor.calc();
```

Shell redraws. Flush path repopulates active zone via `set()`.

---

## 9. Universal Foundation for WHELMED

`jam::Char`, `jam::String`, `jam::ParagraphsModel`, and `jam::TextEditor` know
nothing about terminal protocols, VT sequences, or markdown syntax. They are
universal attributed text types.

WHELMED is another producer that feeds the same `ParagraphsModel` via the same
editing API:
- Markdown paragraph → `jam::String` (Chars with styleIds for heading, emphasis, code).
- Feed into `textEditor.insert(screen, lineIndex, string)`.
- Renders through the same `ParagraphStorage` lazy shaping → `Arrangement` →
  `ContentView::paint`.

Future features built on this foundation:

**Text selection** — `juce::Range<int>` of line indices + column range within
boundary lines. Line-indexed coordinates, same as Neovim visual selection
`{startLine, startCol}` → `{endLine, endCol}`.

**Copy** — walks `ParagraphsModel` lines, extracts `jam::Char` codepoints via
`Grapheme` table for clusters, encodes UTF-8 for clipboard. Same path for
terminal and WHELMED.

**Search** — iterates `ParagraphsModel`, reads `jam::String` atoms, pattern
match per line. Domain-agnostic.

**Cursor movement** — moves through line indices and column offsets.
`getWrappedLines(viewWidth)` maps logical lines to screen rows for visual up/
down across wrap boundaries. Identical for terminal and WHELMED.

The invariant: `jam::ParagraphsModel` is the universal document model.
`jam::Char` is the universal attributed character. `jam::String` is the universal
attributed line. These types are text. Every future feature builds on top of
them unchanged.

---

## 10. Scope of Changes

### New in jam_graphics

- `jam_char.h` — `jam::Char` type (formerly `jam::Cell` character content)
- `jam_ParagraphStorage.h` — `jam::ParagraphStorage` completed (JUCE structure)
- `jam_ParagraphsModel.h` — `jam::ParagraphsModel` completed (Neovim API)
- `jam_string.h` — `jam::String` (renamed from `TextLine`, field renamed `chars`)
- `jam_string_array.h` — `jam::StringArray` (renamed from `TextLineArray`)

### Changed in jam_graphics

- `jam_cell.h` — reduced to coordinate types only (`Cell`, `Cell::Point`,
  `Cell::Rectangle`). `Cell::Unit` collapses into `Cell`. Dead code deleted
  (`RowState`, `getKey`). Doxygen corrected.
- `jam_ParagraphStorage.h` — complete rewrite. Doxygen corrected.
- `jam_row.h` — `FlexType = Char`, `Char chars[]` (renamed from `Cell cells[]`).
- `jam_graphics.h` — include order updated. All new files added.
- `jam_glyph_arrangement.h/.cpp` — `shape(TextLineArray)` overload renamed to
  `shape(StringArray)`. References to `Cell` → `Char` throughout.

### Changed in jam_gui

- `jam_text_editor.h/.cpp` — `std::array<ParagraphsModel, 2> screens`. New
  editing API (`insert`, `remove`, `set`, `setActiveScreen`). `calc()` updated
  to sum `getHeight(viewWidth)` per paragraph. `setText(TextLineArray)` removed,
  replaced by editing API.

### Changed in END

- `terminal/Processor.h/.cpp` — Holds `int previousActiveCount`. No
  `StringArray`. Calls `textEditor.insert/set/remove/setActiveScreen` directly.
  `liveRows` State parameter removed. `prepare()` decoupled from TextEditor.
- `terminal/Video.cpp` — `row->cells[col]` → `row->chars[col]` throughout.
- All END source referencing `jam::Cell` (character type) → `jam::Char`.
- All END source referencing `jam::TextLine` → `jam::String`.
- All END source referencing `jam::TextLineArray` → `jam::StringArray`.

### Unchanged

- `jam::Cell::Point`, `jam::Cell::Rectangle` — nested coordinate types, unchanged.
- `using cell = jam::Cell` — alias unchanged. All call sites using `cell`
  unchanged.
- `_cell` literal — unchanged.
- `jam::Buffer<jam::Row>`, `jam::Block<jam::Row>` — unchanged. Video scratch surface.
- `glyph::Arrangement` struct and draw run machinery — unchanged.
- `Video` write path — `print()`, `scrollUpAndFill()`, `eraseInDisplay()` etc.
  unchanged except `cells` → `chars` cascade rename.
- `jam::Stamp`, `jam::Grapheme`, `jam::Typeface`, `jam::Font` — unchanged.
- `jam::TextEditor` ValueTree state properties — unchanged.
- `jam::TextEditor::setCaretPosition()` — unchanged.
- `jam::TextEditor::updateWinsize()` — unchanged. TextEditor IS the sole author
  of viewport dimensions. This contract is unchanged.
- `CellFifo` — unchanged. Sole cross-thread bridge.
