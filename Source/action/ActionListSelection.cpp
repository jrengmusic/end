/**
 * @file ActionListSelection.cpp
 * @brief Action::List — filtering, selection, layout, and query.
 */

#include "ActionList.h"

namespace Action
{ /*____________________________________________________________________________*/

//==============================================================================
void List::filterRows (const juce::String& query)
{
    // Row 0 is always visible (it IS the search box).
    for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
    {
        if (query.isEmpty())
        {
            rows.at (static_cast<std::size_t> (i))->setVisible (true);
        }
        else
        {
            if (rows.at (static_cast<std::size_t> (i))->getKind() != RowKind::separator)
                rows.at (static_cast<std::size_t> (i))->setVisible (false);
        }
    }

    if (query.isNotEmpty())
    {
        juce::StringArray dataset;
        dataset.ensureStorageAllocated (static_cast<int> (rows.size()) - 1);

        for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
        {
            if (auto* label { rows.at (static_cast<std::size_t> (i))->getNameLabel() }; label != nullptr)
                dataset.add (label->getText());
        }

        const auto results { jam::Fuzzy::Search::getResult (query, dataset) };

        std::unordered_set<juce::String> matchedNames;

        for (const auto& result : results)
            matchedNames.insert (result.second);

        for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
        {
            if (auto* label { rows.at (static_cast<std::size_t> (i))->getNameLabel() }; label != nullptr)
            {
                const bool isMatch { matchedNames.count (label->getText()) > 0 };
                rows.at (static_cast<std::size_t> (i))->setVisible (isMatch);
            }
        }
    }

    layoutRows();
    selectRow (0);
}

//==============================================================================
void List::layoutRows()
{
    const int rowWidth { viewport.getWidth() - viewport.getScrollBarThickness() };
    int yPos { 0 };

    for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows.at (static_cast<std::size_t> (i))->isVisible())
        {
            const int height { rows.at (static_cast<std::size_t> (i))->getKind() == RowKind::separator
                                   ? separatorRowHeight
                                   : rowHeight };
            rows.at (static_cast<std::size_t> (i))->setBounds (0, yPos, rowWidth, height);
            yPos += height;
        }
    }

    rowContainer.setSize (rowWidth, yPos);
}

//==============================================================================
int List::findSelectableRow (int index) const
{
    const int count { static_cast<int> (rows.size()) };

    int target { juce::jlimit (0, count - 1, index) };

    if (target > 0 and (not rows.at (static_cast<std::size_t> (target))->isSelectable()
                        or not rows.at (static_cast<std::size_t> (target))->isVisible()))
    {
        const int current { getSelectedIndex() };
        const int direction { target >= current ? 1 : -1 };

        while (target > 0 and target < count
               and (not rows.at (static_cast<std::size_t> (target))->isSelectable()
                    or not rows.at (static_cast<std::size_t> (target))->isVisible()))
        {
            target += direction;
        }

        target = juce::jlimit (0, count - 1, target);
    }

    if (target > 0 and (not rows.at (static_cast<std::size_t> (target))->isSelectable()
                        or not rows.at (static_cast<std::size_t> (target))->isVisible()))
    {
        target = 0;
    }

    return target;
}

//==============================================================================
void List::selectRow (int index)
{
    const int target { findSelectableRow (index) };
    const int count { static_cast<int> (rows.size()) };

    for (auto& row : rows)
        row->getValueObject().setValue (false);

    if (target >= 0 and target < count)
    {
        rows.at (static_cast<std::size_t> (target))->getValueObject().setValue (true);

        if (target > 0)
        {
            grabKeyboardFocus();

            const int rowY { rows.at (static_cast<std::size_t> (target))->getY() };
            const int viewTop { viewport.getViewPositionY() };
            const int viewBottom { viewTop + viewport.getViewHeight() };

            if (rowY < viewTop)
                viewport.setViewPosition (0, rowY);

            if (rowY + rowHeight > viewBottom)
                viewport.setViewPosition (0, rowY + rowHeight - viewport.getViewHeight());
        }
    }
}

//==============================================================================
void List::executeSelected()
{
    int target { getSelectedIndex() };

    if (target == 0)
    {
        for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
        {
            if (rows.at (static_cast<std::size_t> (i))->isVisible() and target == 0)
                target = i;
        }
    }

    if (target > 0 and target < static_cast<int> (rows.size()))
    {
        if (rows.at (static_cast<std::size_t> (target))->run != nullptr)
            rows.at (static_cast<std::size_t> (target))->run();

        if (lua::Engine::getContext()->display.actionList.closeOnRun)
        {
            if (onActionRun != nullptr)
                onActionRun();
        }
    }
}

//==============================================================================
int List::visibleRowCount() const noexcept
{
    int count { 0 };

    for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows.at (static_cast<std::size_t> (i))->isVisible()
            and rows.at (static_cast<std::size_t> (i))->isSelectable())
            ++count;
    }

    return count;
}

//==============================================================================
int List::getSelectedIndex() const noexcept
{
    int result { 0 };

    for (int i { 0 }; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows.at (static_cast<std::size_t> (i))->isSelected() and result == 0)
            result = i;
    }

    return result;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Action
