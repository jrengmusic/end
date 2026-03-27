#include "TableBlock.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

//==============================================================================
TableBlock::TableBlock (juce::Font bodyFont)
    : font       (bodyFont),
      headerFont (bodyFont.withStyle (juce::Font::bold))
{
}

//==============================================================================
void TableBlock::setTableMarkdown (const juce::String& markdown)
{
    parseMarkdown (markdown);
}

void TableBlock::setColourScheme (const ColourScheme& scheme)
{
    colours = scheme;
}

void TableBlock::layout (int width)
{
    if (width != lastMeasuredWidth and width > 0)
    {
        measureLayout (width);
        lastMeasuredWidth = width;
    }
}

int TableBlock::getPreferredHeight (int /*width*/) const noexcept
{
    return layoutCache.totalHeight;
}

//==============================================================================
void TableBlock::paint (juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (layoutCache.isValid())
    {
        g.saveState();
        g.setOrigin (area.getX(), area.getY());

        g.setColour (colours.background);
        g.fillRect (0, 0, layoutCache.measuredWidth, layoutCache.totalHeight);

        paintRows   (g);
        paintHeader (g);
        paintBorders (g);

        g.restoreState();
    }
}

//==============================================================================
// Parsing
//==============================================================================
void TableBlock::parseMarkdown (const juce::String& markdown)
{
    table = {};

    const auto lines { juce::StringArray::fromLines (markdown.trim()) };

    if (lines.size() >= 2)
    {
        auto splitRow = [] (const juce::String& line) -> juce::StringArray
        {
            auto s { line.trim() };

            if (s.startsWith ("|")) s = s.substring (1);
            if (s.endsWith   ("|")) s = s.dropLastCharacters (1);

            juce::StringArray cells;
            cells.addTokens (s, "|", "");

            for (auto& c : cells)
                c = c.trim();

            return cells;
        };

        const auto headerCells { splitRow (lines[0]) };
        table.numCols = headerCells.size();

        for (const auto& h : headerCells)
        {
            Cell c;
            c.text  = h;
            c.align = Align::Left;
            table.headers.add (c);
        }

        juce::Array<Align> alignments;

        for (const auto& sep : splitRow (lines[1]))
        {
            const bool left  { sep.startsWith (":") };
            const bool right { sep.endsWith (":") };

            if (left and right)
                alignments.add (Align::Centre);
            else if (right)
                alignments.add (Align::Right);
            else
                alignments.add (Align::Left);
        }

        for (int i { 0 }; i < juce::jmin (alignments.size(), table.headers.size()); ++i)
            table.headers.getReference (i).align = alignments[i];

        for (int lineIdx { 2 }; lineIdx < lines.size(); ++lineIdx)
        {
            const auto rawLine { lines[lineIdx].trim() };

            if (not rawLine.isEmpty())
            {
                const auto cells { splitRow (rawLine) };
                juce::Array<Cell> row;

                for (int c { 0 }; c < table.numCols; ++c)
                {
                    Cell cell;
                    cell.text  = c < cells.size() ? cells[c] : juce::String{};
                    cell.align = c < alignments.size() ? alignments[c] : Align::Left;
                    row.add (cell);
                }

                table.rows.add (row);
            }
        }
    }
}

