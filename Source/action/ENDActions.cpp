#include "end/ENDView.h"

void ENDView::createDockPane (int positionKey)
{
}

void ENDView::registerActions()
{
    actions.actions.add (ID::newSession,
                          [this]
                          {
                              auto& session { nexus.createSession() };
                              const jam::UUID sessionUuid { static_cast<int64_t> (
                                  session.state.getProperty (jam::ID::id)) };

                              auto [entry, inserted] = sessions.try_emplace (
                                  sessionUuid, std::make_unique<SessionView> (model, session.state));
                              jassert (inserted);
                              auto& [key, sessionView] = *entry;

                              addAndMakeVisible (*sessionView);
                              sessionView->toBehind (&messageOverlay);
                              attachments.try_emplace (sessionUuid,
                                                       std::make_unique<jam::Model::Attachment> (*sessionView));
                              resized();

                              actions.run (ID::newTab);
                          });

    actions.actions.add (ID::newTab,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                              {
                                  jam::UUID uuid {};
                                  sessionView->add (uuid);

                                  const juce::Identifier edge {};
                                  actions.actions.get (ID::newPane, edge);
                              }
                          });

    actions.actions.add<const juce::Identifier&> (
        ID::newPane,
        [this] (const juce::Identifier& edge)
        {
            if (auto* sessionView { getActiveSessionView() })
            {
                if (auto* tabView { sessionView->getActiveTabView() })
                {
                    jam::UUID uuid {};

                    if (edge.isValid())
                        uuid = tabView->split (edge);
                    else
                        uuid = tabView->add();

                    if (uuid != jam::UUID::none())
                        actions.actions.get (ID::newPlugin, std::move (uuid));
                }
            }
        });

    actions.actions.add<jam::UUID> (
        ID::newPlugin,
        [this] (jam::UUID uuid)
        {
            nexus.getActiveSession().newPlugin (uuid, {}, nullptr);
        });

    actions.actions.add (ID::closeTab,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                              {
                                  if (sessionView->getChildCount() > 1)
                                  {
                                      sessionView->remove (sessionView->getFocusedChild());
                                  }
                                  else
                                  {
                                      juce::JUCEApplication::getInstance()->systemRequestedQuit();
                                  }
                              }
                          });

    actions.actions.add (ID::nextTab,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  sessionView->nextTab();
                          });

    actions.actions.add (ID::prevTab,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  sessionView->prevTab();
                          });

    actions.actions.add (ID::splitHorizontal,
                          [this]
                          {
                              actions.actions.get (ID::newPane, jam::ID::bottom);
                          });

    actions.actions.add (ID::splitVertical,
                          [this]
                          {
                              actions.actions.get (ID::newPane, jam::ID::right);
                          });

    actions.actions.add (ID::closePane,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                              {
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                  {
                                      if (tabView->getChildCount() > 1)
                                      {
                                          const auto focusedUuid { tabView->getFocusedChild() };

                                          nexus.getActiveSession().removePlugin (focusedUuid);
                                          tabView->remove (focusedUuid);
                                      }
                                      else
                                      {
                                          actions.run (ID::closeTab);
                                      }
                                  }
                              }
                          });

    actions.actions.add (
        ID::zoomIn,
        [this]
        {
            const jam::UUID id { static_cast<int64_t> (
                model.getValue (IDtype::sessions, jam::ID::focusedPane)) };

            if (id.value != 0)
            {
                const float step { config.getValue (IDtype::display, ID::zoomStep) };
                const juce::Identifier paneGroup { IDtype::pane.toString() + "#" + juce::String (id.value) };
                auto* zoomParameter { model.getParameter<jam::Parameter<float>> (paneGroup, ID::zoom) };

                jassert (zoomParameter != nullptr);
                zoomParameter->setValue (juce::jlimit (EditorView::zoomMin, EditorView::zoomMax, zoomParameter->getValue() + step));
            }
        });

    actions.actions.add (
        ID::zoomOut,
        [this]
        {
            const jam::UUID id { static_cast<int64_t> (
                model.getValue (IDtype::sessions, jam::ID::focusedPane)) };

            if (id.value != 0)
            {
                const float step { config.getValue (IDtype::display, ID::zoomStep) };
                const juce::Identifier paneGroup { IDtype::pane.toString() + "#" + juce::String (id.value) };
                auto* zoomParameter { model.getParameter<jam::Parameter<float>> (paneGroup, ID::zoom) };

                jassert (zoomParameter != nullptr);
                zoomParameter->setValue (juce::jlimit (EditorView::zoomMin, EditorView::zoomMax, zoomParameter->getValue() - step));
            }
        });

    actions.actions.add (ID::zoomReset,
                          [this]
                          {
                              const jam::UUID id { static_cast<int64_t> (
                                  model.getValue (IDtype::sessions, jam::ID::focusedPane)) };

                              if (id.value != 0)
                              {
                                  const juce::Identifier paneGroup { IDtype::pane.toString() + "#" + juce::String (id.value) };
                                  auto* zoomParameter { model.getParameter<jam::Parameter<float>> (paneGroup, ID::zoom) };

                                  jassert (zoomParameter != nullptr);
                                  zoomParameter->setValue (juce::jlimit (EditorView::zoomMin, EditorView::zoomMax, EditorView::defaultZoom));
                              }
                          });

    actions.actions.add (ID::paneLeft,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->focusPane (jam::ID::left);
                          });

    actions.actions.add (ID::paneRight,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->focusPane (jam::ID::right);
                          });

    actions.actions.add (ID::paneUp,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->focusPane (jam::ID::top);
                          });

    actions.actions.add (ID::paneDown,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->focusPane (jam::ID::bottom);
                          });

    actions.actions.add (ID::joinLeft,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                  {
                                      const auto target { tabView->join (jam::ID::left) };

                                      if (target != jam::UUID::none())
                                          nexus.getActiveSession().removePlugin (target);
                                  }
                          });

    actions.actions.add (ID::joinDown,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                  {
                                      const auto target { tabView->join (jam::ID::bottom) };

                                      if (target != jam::UUID::none())
                                          nexus.getActiveSession().removePlugin (target);
                                  }
                          });

    actions.actions.add (ID::joinUp,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                  {
                                      const auto target { tabView->join (jam::ID::top) };

                                      if (target != jam::UUID::none())
                                          nexus.getActiveSession().removePlugin (target);
                                  }
                          });

    actions.actions.add (ID::joinRight,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                  {
                                      const auto target { tabView->join (jam::ID::right) };

                                      if (target != jam::UUID::none())
                                          nexus.getActiveSession().removePlugin (target);
                                  }
                          });

    actions.actions.add (ID::swapLeft,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->swap (jam::ID::left);
                          });

    actions.actions.add (ID::swapDown,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->swap (jam::ID::bottom);
                          });

    actions.actions.add (ID::swapUp,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->swap (jam::ID::top);
                          });

    actions.actions.add (ID::swapRight,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->swap (jam::ID::right);
                          });

    actions.actions.add (
        ID::reducePaneWidth,
        [this]
        {
            if (auto* sessionView { getActiveSessionView() })
                if (auto* tabView { sessionView->getActiveTabView() })
                {
                    const float step { config.getValue (IDtype::display, ID::paneStep) };
                    tabView->reducePane (tabView->getFocusedChild(), jam::ID::width, step);
                }
        });

    actions.actions.add (
        ID::reducePaneHeight,
        [this]
        {
            if (auto* sessionView { getActiveSessionView() })
                if (auto* tabView { sessionView->getActiveTabView() })
                {
                    const float step { config.getValue (IDtype::display, ID::paneStep) };
                    tabView->reducePane (tabView->getFocusedChild(), jam::ID::height, step);
                }
        });

    actions.actions.add (
        ID::expandPaneWidth,
        [this]
        {
            if (auto* sessionView { getActiveSessionView() })
                if (auto* tabView { sessionView->getActiveTabView() })
                {
                    const float step { config.getValue (IDtype::display, ID::paneStep) };
                    tabView->expandPane (tabView->getFocusedChild(), jam::ID::width, step);
                }
        });

    actions.actions.add (
        ID::expandPaneHeight,
        [this]
        {
            if (auto* sessionView { getActiveSessionView() })
                if (auto* tabView { sessionView->getActiveTabView() })
                {
                    const float step { config.getValue (IDtype::display, ID::paneStep) };
                    tabView->expandPane (tabView->getFocusedChild(), jam::ID::height, step);
                }
        });

    for (const auto& [key, id] : jam::Position::get())
    {
        const int positionKey { key };

        actions.actions.add (
            jam::Position::getPropertyId (key),
            [this, positionKey]
            {
                if (positionKey != jam::Position::center)
                {
                    auto leaf { state.getChildWithProperty (
                        ID::position, jam::Position::get (positionKey)) };

                    if (leaf.isValid())
                    {
                        const bool visible { jam::toBool (leaf.getProperty (jam::ID::visible)) };

                        leaf.setProperty (
                            jam::ID::visible, static_cast<int> (not visible), nullptr);
                    }
                    else
                    {
                        createDockPane (positionKey);
                    }
                }
            });
    }
}
