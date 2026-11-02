#include "ui/ENDView.h"

ENDView::ENDView()
    : jam::Model::Component<ENDView> (ENDModel::getInstance()->getChildWithName (Id::toType (Id::window)))
    , messageOverlay (model.getChildWithName (Id::toType (Id::overlay)), [] { return ENDLookAndFeel::getInstance()->getOverlayFont(); })
{
    setOpaque (false);
    toFront (true);

    registerActions();
    registerEvents();

    commandManager->registerAllCommandsForTarget (this);
    commandManager->setKeyPresses (config.getChildWithName (Id::toType (Id::keys)));

    addAndMakeVisible (background);
    addMouseListener (&background, true);

    addChildComponent (messageOverlay);

    createAndAttachParameters();

    config.addListener (this);
    model.addListener (this);

    juce::MessageManager::callAsync (
        [this]
        {
            events.get (Id::enabled, config.state);

            actions.run (Id::newSession);
            grabKeyboardFocus();
        });

    //==============================================================================
}

ENDView::~ENDView()
{
    model.removeListener (this);
    config.removeListener (this);
}

void ENDView::resized()
{
    setViewState (jam::Size<int16_t> (getWidth(), getHeight()));

    background.setBounds (getLocalBounds());
    messageOverlay.setBounds (getLocalBounds());

    auto* sessionView { getActiveSessionView() };

    if (sessionView != nullptr)
        sessionView->setBounds (getLocalBounds());
}

bool ENDView::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return commandManager->invokeKey (key, state);
}

void ENDView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (events.contains (property))
    {
        events.get (property, tree);
    }
    else if (events.contains (tree.getType()))
    {
        events.get (tree.getType(), tree);
    }
    else if (tree.getParent().hasType (Id::toType (Id::keys)))
    {
        commandManager->setKeyPresses (tree.getParent());
    }
}

juce::ApplicationCommandTarget* ENDView::getNextCommandTarget()
{
    return nullptr;
}

void ENDView::getAllCommands (juce::Array<juce::CommandID>& commands)
{
    for (const auto& [command, name] : map::CommandId::getInstance()->get())
        commands.add (command);
}

void ENDView::getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result)
{
    const auto name { map::CommandId::getInstance()->get (commandID).toString() };
    result.setInfo (name, name, {}, result.flags & ~ juce::ApplicationCommandInfo::isDisabled);
}

bool ENDView::perform (const InvocationInfo& info)
{
    return actions.run (juce::Identifier { commandManager->getNameOfCommand (info.commandID) });
}

void ENDView::createAndAttachParameters()
{
    const auto windowSize { config.getRowValue (Id::toType (Id::display), Id::size) };
    const int width { windowSize[0] };
    const int height { windowSize[1] };

    //==============================================================================
    model.createAndAddParameter<jam::Parameter<int>> (
        state, Id::size, jam::Size<int16_t> (width, height).toInt());

    messageOverlay.registerParameters (model);

    //==============================================================================
    setSize (width, height);
}

void ENDView::setViewState (jam::Size<int16_t> size)
{
    state.setProperty (Id::size, size.toInt(), nullptr);
}

