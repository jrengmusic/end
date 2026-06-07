#include "end/View.h"

namespace end
{
/*____________________________________________________________________________*/

View::View (jam::Model& m)
    : jam::Model::Component { m, IDtype::view }
    , config (config::Model::get())
    , model (m.state)
    , tabs (m)
{
    state.setProperty (ID::focusedPane, juce::String(), nullptr);

    //==============================================================================
    setOpaque (false);
    addKeyListener (this);
    setWantsKeyboardFocus (true);
    toFront (true);
    registerActions();

    addAndMakeVisible (tabs);
    setTabOrientation();

    //==============================================================================
    attachments.add (std::make_unique<jam::Model::Attachment> (*this));
    attachments.add (std::make_unique<jam::Model::Attachment> (tabs));

    config.addListener (this);
    model.addListener (this);

    tabs.addNewTab();

    //==============================================================================
    auto init { config::Model::getInitWindowSize() };
    setSize (init.getWidth(), init.getHeight());
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

void View::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == ID::orientation)
        setTabOrientation();

    if (jam::toBool (tree.getProperty (ID::focus)))
    {
        auto id { tree.getProperty (jam::ID::id) };
        state.setProperty (ID::focusedPane, id, nullptr);
    }
}

void View::valueTreeChildAdded (juce::ValueTree& parentTree,
                                juce::ValueTree& childWhichHasBeenAdded)
{
    auto id { childWhichHasBeenAdded.getChildWithName (IDtype::pane).getProperty (jam::ID::id) };
    state.setProperty (ID::focusedPane, id, nullptr);
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
