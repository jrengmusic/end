/**
 * @file Mouse.cpp
 * @brief Implementation of mouse input routing for a terminal processor.
 *
 * All selection state is written to State parameters.  TerminalDisplay::onVBlank()
 * builds ScreenSelection from those State params — this handler never touches
 * ScreenSelection directly.
 *
 * @see Terminal::Mouse
 */

#include "Mouse.h"
#include "Processor.h"
#include <jam_tui/jam_tui.h>
#include "../selection/LinkManager.h"
#include "../../SelectionType.h"
#include "../data/Identifier.h"
#include "../data/State.h"
#include "../../lua/Engine.h"

namespace Terminal
{

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

void Mouse::handleDown (const juce::MouseEvent& event)
{
    if (processor.getState().isPreviewActive())
    {
        processor.getState().dismissPreview();
    }
    else if (shouldForwardToPty())
    {
        const auto hitCell { cell::Point (jam::Bounds { physCellWidth, physCellHeight }, juce::Point<int> { event.x, event.y }) };
        const auto bytes { processor.encodeMouseEvent (0, hitCell.x, hitCell.y, true) };

        if (processor.events.contains (Terminal::ID::writeInput))
            processor.events.get (Terminal::ID::writeInput, bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));
    }
    else if (event.getNumberOfClicks() == 3)
    {
        // Triple-click: select the entire clicked row (visualLine).
        const auto hitCell { cell::Point (jam::Bounds { physCellWidth, physCellHeight }, juce::Point<int> { event.x, event.y }) };
        const int absRow { toAbsoluteRow (hitCell.y) };

        processor.getState().setSelectionType (static_cast<int> (Terminal::SelectionType::visualLine));
        processor.getState().setSelectionAnchor (cell (absRow), 0_cell);
        processor.getState().setSelectionCursor (cell (absRow), 0_cell);
        processor.getState().setDragAnchor (cell (absRow), 0_cell);
        processor.getState().setDragActive (false);
    }
    else
    {
        const auto hitCell { cell::Point (jam::Bounds { physCellWidth, physCellHeight }, juce::Point<int> { event.x, event.y }) };
        const int absRow { toAbsoluteRow (hitCell.y) };

        // Click-mode dispatch: hit-test against clickable link spans.
        // Only active when no modal is open.
        if (not processor.getState().isModal())
        {
            const Terminal::LinkSpan* matched { linkManager.hitTest (hitCell.getY(), hitCell.getX()) };

            if (matched != nullptr)
            {
                linkManager.dispatch (*matched);
                processor.getState().setSelectionType (static_cast<int> (Terminal::SelectionType::none));
                processor.getState().setDragAnchor (cell (absRow), cell (hitCell.x));
                processor.getState().setDragActive (false);
            }
            else
            {
                // Single click on non-link: record anchor for potential drag.
                // Clear any existing drag selection.
                processor.getState().setSelectionType (static_cast<int> (Terminal::SelectionType::none));
                processor.getState().setDragAnchor (cell (absRow), cell (hitCell.x));
                processor.getState().setDragActive (false);
            }
        }
        else
        {
            // Single click: record anchor for potential drag.
            // Clear any existing drag selection.
            processor.getState().setSelectionType (static_cast<int> (Terminal::SelectionType::none));
            processor.getState().setDragAnchor (cell (absRow), cell (hitCell.x));
            processor.getState().setDragActive (false);
        }
    }
}

void Mouse::handleDoubleClick (const juce::MouseEvent& event)
{
    if (shouldForwardToPty())
    {
        const auto hitCell { cell::Point (jam::Bounds { physCellWidth, physCellHeight }, juce::Point<int> { event.x, event.y }) };
        const auto bytes { processor.encodeMouseEvent (0, hitCell.x, hitCell.y, true) };

        if (processor.events.contains (Terminal::ID::writeInput))
            processor.events.get (Terminal::ID::writeInput, bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));
    }
    else
    {
        const auto hitCell { cell::Point (jam::Bounds { physCellWidth, physCellHeight }, juce::Point<int> { event.x, event.y }) };

        // Word boundary scan reads cell content via the visible row mapping.
        // Wire through Display's visibleMapping for word selection when
        // Display exposes a per-frame visibleMapping accessor.
        const int wordStart { hitCell.x };
        const int wordEnd { hitCell.x };

        // Write to State selection params — screenSelection rebuilt in onVBlank.
        const int absRow { toAbsoluteRow (hitCell.y) };

        processor.getState().setSelectionType (static_cast<int> (Terminal::SelectionType::visual));
        processor.getState().setSelectionAnchor (cell (absRow), cell (wordStart));
        processor.getState().setSelectionCursor (cell (absRow), cell (wordEnd));
        processor.getState().setDragAnchor (cell (absRow), cell (wordStart));
        processor.getState().setDragActive (false);
    }
}

