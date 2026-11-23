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
 *
 * It gives the keyboard focus to the focused pane of the active tab when that pane changes,
 * when the plugin of a pane gets its name, and when the current tab changes.
 */
class SessionView
    : public jam::TabbedComponent
{
public:
    /**
     * @brief Adopts the session row and mounts a TabView for each tab row under it. The tab bar starts with zero depth.
     *
     * Adding a tab makes it the current tab. The constructor therefore restores the focused tab that the row held before the
     * adds, then makes that tab current when it is mounted.
     *
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
    /** @brief Command manager of the session view. childRemoved() invokes the quit command through it when the last tab leaves the session. */
    jam::SharedInstance<jam::CommandManager> commandManager { std::in_place };

    /** @brief Application look and feel. It gives the tab bar depth and position. */
    ENDLookAndFeel& lookAndFeel { *ENDLookAndFeel::getInstance() };

    /**
     * @brief Reapplies the displayed name of each tab that owns the changed tree.
     *
     * The call first runs the base handler of jam::OwnerComponent.
     * On a tab or pane row, Id::name or Id::focusedPane reapplies the tab name and calls focusPane().
     * On a terminal row, Id::title, Id::foregroundProcess, or Id::cwd reapplies the tab name without focusPane().
     * A tab owns the tree when the tree is its row or a descendant row.
     *
     * @param tree      The ValueTree whose property changed.
     * @param property  The identifier of the changed property.
     */
    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /**
     * @brief Mounts a TabView for a new tab row, or reapplies the owning tab name under a plugin row.
     *
     * When a tab row arrives directly under the session row, the call mounts its TabView.
     * When a child arrives under a plugin row, the call reapplies the name of the owning tab.
     * Any other added child has no effect.
     *
     * @param parentTree  Tree that gained the child.
     * @param child       Added child tree.
     */
    void valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& child) override;

    /**
     * @brief Unmounts the TabView of each tab row removed from the session row.
     *
     * The tab bar takes its depth and position from the theme again.
     *
     * @param parentTree  Tree that lost the child. Only the session row unmounts tabs.
     * @param child       Removed child. A tab row is unmounted.
     */
    void valueTreeChildRemoved (juce::ValueTree& parentTree, juce::ValueTree& child, int) override;

    /**
     * @brief Removes the tab from the tab bar. The quit command runs when no tab remains.
     *
     * @param uuid  Identity of the removed tab.
     * @param row   State row of the removed tab.
     */
    void childRemoved (jam::UUID uuid, const juce::ValueTree& row) override;

    /**
     * @brief Gives the keyboard focus to the focused pane of the new current tab.
     *
     * jam::TabbedComponent calls it after the new current tab is laid out and visible. The call does not use the parameters.
     *
     * @param newCurrentTab      Identity of the new current tab. Not used.
     * @param newCurrentTabName  Displayed name of the new current tab. Not used.
     */
    void currentTabChanged (jam::UUID newCurrentTab, const juce::String& newCurrentTabName) override;

    /**
     * @brief Grabs the keyboard focus for the default component of the active tab.
     *
     * The default component comes from the keyboard focus traverser of the tab. The traverser returns the focused child
     * of the tab when the tab holds one. The call does nothing when no tab is active or the traverser returns no component.
     * It writes nothing to the Model.
     */
    void focusPane();

    /** @brief Reapplies tab bar depth and position from the current theme. */
    void lookAndFeelChanged() override;

    /**
     * @brief Resolves a tab's displayed name.
     *
     * A non-empty Id::name rename on tabState wins first.
     * Otherwise the call looks up sourcePane by Id::focusedPane, then inspects Session::getTerminal (sourcePane).
     * From a valid terminal row, it checks Id::title, then Id::foregroundProcess.
     * Next it takes the local directory name from Id::cwd when juce::URL is a local file.
     * If the terminal gives no non-empty name, it falls back to Id::name on sourcePane.
     * It returns an empty string when sourcePane is invalid.
     *
     * @param tabState  The tab's own ValueTree row.
     * @return The resolved name, or an empty string when no source holds a value.
     */
    juce::String getTabName (const juce::ValueTree& tabState) const;

    /** @brief Applies the resolved tab name to both the tab-bar name and label.
     *  @param uuid Identity of a mounted tab. The call looks the tab up with get(), so the tab must be mounted.
     */
    void applyTabName (jam::UUID uuid);

    /**
     * @brief Mounts the TabView of a tab row in the tab bar, then applies its displayed name and the depth and position of the theme.
     *
     * The tab button label writes its text to the Id::name property of the tab row whenever the text changes.
     *
     * @param tabRow  Row of the tab to mount.
     */
    void addTabView (const juce::ValueTree& tabRow);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionView)
};
