/**
 * @file ActionList.h
 * @brief Command palette — modal glass window with fuzzy search and action list.
 */

#pragma once

#include <JuceHeader.h>
#include "../config/Config.h"
#include "Action.h"
#include "ActionRow.h"
#include "KeyRemapDialog.h"
#include "LookAndFeel.h"

namespace Action
{ /*____________________________________________________________________________*/

/**
 * @class List
 * @brief Command palette modal glass window with fuzzy search and keyboard navigation.
 *
 * Created on the fly by MainComponent, enters modal state, destroyed on dismiss.
 * Owns a jreng::ValueTree for ephemeral shortcut binding state.
 *
 * @see Action::Registry
 * @see Action::Row
 * @see Action::KeyRemapDialog
 */
class List : public jreng::ModalWindow
{
public:
    explicit List (juce::Component& caller);
    ~List() override;

    bool keyPressed (const juce::KeyPress& key) override;
    int getDesktopWindowStyleFlags() const override;

private:
    static constexpr int searchBoxHeight { 24 };
    static constexpr int rowHeight       { 28 };
    static constexpr int maxVisibleRows  { 12 };

    Action::LookAndFeel       lookAndFeel;
    juce::TextEditor          searchBox;
    juce::Viewport            viewport;
    juce::Component           rowContainer;
    jreng::Owner<Action::Row> rows;

    int selectedIndex { -1 };

    KeyRemapDialog remapDialog;
    Row*           activeRemapRow { nullptr };

    void buildRows();
    void filterRows (const juce::String& query);
    void layoutRows();
    void selectRow (int index);
    void executeSelected();
    int visibleRowCount() const noexcept;
    void resized() override;

    void setupSearchBox();
    void handleShortcutClicked (Row* clickedRow);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (List)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Action
