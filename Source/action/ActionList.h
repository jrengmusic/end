/**
 * @file ActionList.h
 * @brief Command palette — modal glass window with fuzzy search and action list.
 */

#pragma once

#include <JuceHeader.h>
#include "../config/Config.h"
#include "../component/MessageOverlay.h"
#include "Action.h"
#include "ActionRow.h"
#include "KeyHandler.h"
#include "LookAndFeel.h"

namespace Action
{ /*____________________________________________________________________________*/

/**
 * @class List
 * @brief Command palette modal glass window with fuzzy search and keyboard navigation.
 *
 * Created on the fly by MainComponent, enters modal state, destroyed on dismiss.
 * Owns a jreng::ValueTree for ephemeral state. Row selection is Value-driven.
 *
 * @see Action::Registry
 * @see Action::Row
 */
class List : public jreng::ModalWindow
{
public:
    explicit List (juce::Component& caller);
    ~List() override;

    bool keyPressed (const juce::KeyPress& key) override;
    int getDesktopWindowStyleFlags() const override;

private:
    static constexpr int rowHeight              { 28 };
    static constexpr int separatorRowHeight     { 12 };
    static constexpr int bindingModeTimeoutMs   { 60000 };
    static const juce::Identifier bindingRowIndexId;
    static const juce::Identifier bindingsDirtyId;

    jreng::ValueTree           state { "ACTION_LIST" };
    Action::LookAndFeel        lookAndFeel;
    juce::Viewport             viewport;
    juce::Component            rowContainer;
    jreng::Owner<Action::Row>  rows;
    MessageOverlay             messageOverlay;

    juce::Component&           callerRef;
    std::optional<KeyHandler>  keyHandler;

    void buildRows();
    void configureSearchBox (juce::TextEditor& editor);
    void configureActionRow (Row& row);
    void filterRows (const juce::String& query);
    void layoutRows();
    void selectRow (int index);
    void executeSelected();
    int  visibleRowCount() const noexcept;
    int  getSelectedIndex() const noexcept;
    void resized() override;
    juce::Colour getHighlightColour() const;

    int  getBindingRowIndex() const;
    void setBindingRowIndex (int index);
    void enterBindingMode();
    void exitBindingMode();
    bool handleBindingKey (const juce::KeyPress& key);

    void handleValueChanged();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (List)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Action
