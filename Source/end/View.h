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
    explicit View (jam::Model& model);
    ~View() override;

    void resized() override;
    void paint (juce::Graphics&) override {}

    /** @brief Routes key presses to the action registry.
     *  @return true if consumed by an action binding.
     */
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

private:
    jam::Model& model;
    juce::ValueTree config { config::Model::get() };

    //==============================================================================
    void registerActions();
    void setTabOrientation();
    void setViewState (int width, int height);
    //==============================================================================
    Tabs tabs;
    std::unique_ptr<jam::Model::Attachment> attachment;
    action::Registry registry;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (View)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
