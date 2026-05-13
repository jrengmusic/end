#include "Screen.h"
#include <cstring>

namespace Terminal
{ /*____________________________________________________________________________*/

struct Screen::ContentView : public juce::Component
{
    explicit ContentView (Screen& o)
        : owner (o)
    {
        setWantsKeyboardFocus (false);
        setInterceptsMouseClicks (false, true);
        setBufferedToImage (true);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (findColour (juce::TextEditor::backgroundColourId));

        auto& atlas { jam::Typeface::getAtlas() };
        atlas.advanceFrame();

        const auto clip { g.getClipBounds() };

        owner.glyphGraphics.push (jam::Bounds { owner.getWidth(), owner.getHeight() }, clip);

        owner.drawContent();

        owner.glyphGraphics.pop (g, clip.getX(), clip.getY());
    }

private:
    Screen& owner;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentView)
};

Screen::~Screen() = default;

Screen::Screen (State& stateRef) noexcept
    : state (stateRef)
{
    cells.add (std::make_unique<jam::Cells>());// Normal screen (index 0)
    cells.add (std::make_unique<jam::Cells>());// Alternate screen (index 1)

    contentView = std::make_unique<ContentView> (*this);
    viewport = std::make_unique<juce::Viewport>();
    addAndMakeVisible (viewport.get());
    viewport->setViewedComponent (contentView.get(), false);
    viewport->setScrollBarsShown (true, false);

    caret = std::make_unique<jam::CaretComponent> (this);
    contentView->addChildComponent (caret.get());

    setColour (juce::CaretComponent::caretColourId, config.display.colours.cursor);
    // setOpaque (false);
    setWantsKeyboardFocus (true);
    toFront (true);
}

void Screen::resized() { viewport->setBounds (getLocalBounds()); }

void Screen::setScrollBarThickness (int thickness) noexcept { viewport->setScrollBarThickness (thickness); }

void Screen::setDimensions (int newCols, int newRows, const jam::Font& font) noexcept
{
    scrollbackRows = 0;

    if (cells.at (0) != nullptr)
        cells.at (0)->resize (static_cast<size_t> (newRows * newCols));

    if (cells.at (1) != nullptr)
        cells.at (1)->resize (static_cast<size_t> (newRows * newCols));

    caret->setFont (font);
    caret->setChar (jam::toChar (config.display.cursor.codepoint));
    caret->setShape (config.display.cursor.style);

    shapeAndRepaint (font);
}

void Screen::setActive (int index) noexcept
{
    juce::ignoreUnused (index);
    scrollbackRows = 0;

    const jam::Font font { config.display.font.family, state.getFontSize() };
    shapeAndRepaint (font);
}

void Screen::setCaretPosition (int index) noexcept
{
    const int cellW { state.getCellWidth() };
    const int cellH { state.getCellHeight() };

    if (cellW > 0 and cellH > 0)
    {
        const int cols { state.getCols() };
        const int col { (cols > 0) ? (index % cols) : 0 };
        const int row { (cols > 0) ? (index / cols) : 0 };

        caret->setCaretPosition ({ col * cellW, row * cellH, cellW, cellH });
        caret->setVisible (true);
    }
}

void Screen::shapeAndRepaint (const jam::Font& font) noexcept
{
    const int activeScreen { state.getActiveScreen() };
    const int cols         { state.getCols() };
    const int cellW        { state.getCellWidth() };
    const int cellH        { state.getCellHeight() };
    const int visibleRows  { state.getVisibleRows() };

    if (activeScreen >= 0 and activeScreen < static_cast<int> (cells.size())
        and cells.at (static_cast<size_t> (activeScreen)) != nullptr and cols > 0 and cellW > 0 and cellH > 0)
    {
        glyphGraphics.clear();

        const auto& activeCells { *cells.at (static_cast<size_t> (activeScreen)) };
        const int totalDocRows { static_cast<int> (activeCells.size()) / cols };

        arrangement.shape (activeCells, font, cols);

        const int contentW { cols * cellW };
        const int contentH { juce::jmax (visibleRows, totalDocRows) * cellH };
        contentView->setSize (contentW, contentH);

        const int maxY { juce::jmax (0, contentH - viewport->getMaximumVisibleHeight()) };
        viewport->setViewPosition (0, maxY);

        contentView->repaint();
    }
}

void Screen::drawContent() noexcept
{
    if (arrangement.isValid())
    {
        auto& atlas { jam::Typeface::getAtlas() };
        const int cellW      { state.getCellWidth() };
        const int cellH      { state.getCellHeight() };
        const int baseline   { state.getBaseline() };
        const float fontSize { state.getFontSize() };

        for (int r { 0 }; r < arrangement.getNumDrawRuns(); ++r)
        {
            const auto& run { arrangement.getDrawRun (r) };

            if (run.count > 0 and run.fontHandle != nullptr)
            {
                const juce::Colour resolvedColour { run.colour.getAlpha() == 0 ? config.display.colours.foreground
                                                                               : run.colour };

                glyphGraphics.drawGlyphs (atlas,
                                          run.fontHandle,
                                          run.glyphCodes.getData(),
                                          run.codepoints.getData(),
                                          run.spans.getData(),
                                          run.styles.getData(),
                                          run.bgColours.getData(),
                                          run.positions.getData(),
                                          run.count,
                                          fontSize,
                                          resolvedColour,
                                          run.isEmoji,
                                          jam::Bounds { cellW, cellH },
                                          baseline);
            }
        }
    }
}

void Screen::appendScrollbackRows (const jam::Cell* const* rows, int rowCount, int numCols) noexcept
{
    const int scr        { state.getActiveScreen() };
    const int cols       { state.getCols() };
    const int visibleRows { state.getVisibleRows() };

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
    const int scr        { state.getActiveScreen() };
    const int cols       { state.getCols() };
    const int visibleRows { state.getVisibleRows() };

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

void Screen::repaintContent() noexcept
{
    const jam::Font font { config.display.font.family, state.getFontSize() };
    shapeAndRepaint (font);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