//==============================================================================
// Layout
//==============================================================================
juce::Array<int> TableBlock::distributeColumns (int forWidth) const
{
    juce::Array<int> result;

    if (table.numCols > 0)
    {
        auto textWidth = [] (const juce::Font& f, const juce::String& text) -> float
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (f, text, 0.0f, 0.0f);
            return ga.getBoundingBox (0, -1, false).getWidth();
        };

        juce::Array<float> weights;
        float totalWeight { 0.0f };

        for (int c { 0 }; c < table.numCols; ++c)
        {
            float maxW { textWidth (headerFont, table.headers[c].text)
                       + kCellPaddingH * 2 };

            for (const auto& row : table.rows)
            {
                if (c < row.size())
                {
                    const float w { textWidth (font, row[c].text)
                                  + kCellPaddingH * 2 };
                    maxW = juce::jmax (maxW, w);
                }
            }

            maxW = juce::jmax (maxW, (float) kMinColWidth);
            weights.add (maxW);
            totalWeight += maxW;
        }

        const int available { forWidth - kBorderWidth * (table.numCols + 1) };

        int allocated { 0 };

        for (int c { 0 }; c < table.numCols; ++c)
        {
            const int w { (c < table.numCols - 1)
                        ? juce::roundToInt ((weights[c] / totalWeight) * available)
                        : available - allocated };

            result.add (juce::jmax (w, kMinColWidth));
            allocated += result.getLast();
        }
    }

    return result;
}

juce::TextLayout TableBlock::makeLayout (const Cell& cell, int colWidth,
                                         bool isHeader, int /*maxWidth*/) const
{
    const int innerWidth { juce::jmax (1, colWidth - kCellPaddingH * 2) };

    juce::AttributedString as;
    as.setText (cell.text);
    as.setFont (isHeader ? headerFont : font);
    as.setColour (isHeader ? colours.headerText : colours.cellText);

    switch (cell.align)
    {
        case Align::Centre: as.setJustification (juce::Justification::centredTop); break;
        case Align::Right:  as.setJustification (juce::Justification::topRight);   break;
        default:            as.setJustification (juce::Justification::topLeft);    break;
    }

    juce::TextLayout tl;
    tl.createLayout (as, (float) innerWidth);

    return tl;
}

int TableBlock::measureHeaderHeight (int /*forWidth*/, const juce::Array<int>& colWidths) const
{
    int maxH { 0 };

    for (int c { 0 }; c < table.headers.size(); ++c)
    {
        const int colW { c < colWidths.size() ? colWidths[c] : kMinColWidth };
        const auto tl  { makeLayout (table.headers[c], colW, true, colW) };
        maxH = juce::jmax (maxH, juce::roundToInt (tl.getHeight()));
    }

    return maxH + kCellPaddingV * 2;
}

int TableBlock::measureRowHeight (int rowIndex, int /*forWidth*/,
                                   const juce::Array<int>& colWidths) const
{
    int result { 0 };

    if (rowIndex < table.rows.size())
    {
        const auto& row { table.rows[rowIndex] };
        int maxH { 0 };

        for (int c { 0 }; c < row.size(); ++c)
        {
            const int colW { c < colWidths.size() ? colWidths[c] : kMinColWidth };
            const auto tl  { makeLayout (row[c], colW, false, colW) };
            maxH = juce::jmax (maxH, juce::roundToInt (tl.getHeight()));
        }

        result = maxH + kCellPaddingV * 2;
    }

    return result;
}

void TableBlock::measureLayout (int forWidth)
{
    layoutCache = {};

    if (not table.isEmpty() and forWidth > 0)
    {
        layoutCache.measuredWidth = forWidth;
        layoutCache.colWidths     = distributeColumns (forWidth);

        layoutCache.headerHeight = measureHeaderHeight (forWidth, layoutCache.colWidths);

        for (int c { 0 }; c < table.headers.size(); ++c)
        {
            const int colW { c < layoutCache.colWidths.size() ? layoutCache.colWidths[c] : kMinColWidth };
            auto* tl { new juce::TextLayout (makeLayout (table.headers[c], colW, true, colW)) };
            layoutCache.headerLayouts.add (tl);
        }

        for (int r { 0 }; r < table.rows.size(); ++r)
        {
            layoutCache.rowHeights.add (measureRowHeight (r, forWidth, layoutCache.colWidths));

            const auto& row { table.rows[r] };

            for (int c { 0 }; c < table.numCols; ++c)
            {
                const int colW { c < layoutCache.colWidths.size() ? layoutCache.colWidths[c] : kMinColWidth };
                const Cell& cell { c < row.size() ? row[c] : Cell{} };
                auto* tl { new juce::TextLayout (makeLayout (cell, colW, false, colW)) };
                layoutCache.cellLayouts.add (tl);
            }
        }

        layoutCache.totalHeight = kBorderWidth;
        layoutCache.totalHeight += layoutCache.headerHeight;
        layoutCache.totalHeight += kBorderWidth;

        for (int r { 0 }; r < layoutCache.rowHeights.size(); ++r)
        {
            layoutCache.totalHeight += layoutCache.rowHeights[r];
            layoutCache.totalHeight += kBorderWidth;
        }
    }
}

