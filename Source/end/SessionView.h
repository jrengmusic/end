/**
 * @file SessionView.h
 * @brief One session's tab bar — hosts TabView tabs and keeps each tab's
 *        displayed name in sync with its state.
 */
#pragma once
#include <JuceHeader.h>
#include "end/TabView.h"
#include "config/ConfigModel.h"
#include "lookAndFeel/ENDLookAndFeel.h"
#include "generated/Generated.h"

/**
 * @class SessionView
 * @brief Owns one session's TabView tabs and drives the tab bar's depth,
 *        position, and per-tab displayed name from config and state.
 */
class SessionView
    : public jam::TabbedComponent
{
public:
    SessionView (jam::Model& model, juce::ValueTree sessionState);
    ~SessionView() override = default;

    /** @brief Mints and mounts a new tab, wiring its tab-bar label to state.
     *  @param uuid Identity of the new tab.
     *  @return The newly constructed TabView.
     */
    TabView& add (jam::UUID uuid);

    /** @brief Removes a tab's state row, then its component.
     *  @param uuid Identity of the tab to remove.
     */
    void remove (jam::UUID uuid);

    /** @brief Resolves a child tab by identity.
     *  @param uuid Identity of the tab.
     *  @return The matching TabView.
     */
    TabView& get (jam::UUID uuid);

    /** @brief Resolves the focused tab, when the focused child is a tab.
     *  @return The focused TabView, or nullptr when no child is focused.
     */
    TabView* getActiveTabView() noexcept;

private:
    ENDLookAndFeel& lookAndFeel { *ENDLookAndFeel::getInstance() };

    /** @brief Reapplies a tab's displayed name on Id::name/Id::pluginId changes.
     *  @param tree     The ValueTree whose property changed.
     *  @param property The identifier of the changed property.
     */
    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /** @brief Reapplies tab bar depth and position from the current theme. */
    void lookAndFeelChanged() override;

    /** @brief Resolves a tab's displayed name — the tab's own rename when set,
     *  otherwise the name of the tab's currently focused pane.
     *  @param tabState The tab's own ValueTree row.
     *  @return The resolved name, or an empty string when neither is set.
     */
    juce::String getTabName (const juce::ValueTree& tabState) const;

    /** @brief Applies the resolved tab name to both the tab-bar name and label.
     *  @param uuid Identity of the tab to update.
     */
    void applyTabName (jam::UUID uuid);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionView)
};
