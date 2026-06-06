#include "end/View.h"

namespace end
{
/*____________________________________________________________________________*/

View::View()
    : jam::ValueTree::Component { IDtype::view }
    , tabsAttachment { state, tabs }
{
    initialise();
    registerActions();
    config.addListener (this);

    tabs.addNewTab();
    addAndMakeVisible (tabs);
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

//==============================================================================
void View::initialise()
{
    setOpaque (false);
    addKeyListener (this);
    setWantsKeyboardFocus (true);
    toFront (true);

    auto window { jam::ValueTree::getChildWithName (config, IDtype::window) };
    auto csv { window.getProperty (ID::size).toString() };
    auto parts { juce::StringArray::fromTokens (csv, ",", "") };
    assert (parts.size() == 2);
    setSize (parts[0].trim().getIntValue(), parts[1].trim().getIntValue());
}

//==============================================================================
void View::registerActions()
{
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
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
