/**
 * @file end/Tabs.h
 * @brief Tabbed container — owns per-tab Panes, grafts TABS node into end::Model.
 */
#pragma once
#include <JuceHeader.h>
#include "end/Panes.h"
#include "Identifier.h"

namespace end
{
/*____________________________________________________________________________*/

/** @class Tabs
 *  @brief TabbedComponent with ValueTree state and per-tab Panes.
 *
 *  Inherits jam::TabbedComponent for tab strip and content management.
 *  Each tab's content component is an end::Panes instance, owned by
 *  TabbedComponent (deleteComponentWhenNotNeeded = true).
 */
class Tabs
    : public jam::TabbedComponent
    , public jam::Model::Component
{
public:
    Tabs();

    /** @brief Adds a new tab with a fresh Panes container. */
    void addNewTab();

    /** @brief Removes the currently active tab and its Panes. */
    void removeCurrentTab();

    /** @brief Returns the active tab's Panes via getTabContentComponent. */
    Panes* getActivePanes() noexcept;

protected:
    void currentTabChanged (int newCurrentTabIndex, const juce::String& newCurrentTabName) override;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tabs)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
