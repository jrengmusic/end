#include "end/ENDView.h"

static constexpr const char* whelmedPluginId { "com.jreng.whelmed" };

void ENDView::createDockPane (int positionKey)
{
}

void ENDView::registerActions()
{
    actions.actions.add (Id::newSession,
                          [this]
                          {
                              auto& session { nexus.createSession() };
                              const jam::UUID sessionUuid { static_cast<int64_t> (
                                  session.state.getProperty (Id::id)) };

                              auto [entry, inserted] = sessions.try_emplace (
                                  sessionUuid, std::make_unique<SessionView> (model, session.state));
                              jassert (inserted);
                              auto& [key, sessionView] = *entry;

                              addAndMakeVisible (*sessionView);
                              sessionView->toBehind (&messageOverlay);
                              attachments.try_emplace (sessionUuid,
                                                       std::make_unique<jam::Model::Attachment> (*sessionView));
                              resized();

                              actions.run (Id::newTab);
                          });

    actions.actions.add (Id::newTab,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                              {
                                  jam::UUID uuid {};
                                  sessionView->add (uuid);

                                  const juce::Identifier edge {};
                                  actions.actions.get (Id::newPane, edge);
                              }
                          });

    actions.actions.add<const juce::Identifier&> (
        Id::newPane,
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
                        actions.actions.get (Id::newPlugin, std::move (uuid));
                }
            }
        });

    actions.actions.add<jam::UUID> (
        Id::newPlugin,
        [this] (jam::UUID uuid)
        {
            nexus.createPlugin (whelmedPluginId,
                [this, uuid] (std::unique_ptr<juce::AudioPluginInstance> instance)
                {
                    if (instance != nullptr)
                    {
                        nexus.createVirtualClock (uuid, *instance);
                        nexus.getActiveSession().newPlugin (uuid, whelmedPluginId, std::move (instance));
                    }
                    else
                    {
                        nexus.getActiveSession().newPlugin (uuid, {}, nullptr);
                    }
                });
        });

    actions.actions.add (Id::closeTab,
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

    actions.actions.add (Id::nextTab,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  sessionView->nextTab();
                          });

    actions.actions.add (Id::prevTab,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  sessionView->prevTab();
                          });

    actions.actions.add (Id::splitHorizontal,
                          [this]
                          {
                              actions.actions.get (Id::newPane, Id::bottom);
                          });

    actions.actions.add (Id::splitVertical,
                          [this]
                          {
                              actions.actions.get (Id::newPane, Id::right);
                          });

    actions.actions.add (Id::closePane,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                              {
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                  {
                                      if (tabView->getChildCount() > 1)
                                      {
                                          const auto focusedUuid { tabView->getFocusedChild() };

                                          tabView->remove (focusedUuid);
                                          nexus.removeVirtualClock (focusedUuid);
                                          nexus.getActiveSession().removePlugin (focusedUuid);
                                      }
                                      else
                                      {
                                          actions.run (Id::closeTab);
                                      }
                                  }
                              }
                          });

    actions.actions.add (
        Id::zoomIn,
        [this]
        {
            const jam::UUID id { static_cast<int64_t> (
                model.getValue (::Id::toType (::Id::sessions), ::Id::focusedPane)) };

            if (id.value != 0)
            {
                const float step { config.getValue (::Id::toType (::Id::display), ::Id::zoomStep) };
                const juce::Identifier paneGroup { ::Id::toType (::Id::pane).toString() + "#" + juce::String (id.value) };
                auto* zoomParameter { model.getParameter<jam::Parameter<float>> (paneGroup, ::Id::zoom) };

                jassert (zoomParameter != nullptr);
                zoomParameter->setValue (juce::jlimit (EditorView::zoomMin, EditorView::zoomMax, zoomParameter->getValue() + step));
            }
        });

    actions.actions.add (
        Id::zoomOut,
        [this]
        {
            const jam::UUID id { static_cast<int64_t> (
                model.getValue (::Id::toType (::Id::sessions), ::Id::focusedPane)) };

            if (id.value != 0)
            {
                const float step { config.getValue (::Id::toType (::Id::display), ::Id::zoomStep) };
                const juce::Identifier paneGroup { ::Id::toType (::Id::pane).toString() + "#" + juce::String (id.value) };
                auto* zoomParameter { model.getParameter<jam::Parameter<float>> (paneGroup, ::Id::zoom) };

                jassert (zoomParameter != nullptr);
                zoomParameter->setValue (juce::jlimit (EditorView::zoomMin, EditorView::zoomMax, zoomParameter->getValue() - step));
            }
        });

    actions.actions.add (Id::zoomReset,
                          [this]
                          {
                              const jam::UUID id { static_cast<int64_t> (
                                  model.getValue (::Id::toType (::Id::sessions), ::Id::focusedPane)) };

                              if (id.value != 0)
                              {
                                  const juce::Identifier paneGroup { ::Id::toType (::Id::pane).toString() + "#" + juce::String (id.value) };
                                  auto* zoomParameter { model.getParameter<jam::Parameter<float>> (paneGroup, ::Id::zoom) };

                                  jassert (zoomParameter != nullptr);
                                  zoomParameter->setValue (juce::jlimit (EditorView::zoomMin, EditorView::zoomMax, EditorView::defaultZoom));
                              }
                          });

    actions.actions.add (Id::paneLeft,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->focusPane (Id::left);
                          });

    actions.actions.add (Id::paneRight,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->focusPane (Id::right);
                          });

    actions.actions.add (Id::paneUp,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->focusPane (Id::top);
                          });

    actions.actions.add (Id::paneDown,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->focusPane (Id::bottom);
                          });

    actions.actions.add (Id::joinLeft,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                  {
                                      const auto target { tabView->join (Id::left) };

                                      if (target != jam::UUID::none())
                                      {
                                          nexus.removeVirtualClock (target);
                                          nexus.getActiveSession().removePlugin (target);
                                      }
                                  }
                          });

    actions.actions.add (Id::joinDown,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                  {
                                      const auto target { tabView->join (Id::bottom) };

                                      if (target != jam::UUID::none())
                                      {
                                          nexus.removeVirtualClock (target);
                                          nexus.getActiveSession().removePlugin (target);
                                      }
                                  }
                          });

    actions.actions.add (Id::joinUp,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                  {
                                      const auto target { tabView->join (Id::top) };

                                      if (target != jam::UUID::none())
                                      {
                                          nexus.removeVirtualClock (target);
                                          nexus.getActiveSession().removePlugin (target);
                                      }
                                  }
                          });

    actions.actions.add (Id::joinRight,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                  {
                                      const auto target { tabView->join (Id::right) };

                                      if (target != jam::UUID::none())
                                      {
                                          nexus.removeVirtualClock (target);
                                          nexus.getActiveSession().removePlugin (target);
                                      }
                                  }
                          });

    actions.actions.add (Id::swapLeft,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->swap (Id::left);
                          });

    actions.actions.add (Id::swapDown,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->swap (Id::bottom);
                          });

    actions.actions.add (Id::swapUp,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->swap (Id::top);
                          });

    actions.actions.add (Id::swapRight,
                          [this]
                          {
                              if (auto* sessionView { getActiveSessionView() })
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->swap (Id::right);
                          });

    actions.actions.add (
        Id::reducePaneWidth,
        [this]
        {
            if (auto* sessionView { getActiveSessionView() })
                if (auto* tabView { sessionView->getActiveTabView() })
                {
                    const float step { config.getValue (Id::toType (Id::display), Id::paneStep) };
                    tabView->reducePane (tabView->getFocusedChild(), Id::width, step);
                }
        });

    actions.actions.add (
        Id::reducePaneHeight,
        [this]
        {
            if (auto* sessionView { getActiveSessionView() })
                if (auto* tabView { sessionView->getActiveTabView() })
                {
                    const float step { config.getValue (Id::toType (Id::display), Id::paneStep) };
                    tabView->reducePane (tabView->getFocusedChild(), Id::height, step);
                }
        });

    actions.actions.add (
        Id::expandPaneWidth,
        [this]
        {
            if (auto* sessionView { getActiveSessionView() })
                if (auto* tabView { sessionView->getActiveTabView() })
                {
                    const float step { config.getValue (Id::toType (Id::display), Id::paneStep) };
                    tabView->expandPane (tabView->getFocusedChild(), Id::width, step);
                }
        });

    actions.actions.add (
        Id::expandPaneHeight,
        [this]
        {
            if (auto* sessionView { getActiveSessionView() })
                if (auto* tabView { sessionView->getActiveTabView() })
                {
                    const float step { config.getValue (Id::toType (Id::display), Id::paneStep) };
                    tabView->expandPane (tabView->getFocusedChild(), Id::height, step);
                }
        });

    // `id` (structured binding, lowercase) does not shadow `Id::` (the
    // vocabulary namespace, case-sensitive) — ::Id::position and
    // ::Id::visible below are globally qualified for consistency only.
    for (const auto& [key, id] : Id::Position::get())
    {
        const int positionKey { key };

        actions.actions.add (
            juce::Identifier { Id::Position::get (key) },
            [this, positionKey]
            {
                if (positionKey != Id::Position::center)
                {
                    auto leaf { state.getChildWithProperty (
                        ::Id::position, Id::Position::get (positionKey)) };

                    if (leaf.isValid())
                    {
                        const bool visible { jam::toBool (leaf.getProperty (::Id::visible)) };

                        leaf.setProperty (
                            ::Id::visible, static_cast<int> (not visible), nullptr);
                    }
                    else
                    {
                        createDockPane (positionKey);
                    }
                }
            });
    }
}
