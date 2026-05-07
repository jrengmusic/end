# PLAN: Terminal Rendering Pipeline

**RFC:** RFC-terminal-rendering-pipeline.md
**Date:** 2026-05-07
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE
**Scope:** COMPLETE WORKING TERMINAL

## Overview

Replace JUCE's ShapedText with jam's own shaping/layout engine, reconnecting the proven codepoint → Typeface → Atlas pipeline. TextEditor keeps its architectural role. jam owns the entire text pipeline: storage (Pens), shaping (shapeText), layout (ShapedText), rendering (Graphics).

**Priority order (ARCHITECT directive):**
1. Rich text rendering — ALL chars with correct cell spacing (emoji, ligatures, NF icons, box drawing, CJK)
2. Cursor.h as TextEditor's CaretComponent
3. TextEditor stripping — least priority (deferred)

## Completed

### Step 1: Clean Screen ✓
Screen stripped to minimal jam::TextEditor subclass. ColourIds enum, constructor, destructor only.

### Step 2: Display owns CellMetrics ✓
Display owns CellMetrics member. Mouse takes const CellMetrics& instead of Screen&. Config pattern enforced.

### Step 3: Comprehensive dummy text ✓
Screen constructor sets comprehensive UTF-8 test corpus (32 sections: ASCII through Thai). All non-ASCII as \xNN bytes in CharPointer_UTF8. Monochrome — colours are later steps.

### Glyph::Graphics optimization ✓ (already in jam)
Persistent renderTarget, clip-aware push/pop, memmove scroll optimisation. No work needed.

### Grid logical lines ✓ (already in Grid)
TerminalLine with HeapBlock<Cell> + Lines<TerminalLine> ring buffer + Grid::Row view mapping. No work needed.

## Validation Gate

Each step MUST be validated by ARCHITECT (compile, run, visual check) before proceeding. Each step MUST comply with MANIFESTO.md (BLESSED), NAMES.md, JRENG-CODING-STANDARD.md, and locked PLAN decisions.

## Strategy

**Dummy text validates rendering BEFORE Grid wiring.** The pipeline has failed every previous wiring attempt. Static content isolates rendering from Grid/Parser/PTY.

Sequence: new jam types → shaping → layout → TextEditor integration → validate on dummy text → colours → cursor → THEN wire Grid.

## Steps

### Step 4: Glyph::Pen, Glyph::Grapheme, Glyph::Pens — jam types

**Scope:** jam/jam_graphics/rendering/ (new files)

**Action:**
- `Glyph::Pen` — 16-byte POD: uint32_t codepoint, juce::Colour fg, juce::Colour bg, uint8_t style/width/layout/reserved. Per RFC §New jam Types.
- `Glyph::Grapheme` — combining marks sidecar: std::array<uint32_t, 7> extraCodepoints + uint8_t count. Per RFC.
- `Glyph::Pens` — rich string: HeapBlock<Pen> + HeapBlock<Grapheme> sidecar + count. Builder from CharPointer_UTF8 (convenience for dummy text). Per RFC.
- static_assert on Pen size (16) and trivially_copyable.

**Validation:** compiles. Pens can be constructed from CharPointer_UTF8 string.

### Step 5: Typeface::shapeText — codepoint → glyph resolution with fallback

**Scope:** jam_typeface.h, jam_typeface.cpp, jam_typeface.mm

**Action:**
- Add `shapeText (uint8_t style, const uint32_t* codepoints, int count)` method to jam::Typeface.
- Returns `GlyphRun { struct ShapedGlyph { uint16_t glyphIndex; void* fontHandle; float advance; bool isEmoji; }; HeapBlock<ShapedGlyph> glyphs; int count; }`.
- Implementation: for each codepoint, try primary font → userFallbackFonts in order via glyphForCodepoint (macOS) / FT_Get_Char_Index (FreeType). Return correct fontHandle + glyphIndex per glyph.
- macOS: promote file-static `glyphForCodepoint` to Typeface method or internal helper callable per-font.
- FreeType: equivalent lookup via FT_Get_Char_Index on each FT_Face.

**Note:** RFC says "shapeText already exists" — it does NOT. glyphForCodepoint exists (macOS, file-static), addFallbackFont/userFallbackFonts exist but metrics-only. This step implements the actual shaping method.

**Validation:** compiles. shapeText resolves codepoints from primary + fallback fonts. Non-primary glyphs get correct fontHandle.

### Step 6: Glyph::ShapedText — layout engine

**Scope:** jam/jam_graphics/rendering/ (new file)

**Action:**
- Replaces JUCE's ShapedText. Consumes Pens, produces positioned glyph runs via Typeface::shapeText.
- Input: Pens + layout mode (mono/proportional) + wrap constraint.
- Monospace: fixed advance = cell width. Character-level wrap at column count.
- Proportional: natural advance from Typeface metrics. Word-aware wrap at pixel boundary.
- Output: positioned glyph runs per line — glyphIndex, fontHandle, position, fg colour, isEmoji.
- Grapheme clusters: when Pen has HAS_GRAPHEME flag, pass base + combining codepoints to shapeText together.

**Validation:** compiles. ShapedText produces correct positions for monospace layout (each glyph at col × cellWidth).

### Step 7: TextEditor Pens integration — the font system fix

**Scope:** jam_text_editor.h, jam_text_editor.cpp