void Mouse::handleDrag (const juce::MouseEvent& event)
{
    if (shouldForwardToPty())
    {
        const auto hitCell { cell::Point (jam::Bounds { physCellWidth, physCellHeight }, juce::Point<int> { event.x, event.y }) };
        const auto bytes { processor.encodeMouseEvent (32, hitCell.x, hitCell.y, true) };

        if (processor.events.contains (Terminal::ID::writeInput))
            processor.events.get (Terminal::ID::writeInput, bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));
    }
    else
    {
        const auto hitCell { cell::Point (jam::Bounds { physCellWidth, physCellHeight }, juce::Point<int> { event.x, event.y }) };
        const int maxCol { processor.getState().getCols() - 1 };
        const int maxVisRow { processor.getState().getVisibleRows().value - 1 };

        const int clampedCol { juce::jlimit (0, maxCol, hitCell.x) };
        const int clampedVisRow { juce::jlimit (0, maxVisRow, hitCell.y) };
        const int clampedAbsRow { toAbsoluteRow (clampedVisRow) };

        // 2-cell Manhattan distance threshold before starting a drag selection.
        const int anchorAbsRow { processor.getState().getDragAnchorRow().value };
        const int anchorCol { processor.getState().getDragAnchorCol().value };
        const int manhattanDist { std::abs (clampedCol - anchorCol)
                                + std::abs (clampedAbsRow - anchorAbsRow) };

        if (manhattanDist >= 2)
        {
            if (not processor.getState().isDragActive())
            {
                // Threshold crossed — write anchor and cursor to State.
                // onVBlank builds screenSelection from State params.
                processor.getState().setSelectionType (static_cast<int> (Terminal::SelectionType::visual));
                processor.getState().setSelectionAnchor (cell (anchorAbsRow), cell (anchorCol));
                processor.getState().setDragActive (true);
            }

            // Extend or update the drag cursor to current cell.
            processor.getState().setSelectionCursor (cell (clampedAbsRow), cell (clampedCol));
        }
    }
}

void Mouse::handleUp (const juce::MouseEvent& event)
{
    if (shouldForwardToPty())
    {
        const auto hitCell { cell::Point (jam::Bounds { physCellWidth, physCellHeight }, juce::Point<int> { event.x, event.y }) };
        const auto bytes { processor.encodeMouseEvent (0, hitCell.x, hitCell.y, false) };

        if (processor.events.contains (Terminal::ID::writeInput))
            processor.events.get (Terminal::ID::writeInput, bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));
    }
    else
    {
        // If a drag selection was active, keep the selection highlighted.
        // The user can Cmd+C to copy or click elsewhere to clear it.
        // Reset active flag so subsequent clicks don't extend the selection.
        processor.getState().setDragActive (false);
    }
}

void Mouse::handleMove (const juce::MouseEvent& event, juce::Component& component)
{
    if (not shouldForwardToPty() and not processor.getState().isModal())
    {
        const auto hitCell { cell::Point (jam::Bounds { physCellWidth, physCellHeight }, juce::Point<int> { event.x, event.y }) };
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

        if (activeScreen == Terminal::Screen::Map::alternate)
        {
            if (shouldForwardToPty())
            {
                const int button { scrollUp ? 64 : 65 };
                const auto hitCell { cell::Point (jam::Bounds { physCellWidth, physCellHeight }, juce::Point<int> { event.x, event.y }) };

                for (int i { 0 }; i < scrollLines; ++i)
                {
                    const auto bytes { processor.encodeMouseEvent (button, hitCell.x, hitCell.y, true) };

                    if (processor.events.contains (Terminal::ID::writeInput))
                        processor.events.get (Terminal::ID::writeInput, bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));
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

            if (activeScreen == Terminal::Screen::Map::alternate)
            {
                if (shouldForwardToPty())
                {
                    const int button { lines > 0 ? 64 : 65 };
                    const auto hitCell { cell::Point (jam::Bounds { physCellWidth, physCellHeight }, juce::Point<int> { event.x, event.y }) };
                    const int count { std::abs (lines) };

                    for (int i { 0 }; i < count; ++i)
                    {
                        const auto bytes { processor.encodeMouseEvent (button, hitCell.x, hitCell.y, true) };

                        if (processor.events.contains (Terminal::ID::writeInput))
                            processor.events.get (Terminal::ID::writeInput, bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));
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
    return st.getMode (Terminal::ID::mouseTracking)
        or st.getMode (Terminal::ID::mouseMotionTracking)
        or st.getMode (Terminal::ID::mouseAllTracking);
}

int Mouse::toAbsoluteRow (int visibleRow) const noexcept
{
    return visibleRow;
}

} // namespace Terminal
