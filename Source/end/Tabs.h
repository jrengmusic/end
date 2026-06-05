/**
 * @file end/Tabs.h
 * @brief Tabbed container — owns per-tab Panes and grafts TABS node into end::Model.
 */
#pragma once
#include <JuceHeader.h>
#include "end/Panes.h"
#include "Identifier.h"

namespace end
{
/*____________________________________________________________________________*/

/** @class Tabs
 *  @brief TabbedComponent with ValueTree node and per-tab Panes ownership.
 *
 *  Inherits jam::TabbedComponent for tab strip (button::Group, sliding indicator)
 *  and jam::ValueTree::Component for the TABS node. Each tab has a corresponding
 *  end::Panes instance stored by index. currentTabChanged() swaps visible Panes.
 *
 *  The TABS node (owned by ValueTree::Component as `node`) is grafted into
 *  end::Model's root tree via Attachment owned by the orchestrator.
 */
class Tabs
    : public jam::TabbedComponent
    , public jam::ValueTree::Component
{
public:
    Tabs();

    /** @brief Adds a new tab with a fresh Panes container. */
    void addNewTab();

    /** @brief Removes the currently active tab and its Panes. */
    void removeCurrentTab();

    /** @brief Returns the active tab's Panes, or nullptr if none. */
    Panes* getActivePanes() noexcept;

    void resized() override;

protected:
    /** @brief Swaps visible Panes to match the newly selected tab. */
    void currentTabChanged (int newCurrentTabIndex, const juce::String& newCurrentTabName) override;

private:
    jam::Owner<Panes> tabPanes;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tabs)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
