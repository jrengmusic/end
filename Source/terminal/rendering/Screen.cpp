#include "Screen.h"
#include "CellMetrics.h"
#include "../../AppState.h"

namespace Terminal
{ /*____________________________________________________________________________*/

Screen::Screen() noexcept
    : jam::TextEditor ("Screen")
{
    setMultiLine (true);
    setReadOnly (true);
    setScrollbarsShown (true);
    setCaretVisible (false);
    setOpaque (false);

    const auto* cfg { AppState::getContext() };
    if (cfg != nullptr)
    {
        setFont (juce::Font (juce::FontOptions (cfg->getFontFamily(), cfg->getFontSize(), juce::Font::plain)));
    }

    auto* typeface { jam::Typeface::findTypeface (AppState::getContext()->getFontFamily()) };

    if (typeface != nullptr)
    {
        juce::String dummy;

        for (int i { 0 }; i < 1000; ++i)
            dummy += "Line " + juce::String (i) + ": The quick brown fox jumps over the lazy dog\n";

        setText (dummy);
    }

}

Screen::~Screen() {}

void Screen::setScrollBarWidth (int width) noexcept
{
    setScrollBarThickness (width);
}

void Screen::render (const Grid& /*grid*/, const CursorInfo& cursor) noexcept
{
    // TEMP: dummy content to test rendering pipeline
    // Guard: only render if a typeface is registered for the configured font family

    lastCursor = cursor;
}

void Screen::updateCellMetrics()
{
    auto* typeface { jam::Typeface::findTypeface (AppState::getContext()->getFontFamily()) };

    if (typeface != nullptr)
    {
        const float fontSize { AppState::getContext()->getFontSize() };

        if (fontSize > 0.0f)
        {
            const float displayScale { jam::Typeface::getDisplayScale() };
            const auto fm { CellMetrics::compute (*typeface, fontSize, displayScale) };

            if (fm.isValid())
            {
                cellWidth = fm.cellWidth;
                cellHeight = fm.cellHeight;
                physCellWidth = fm.physCellWidth;
                physCellHeight = fm.physCellHeight;
            }
        }
    }
}

void Screen::resized()
{
    updateCellMetrics();
    jam::TextEditor::resized();
}

void Screen::lookAndFeelChanged()
{
    defaultFg = findColour (defaultForegroundColourId);
    defaultBg = findColour (defaultBackgroundColourId);

    for (int i { 0 }; i < 16; ++i)
        ansiColours.at (static_cast<size_t> (i)) = findColour (ansi0ColourId + i);
}

juce::Colour Screen::resolveColour (const Color& c, bool isFg) const noexcept
{
    juce::Colour result { isFg ? defaultFg : defaultBg };

    if (c.isRGB())
    {
        result = juce::Colour (c.red, c.green, c.blue);
    }
    else if (c.isPalette())
    {
        const int index { static_cast<int> (c.paletteIndex()) };

        if (index < 16)
        {
            result = ansiColours.at (static_cast<size_t> (index));
        }
        else if (index < 232)
        {
            const int i { index - 16 };
            const int b { i % 6 };
            const int g { (i / 6) % 6 };
            const int r { i / 36 };

            const auto expand { [] (int v) -> uint8_t
                                {
                                    return static_cast<uint8_t> (v == 0 ? 0 : 55 + v * 40);
                                } };

            result = juce::Colour (expand (r), expand (g), expand (b));
        }
        else
        {
            const int level { index - 232 };
            const auto grey { static_cast<uint8_t> (8 + level * 10) };
            result = juce::Colour (grey, grey, grey);
        }
    }

    return result;
}

void Screen::rebuildContent (const Grid& grid)
{
    const int lineCount { grid.getLineCount() };
    const int cols { grid.getCols() };

    juce::String fullText;
    fullText.preallocateBytes (static_cast<size_t> (lineCount * (cols + 1) * 4));

    for (int li { 0 }; li < lineCount; ++li)
    {
        const Grid::TerminalLine& line { grid.getLine (li) };
        const int len { line.length };

        for (int ci { 0 }; ci < len; ++ci)
        {
            const Cell& cell { line.cells.get()[ci] };

            if (not cell.isWideContinuation())
            {
                if (cell.codepoint != 0)
                {
                    fullText += juce::String::charToString (static_cast<juce::juce_wchar> (cell.codepoint));
                }
                else
                {
                    fullText += ' ';
                }
            }
        }

        if (li < lineCount - 1)
            fullText += '\n';
    }

    setText (fullText);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
