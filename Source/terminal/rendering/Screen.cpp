#include "Screen.h"
#include <cstring>
#include <jam_debug/jam_debug.h>

namespace Terminal
{ /*____________________________________________________________________________*/

struct Screen::ContentView : public juce::Component
{
    explicit ContentView (Screen& o)
        : owner (o)
    {
        setWantsKeyboardFocus (false);
        setInterceptsMouseClicks (false, true);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (findColour (juce::TextEditor::backgroundColourId));

        auto& atlas { jam::Typeface::getAtlas() };
        atlas.advanceFrame();

        const float scale { jam::Typeface::getDisplayScale() };
        const auto clip { g.getClipBounds() };

        owner.glyphGraphics.push (juce::jmax (1, static_cast<int> (owner.getWidth() * scale)),
                                  juce::jmax (1, static_cast<int> (owner.getHeight() * scale)),
                                  static_cast<int> (clip.getX() * scale),
                                  static_cast<int> (clip.getY() * scale),
                                  static_cast<int> (clip.getWidth() * scale),
                                  static_cast<int> (clip.getHeight() * scale));

        owner.drawContent();

        owner.glyphGraphics.pop (g, clip.getX(), clip.getY());
    }

private:
    Screen& owner;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentView)
};

Screen::Screen (State& stateRef) noexcept
    : state (stateRef)
{
    cells.add (std::make_unique<jam::Cells>());// Normal screen (index 0)
    cells.add (std::make_unique<jam::Cells>());// Alternate screen (index 1)

    viewport = std::make_unique<juce::Viewport>();
    addAndMakeVisible (viewport.get());
    viewport->setViewedComponent (contentView = new ContentView (*this), true);
    viewport->setScrollBarsShown (true, false);

    caret = std::make_unique<jam::CaretComponent> (this);
    contentView->addChildComponent (caret.get());

    setColour (juce::CaretComponent::caretColourId, config.display.colours.cursor);
    setOpaque (false);
    setWantsKeyboardFocus (true);
    toFront (true);

    computeCellMetrics();
}

void Screen::computeCellMetrics() noexcept
{
    auto* tf { jam::Typeface::findTypeface (config.display.font.family) };

    if (tf != nullptr)
    {
        const float fs { config.dpiCorrectedFontSize() };
        const auto fm { tf->getMetrics() };

        if (fm.isValid() and fs > 0.0f)
        {
            const float ascent { fm.ascent * fs };
            const float descent { fm.descent * fs };
            const float leading { fm.leading * fs };

            float maxAdvance { 0.0f };

            for (uint32_t code { 32 }; code <= 127; ++code)
            {
                const float adv { tf->getAdvanceWidth (code) * fs };

                if (adv > maxAdvance)
                    maxAdvance = adv;
            }

            if (maxAdvance <= 0.0f)
                maxAdvance = fs;

            cellW = jam::toInt (maxAdvance, true);
            cellH = jam::toInt (ascent + descent + leading, true);
            baseline = jam::toInt (ascent, true);
            fontSize = fs;

            rasterizeCursorGlyph();
        }
    }
}

