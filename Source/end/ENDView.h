#pragma once
#include <JuceHeader.h>
#include "end/Tabs.h"
#include "end/MessageOverlay.h"
#include "action/ActionRegistry.h"
#include "config/ConfigModel.h"
#include "../lookAndFeel/ENDLookAndFeel.h"
#include "Bimap.h"

// Root content component — jam::PaneManager adopts the same bare WINDOW
// tree Nexus bootstraps. tabs is constructed directly (ctor init list) from
// the active Session's own TABS tree; createDockPane() mints the dock
// leaves (jam::PaneComponent instances) the Position-toggle loop adds.
class ENDView
    : public juce::Component
    , public jam::Model::Component
    , public juce::ValueTree::Listener
    , public juce::KeyListener
{
public:
    explicit ENDView (jam::Model& m);

    ~ENDView() override;

    void resized() override;

    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

private:
    ConfigModel& config { *ConfigModel::getInstance() };
    ENDLookAndFeel& endLookAndFeel { *ENDLookAndFeel::getInstance() };

    void createAndAttachParameters();

    // Mints a fresh dock jam::PaneComponent/PANE pair and lands it via
    // jam::PaneManager::split() against the WINDOW tree's own center PANE
    // (the sole PANE child of state carrying no ID::position property).
    // extent is explicit pixels, derived from ENDLookAndFeel::getPaneSidebarSize()
    // against the window's own axis for positionKey's edge. jam::ID::visible
    // is registered true at creation, jam::ID::position is a plain property.
    // Called only from the Position-toggle loop below when no leaf for
    // positionKey exists yet.
    void createDockPane (int positionKey);

    void registerActions();

    void registerEvents();

    void setBackground();

    void setBackgroundParams();

    void setPostProcess();

    void setPostProcessParams();

    void setMouseConfig();

    void setViewState (jam::Size<int16_t> size);

    //==============================================================================
    ActionRegistry registry;

    jam::vulkan::ShaderComponent background;

    // Constructed (m, WINDOW state, *this).
    jam::PaneManager paneManager;
    jam::HashMap<int64_t, std::unique_ptr<juce::Component>> components;

    Tabs tabs;

    MessageOverlay messageOverlay;

    jam::Function::Map<juce::Identifier, void> events;

//==============================================================================
#if JUCE_DEBUG
    // ValueTree inspector widget, debug builds only — pointed at the model
    // ROOT (topology under SESSIONS, size under WINDOW, OVERLAY at root).
    jam::debug::Widget widget { this, model.state, false };
#endif
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDView)
};
