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
#include "../rendering/Screen.h"
#include "../selection/LinkManager.h"
#include "../../SelectionType.h"
#include "../data/Identifier.h"
#include "../data/State.h"
#include "../../config/Config.h"

namespace Terminal
{

Mouse::Mouse (Processor& p,
              ScreenBase& sc,
              LinkManager& lm) noexcept
    : processor (p)
    , screen (sc)
    , linkManager (lm)
{
}

void Mouse::handleDown (const juce::MouseEvent& event)
{
    if (shouldForwardToPty())
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        const auto bytes { processor.encodeMouseEvent (0, cell.x, cell.y, true) };
        processor.writeInput (bytes.toRawUTF8(), static_cast<int> (bytes.getNumBytesAsUTF8()));
    }
    else if (event.getNumberOfClicks() == 3)
    {
        // Triple-click: select the entire clicked row (visualLine).
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        const int absRow { toAbsoluteRow (cell.y) };

        processor.getState().setSelectionType (static_cast<int> (Terminal::SelectionType::visualLine));
        processor.getState().setSelectionAnchor (absRow, 0);
        processor.getState().setSelectionCursor (absRow, 0);
        processor.getState().setDragAnchor (absRow, 0);
        processor.getState().setDragActive (false);
    }
    else
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        const int absRow { toAbsoluteRow (cell.y) };

        // Click-mode dispatch: hit-test against clickable link spans.
        // Only active when no modal is open.
        if (not processor.getState().isModal())
        {
            const Terminal::LinkSpan* matched { linkManager.hitTest (cell.y, cell.x) };

            if (matched != nullptr)
            {
                linkManager.dispatch (*matched);
            }
            else
            {
                // Single click on non-link: record anchor for potential drag.
                // Clear any existing drag selection.
                processor.getState().setSelectionType (static_cast<int> (Terminal::SelectionType::none));
                processor.getState().setDragAnchor (absRow, cell.x);
                processor.getState().setDragActive (false);
            }
        }
        else
        {
            // Single click: record anchor for potential drag.
            // Clear any existing drag selection.
            processor.getState().setSelectionType (static_cast<int> (Terminal::SelectionType::none));
            processor.getState().setDragAnchor (absRow, cell.x);
            processor.getState().setDragActive (false);
        }
    }

}

void Mouse::handleDoubleClick (const juce::MouseEvent& event)
{
    const juce::ScopedLock lock (processor.getGrid().getResizeLock());

    if (shouldForwardToPty())
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        const auto bytes { processor.encodeMouseEvent (0, cell.x, cell.y, true) };
        processor.writeInput (bytes.toRawUTF8(), static_cast<int> (bytes.getNumBytesAsUTF8()));
    }
    else
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        const int scrollOffset { processor.getState().getScrollOffset() };
        const int cols { processor.getGrid().getCols() };

        // Scan the visible row (with scroll offset applied) to find word boundaries.
        const Terminal::Cell* rowCells { processor.getGrid().scrollbackRow (cell.y, scrollOffset) };

        int wordStart { cell.x };
        int wordEnd { cell.x };

        if (rowCells != nullptr)
        {
            // Scan left: stop at space (codepoint <= 0x20) or column 0.
            int c { cell.x - 1 };
            while (c >= 0 and rowCells[c].codepoint > 0x20)
            {
                wordStart = c;
                --c;
            }

            // Scan right: stop at space or end of line.
            c = cell.x + 1;
            while (c < cols and rowCells[c].codepoint > 0x20)
            {
                wordEnd = c;
                ++c;
            }
        }

        // Write to State selection params — screenSelection rebuilt in onVBlank.
        const int absRow { toAbsoluteRow (cell.y) };

        processor.getState().setSelectionType (static_cast<int> (Terminal::SelectionType::visual));
        processor.getState().setSelectionAnchor (absRow, wordStart);
        processor.getState().setSelectionCursor (absRow, wordEnd);
        processor.getState().setDragAnchor (absRow, wordStart);
        processor.getState().setDragActive (false);
    }
}

