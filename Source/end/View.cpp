#include "end/View.h"

namespace end
{
/*____________________________________________________________________________*/

View::View()
{
    setOpaque (false);
    addAndMakeVisible (tabs);
    addKeyListener (this);
    setWantsKeyboardFocus (true);
    toFront (true);

    auto window { jam::ValueTree::getChildWithName (config, IDtype::window) };
    int width { window.getProperty (jam::ID::width) };
    int height { window.getProperty (jam::ID::height) };
    setSize (width, height);

    registry.actions.add (ID::newTab,
                          [this]
                          {
                              tabs.addNewTab();
                          });
    registry.actions.add (ID::closeTab,
                          [this]
                          {
                              tabs.removeCurrentTab();
                          });
    registry.actions.add (ID::nextTab,
                          [this]
                          {
                              auto next { tabs.getCurrentTabIndex() + 1 };

                              if (next < tabs.getNumTabs())
                                  tabs.setCurrentTabIndex (next);
                          });

    registry.actions.add (ID::prevTab,
                          [this]
                          {
                              auto prev { tabs.getCurrentTabIndex() - 1 };

                              if (prev >= 0)
                                  tabs.setCurrentTabIndex (prev);
                          });

    tabs.addNewTab();

    config.addListener (this);
}

View::~View()
{
    config.removeListener (this);
    removeKeyListener (this);
}

void View::resized() { tabs.setBounds (getLocalBounds()); }

void View::paint (juce::Graphics&) {}

bool View::keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused (originatingComponent);
    return registry.keyPressed (key);
}

void View::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) {}

Tabs& View::getTabs() noexcept { return tabs; }

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
