/**
 * @file end/View.h
 * @brief Root content component — owns Tabs and Registry, routes keyboard input.
 */
#pragma once
#include <JuceHeader.h>
#include "end/Tabs.h"
#include "action/Registry.h"
#include "config/Config.h"

namespace end
{
/*____________________________________________________________________________*/

/** @class View
 *  @brief Root content component inside end::Window.
 *
 *  Owns the tab system and action registry. Implements KeyListener to
 *  intercept key presses and route them through the Registry's prefix
 *  key state machine. Transparent — glass shows through from Window.
 */
class View
    : public juce::Component
    , public juce::ValueTree::Listener
    , public juce::KeyListener
{
public:
    View();
    ~View() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    /** @brief Routes key presses to the action registry.
     *  @return true if consumed by an action binding.
     */
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    /** @brief Returns the Tabs component for Attachment wiring. */
    Tabs& getTabs() noexcept;

private:
    juce::ValueTree config { config::Model::get() };
    Tabs tabs;
    action::Registry registry;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (View)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
