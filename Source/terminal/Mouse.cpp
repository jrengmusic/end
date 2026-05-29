/**
 * @file Mouse.cpp
 * @brief Implementation of mouse input routing for a terminal processor.
 *
 * Selection state is written to TextEditor's grafted node.  Drag state is
 * Mouse-private.  The message thread rebuilds ScreenSelection from the TextEditor
 * node props during render — this handler never touches ScreenSelection directly.
 *
 * @see terminal::Mouse
 */

#include "Mouse.h"

namespace terminal
{
/*____________________________________________________________________________*/

Mouse::Mouse (Processor& p,
              int pcw,
              int pch,
              LinkManager& lm) noexcept
    : processor (p)
    , physCellWidth (pcw)
    , physCellHeight (pch)
    , linkManager (lm)
{
}

void Mouse::setCellSize (int cellWidth, int cellHeight) noexcept
{
    physCellWidth  = cellWidth;
    physCellHeight = cellHeight;
}

void Mouse::handleDown (const juce::MouseEvent& event)
{
    if (processor.getState().isPreviewActive())
    {
        processor.getState().dismissPreview();
    }
    else if (shouldForwardToPty())
    {
        const auto hitCell { jam::Cell::Point::fromPixel (juce::Point<int> { event.x, event.y }, physCellWidth, physCellHeight) };
        const auto bytes { processor.encodeMouseEvent (0, cell (hitCell.x), cell (hitCell.y), true) };

        processor.writeInput (bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));
    }
    else if (event.getNumberOfClicks() == 3)
    {
        const auto hitCell { jam::Cell::Point::fromPixel (juce::Point<int> { event.x, event.y }, physCellWidth, physCellHeight) };
        const int absRow { toAbsoluteRow (hitCell.y) };
        auto node { processor.getState().get().getChildWithName (jam::TextEditor::properties.at (jam::TextEditor::textEditorId)) };

        if (node.isValid())
        {
            node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionTypeId), static_cast<int> (terminal::SelectionType::visualLine), nullptr);
            node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionAnchorRowId), absRow, nullptr);
            node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionAnchorColId), 0, nullptr);
            node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionCursorRowId), absRow, nullptr);
            node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionCursorColId), 0, nullptr);
        }

        dragAnchor = { hitCell.x, absRow };
        dragActive = false;
    }
    else
    {
        const auto hitCell { jam::Cell::Point::fromPixel (juce::Point<int> { event.x, event.y }, physCellWidth, physCellHeight) };
        const int absRow { toAbsoluteRow (hitCell.y) };
        auto node { processor.getState().get().getChildWithName (jam::TextEditor::properties.at (jam::TextEditor::textEditorId)) };

        if (not processor.getState().isModal())
        {
            const terminal::LinkSpan* matched { linkManager.hitTest (hitCell.getY(), hitCell.getX()) };

            if (matched != nullptr)
            {
                linkManager.dispatch (*matched);

                if (node.isValid())
                    node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionTypeId), static_cast<int> (terminal::SelectionType::none), nullptr);

                dragAnchor = { hitCell.x, absRow };
                dragActive = false;
            }
            else
            {
                if (node.isValid())
                    node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionTypeId), static_cast<int> (terminal::SelectionType::none), nullptr);

                dragAnchor = { hitCell.x, absRow };
                dragActive = false;
            }
        }
        else
        {
            if (node.isValid())
                node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionTypeId), static_cast<int> (terminal::SelectionType::none), nullptr);

            dragAnchor = { hitCell.x, absRow };
            dragActive = false;
        }
    }
}

void Mouse::handleDoubleClick (const juce::MouseEvent& event)
{
    if (shouldForwardToPty())
    {
        const auto hitCell { jam::Cell::Point::fromPixel (juce::Point<int> { event.x, event.y }, physCellWidth, physCellHeight) };
        const auto bytes { processor.encodeMouseEvent (0, cell (hitCell.x), cell (hitCell.y), true) };

        processor.writeInput (bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));
    }
    else
    {
        const auto hitCell { jam::Cell::Point::fromPixel (juce::Point<int> { event.x, event.y }, physCellWidth, physCellHeight) };
        const int wordStart { hitCell.x };
        const int wordEnd { hitCell.x };
        const int absRow { toAbsoluteRow (hitCell.y) };
        auto node { processor.getState().get().getChildWithName (jam::TextEditor::properties.at (jam::TextEditor::textEditorId)) };

        if (node.isValid())
        {
            node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionTypeId), static_cast<int> (terminal::SelectionType::visual), nullptr);
            node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionAnchorRowId), absRow, nullptr);
            node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionAnchorColId), wordStart, nullptr);
            node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionCursorRowId), absRow, nullptr);
            node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionCursorColId), wordEnd, nullptr);
        }

        dragAnchor = { wordStart, absRow };
        dragActive = false;
    }
}

