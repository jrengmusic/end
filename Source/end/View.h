/**
 * @file end/View.h
 * @brief Root content component — owns Tabs and Registry, routes keyboard input.
 */
#pragma once
#include <JuceHeader.h>
#include "end/Tabs.h"
#include "action/Registry.h"
#include "config/Config.h"
#include "Map.h"

namespace end
{
/*____________________________________________________________________________*/

/** @class View
 *  @brief Root content component inside end::Window.
 *
 *  Owns the tab system, action registry, and attachments. Receives the jam::Model
 *  reference and creates its own Attachments — grafting its own state and Tabs'
 *  state into the model tree. Implements KeyListener to intercept key presses and route
 *  them through the Registry's prefix key state machine. Transparent — glass
 *  shows through from Window.
 */
class View
    : public juce::Component
    , public jam::Model::Component
    , public juce::ValueTree::Listener
    , public juce::KeyListener
{
public:
    explicit View (jam::Model& m);
    ~View() override;

    void resized() override;
    void paint (juce::Graphics&) override {}

    /** @brief Routes key presses to the action registry.
     *  @return true if consumed by an action binding.
     */
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree& parentTree,
                              juce::ValueTree& childWhichHasBeenAdded) override;

private:
    juce::ValueTree config;
    juce::ValueTree model;
    //==============================================================================
    void registerActions();
    void setTabOrientation();
    void setViewState (int width, int height);
    //==============================================================================
    Tabs tabs;
    jam::Owner<jam::Model::Attachment> attachments;
    action::Registry registry;

    //==============================================================================
#if JUCE_DEBUG
    jam::debug::Widget widget { this, model, false };
#endif
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (View)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