SessionView* ENDView::getActiveSessionView() noexcept
{
    auto* focusedSessionParameter { model.getParameter<jam::Parameter<int64_t>> (
        Id::toType (Id::sessions), Id::focusedSession) };
    jassert (focusedSessionParameter != nullptr);

    const jam::UUID sessionUuid { focusedSessionParameter->getValue() };

    return sessions.contains (sessionUuid) ? sessions.at (sessionUuid).get() : nullptr;
}

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
                     sessionView->getValueTree().setProperty (Id::focus, sessionView.get() == getActiveSessionView() ? 1 : 0, nullptr);
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
                    if (edge.isValid())
                        tabView->split (edge);
                    else
                        tabView->add();
                }
            }
        });

    actions.add (Id::closeTab,
                 [this]
                 {
                     if (auto* sessionView { getActiveSessionView() })
                     {
                         if (sessionView->getChildCount() > 1)
                         {
                             std::function<void (const juce::ValueTree&)> removePanes { [this, &removePanes] (const juce::ValueTree& row)
                             {
                                 if (row.hasType (Id::toType (Id::pane)))
                                     nexus.getActiveSession().removePlugin (
                                         jam::UUID { static_cast<int64_t> (row.getProperty (Id::id)) });

                                 for (const auto& child : row)
                                     removePanes (child);
                             } };

                             const auto focusedTab { sessionView->getFocusedChild() };
                             const auto tabState { sessionView->get (focusedTab).state };

                             sessionView->remove (focusedTab);
                             removePanes (tabState);
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

void ENDView::registerZoomActions()
{
    for (const auto& command : { Id::zoomIn, Id::zoomOut, Id::zoomReset })
    {
        actions.add (command,
                     [this, command]
                     {
                         const float step { config.getRowValue (Id::toType (Id::display), Id::zoomStep) };

                         if (auto* sessionView { getActiveSessionView() })
                         {
                             if (static_cast<bool> (config.getRowValue (Id::toType (Id::display), Id::exclusiveZoom)))
                             {
                                 if (auto* tabView { sessionView->getActiveTabView() })
                                     tabView->get (tabView->getFocusedChild()).setZoom (command, step);
                             }
                             else
                             {
                                 sessionView->setZoom (command, step);
                             }
                         }
                     });
    }
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
                                 const float step { config.getRowValue (Id::toType (Id::display), Id::paneStep) };
                                 (tabView->*resize) (tabView->getFocusedChild(), axis, step);
                             }
                     });
    }

    actions.add (Id::normal,
                 [this]
                 {
                     if (auto* sessionView { getActiveSessionView() })
                         if (auto* tabView { sessionView->getActiveTabView() })
                             tabView->get (tabView->getFocusedChild()).setMode (map::EditorMode::normal);
                 });
}

void ENDView::registerActions()
{
    registerSessionActions();
    registerTabActions();
    registerZoomActions();
    registerPaneActions();
}

void ENDView::registerGraphicsEvents()
{
    for (const auto& id : { Id::backgroundOpacity, Id::frameRate, Id::backgroundResolution })
        events.add<juce::ValueTree&> (id, [this] (juce::ValueTree&) { setBackgroundParams(); });
}

void ENDView::registerMouseEvents()
{
    for (const auto& id : { Id::enabled, Id::imouse, Id::orbit, Id::reset })
        events.add<juce::ValueTree&> (id, [this] (juce::ValueTree&) { setMouseConfig(); });
}

void ENDView::registerEvents()
{
    registerGraphicsEvents();
    registerMouseEvents();
}

void ENDView::setBackground (std::unique_ptr<jam::VulkanShader> shader)
{
    background.setShader (std::move (shader),
                          static_cast<float> (config.getRowValue (Id::toType (Id::graphics), Id::backgroundOpacity)),
                          static_cast<float> (config.getRowValue (Id::toType (Id::graphics), Id::backgroundResolution)),
                          static_cast<int> (config.getRowValue (Id::toType (Id::graphics), Id::frameRate)));
}

void ENDView::setBackgroundParams()
{
    background.setParams (static_cast<float> (config.getRowValue (Id::toType (Id::graphics), Id::backgroundOpacity)),
                          static_cast<float> (config.getRowValue (Id::toType (Id::graphics), Id::backgroundResolution)),
                          static_cast<int> (config.getRowValue (Id::toType (Id::graphics), Id::frameRate)));
}

void ENDView::setMouseConfig()
{
    const bool enabled { config.getRowValue (Id::toType (Id::mouse), Id::enabled) };
    const auto imouseButton { static_cast<map::MouseButton::value> (map::MouseButton::getInstance()->get (
        config.getRowValue (Id::toType (Id::mouse), Id::imouse).toString())) };
    const auto orbitButton { static_cast<map::MouseButton::value> (map::MouseButton::getInstance()->get (
        config.getRowValue (Id::toType (Id::mouse), Id::orbit).toString())) };
    const auto resetButton { static_cast<map::MouseButton::value> (map::MouseButton::getInstance()->get (
        config.getRowValue (Id::toType (Id::mouse), Id::reset).toString())) };

    background.setMouseConfig (enabled, imouseButton, orbitButton, resetButton);
}
