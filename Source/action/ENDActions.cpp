#include "end/ENDView.h"

static constexpr const char* whelmedPluginId { "com.jreng.whelmed" };
static const juce::String groupKeySeparator { "#" };

void ENDView::registerSessionActions()
{
    actions.add (Id::newSession,
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
}

void ENDView::registerTabActions()
{
    actions.add (Id::newTab,
                 [this]
                 {
                     if (auto* sessionView { getActiveSessionView() })
                     {
                         jam::UUID uuid {};
                         sessionView->add (uuid);

                         const juce::Identifier edge {};
                         actions.get (Id::newPane, edge);
                     }
                 });

    actions.add<const juce::Identifier&> (
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
                        actions.get (Id::newPlugin, std::move (uuid));
                }
            }
        });

    actions.add<jam::UUID> (
        Id::newPlugin,
        [&nexus = nexus] (jam::UUID uuid)
        {
            auto& session { nexus.getActiveSession() };

            nexus.createPlugin (whelmedPluginId,
                [&nexus = nexus, &session, uuid] (std::unique_ptr<juce::AudioPluginInstance> instance)
                {
                    if (instance != nullptr)
                    {
                        nexus.createVirtualClock (uuid, *instance);
                        session.newPlugin (uuid, whelmedPluginId, std::move (instance));
                    }
                    else
                    {
                        session.newPlugin (uuid, {}, nullptr);
                    }
                });
        });

    actions.add (Id::closeTab,
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

    actions.add (Id::quit,
                 [this]
                 {
                     juce::JUCEApplication::getInstance()->systemRequestedQuit();
                 });

    actions.add (Id::reload,
                 [this]
                 {
                     config.loadFromPath();
                 });

    actions.add (Id::nextTab,
                 [this]
                 {
                     if (auto* sessionView { getActiveSessionView() })
                         sessionView->nextTab();
                 });

    actions.add (Id::prevTab,
                 [this]
                 {
                     if (auto* sessionView { getActiveSessionView() })
                         sessionView->prevTab();
                 });

    actions.add (Id::splitHorizontal,
                 [this]
                 {
                     actions.get (Id::newPane, Id::bottom);
                 });

    actions.add (Id::splitVertical,
                 [this]
                 {
                     actions.get (Id::newPane, Id::right);
                 });

    actions.add (Id::closePane,
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
}

static void registerZoomAction (ENDActions& actionRegistry,
                                 jam::Model& model,
                                 const juce::Identifier& actionId,
                                 const std::function<float (float)>& computeZoom)
{
    actionRegistry.add (actionId,
                        [&model, computeZoom]
                        {
                            const jam::UUID id { static_cast<int64_t> (
                                model.getValue (Id::toType (Id::sessions), Id::focusedPane)) };

                            if (id.value != 0)
                            {
                                const juce::Identifier paneGroup { Id::toType (Id::pane).toString()
                                                                    + groupKeySeparator
                                                                    + juce::String (id.value) };
                                auto* zoomParameter { model.getParameter<jam::Parameter<float>> (
                                    paneGroup, Id::zoom) };

                                jassert (zoomParameter != nullptr);
                                zoomParameter->setValue (juce::jlimit (
                                    EditorView::zoomMin, EditorView::zoomMax,
                                    computeZoom (zoomParameter->getValue())));
                            }
                        });
}

void ENDView::registerZoomActions()
{
    registerZoomAction (actions, model, Id::zoomIn,
        [&config = config] (float currentZoom)
        {
            const float step { config.getValue (Id::toType (Id::display), Id::zoomStep) };
            return currentZoom + step;
        });

    registerZoomAction (actions, model, Id::zoomOut,
        [&config = config] (float currentZoom)
        {
            const float step { config.getValue (Id::toType (Id::display), Id::zoomStep) };
            return currentZoom - step;
        });

    registerZoomAction (actions, model, Id::zoomReset,
        [] (float)
        {
            return EditorView::defaultZoom;
        });
}

void ENDView::registerPaneActions()
{
    static const std::array<std::pair<juce::Identifier, juce::Identifier>, 4> focusDirections {{
        { Id::paneLeft, Id::left },
        { Id::paneRight, Id::right },
        { Id::paneUp, Id::top },
        { Id::paneDown, Id::bottom },
    }};

    for (const auto& [actionId, direction] : focusDirections)
    {
        actions.add (actionId,
                     [this, direction = direction]
                     {
                         if (auto* sessionView { getActiveSessionView() })
                             if (auto* tabView { sessionView->getActiveTabView() })
                                 tabView->focusPane (direction);
                     });
    }

    static const std::array<std::pair<juce::Identifier, juce::Identifier>, 4> joinDirections {{
        { Id::joinLeft, Id::left },
        { Id::joinDown, Id::bottom },
        { Id::joinUp, Id::top },
        { Id::joinRight, Id::right },
    }};

    for (const auto& [actionId, direction] : joinDirections)
    {
        actions.add (actionId,
                     [this, direction = direction]
                     {
                         if (auto* sessionView { getActiveSessionView() })
                             if (auto* tabView { sessionView->getActiveTabView() })
                             {
                                 const auto target { tabView->join (direction) };

                                 if (target != jam::UUID::none())
                                 {
                                     nexus.removeVirtualClock (target);
                                     nexus.getActiveSession().removePlugin (target);
                                 }
                             }
                     });
    }

    static const std::array<std::pair<juce::Identifier, juce::Identifier>, 4> swapDirections {{
        { Id::swapLeft, Id::left },
        { Id::swapDown, Id::bottom },
        { Id::swapUp, Id::top },
        { Id::swapRight, Id::right },
    }};

    for (const auto& [actionId, direction] : swapDirections)
    {
        actions.add (actionId,
                     [this, direction = direction]
                     {
                         if (auto* sessionView { getActiveSessionView() })
                             if (auto* tabView { sessionView->getActiveTabView() })
                                 tabView->swap (direction);
                     });
    }

    using PaneResize = void (TabView::*) (jam::UUID, const juce::Identifier&, float);

    static const std::array<std::tuple<juce::Identifier, juce::Identifier, PaneResize>, 4> paneResizes {{
        { Id::reducePaneWidth, Id::width, &TabView::reducePane },
        { Id::reducePaneHeight, Id::height, &TabView::reducePane },
        { Id::expandPaneWidth, Id::width, &TabView::expandPane },
        { Id::expandPaneHeight, Id::height, &TabView::expandPane },
    }};

    for (const auto& [actionId, axis, resize] : paneResizes)
    {
        actions.add (actionId,
                     [this, axis = axis, resize = resize]
                     {
                         if (auto* sessionView { getActiveSessionView() })
                             if (auto* tabView { sessionView->getActiveTabView() })
                             {
                                 const float step { config.getValue (Id::toType (Id::display), Id::paneStep) };
                                 (tabView->*resize) (tabView->getFocusedChild(), axis, step);
                             }
                     });
    }
}

void ENDView::registerWindowActions()
{
    for (const auto& [key, id] : map::Position::getInstance()->get())
    {
        const int positionKey { key };

        actions.add (
            juce::Identifier { map::Position::getInstance()->get (key) },
            [this, positionKey]
            {
                if (positionKey != map::Position::center)
                {
                    auto leaf { state.getChildWithProperty (
                        Id::position, map::Position::getInstance()->get (positionKey)) };

                    if (leaf.isValid())
                    {
                        const bool visible { jam::toBool (leaf.getProperty (Id::visible)) };

                        leaf.setProperty (
                            Id::visible, static_cast<int> (not visible), nullptr);
                    }
                }
            });
    }
}

void ENDView::registerActions()
{
    registerSessionActions();
    registerTabActions();
    registerZoomActions();
    registerPaneActions();
    registerWindowActions();
}