void Screen::rasterizeCursorGlyph() noexcept
{
    if (caret != nullptr and cellW > 0 and cellH > 0)
    {
        auto* tf { jam::Typeface::findTypeface (config.display.font.family) };

        if (tf != nullptr)
        {
            const uint32_t cp { config.display.cursor.codepoint };
            const float scale { jam::Typeface::getDisplayScale() };
            const int physCellW { jam::toInt (static_cast<float> (cellW) * scale, true) };
            const int physCellH { jam::toInt (static_cast<float> (cellH) * scale, true) };
            const int physBase  { jam::toInt (static_cast<float> (baseline) * scale, true) };

            // Determine if emoji
            const bool isEmoji { (cp >= 0x1F000u) };

            // Shape
            jam::Typeface::GlyphRun run;

            if (isEmoji)
                run = tf->shapeEmoji (&cp, 1);
            else
                run = tf->shapeText (jam::Typeface::Style::regular, &cp, 1);

            if (run.count > 0)
            {
                void* fontHandle { run.fontHandle };

                if (fontHandle == nullptr)
                {
                    if (isEmoji)
                        fontHandle = tf->getEmojiFontHandle();
                    else
                        fontHandle = tf->getFontHandle (jam::Typeface::Style::regular);
                }

                auto& atlas { jam::Typeface::getAtlas() };
                atlas.ensureImages();
                const jam::glyph::Constraint constraint { jam::glyph::getConstraint (cp) };
                const jam::glyph::Key key { run.glyphs[0].glyphIndex, fontHandle, fontSize * scale, 1 };

                auto* region { atlas.getOrRasterize (key, fontHandle, isEmoji, constraint, physCellW, physCellH, physBase) };

                if (region != nullptr)
                {
                    const int dim { atlas.getDimension() };
                    const int srcX { juce::roundToInt (region->textureCoordinates.getX() * static_cast<float> (dim)) };
                    const int srcY { juce::roundToInt (region->textureCoordinates.getY() * static_cast<float> (dim)) };

                    // Create cell-sized ARGB image
                    juce::Image cursorImage (juce::Image::ARGB, physCellW, physCellH, true, juce::SoftwareImageType());

                    if (isEmoji)
                    {
                        // Emoji: copy ARGB pixels directly
                        const auto& emojiAtlas { atlas.getEmojiAtlas() };

                        if (emojiAtlas.isValid())
                        {
                            juce::Graphics g (cursorImage);
                            const auto glyphClip { emojiAtlas.getClippedImage ({ srcX, srcY, region->widthPixels, region->heightPixels }) };
                            g.drawImageAt (glyphClip, region->bearingX, physBase - region->bearingY);
                        }
                    }
                    else
                    {
                        // Mono: tint alpha mask with cursor colour
                        const auto& monoAtlas { atlas.getMonoAtlas() };

                        if (monoAtlas.isValid())
                        {
                            const juce::Colour cursorColour { findColour (juce::CaretComponent::caretColourId) };
                            const uint8_t cr { cursorColour.getRed() };
                            const uint8_t cg { cursorColour.getGreen() };
                            const uint8_t cb { cursorColour.getBlue() };

                            juce::Image::BitmapData srcData (monoAtlas, juce::Image::BitmapData::readOnly);
                            juce::Image::BitmapData dstData (cursorImage, juce::Image::BitmapData::readWrite);

                            const int dstX { region->bearingX };
                            const int dstY { physBase - region->bearingY };

                            for (int row { 0 }; row < region->heightPixels; ++row)
                            {
                                const int dy { dstY + row };

                                if (dy >= 0 and dy < physCellH)
                                {
                                    for (int col { 0 }; col < region->widthPixels; ++col)
                                    {
                                        const int dx { dstX + col };

                                        if (dx >= 0 and dx < physCellW)
                                        {
                                            // SingleChannel atlas: pixel value is alpha coverage
                                            const uint8_t alpha { *srcData.getPixelPointer (srcX + col, srcY + row) };

                                            if (alpha > 0)
                                            {
                                                auto* dst { dstData.getPixelPointer (dx, dy) };
                                                // ARGB premultiplied — BGRA byte order on macOS
                                                dst[0] = static_cast<uint8_t> ((cb * alpha) >> 8);
                                                dst[1] = static_cast<uint8_t> ((cg * alpha) >> 8);
                                                dst[2] = static_cast<uint8_t> ((cr * alpha) >> 8);
                                                dst[3] = alpha;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    caret->setGlyphImage (cursorImage);
                }
                else
                {
                    caret->setGlyphImage ({});
                }
            }
            else
            {
                caret->setGlyphImage ({});
            }
        }
        else
        {
            caret->setGlyphImage ({});
        }
    }
}

void Screen::resized() { viewport->setBounds (getLocalBounds()); }

void Screen::setScrollBarThickness (int thickness) noexcept { viewport->setScrollBarThickness (thickness); }

void Screen::setDimensions (int newCols, int newRows) noexcept
{
    cols = newCols;
    visibleRows = newRows;
    scrollbackRows = 0;

    // Normal screen: ensure at least visibleRows
    if (cells.at (0) != nullptr)
        cells.at (0)->resize (static_cast<size_t> (newRows * newCols));

    // Alternate screen: fixed framebuffer, exact size
    if (cells.at (1) != nullptr)
        cells.at (1)->resize (static_cast<size_t> (newRows * newCols));

    shapeAndRepaint();
}

void Screen::setActive (int index) noexcept
{
    activeScreen = index;
    scrollbackRows = 0;
    shapeAndRepaint();
}

void Screen::setCaretPosition (int index) noexcept
{
    if (caret != nullptr and cellW > 0 and cellH > 0)
    {
        const int col { (cols > 0) ? (index % cols) : 0 };
        const int row { (cols > 0) ? (index / cols) : 0 };

        // ---- Diagnostic instrumentation (temporary) ----
        static int caretLogCount { 0 };
        ++caretLogCount;

        if (caretLogCount <= 20)
        {
            jam::debug::Log::write ("setCaretPosition index=" + juce::String (index) + " cols=" + juce::String (cols)
                                    + " scrollbackRows=" + juce::String (scrollbackRows)
                                    + " -> col=" + juce::String (col) + " row=" + juce::String (row));

            // Log cell content at cursor position
            const int scr { activeScreen };

            if (scr >= 0 and scr < static_cast<int> (cells.size()) and cells.at (static_cast<size_t> (scr)) != nullptr)
            {
                const auto& activeCells { *cells.at (static_cast<size_t> (scr)) };
                const int cursorIdx { (scrollbackRows + row) * cols + col };

                if (cursorIdx >= 0 and cursorIdx < static_cast<int> (activeCells.size()))
                {
                    const auto& cursorCell { activeCells[static_cast<size_t> (cursorIdx)] };
                    jam::debug::Log::write ("  cellAtCursor: U+"
                                            + juce::String::toHexString (static_cast<int> (cursorCell.codepoint))
                                            + " w=" + juce::String (static_cast<int> (cursorCell.width))
                                            + " style=" + juce::String (static_cast<int> (cursorCell.style)));
                }
            }
        }
        // ---- End diagnostic instrumentation ----

        caret->setCaretPosition ({ col * cellW, row * cellH, cellW, cellH });
        caret->setVisible (true);
    }
}

void Screen::shapeAndRepaint() noexcept
{
    if (activeScreen >= 0 and activeScreen < static_cast<int> (cells.size())
        and cells.at (static_cast<size_t> (activeScreen)) != nullptr and cols > 0 and cellW > 0 and cellH > 0)
    {
        // ---- Diagnostic instrumentation (temporary) ----
        static int frameCount { 0 };
        ++frameCount;

        if (frameCount <= 50)
        {
            const auto& activeCells { *cells.at (static_cast<size_t> (activeScreen)) };
            const int totalDocRows { static_cast<int> (activeCells.size()) / cols };

            jam::debug::Log::write ("--- shapeAndRepaint frame=" + juce::String (frameCount)
                                    + " cols=" + juce::String (cols) + " visibleRows=" + juce::String (visibleRows)
                                    + " scrollbackRows=" + juce::String (scrollbackRows)
                                    + " cellCount=" + juce::String (static_cast<int> (activeCells.size()))
                                    + " totalDocRows=" + juce::String (totalDocRows));

            // Dump first row cells: codepoint and width
            const int dumpCols { juce::jmin (cols, 40) };
            juce::String row0;

            for (int c { 0 }; c < dumpCols; ++c)
            {
                const auto& cell { activeCells[static_cast<size_t> (c)] };
                row0 += "[" + juce::String::toHexString (static_cast<int> (cell.codepoint))
                        + " w=" + juce::String (static_cast<int> (cell.width)) + "]";
            }

            jam::debug::Log::write ("  row0: " + row0);

            // Dump second row if it exists
            if (totalDocRows > 1)
            {
                juce::String row1;

                for (int c { 0 }; c < dumpCols; ++c)
                {
                    const auto& cell { activeCells[static_cast<size_t> (cols + c)] };
                    row1 += "[" + juce::String::toHexString (static_cast<int> (cell.codepoint))
                            + " w=" + juce::String (static_cast<int> (cell.width)) + "]";
                }

                jam::debug::Log::write ("  row1: " + row1);
            }
        }
        // ---- End diagnostic instrumentation ----

        glyphGraphics.clear();

        const auto& activeCells { *cells.at (static_cast<size_t> (activeScreen)) };
        const int totalDocRows { static_cast<int> (activeCells.size()) / cols };

        // Shape cells into draw runs
        if (auto* tf = jam::Typeface::findTypeface (config.display.font.family))
            shapedText.shape (activeCells, *tf, cols);

        // ---- Diagnostic instrumentation (temporary) ----
        if (frameCount <= 50)
        {
            jam::debug::Log::write ("  drawRuns=" + juce::String (shapedText.getNumDrawRuns()));

            for (int r { 0 }; r < shapedText.getNumDrawRuns(); ++r)
            {
                const auto& run { shapedText.getDrawRun (r) };
                juce::String sample;

                for (int i { 0 }; i < juce::jmin (run.count, 10); ++i)
                {
                    sample += "(" + juce::String (run.cols[i]) + "," + juce::String (run.rows[i]) + " U+"
                              + juce::String::toHexString (static_cast<int> (run.codepoints[i])) + ")";
                }

                jam::debug::Log::write ("  run[" + juce::String (r) + "] count=" + juce::String (run.count)
                                        + " emoji=" + juce::String (run.isEmoji ? 1 : 0) + " first10: " + sample);
            }
        }
        // ---- End diagnostic instrumentation ----

        // Resize content to match document
        const int contentW { cols * cellW };
        const int contentH { juce::jmax (visibleRows, totalDocRows) * cellH };
        contentView->setSize (contentW, contentH);

        // Auto-scroll to bottom
        const int maxY { juce::jmax (0, contentH - viewport->getMaximumVisibleHeight()) };
        viewport->setViewPosition (0, maxY);

        contentView->repaint();
    }
}

void Screen::drawContent() noexcept
{
    if (shapedText.isValid())
    {
        auto& atlas { jam::Typeface::getAtlas() };
        const float scale { jam::Typeface::getDisplayScale() };
        const int physCellW { jam::toInt (static_cast<float> (cellW) * scale, true) };
        const int physCellH { jam::toInt (static_cast<float> (cellH) * scale, true) };
        const int physBase { jam::toInt (static_cast<float> (baseline) * scale, true) };

        for (int r { 0 }; r < shapedText.getNumDrawRuns(); ++r)
        {
            const auto& run { shapedText.getDrawRun (r) };

            if (run.count > 0 and run.fontHandle != nullptr)
            {
                juce::HeapBlock<juce::Point<float>> positions;
                positions.malloc (run.count);

                for (int i { 0 }; i < run.count; ++i)
                {
                    positions[i] = { static_cast<float> (run.cols[i] * cellW),
                                     static_cast<float> (run.rows[i] * cellH + baseline) };
                }

                const juce::Colour resolvedColour { run.colour.getAlpha() == 0 ? config.display.colours.foreground
                                                                               : run.colour };

                glyphGraphics.drawGlyphs (atlas,
                                          run.fontHandle,
                                          run.glyphCodes.getData(),
                                          run.codepoints.getData(),
                                          run.spans.getData(),
                                          run.styles.getData(),
                                          run.bgColours.getData(),
                                          positions.getData(),
                                          run.count,
                                          fontSize,
                                          resolvedColour,
                                          run.isEmoji,
                                          physCellW,
                                          physCellH,
                                          physBase);
            }
        }
    }
}

void Screen::appendScrollbackRows (const jam::Cell* const* rows, int rowCount, int numCols) noexcept
{
    const int scr { activeScreen };

    if (cols > 0 and rows != nullptr and rowCount > 0 and scr >= 0 and scr < static_cast<int> (cells.size())
        and cells.at (static_cast<size_t> (scr)) != nullptr)
    {
        auto& activeCells { *cells.at (static_cast<size_t> (scr)) };

        const int oldSize { static_cast<int> (activeCells.size()) };
        const int newSize { oldSize + rowCount * cols };
        activeCells.resize (static_cast<size_t> (newSize));

        // Shift visible region forward by rowCount rows (single memmove)
        const int visibleStart { scrollbackRows * cols };
        const int visibleCount { visibleRows * cols };

        if (visibleCount > 0 and visibleStart + visibleCount <= oldSize)
        {
            std::memmove (activeCells.data() + (scrollbackRows + rowCount) * cols,
                          activeCells.data() + scrollbackRows * cols,
                          static_cast<size_t> (visibleCount) * sizeof (jam::Cell));
        }

        // Copy each scrollback row into its slot
        for (int i { 0 }; i < rowCount; ++i)
        {
            const int dstIdx { (scrollbackRows + i) * cols };
            const int copyCount { juce::jmin (numCols, cols) };

            if (rows[i] != nullptr)
            {
                std::memcpy (
                    activeCells.data() + dstIdx, rows[i], static_cast<size_t> (copyCount) * sizeof (jam::Cell));
            }

            if (copyCount < cols)
            {
                std::memset (activeCells.data() + dstIdx + copyCount,
                             0,
                             static_cast<size_t> (cols - copyCount) * sizeof (jam::Cell));
            }
        }

        scrollbackRows += rowCount;
    }
}

void Screen::appendScrollbackRow (const jam::Cell* src, int numCols) noexcept
{
    appendScrollbackRows (&src, 1, numCols);
}

void Screen::updateVisibleRow (int row, const jam::Cell* src, int numCols) noexcept
{
    const int scr { activeScreen };

    if (cols > 0 and src != nullptr and row >= 0 and row < visibleRows and scr >= 0
        and scr < static_cast<int> (cells.size()) and cells.at (static_cast<size_t> (scr)) != nullptr)
    {
        auto& activeCells { *cells.at (static_cast<size_t> (scr)) };

        // Visible rows begin after scrollback
        const int targetIdx { (scrollbackRows + row) * cols };
        const int totalNeeded { (scrollbackRows + visibleRows) * cols };

        if (static_cast<int> (activeCells.size()) < totalNeeded)
            activeCells.resize (static_cast<size_t> (totalNeeded));

        const int copyCount { juce::jmin (numCols, cols) };
        std::memcpy (activeCells.data() + targetIdx, src, static_cast<size_t> (copyCount) * sizeof (jam::Cell));

        if (copyCount < cols)
        {
            std::memset (activeCells.data() + targetIdx + copyCount,
                         0,
                         static_cast<size_t> (cols - copyCount) * sizeof (jam::Cell));
        }
    }
}

void Screen::repaintContent() noexcept { shapeAndRepaint(); }

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
