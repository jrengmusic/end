#include "Screen.h"

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
    viewport->setViewedComponent (contentView = new ContentView (*this), false);
    viewport->setScrollBarsShown (true, false);

    caret = std::make_unique<jam::CaretComponent> (this);
    contentView->addChildComponent (caret.get());

    setColour (juce::CaretComponent::caretColourId, config.display.colours.cursor);
    setOpaque (false);
    setWantsKeyboardFocus (true);

    computeCellMetrics();
}

Screen::~Screen()
{
    viewport.reset();
    contentView = nullptr;
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
    cols        = newCols;
    visibleRows = newRows;

    // Normal screen: ensure at least visibleRows, never shrink content
    if (cells.at (0) != nullptr)
    {
        const int minSize { newRows * newCols };

        if (cells.at (0)->size() < minSize)
            cells.at (0)->resize (minSize);
    }

    // Alternate screen: fixed framebuffer, exact size
    if (cells.at (1) != nullptr)
        cells.at (1)->resize (newRows * newCols);

    shapeAndRepaint();
}

void Screen::setActive (int index) noexcept
{
    activeScreen = index;
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

void Screen::makeLayout (const Command* commands, int count) noexcept
{
    const int scr { activeScreen };

    if (scr >= 0 and scr < static_cast<int> (cells.size())
        and cells.at (static_cast<size_t> (scr)) != nullptr
        and cols > 0 and visibleRows > 0)
    {
        auto& activeCells { *cells.at (static_cast<size_t> (scr)) };

        for (int i { 0 }; i < count; ++i)
        {
            const auto& cmd { commands[i] };
            const int totalDocRows { static_cast<int> (activeCells.size()) / cols };
            const int firstVisible { juce::jmax (0, totalDocRows - visibleRows) };

            switch (cmd.type)
            {
                case Command::Type::Print:
                {
                    const int logicalRow { firstVisible + cmd.row };
                    const int idx { logicalRow * cols + cmd.col };

                    if (idx >= static_cast<int> (activeCells.size()))
                        activeCells.resize (static_cast<size_t> ((logicalRow + 1) * cols));

                    activeCells[static_cast<size_t> (idx)] = cmd.cell;
                    break;
                }

                case Command::Type::LineFeed:
                {
                    const int newTotalRows { totalDocRows + 1 };
                    activeCells.resize (static_cast<size_t> (newTotalRows * cols));

                    if (cmd.fillBg.getAlpha() > 0)
                    {
                        const int rowStart { (newTotalRows - 1) * cols };

                        for (int c { 0 }; c < cols; ++c)
                            activeCells[static_cast<size_t> (rowStart + c)].bg = cmd.fillBg;
                    }

                    break;
                }

                case Command::Type::CarriageReturn:
                case Command::Type::Tab:
                case Command::Type::Backspace:
                    break;

                case Command::Type::EraseInLine:
                {
                    const int logicalRow { firstVisible + cmd.row };
                    const int rowStart { logicalRow * cols };

                    if (rowStart + cols <= static_cast<int> (activeCells.size()))
                    {
                        const jam::Cell fill { 0, 0, 0, 1, 0, {}, cmd.fillBg };

                        if (cmd.param == 0)
                        {
                            for (int c { cmd.col }; c < cols; ++c)
                                activeCells[static_cast<size_t> (rowStart + c)] = fill;
                        }
                        else if (cmd.param == 1)
                        {
                            for (int c { 0 }; c <= cmd.col; ++c)
                                activeCells[static_cast<size_t> (rowStart + c)] = fill;
                        }
                        else
                        {
                            for (int c { 0 }; c < cols; ++c)
                                activeCells[static_cast<size_t> (rowStart + c)] = fill;
                        }
                    }

                    break;
                }

                case Command::Type::EraseInDisplay:
                {
                    const jam::Cell fill { 0, 0, 0, 1, 0, {}, cmd.fillBg };

                    if (cmd.param == 0)
                    {
                        const int logicalRow { firstVisible + cmd.row };
                        const int startIdx { logicalRow * cols + cmd.col };
                        const int endIdx { (firstVisible + visibleRows) * cols };

                        for (int c { startIdx }; c < juce::jmin (endIdx, static_cast<int> (activeCells.size())); ++c)
                            activeCells[static_cast<size_t> (c)] = fill;
                    }
                    else if (cmd.param == 1)
                    {
                        const int startIdx { firstVisible * cols };
                        const int endIdx { (firstVisible + cmd.row) * cols + cmd.col };

                        for (int c { startIdx }; c <= juce::jmin (endIdx, static_cast<int> (activeCells.size()) - 1); ++c)
                            activeCells[static_cast<size_t> (c)] = fill;
                    }
                    else
                    {
                        const int startIdx { firstVisible * cols };
                        const int endIdx { (firstVisible + visibleRows) * cols };

                        for (int c { startIdx }; c < juce::jmin (endIdx, static_cast<int> (activeCells.size())); ++c)
                            activeCells[static_cast<size_t> (c)] = fill;
                    }

                    break;
                }

                case Command::Type::InsertLines:
                {
                    const int logicalRow { firstVisible + cmd.row };
                    const int visibleBottom { firstVisible + visibleRows - 1 };
                    const int linesToInsert { juce::jmin (cmd.param, visibleBottom - logicalRow + 1) };

                    if (logicalRow >= firstVisible and logicalRow <= visibleBottom and linesToInsert > 0
                        and (visibleBottom + 1) * cols <= static_cast<int> (activeCells.size()))
                    {
                        if (visibleBottom - linesToInsert >= logicalRow)
                        {
                            std::memmove (activeCells.data() + (logicalRow + linesToInsert) * cols,
                                          activeCells.data() + logicalRow * cols,
                                          static_cast<size_t> ((visibleBottom - logicalRow - linesToInsert + 1) * cols) * sizeof (jam::Cell));
                        }

                        const jam::Cell fill { 0, 0, 0, 1, 0, {}, cmd.fillBg };

                        for (int r { logicalRow }; r < logicalRow + linesToInsert; ++r)
                        {
                            for (int c { 0 }; c < cols; ++c)
                                activeCells[static_cast<size_t> (r * cols + c)] = fill;
                        }
                    }

                    break;
                }

                case Command::Type::DeleteLines:
                {
                    const int logicalRow { firstVisible + cmd.row };
                    const int visibleBottom { firstVisible + visibleRows - 1 };
                    const int linesToDelete { juce::jmin (cmd.param, visibleBottom - logicalRow + 1) };

                    if (logicalRow >= firstVisible and logicalRow <= visibleBottom and linesToDelete > 0
                        and (visibleBottom + 1) * cols <= static_cast<int> (activeCells.size()))
                    {
                        if (logicalRow + linesToDelete <= visibleBottom)
                        {
                            std::memmove (activeCells.data() + logicalRow * cols,
                                          activeCells.data() + (logicalRow + linesToDelete) * cols,
                                          static_cast<size_t> ((visibleBottom - logicalRow - linesToDelete + 1) * cols) * sizeof (jam::Cell));
                        }

                        const jam::Cell fill { 0, 0, 0, 1, 0, {}, cmd.fillBg };

                        for (int r { visibleBottom - linesToDelete + 1 }; r <= visibleBottom; ++r)
                        {
                            for (int c { 0 }; c < cols; ++c)
                                activeCells[static_cast<size_t> (r * cols + c)] = fill;
                        }
                    }

                    break;
                }

                case Command::Type::InsertChars:
                {
                    const int logicalRow { firstVisible + cmd.row };
                    const int rowStart { logicalRow * cols };

                    if (rowStart + cols <= static_cast<int> (activeCells.size()))
                    {
                        const int charsToInsert { juce::jmin (cmd.param, cols - cmd.col) };

                        if (charsToInsert > 0 and cmd.col < cols)
                        {
                            std::memmove (activeCells.data() + rowStart + cmd.col + charsToInsert,
                                          activeCells.data() + rowStart + cmd.col,
                                          static_cast<size_t> (cols - cmd.col - charsToInsert) * sizeof (jam::Cell));

                            const jam::Cell fill { 0, 0, 0, 1, 0, {}, cmd.fillBg };

                            for (int c { cmd.col }; c < cmd.col + charsToInsert; ++c)
                                activeCells[static_cast<size_t> (rowStart + c)] = fill;
                        }
                    }

                    break;
                }

                case Command::Type::DeleteChars:
                {
                    const int logicalRow { firstVisible + cmd.row };
                    const int rowStart { logicalRow * cols };

                    if (rowStart + cols <= static_cast<int> (activeCells.size()))
                    {
                        const int charsToDelete { juce::jmin (cmd.param, cols - cmd.col) };

                        if (charsToDelete > 0 and cmd.col < cols)
                        {
                            std::memmove (activeCells.data() + rowStart + cmd.col,
                                          activeCells.data() + rowStart + cmd.col + charsToDelete,
                                          static_cast<size_t> (cols - cmd.col - charsToDelete) * sizeof (jam::Cell));

                            const jam::Cell fill { 0, 0, 0, 1, 0, {}, cmd.fillBg };

                            for (int c { cols - charsToDelete }; c < cols; ++c)
                                activeCells[static_cast<size_t> (rowStart + c)] = fill;
                        }
                    }

                    break;
                }

                case Command::Type::EraseChars:
                {
                    const int logicalRow { firstVisible + cmd.row };
                    const int rowStart { logicalRow * cols };

                    if (rowStart + cols <= static_cast<int> (activeCells.size()))
                    {
                        const jam::Cell fill { 0, 0, 0, 1, 0, {}, cmd.fillBg };

                        for (int c { cmd.col }; c < juce::jmin (cmd.col + cmd.param, cols); ++c)
                            activeCells[static_cast<size_t> (rowStart + c)] = fill;
                    }

                    break;
                }

                case Command::Type::SetScreen:
                case Command::Type::ClearScrollback:
                case Command::Type::SetTabStop:
                case Command::Type::ClearTabStop:
                case Command::Type::ClearAllTabStops:
                    break;
            }
        }

        shapeAndRepaint();
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
