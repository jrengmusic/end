#include "Screen.h"
#include <cstring>

namespace Terminal
{ /*____________________________________________________________________________*/

struct Screen::ContentView : public juce::Component
{
    explicit ContentView (Screen& o) : owner (o)
    {
        setWantsKeyboardFocus (false);
        setInterceptsMouseClicks (false, true);
    }

    void paint (juce::Graphics& g) override
    {
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
    cells.add (std::make_unique<jam::Cells>());  // Normal screen (index 0)
    cells.add (std::make_unique<jam::Cells>());  // Alternate screen (index 1)

    viewport = std::make_unique<juce::Viewport>();
    addAndMakeVisible (viewport.get());
    viewport->setViewedComponent (contentView = new ContentView (*this), true);
    viewport->setScrollBarsShown (true, false);

    caret = std::make_unique<jam::CaretComponent> (this);
    contentView->addChildComponent (caret.get());

    setColour (juce::CaretComponent::caretColourId, config.display.colours.cursor);
    setOpaque (false);
    setWantsKeyboardFocus (true);

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
            const float ascent  { fm.ascent  * fs };
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

            cellW    = jam::toInt (maxAdvance, true);
            cellH    = jam::toInt (ascent + descent + leading, true);
            baseline = jam::toInt (ascent, true);
            fontSize = fs;
        }
    }
}

void Screen::resized()
{
    viewport->setBounds (getLocalBounds());
}

void Screen::setScrollBarThickness (int thickness) noexcept
{
    viewport->setScrollBarThickness (thickness);
}

void Screen::setDimensions (int newCols, int newRows) noexcept
{
    cols           = newCols;
    visibleRows    = newRows;
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
    activeScreen   = index;
    scrollbackRows = 0;
    shapeAndRepaint();
}

void Screen::setCaretPosition (int index) noexcept
{
    if (caret != nullptr and cellW > 0 and cellH > 0)
    {
        const int col { (cols > 0) ? (index % cols) : 0 };
        const int row { (cols > 0) ? (index / cols) : 0 };

        caret->setCaretPosition ({ col * cellW, row * cellH, cellW, cellH });
        caret->setVisible (true);
    }
}

void Screen::shapeAndRepaint() noexcept
{
    if (activeScreen >= 0 and activeScreen < static_cast<int> (cells.size())
        and cells.at (static_cast<size_t> (activeScreen)) != nullptr
        and cols > 0 and cellW > 0 and cellH > 0)
    {
        const auto& activeCells { *cells.at (static_cast<size_t> (activeScreen)) };
        const int totalDocRows { static_cast<int> (activeCells.size()) / cols };

        // Shape cells into draw runs
        if (auto* tf = jam::Typeface::findTypeface (config.display.font.family))
            shapedText.shape (activeCells, *tf, cols);

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
        const int physBase  { jam::toInt (static_cast<float> (baseline) * scale, true) };

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

                const juce::Colour resolvedColour { run.colour.getAlpha() == 0
                                                    ? config.display.colours.foreground
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

    if (cols > 0 and rows != nullptr and rowCount > 0
        and scr >= 0 and scr < static_cast<int> (cells.size())
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
                std::memcpy (activeCells.data() + dstIdx,
                             rows[i],
                             static_cast<size_t> (copyCount) * sizeof (jam::Cell));
            }

            if (copyCount < cols)
            {
                std::memset (activeCells.data() + dstIdx + copyCount, 0,
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

    if (cols > 0 and src != nullptr and row >= 0 and row < visibleRows
        and scr >= 0 and scr < static_cast<int> (cells.size())
        and cells.at (static_cast<size_t> (scr)) != nullptr)
    {
        auto& activeCells { *cells.at (static_cast<size_t> (scr)) };

        // Visible rows begin after scrollback
        const int targetIdx { (scrollbackRows + row) * cols };
        const int totalNeeded { (scrollbackRows + visibleRows) * cols };

        if (static_cast<int> (activeCells.size()) < totalNeeded)
            activeCells.resize (static_cast<size_t> (totalNeeded));

        const int copyCount { juce::jmin (numCols, cols) };
        std::memcpy (activeCells.data() + targetIdx,
                     src,
                     static_cast<size_t> (copyCount) * sizeof (jam::Cell));

        if (copyCount < cols)
        {
            std::memset (activeCells.data() + targetIdx + copyCount, 0,
                         static_cast<size_t> (cols - copyCount) * sizeof (jam::Cell));
        }
    }
}

void Screen::repaintContent() noexcept
{
    shapeAndRepaint();
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
