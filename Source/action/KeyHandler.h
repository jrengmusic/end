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
        std::function<void()>      executeSelected;
        std::function<void()>      dismiss;
        std::function<void()>      enterBindingMode;
        std::function<void(int)>   selectRow;
        std::function<int()>       visibleRowCount;
        std::function<int()>       selectedIndex;
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
}// namespace Action
