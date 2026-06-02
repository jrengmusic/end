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
    const juce::ValueTree mouseDownRoot { processor.getState().getRootTree() };
    auto mouseDownPreviewParam { jam::ValueTree::getChildWithID (mouseDownRoot, terminal::id::preview.toString()) };
    const bool mouseDownPreviewActive { mouseDownPreviewParam.isValid() and static_cast<int> (mouseDownPreviewParam.getProperty (terminal::id::value)) != 0 };

    if (mouseDownPreviewActive)
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
        auto node { processor.getState().getChildWithName (jam::CodeView::properties.at (jam::CodeView::codeViewId)) };

        if (node.isValid())
        {
            node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionTypeId), static_cast<int> (terminal::SelectionType::visualLine), nullptr);
            node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionAnchorRowId), absRow, nullptr);
            node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionAnchorColId), 0, nullptr);
            node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionCursorRowId), absRow, nullptr);
            node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionCursorColId), 0, nullptr);
        }

        dragAnchor = { hitCell.x, absRow };
        dragActive = false;
    }
    else
    {
        const auto hitCell { jam::Cell::Point::fromPixel (juce::Point<int> { event.x, event.y }, physCellWidth, physCellHeight) };
        const int absRow { toAbsoluteRow (hitCell.y) };
        auto node { processor.getState().getChildWithName (jam::CodeView::properties.at (jam::CodeView::codeViewId)) };

        const bool handleDownModal { static_cast<ModalType> (AppModel::getContext()->getModalType()) != ModalType::none };
        if (not handleDownModal)
        {
            const terminal::LinkSpan* matched { linkManager.hitTest (hitCell.getY(), hitCell.getX()) };

            if (matched != nullptr)
            {
                linkManager.dispatch (*matched);

                if (node.isValid())
                    node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionTypeId), static_cast<int> (terminal::SelectionType::none), nullptr);

                dragAnchor = { hitCell.x, absRow };
                dragActive = false;
            }
            else
            {
                if (node.isValid())
                    node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionTypeId), static_cast<int> (terminal::SelectionType::none), nullptr);

                dragAnchor = { hitCell.x, absRow };
                dragActive = false;
            }
        }
        else
        {
            if (node.isValid())
                node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionTypeId), static_cast<int> (terminal::SelectionType::none), nullptr);

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
        auto node { processor.getState().getChildWithName (jam::CodeView::properties.at (jam::CodeView::codeViewId)) };

        if (node.isValid())
        {
            node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionTypeId), static_cast<int> (terminal::SelectionType::visual), nullptr);
            node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionAnchorRowId), absRow, nullptr);
            node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionAnchorColId), wordStart, nullptr);
            node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionCursorRowId), absRow, nullptr);
            node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionCursorColId), wordEnd, nullptr);
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
        // Replaces getCols() / getVisibleRows() — reads CODE_VIEW viewport packed rect directly.
        const auto dragCvNode  { processor.getState().getChildWithName (jam::CodeView::properties.at (jam::CodeView::codeViewId)) };
        const int64_t dragPacked { static_cast<int64_t> (dragCvNode.getProperty (jam::CodeView::properties.at (jam::CodeView::viewportId), 0)) };
        const auto dragRect    { jam::Cell::Rectangle::unpack (dragPacked) };
        const int maxCol { dragRect.getWidth().value - 1 };
        const int maxVisRow { dragRect.getHeight().value - 1 };

        const int clampedCol { juce::jlimit (0, maxCol, hitCell.x) };
        const int clampedVisRow { juce::jlimit (0, maxVisRow, hitCell.y) };
        const int clampedAbsRow { toAbsoluteRow (clampedVisRow) };

        const int manhattanDist { std::abs (clampedCol - dragAnchor.x)
                                + std::abs (clampedAbsRow - dragAnchor.y) };

        if (manhattanDist >= 2)
        {
            auto node { processor.getState().getChildWithName (jam::CodeView::properties.at (jam::CodeView::codeViewId)) };

            if (not dragActive)
            {
                if (node.isValid())
                {
                    node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionTypeId), static_cast<int> (terminal::SelectionType::visual), nullptr);
                    node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionAnchorRowId), dragAnchor.y, nullptr);
                    node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionAnchorColId), dragAnchor.x, nullptr);
                }

                dragActive = true;
            }

            if (node.isValid())
            {
                node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionCursorRowId), clampedAbsRow, nullptr);
                node.setProperty (jam::CodeView::properties.at (jam::CodeView::selectionCursorColId), clampedCol, nullptr);
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
    const bool handleMoveModal { static_cast<ModalType> (AppModel::getContext()->getModalType()) != ModalType::none };
    if (not shouldForwardToPty() and not handleMoveModal)
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
    const auto activeScreen { static_cast<int> (jam::ValueTree::getValueFromChildWithID (processor.getState().getRootTree(), terminal::id::activeScreen).getValue()) };

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
    const juce::ValueTree modesNode { st.getChildWithName (terminal::id::MODES) };
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (modesNode, terminal::id::mouseTracking).getValue()) != 0
        or static_cast<int> (jam::ValueTree::getValueFromChildWithID (modesNode, terminal::id::mouseMotionTracking).getValue()) != 0
        or static_cast<int> (jam::ValueTree::getValueFromChildWithID (modesNode, terminal::id::mouseAllTracking).getValue()) != 0;
}

int Mouse::toAbsoluteRow (int visibleRow) const noexcept
{
    return visibleRow;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
