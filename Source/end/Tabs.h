/**
 * @file end/Tabs.h
 * @brief Tabbed container — owns per-tab Panes and their Model Attachments.
 */
#pragma once
#include <JuceHeader.h>
#include "end/Panes.h"
#include "config/Config.h"
#include "Identifier.h"

namespace end
{
/*____________________________________________________________________________*/

/** @class Tabs
 *  @brief TabbedComponent with ValueTree state, owns Attachments for child Panes.
 *
 *  Inherits jam::TabbedComponent for tab strip and content management.
 *  Each tab's content component is an end::Panes instance, owned by
 *  TabbedComponent (deleteComponentWhenNotNeeded = true).
 *  Each Panes child has a corresponding jam::Model::Attachment held in
 *  tabAttachments — RAII ensures detach on removal.
 *
 *  Listens to the config ValueTree so that bar depth re-computes whenever
 *  the display tab node changes (depth, font_size, font_family).
 */
class Tabs
    : public jam::TabbedComponent
    , public jam::Model::Component
    , public juce::ValueTree::Listener
{
public:
    explicit Tabs (jam::Model& model);
    ~Tabs() override;

    /** @brief Adds a new tab with a fresh Panes container. */
    void addNewTab();

    /** @brief Removes the currently active tab and its Panes. */
    void removeCurrentTab();

    /** @brief Returns the active tab's Panes via getTabContentComponent. */
    Panes* getActivePanes() noexcept;

protected:
    void currentTabChanged (int newCurrentTabIndex, const juce::String& newCurrentTabName) override;

private:
    /** @brief Re-computes bar depth and relays tab positions when the display tab node changes.
     *  Calls updateTabBarVisibility() then resized() so that padding and depth
     *  changes both take effect without requiring a second event.
     */
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    void updateTabBarVisibility();

    config::Model& config { *config::Model::getInstance() };

    jam::Owner<jam::Model::Attachment> attachments;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tabs)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
