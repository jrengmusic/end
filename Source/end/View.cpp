#include "end/View.h"

namespace end
{
/*____________________________________________________________________________*/

View::View (jam::Model& m)
    : jam::Model::Component { IDtype::view }
    , model (m)
{
    setOpaque (false);
    addKeyListener (this);
    setWantsKeyboardFocus (true);
    toFront (true);

    addAndMakeVisible (tabs);
    setTabOrientation();
    tabs.addNewTab();

    auto init { config::Model::getInitWindowSize() };
    setSize (init.getWidth(), init.getHeight());

    //==============================================================================
    attachment = std::make_unique<jam::Model::Attachment> (model, *this);

    registerActions();

    config.addListener (this);

    cout (model.getXml()->toString());
}

View::~View()
{
    config.removeListener (this);
    removeKeyListener (this);
}

void View::resized()
{
    setViewState (getWidth(), getHeight());

    //==============================================================================
    tabs.setBounds (getLocalBounds());
}

bool View::keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused (originatingComponent);
    return registry.keyPressed (key);
}

void View::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& property)
{
    if (property == ID::orientation)
        setTabOrientation();
}

//==============================================================================
void View::setViewState (int width, int height)
{
    state.setProperty (jam::ID::width, width, nullptr);
    state.setProperty (jam::ID::height, height, nullptr);
}

void View::setTabOrientation()
{
    auto display { config.getChildWithName (IDtype::display) };
    auto tabNode { display.getChildWithName (IDtype::tab) };
    auto pos { tabNode.getProperty (ID::orientation).toString() };

    tabs.setOrientation (TabOrientation::get (pos));
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
