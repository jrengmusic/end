/**
 * @file KeyHandler.h
 * @brief Keyboard dispatch for Action::List — unified navigation.
 */

#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include <functional>

namespace Action
{ /*____________________________________________________________________________*/

/**
 * @class KeyHandler
 * @brief Translates key presses to List actions via a single lookup table.
 *
 * Row 0 selected = search mode (typing filters). Row > 0 = navigation.
 * Binding mode is handled by List directly, not by KeyHandler.
 */
class KeyHandler
{
public:
    struct Callbacks
    {
        std::function<void()>      executeSelected;   ///< Invoked when Return is pressed on a selected row.
        std::function<void()>      enterBindingMode;  ///< Invoked when Shift+Return is pressed in navigation mode.
        std::function<void(int)>   selectRow;         ///< Invoked with the target row index to select.
        std::function<int()>       visibleRowCount;   ///< Returns the number of visible, selectable rows.
        std::function<int()>       selectedIndex;     ///< Returns the currently selected row index.
    };

    explicit KeyHandler (Callbacks callbacks);

    bool handleKey (const juce::KeyPress& key);

private:
    using KeyAction = std::function<void()>;
    using KeyTable  = std::unordered_map<int, KeyAction>;

    Callbacks callbacks;

    KeyTable searchTable;
    KeyTable navigationTable;

    void buildSearchTable();
    void buildNavigationTable();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyHandler)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Action
