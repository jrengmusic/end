/**
 * @file SessionView.h
 * @brief One session's tab bar — hosts TabView tabs and keeps each tab's
 *        displayed name in sync with its state.
 */
#pragma once
#include <JuceHeader.h>
#include "ui/TabView.h"
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
    /**
     * @brief Constructs the view of a session. Its tab bar starts with zero depth.
     * @param newModel      Owning model.
     * @param sessionState  Row of the session that holds the tabs.
     */
    SessionView (jam::Model& newModel, juce::ValueTree sessionState);

    /** @brief Default destructor. */
    ~SessionView() override = default;

    /** @brief Appends a tab row to the session's state. The tab mounts from the child-added event.
     *  @param uuid Identity of the new tab.
     */
    void add (jam::UUID uuid);

    /** @brief Removes a tab's state row. The tab unmounts from the child-removed event.
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

    /**
     * @brief Sets the zoom parameter of each pane of each tab of the session with a zoom command.
     * @param command  Zoom command name.
     * @param step     Zoom step that the command uses.
     */
    void setZoom (const juce::Identifier& command, float step);

private:
    /** @brief Application look and feel. It gives the tab bar depth and position. */
    ENDLookAndFeel& lookAndFeel { *ENDLookAndFeel::getInstance() };

    /** @brief Reapplies a tab's displayed name when its Id::name or Id::focusedPane changes.
     *  @param tree     The ValueTree whose property changed.
     *  @param property The identifier of the changed property.
     */
    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /**
     * @brief Mounts a TabView for each tab row added to the session row.
     *
     * The tab bar label writes its text to the Id::name of the tab row. The tab then takes its displayed name, and the
     * tab bar takes its depth and position from the theme.
     *
     * @param parentTree  Tree that gained the child. Only the session row mounts tabs.
     * @param child       Added child. A tab row is mounted.
     */
    void valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& child) override;

    /**
     * @brief Unmounts the TabView of each tab row removed from the session row.
     *
     * The tab bar takes its depth and position from the theme again. When no tab remains, the quit action runs.
     *
     * @param parentTree  Tree that lost the child. Only the session row unmounts tabs.
     * @param child       Removed child. A tab row is unmounted.
     */
    void valueTreeChildRemoved (juce::ValueTree& parentTree, juce::ValueTree& child, int) override;

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