**Action:**
- Add `void setText (const Glyph::Pens& pens)` — primary API. Stores Pens, triggers layout via ShapedText.
- Add `void setText (juce::CharPointer_UTF8 text)` — convenience. Creates Pens with default fg/bg, calls setText(Pens).
- Modify `drawContent` (~line 1066): instead of iterating JUCE ShapedText glyph IDs, iterate Glyph::ShapedText glyph runs. Each glyph uses its own fontHandle (from Typeface fallback chain), not the main font.
- Modify `drawGlyphs` signature or add new overload that takes ShapedText glyph runs directly — correct fontHandle per glyph.
- Remove dependency on JUCE's ShapedText for Pens-based content. juce::String path remains for non-terminal consumers.
- Screen's dummy text: change `setText(juce::String(CharPointer_UTF8(...)))` → `setText(CharPointer_UTF8(...))` (new convenience overload).

**ARCHITECT validates:** launch END. ALL chars in dummy text render correctly. No "?" for non-ASCII. Box drawing connects. Emoji visible. CJK double-width. NF icons visible. Powerline symbols visible.

### Step 8: Per-cell foreground colours

**Scope:** Screen.cpp (dummy text), jam_text_editor.cpp (drawContent)

**Action:**
- Dummy text: construct Pens with per-section fg colours (red box drawing, green blocks, cyan emoji, etc.).
- drawContent: read Pen::fg per glyph from ShapedText output, pass to drawGlyphs. Each glyph renders with its own colour.
- Verify: TextEditor renders multi-colour content from a single Pens object.

**ARCHITECT validates:** launch END. Dummy text shows multiple colours per line. Each section in its assigned colour.

### Step 9: Per-cell background colours

**Scope:** jam_text_editor.cpp or Screen.cpp or LookAndFeel

**Decision Gate:** Mechanism — paint bg rectangles in drawContent (before glyph compositing), override paint() in Screen, or LookAndFeel hook. ARCHITECT decides.

**Action:**
- Pen::bg carries resolved background colour per cell.
- Render bg rectangles at correct cell positions before glyph compositing.
- Dummy text: add bg colour sections (dark red bg on some lines, dark blue on others).

**ARCHITECT validates:** launch END. Background colour blocks visible behind text. No bleed into adjacent cells.

### Step 10: Cursor — Cursor.h as CaretComponent

**Scope:** Screen.cpp, LookAndFeel.h/cpp

**Action:**
- Screen constructor: `setCaretVisible(true)`.
- LookAndFeel: override `createCaretComponent()` → CaretComponent draws using Cursor descriptor (Source/Cursor.h).
- CaretComponent reads: codepoint (default U+2588), shape (DECSCUSR), colour, visible, blinkOn.
- Dummy text: hardcode cursor at known position.

**ARCHITECT validates:** launch END. Cursor visible on dummy text. Correct shape. Blinks.

### Step 11: Wire Grid → Display → Screen

**Scope:** TerminalDisplay.cpp, Screen.h/cpp

**Action:**
- Display::onVBlank(): walk Grid lines, build Pens per visible row.
- Color resolution in Display: Terminal::Color → juce::Colour via static helper (default fg from ColourIds, palette from ColourIds, RGB direct, 256-palette computed).
- Build Cursor descriptor from State. Position caret.
- screen.setText(pens) — full rebuild every dirty frame.
- Remove dummy text from Screen constructor.

**ARCHITECT validates:** run `test/render-test.sh`. ALL chars render with correct colours and cell spacing. Cursor at correct position. Content updates on keypress.

### Step 12: Incremental content feed

**Scope:** TerminalDisplay.cpp, Screen.h/cpp, jam_text_editor.h

**Action:**
- Add `void setParagraph (int index, const Glyph::Pens& pens)` to TextEditor.
- Display::onVBlank(): consume dirty rows from Grid's 256-bit atomic bitmask. For each dirty row: build Pens, call screen.setParagraph(row, pens). Full rebuild only on line count change or allDirty.
- ShapedText: per-paragraph layout (only reshape dirty paragraphs).

**Decision Gate:** Names for Screen/TextEditor public methods. ARCHITECT decides.

**ARCHITECT validates:** same visual result as Step 11. Typing responsive. Scrolling smooth. No full-rebuild flicker on single-line changes.

## BLESSED Alignment

- **B:** Pen is POD (trivially copyable). Pens owns HeapBlock storage. ShapedText owns glyph runs. renderTarget owned by Graphics. Clear lifecycle at each layer.
- **L:** No god objects. ShapedText replaces JUCE's ShapedText — no parallel system. One font pipeline via Typeface::shapeText.
- **E:** Pen carries uint32_t codepoints (no encoding ambiguity). Resolved juce::Colour (no deferred resolution). shapeText returns explicit fontHandle per glyph. No magic — every glyph knows which font it came from.
- **S(SSOT):** Typeface::shapeText is the SINGLE authority for codepoint → glyph resolution. No parallel font system. Grid Lines = content truth. Pens = downstream rich text.
- **S(Stateless):** ShapedText is pure function of Pens + layout mode. drawContent composites from ShapedText — no cached terminal state.
- **E(Encap):** Grid doesn't know about pixels. Screen doesn't know about Grid. TextEditor doesn't know about terminal. Typeface owns fallback chain — callers don't pick fonts.
- **D:** Same Pens + same layout = same ShapedText. Same ShapedText + same clip = same pixels.

## Risks / Open Questions

1. **Step 5 — shapeText implementation.** RFC states "already exists" — it does NOT. glyphForCodepoint is file-static (macOS only). FreeType path needs FT_Get_Char_Index per fallback face. Implementation required on both platforms.
2. **Step 7 — TextEditor dual path.** juce::String path must remain for non-terminal consumers (WHELMED). drawContent needs to handle both Pens-based ShapedText and juce::String-based JUCE ShapedText content.
3. **Step 9 — Background colour mechanism.** Decision Gate — ARCHITECT decides approach.
4. **Step 12 — Method names.** setParagraph proposed. ARCHITECT decides.
