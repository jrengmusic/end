#include "ENDView.h"

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
                                  jam::UUID uuid;
                                  sessionView->add (uuid);

                                  actions.actions.get<const juce::Identifier&> (ID::newPane, juce::Identifier {});
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
                    jam::UUID uuid;

                    if (edge.isValid())
                    {
                        const jam::UUID anchor { static_cast<int64_t> (
                            model.getValue (IDtype::sessions, ID::focusedPane)) };

                        uuid = tabView->add (anchor, edge);
                    }
                    else
                    {
                        uuid = tabView->add();
                    }

                    actions.actions.get (ID::newTerminal, std::move (uuid));
                }
            }
        });

    actions.actions.add<jam::UUID> (
        ID::newTerminal,
        [this] (jam::UUID uuid)
        {
            nexus.getActiveSession().newTerminal (uuid);
        });

    actions.actions.add (ID::closeTab,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                              {
                                  if (sessionView->getNumTabs() > 1)
                                  {
                                      const jam::UUID focusedTab { static_cast<int64_t> (
                                          sessionView->getValueTree().getProperty (ID::focusedTab)) };

                                      sessionView->remove (focusedTab);
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
                              {
                                  auto next { sessionView->getCurrentTabIndex() + 1 };

                                  if (next < sessionView->getNumTabs())
                                      sessionView->setCurrentTabIndex (next);
                              }
                          });

    actions.actions.add (ID::prevTab,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                              {
                                  auto prev { sessionView->getCurrentTabIndex() - 1 };

                                  if (prev >= 0)
                                      sessionView->setCurrentTabIndex (prev);
                              }
                          });

    actions.actions.add (ID::splitHorizontal, [this] {});

    actions.actions.add (ID::splitVertical, [this] {});

    actions.actions.add (ID::closePane,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                              {
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                  {
                                      if (tabView->getPaneCount() > 1)
                                      {
                                          const jam::UUID focusedUuid { static_cast<int64_t> (
                                              model.getValue (IDtype::sessions, ID::focusedPane)) };

                                          nexus.getActiveSession().removeTerminal (focusedUuid);
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
            auto& session { nexus.getActiveSession() };
            const jam::UUID id { static_cast<int64_t> (
                model.getValue (IDtype::sessions, ID::focusedPane)) };

            if (id.value != 0)
            {
                const float step { config.getValue (IDtype::display, ID::zoomStep) };
                session.get (id).model.zoomBy (step);
            }
        });

    actions.actions.add (
        ID::zoomOut,
        [this]
        {
            auto& session { nexus.getActiveSession() };
            const jam::UUID id { static_cast<int64_t> (
                model.getValue (IDtype::sessions, ID::focusedPane)) };

            if (id.value != 0)
            {
                const float step { config.getValue (IDtype::display, ID::zoomStep) };
                session.get (id).model.zoomBy (-step);
            }
        });

    actions.actions.add (ID::zoomReset,
                          [this]
                          {
                              auto& session { nexus.getActiveSession() };
                              const jam::UUID id { static_cast<int64_t> (
                                  model.getValue (IDtype::sessions, ID::focusedPane)) };

                              if (id.value != 0)
                                  session.get (id).model.setZoom (TerminalModel::defaultZoom);
                          });

    actions.actions.add (ID::paneLeft,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->focusPane (ID::paneLeft);
                          });

    actions.actions.add (ID::paneRight,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->focusPane (ID::paneRight);
                          });

    actions.actions.add (ID::paneUp,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->focusPane (ID::paneUp);
                          });

    actions.actions.add (ID::paneDown,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->focusPane (ID::paneDown);
                          });

    for (const auto& [key, id] : Position::get())
    {
        const int positionKey { key };

        actions.actions.add (
            Position::getPropertyId (key),
            [this, positionKey]
            {
                if (positionKey != Position::center)
                {
                    auto leaf { state.getChildWithProperty (
                        ID::position, Position::get (positionKey)) };

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
