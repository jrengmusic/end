#include "end/View.h"

namespace end
{
/*____________________________________________________________________________*/

View::View (jam::Model& m)
    : jam::Model::Component { m, IDtype::view }
    , tabs (m)
{
    setOpaque (false);
    addKeyListener (this);
    setWantsKeyboardFocus (true);
    toFront (true);
    registerActions();

    addAndMakeVisible (tabs);
    addChildComponent (messageOverlay);

    //==============================================================================
    attachments.add (std::make_unique<jam::Model::Attachment> (*this));
    attachments.add (std::make_unique<jam::Model::Attachment> (tabs));

    config.addListener (this);

    setTabOrientation();
    tabs.addNewTab();

    //==============================================================================
    auto init { config.getInitWindowSize() };
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
    messageOverlay.setBounds (getLocalBounds());
}

bool View::keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused (originatingComponent);
    return registry.keyPressed (key);
}

void View::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == ID::loadMessage)
        messageOverlay.showMessage (config.getLoadMessage());

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

    tabs.setOrientation (Position::get (pos));
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

    registry.actions.add (ID::splitHorizontal,
                          [this]
                          {
                              auto id { state.getProperty (ID::focusedPane) };
                              if (auto* panes { tabs.getActivePanes() })
                                  panes->split (
                                      jam::UUID (static_cast<int64_t> (id)), jam::ID::horizontal);
                          });

    registry.actions.add (ID::splitVertical,
                          [this]
                          {
                              auto id { state.getProperty (ID::focusedPane) };
                              if (auto* panes { tabs.getActivePanes() })
                                  panes->split (
                                      jam::UUID (static_cast<int64_t> (id)), jam::ID::vertical);
                          });

    registry.actions.add (ID::closePane,
                          [this]
                          {
                              if (auto* panes { tabs.getActivePanes() })
                              {
                                  if (panes->getPaneCount() == 1)
                                  {
                                      tabs.removeCurrentTab();
                                  }
                                  else
                                  {
                                      auto id { state.getProperty (ID::focusedPane) };
                                      panes->removePane (jam::UUID (static_cast<int64_t> (id)));
                                  }
                              }
                          });

    registry.actions.add (ID::paneLeft,
                          [this]
                          {
                              if (auto* p { tabs.getActivePanes() })
                                  p->focusPane (ID::paneLeft);
                          });

    registry.actions.add (ID::paneRight,
                          [this]
                          {
                              if (auto* p { tabs.getActivePanes() })
                                  p->focusPane (ID::paneRight);
                          });

    registry.actions.add (ID::paneUp,
                          [this]
                          {
                              if (auto* p { tabs.getActivePanes() })
                                  p->focusPane (ID::paneUp);
                          });

    registry.actions.add (ID::paneDown,
                          [this]
                          {
                              if (auto* p { tabs.getActivePanes() })
                                  p->focusPane (ID::paneDown);
                          });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