//==============================================================================
// Geometry
//==============================================================================
juce::Rectangle<int> TableBlock::getCellBounds (int row, int col) const
{
    juce::Rectangle<int> result;

    if (layoutCache.isValid() and col >= 0 and col < layoutCache.colWidths.size())
    {
        int x { kBorderWidth };

        for (int c { 0 }; c < col; ++c)
            x += layoutCache.colWidths[c] + kBorderWidth;

        const int w { layoutCache.colWidths[col] };

        if (row == -1)
        {
            result = { x, kBorderWidth, w, layoutCache.headerHeight };
        }
        else
        {
            int y { kBorderWidth + layoutCache.headerHeight + kBorderWidth };

            for (int r { 0 }; r < row; ++r)
                y += layoutCache.rowHeights[r] + kBorderWidth;

            result = { x, y, w, layoutCache.rowHeights[row] };
        }
    }

    return result;
}

//==============================================================================
// Paint
//==============================================================================
void TableBlock::paintHeader (juce::Graphics& g) const
{
    for (int c { 0 }; c < table.headers.size(); ++c)
    {
        const auto bounds { getCellBounds (-1, c) };
        g.setColour (colours.headerBackground);
        g.fillRect (bounds);

        if (c < layoutCache.headerLayouts.size())
        {
            const auto inner { bounds.reduced (kCellPaddingH, kCellPaddingV).toFloat() };
            layoutCache.headerLayouts[c]->draw (g, inner);
        }
    }
}

void TableBlock::paintRows (juce::Graphics& g) const
{
    for (int r { 0 }; r < table.rows.size(); ++r)
    {
        const bool isAlt { (r % 2) == 1 };

        for (int c { 0 }; c < table.numCols; ++c)
        {
            const auto bounds { getCellBounds (r, c) };

            g.setColour (isAlt ? colours.rowAlt : colours.background);
            g.fillRect (bounds);

            const int layoutIdx { r * table.numCols + c };

            if (layoutIdx < layoutCache.cellLayouts.size())
            {
                const auto inner { bounds.reduced (kCellPaddingH, kCellPaddingV).toFloat() };
                layoutCache.cellLayouts[layoutIdx]->draw (g, inner);
            }
        }
    }
}

void TableBlock::paintBorders (juce::Graphics& g) const
{
    g.setColour (colours.borderColour);

    const int totalW { layoutCache.measuredWidth };
    const int totalH { layoutCache.totalHeight };

    g.drawRect (0, 0, totalW, totalH, kBorderWidth);

    int x { kBorderWidth };

    for (int c { 0 }; c < layoutCache.colWidths.size() - 1; ++c)
    {
        x += layoutCache.colWidths[c];
        g.drawVerticalLine (x, 0.0f, (float) totalH);
        x += kBorderWidth;
    }

    const int headerBottom { kBorderWidth + layoutCache.headerHeight };
    g.setColour (colours.borderColour.brighter (0.3f));
    g.drawHorizontalLine (headerBottom, 0.0f, (float) totalW);

    g.setColour (colours.borderColour);
    int y { headerBottom + kBorderWidth };

    for (int r { 0 }; r < layoutCache.rowHeights.size() - 1; ++r)
    {
        y += layoutCache.rowHeights[r];
        g.drawHorizontalLine (y, 0.0f, (float) totalW);
        y += kBorderWidth;
    }
}

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