void Mouse::handleDrag (const juce::MouseEvent& event)
{
    if (shouldForwardToPty())
    {
        const auto hitCell { jam::Cell::Point::fromPixel (juce::Point<int> { event.x, event.y }, physCellWidth, physCellHeight) };
        const auto bytes { processor.encodeMouseEvent (32, cell (hitCell.x), cell (hitCell.y), true) };

        processor.writeInput (bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));
    }
    else
    {
        const auto hitCell { jam::Cell::Point::fromPixel (juce::Point<int> { event.x, event.y }, physCellWidth, physCellHeight) };
        const int maxCol { processor.getState().getCols().value - 1 };
        const int maxVisRow { processor.getState().getVisibleRows().value - 1 };

        const int clampedCol { juce::jlimit (0, maxCol, hitCell.x) };
        const int clampedVisRow { juce::jlimit (0, maxVisRow, hitCell.y) };
        const int clampedAbsRow { toAbsoluteRow (clampedVisRow) };

        const int manhattanDist { std::abs (clampedCol - dragAnchor.x)
                                + std::abs (clampedAbsRow - dragAnchor.y) };

        if (manhattanDist >= 2)
        {
            auto node { processor.getState().get().getChildWithName (jam::TextEditor::properties.at (jam::TextEditor::textEditorId)) };

            if (not dragActive)
            {
                if (node.isValid())
                {
                    node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionTypeId), static_cast<int> (terminal::SelectionType::visual), nullptr);
                    node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionAnchorRowId), dragAnchor.y, nullptr);
                    node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionAnchorColId), dragAnchor.x, nullptr);
                }

                dragActive = true;
            }

            if (node.isValid())
            {
                node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionCursorRowId), clampedAbsRow, nullptr);
                node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionCursorColId), clampedCol, nullptr);
            }
        }
    }
}

void Mouse::handleUp (const juce::MouseEvent& event)
{
    if (shouldForwardToPty())
    {
        const auto hitCell { jam::Cell::Point::fromPixel (juce::Point<int> { event.x, event.y }, physCellWidth, physCellHeight) };
        const auto bytes { processor.encodeMouseEvent (0, cell (hitCell.x), cell (hitCell.y), false) };

        processor.writeInput (bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));
    }
    else
    {
        dragActive = false;
    }
}

void Mouse::handleMove (const juce::MouseEvent& event, juce::Component& component)
{
    if (not shouldForwardToPty() and not processor.getState().isModal())
    {
        const auto hitCell { jam::Cell::Point::fromPixel (juce::Point<int> { event.x, event.y }, physCellWidth, physCellHeight) };
        const bool overLink { linkManager.hitTest (hitCell.getY(), hitCell.getX()) != nullptr };

        if (overLink)
        {
            component.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }
        else
        {
            component.setMouseCursor (juce::MouseCursor::NormalCursor);
        }
    }
    else
    {
        component.setMouseCursor (juce::MouseCursor::NormalCursor);
    }
}

void Mouse::handleWheel (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel,
                         std::function<void (int)> setScrollFn)
{
    const int scrollLines { lua::Engine::getContext()->nexus.terminal.scrollStep };
    const auto activeScreen { processor.getState().getActiveScreen() };

    if (not wheel.isSmooth)
    {
        const bool scrollUp { wheel.deltaY > 0.0f };

        if (activeScreen == Map::Screen::alternate)
        {
            if (shouldForwardToPty())
            {
                const int button { scrollUp ? 64 : 65 };
                const auto hitCell { jam::Cell::Point::fromPixel (juce::Point<int> { event.x, event.y }, physCellWidth, physCellHeight) };

                for (int i { 0 }; i < scrollLines; ++i)
                {
                    const auto bytes { processor.encodeMouseEvent (button, cell (hitCell.x), cell (hitCell.y), true) };

                    processor.writeInput (bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));
                }
            }
        }
        else
        {
            const int delta { scrollUp ? scrollLines : -scrollLines };
            setScrollFn (delta);
        }
    }
    else
    {
        // --- Smooth (trackpad) path ---
        scrollAccumulator += wheel.deltaY * static_cast<float> (scrollLines) * trackpadDeltaScale;

        const int lines { static_cast<int> (scrollAccumulator) };

        if (lines != 0)
        {
            scrollAccumulator -= static_cast<float> (lines);

            if (activeScreen == Map::Screen::alternate)
            {
                if (shouldForwardToPty())
                {
                    const int button { lines > 0 ? 64 : 65 };
                    const auto hitCell { jam::Cell::Point::fromPixel (juce::Point<int> { event.x, event.y }, physCellWidth, physCellHeight) };
                    const int count { std::abs (lines) };

                    for (int i { 0 }; i < count; ++i)
                    {
                        const auto bytes { processor.encodeMouseEvent (button, cell (hitCell.x), cell (hitCell.y), true) };

                        processor.writeInput (bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));
                    }
                }
            }
            else
            {
                setScrollFn (lines);
            }
        }
    }
}

bool Mouse::shouldForwardToPty() const noexcept
{
    const auto& st { processor.getState() };
    return st.getMode (terminal::id::mouseTracking)
        or st.getMode (terminal::id::mouseMotionTracking)
        or st.getMode (terminal::id::mouseAllTracking);
}

int Mouse::toAbsoluteRow (int visibleRow) const noexcept
{
    return visibleRow;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
