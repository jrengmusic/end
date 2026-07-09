/**
 * @file end/Tabs.h
 * @brief Tabbed container — owns per-tab Panes, grafting their TAB state verb-direct.
 */
#pragma once
#include <JuceHeader.h>
#include "end/Panes.h"
#include "config/ConfigModel.h"
#include "lookAndFeel/ENDLookAndFeel.h"
#include "Identifier.h"

/** @class Tabs
 *  @brief TabbedComponent with ValueTree state, grafts child Panes state verb-direct.
 *
 *  Inherits jam::TabbedComponent (tab strip and content management) and
 *  jam::Model::Component, which ADOPTS the active Session's own
 *  TABS tree (its own adopt ctor) — this is the mirror ROOT: every
 *  component-authored TAB/PANE state (Panes, TerminalView) lands
 *  inside SESSION state automatically once grafted here, because this
 *  adoption is the one seam connecting the GUI-authored component mirror to
 *  session state. Each tab's content component is a Panes instance, owned by
 *  TabbedComponent (deleteComponentWhenNotNeeded = true). Each Panes child's
 *  TAB state is grafted/removed verb-direct (add()'s own state.appendChild(),
 *  remove()'s own state.removeChild()) — no RAII token in between
 *  (Attachment Contract's verb-driven placement rule). View lifetime binds
 *  to explicit verbs (add()/remove(), called from the ActionRegistry's own
 *  actions, ActionRegistration.cpp) — NEVER to valueTreeChildAdded/Removed;
 *  this class carries none.
 *
 *  Listens to its own adopted state tree so tab titles re-derive whenever a
 *  TAB's rename/cwd/foregroundProcess property changes anywhere in the
 *  subtree (getTitle()/applyTabTitle()) — gated away from PaneManager's own
 *  split vocabulary (edge/position on RESIZER, bounds on PANE), which lands
 *  on this same subtree during every layout pass.
 */
class Tabs
    : public jam::TabbedComponent
    , public jam::Model::Component
    , public juce::ValueTree::Listener
{
public:
    /** @brief Constructs the container, adopting the active Session's own
     *  TABS tree — the mirror root.
     *  @param model      Model reference — forwarded to Component and child Panes.
     *  @param tabsState  The active Session's own TABS tree to adopt as @c state.
     */
    Tabs (jam::Model& model, juce::ValueTree tabsState);
    ~Tabs() override;

    /** @brief Adds a new tab with a fresh, paneless Panes container and
     *  grafts its TAB state. Builds the Panes, adds it as a tab (name
     *  placeholder: this tab's own uuid — no PANE leaf exists yet at graft
     *  time, so applyTabTitle() below cannot derive a real title until the
     *  first pane's terminal reports cwd/foregroundProcess, which
     *  re-triggers via valueTreePropertyChanged's own bubbling reaction),
     *  wires the tab label's rename persistence (jam::ID::name, the one-way
     *  onTextChange commit getTitle() reads), and derives the initial title.
     *  The tab's first pane is added separately — the ActionRegistry's own
     *  ID::newTab action calls this method, then invokes ID::newPane through
     *  the registry.
     *  @param uuid  UUID for the new TAB tree.
     *  @return Reference to the constructed Panes.
     */
    Panes& add (jam::UUID uuid);

    /** @brief Removes the tab whose TAB state carries @p uuid, and its
     *  Panes — resolves the tab index from @p uuid (never from
     *  getCurrentTabIndex()), grafts the TAB state out via
     *  state.removeChild(), then uses the framework removal path
     *  (removeTab()). Caller (ActionRegistration.cpp's own closeTab body)
     *  decides whether to call this at all: last-tab quit lives in the
     *  action, never here.
     *  @param uuid  Identifier of the TAB to remove.
     */
    void remove (jam::UUID uuid);

    /** @brief Resolves the Panes whose TAB state carries @p uuid.
     *  Asserts the tab is found.
     *  @param uuid  Identifier of the TAB to resolve.
     *  @return Reference to the owned Panes.
     */
    Panes& get (jam::UUID uuid);

    /** @brief Returns the active tab's Panes via getTabContentComponent. */
    Panes* getActivePanes() noexcept;

private:
    /** @brief Re-derives a tab's title when its rename (jam::ID::name), cwd, or
     *  foregroundProcess property changes anywhere in this TABS subtree
     *  (getTitle() precedence, applyTabTitle() the push). Gated away from
     *  jam::PaneManager's own split vocabulary (edge/position on RESIZER,
     *  bounds on PANE), which lands on this same subtree during every
     *  layout pass.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;
    void lookAndFeelChanged() override;
    void currentTabChanged (int newCurrentTabIndex, const juce::String&) override;

    /** @brief Derives the display title for @p tabState: user rename (jam::ID::name on the
     *  TAB, non-empty — LATTER overrides FORMER, so this always wins when set) else the
     *  source pane's foregroundProcess (non-empty) else the source pane's cwd, displayed as
     *  its last path component (ARCHITECT ruling — cwd/foregroundProcess/rename priority,
     *  cwd basename display). Source pane is resolved directly off @p tabState's own
     *  ID::focusedPane param (Panes's own per-tab last-focused memory) via
     *  jam::Model::getChildWithID() — empty title when unset (no pane created yet).
     *  @param tabState  The TAB tree to derive a title for.
     *  @return The resolved title — empty if no rename, foregroundProcess, or cwd is set.
     */
    juce::String getTitle (const juce::ValueTree& tabState);

    /** @brief Walks @p tree's own parent chain up to and including the first TAB-typed
     *  ancestor — resolves which tab a deeply-nested property change (e.g. a TEXT node's
     *  cwd/foregroundProcess, bubbling from an attached TerminalModel tree) belongs to.
     *  @param tree  The changed tree to resolve a TAB ancestor for.
     *  @return The owning TAB tree.
     */
    static juce::ValueTree findAncestorTab (juce::ValueTree tree);

    /** @brief Recomputes @p tabState's title (getTitle()) and pushes it to the matching
     *  tab button: setTabName() (framework name/getTabNames() surface) and the tab's own
     *  label text (the SVG tab's actual painted text, ENDLookAndFeel::drawTabLabel reads
     *  label.getText() directly — setTabName() alone does not repaint it). Finds the tab
     *  index by matching the built Panes component's ValueTree identity.
     *  @param tabState  The TAB tree whose title changed.
     */
    void applyTabTitle (const juce::ValueTree& tabState);

    // /** @brief Singleton LookAndFeel reference — source for tab bar depth/position. */
    ENDLookAndFeel& lookAndFeel { *ENDLookAndFeel::getInstance() };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tabs)
};