void Mouse::handleDrag (const juce::MouseEvent& event)
{
    if (shouldForwardToPty())
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        const auto bytes { processor.encodeMouseEvent (32, cell.x, cell.y, true) };
        processor.writeInput (bytes.toRawUTF8(), static_cast<int> (bytes.getNumBytesAsUTF8()));
    }
    else
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        const int maxCol { processor.getGrid().getCols() - 1 };
        const int maxVisRow { processor.getGrid().getVisibleRows() - 1 };

        const int clampedCol { juce::jlimit (0, maxCol, cell.x) };
        const int clampedVisRow { juce::jlimit (0, maxVisRow, cell.y) };
        const int clampedAbsRow { toAbsoluteRow (clampedVisRow) };

        // 2-cell Manhattan distance threshold before starting a drag selection.
        const int anchorAbsRow { processor.getState().getDragAnchorRow() };
        const int anchorCol { processor.getState().getDragAnchorCol() };
        const int manhattanDist { std::abs (clampedCol - anchorCol)
                                + std::abs (clampedAbsRow - anchorAbsRow) };

        if (manhattanDist >= 2)
        {
            if (not processor.getState().isDragActive())
            {
                // Threshold crossed — write anchor and cursor to State.
                // onVBlank builds screenSelection from State params.
                processor.getState().setSelectionType (static_cast<int> (Terminal::SelectionType::visual));
                processor.getState().setSelectionAnchor (anchorAbsRow, anchorCol);
                processor.getState().setDragActive (true);
            }

            // Extend or update the drag cursor to current cell.
            processor.getState().setSelectionCursor (clampedAbsRow, clampedCol);
        }
    }
}

void Mouse::handleUp (const juce::MouseEvent& event)
{
    if (shouldForwardToPty())
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        const auto bytes { processor.encodeMouseEvent (0, cell.x, cell.y, false) };
        processor.writeInput (bytes.toRawUTF8(), static_cast<int> (bytes.getNumBytesAsUTF8()));
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
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        const bool overLink { linkManager.hitTest (cell.y, cell.x) != nullptr };

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
    const int scrollLines { Config::getContext()->getInt (Config::Key::terminalScrollStep) };
    const auto activeScreen { processor.getState().getActiveScreen() };

    if (not wheel.isSmooth)
    {
        const bool scrollUp { wheel.deltaY > 0.0f };

        if (activeScreen == Terminal::ActiveScreen::alternate)
        {
            if (shouldForwardToPty())
            {
                const int button { scrollUp ? 64 : 65 };
                const auto cell { screen.cellAtPoint (event.x, event.y) };

                for (int i { 0 }; i < scrollLines; ++i)
                {
                    const auto bytes { processor.encodeMouseEvent (button, cell.x, cell.y, true) };
                    processor.writeInput (bytes.toRawUTF8(), static_cast<int> (bytes.getNumBytesAsUTF8()));
                }
            }
        }
        else
        {
            const int delta { scrollUp ? scrollLines : -scrollLines };
            setScrollFn (processor.getState().getScrollOffset() + delta);
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

            if (activeScreen == Terminal::ActiveScreen::alternate)
            {
                if (shouldForwardToPty())
                {
                    const int button { lines > 0 ? 64 : 65 };
                    const auto cell { screen.cellAtPoint (event.x, event.y) };
                    const int count { std::abs (lines) };

                    for (int i { 0 }; i < count; ++i)
                    {
                        const auto bytes { processor.encodeMouseEvent (button, cell.x, cell.y, true) };
                        processor.writeInput (bytes.toRawUTF8(), static_cast<int> (bytes.getNumBytesAsUTF8()));
                    }
                }
            }
            else
            {
                setScrollFn (processor.getState().getScrollOffset() + lines);
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
    const juce::ScopedLock lock (processor.getGrid().getResizeLock());
    const int scrollback { processor.getGrid().getScrollbackUsed() };
    const int scrollOffset { processor.getState().getScrollOffset() };
    return scrollback - scrollOffset + visibleRow;
}

} // namespace Terminal
