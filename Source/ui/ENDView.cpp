#include "ui/ENDView.h"

ENDView::ENDView()
    : jam::Model::Component<ENDView> (ENDModel::getInstance()->getChildWithName (Id::toType (Id::window)))
    , messageOverlay (model.getChildWithName (Id::toType (Id::overlay)), [] {
        return juce::FontOptions()
                   .withName (ConfigModel::getInstance()->getRowValue (Id::toType (Id::overlay), Id::fontFamily).toString())
                   .withPointHeight (static_cast<float> (ConfigModel::getInstance()->getRowValue (Id::toType (Id::overlay), Id::textFontSize)));
    })
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

    focusedPane.addListener (this);
    config.addListener (this);
    model.addListener (this);

    juce::MessageManager::callAsync (
        [this]
        {
            auto display { config.getChildWithName (Id::toType (Id::display)) };
            auto alwaysOnTopRow { display.getChildWithName (Id::alwaysOnTop) };
            auto titleBarButtonsRow { display.getChildWithName (Id::titleBarButtons) };

            events.get (Id::alwaysOnTop, alwaysOnTopRow);
            events.get (Id::titleBarButtons, titleBarButtonsRow);
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
    focusedPane.removeListener (this);
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
    return commandManager->getKeyMappings()->keyPressed (key, this) or commandManager->invokeKey (key, state);
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

void ENDView::valueChanged (juce::Value&)
{
    // focusedPane currently refers to whichever TAB row's focusedPane the
    // Id::focusedPane event last pointed it at — mirrors that value onto
    // the SESSIONS-level focusedPane parameter, a valid registered parameter
    // throughout.
    model.setValue (Id::toType (Id::sessions), Id::focusedPane, focusedPane.getValue());
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
                             std::function<void (const juce::ValueTree&)> removePanes;
                             removePanes = [this, &removePanes] (const juce::ValueTree& row)
                             {
                                 if (row.hasType (Id::toType (Id::pane)))
                                     nexus.getActiveSession().removePlugin (
                                         jam::UUID { static_cast<int64_t> (row.getProperty (Id::id)) });

                                 for (const auto& child : row)
                                     removePanes (child);
                             };

                             removePanes (sessionView->get (sessionView->getFocusedChild()).state);
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
                                ConfigModel& config,
                                Nexus& nexus,
                                const juce::Identifier& actionId,
                                const std::function<float (const juce::AudioProcessorParameter&, float)>& computeZoom)
{
    actionRegistry.add (actionId,
                        [&model, &config, &nexus, computeZoom]
                        {
                            const float step { config.getRowValue (Id::toType (Id::display), Id::zoomStep) };
                            static const juce::String zoomName { jam::Format::toParameterName (Id::master.toString(), Id::zoom.toString()) };
                            const auto applyZoom { [&computeZoom, step] (juce::AudioProcessor& plugin)
                                                   {
                                                       if (auto* parameter { EditorView::getParameter (plugin, zoomName) })
                                                       {
                                                           parameter->beginChangeGesture();
                                                           parameter->setValueNotifyingHost (computeZoom (*parameter, step));
                                                           parameter->endChangeGesture();
                                                       }
                                                   } };
                            auto& session { nexus.getActiveSession() };

                            if (static_cast<bool> (config.getRowValue (Id::toType (Id::display), Id::exclusiveZoom)))
                            {
                                const jam::UUID id { static_cast<int64_t> (
                                    model.getValue (Id::toType (Id::sessions), Id::focusedPane)) };

                                if (session.contains (id))
                                    applyZoom (session.getPlugin (id));
                            }
                            else
                            {
                                session.applyToPlugins (applyZoom);
                            }
                        });
}

void ENDView::registerZoomActions()
{
    registerZoomAction (actions, model, config, nexus, Id::zoomIn,
        [] (const juce::AudioProcessorParameter& parameter, float step)
        {
            return juce::jlimit (0.0f, 1.0f, parameter.getValue() + step);
        });

    registerZoomAction (actions, model, config, nexus, Id::zoomOut,
        [] (const juce::AudioProcessorParameter& parameter, float step)
        {
            return juce::jlimit (0.0f, 1.0f, parameter.getValue() - step);
        });

    registerZoomAction (actions, model, config, nexus, Id::zoomReset,
        [] (const juce::AudioProcessorParameter& parameter, float)
        {
            return parameter.getDefaultValue();
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
                     static const juce::String modeName { jam::Format::toParameterName (Id::master.toString(), Id::mode.toString()) };
                     const jam::UUID id { static_cast<int64_t> (
                         model.getValue (Id::toType (Id::sessions), Id::focusedPane)) };
                     auto& session { nexus.getActiveSession() };

                     if (session.contains (id))
                     {
                         if (auto* parameter { EditorView::getParameter (session.getPlugin (id), modeName) })
                         {
                             parameter->beginChangeGesture();
                             parameter->setValueNotifyingHost (juce::NormalisableRange<float> { static_cast<float> (map::EditorMode::normal), static_cast<float> (map::EditorMode::terminal) }.convertTo0to1 (static_cast<float> (map::EditorMode::normal)));
                             parameter->endChangeGesture();
                         }
                     }
                 });
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

void ENDView::registerGraphicsEvents()
{
    events.add<juce::ValueTree&> (Id::backgroundOpacity,
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackgroundParams();
                                  });

    events.add<juce::ValueTree&> (Id::frameRate,
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackgroundParams();
                                  });

    events.add<juce::ValueTree&> (Id::backgroundResolution,
                                  [this] (juce::ValueTree&)
                                  {
                                      setBackgroundParams();
                                  });
}

void ENDView::registerWindowEvents()
{
    events.add<juce::ValueTree&> (
        Id::alwaysOnTop,
        [this] (juce::ValueTree& row)
        {
            if (auto* window { dynamic_cast<jam::Window*> (getTopLevelComponent()) })
                window->setAlwaysOnTop (row.getProperty (Id::value));
        });

    events.add<juce::ValueTree&> (
        Id::titleBarButtons,
        [this] (juce::ValueTree& row)
        {
            if (auto* window { dynamic_cast<jam::Window*> (getTopLevelComponent()) })
                window->setWindowButtons (row.getProperty (Id::value));
        });
}

void ENDView::registerMouseEvents()
{
    events.add<juce::ValueTree&> (Id::enabled,
                                  [this] (juce::ValueTree&)
                                  {
                                      setMouseConfig();
                                  });

    events.add<juce::ValueTree&> (Id::imouse,
                                  [this] (juce::ValueTree&)
                                  {
                                      setMouseConfig();
                                  });

    events.add<juce::ValueTree&> (Id::orbit,
                                  [this] (juce::ValueTree&)
                                  {
                                      setMouseConfig();
                                  });

    events.add<juce::ValueTree&> (Id::reset,
                                  [this] (juce::ValueTree&)
                                  {
                                      setMouseConfig();
                                  });
}

void ENDView::registerEvents()
{
    events.add<juce::ValueTree&> (
        Id::focusedPane,
        [this] (juce::ValueTree& tree)
        {
            // Re-points the conduit to whichever TAB row last wrote its own
            // focusedPane — valueChanged() below then mirrors that value
            // onto the SESSIONS-level parameter, last change wins.
            if (tree.getType() == Id::toType (Id::tab))
                focusedPane.referTo (tree.getPropertyAsValue (Id::focusedPane, nullptr));
        });

    events.add<juce::ValueTree&> (Id::theme,
                                  [this] (juce::ValueTree&)
                                  {
                                      getTopLevelComponent()->sendLookAndFeelChange();
                                  });

    registerGraphicsEvents();
    registerWindowEvents();
    registerMouseEvents();

    // WINDOW leaf visibility toggles (ENDActions.cpp's own
    // Position-bimap loop) relayout the dormant dock-pane machinery — one
    // registration on the shared Id::visible property key serves all
    // five leaves, no per-leaf type fallback needed (every WINDOW leaf now
    // shares Id::toType (Id::pane), so a type-keyed fallback could no longer
    // distinguish them the way the four old Position-keyed ValueTree TYPES did).
    events.add<juce::ValueTree&> (Id::visible,
                                  [this] (juce::ValueTree&)
                                  {
                                      resized();
                                  });
}

static std::tuple<float, float, int> getBackgroundRenderParams (ConfigModel& config)
{
    const float opacity { config.getRowValue (Id::toType (Id::graphics), Id::backgroundOpacity) };
    const float resolutionScale { config.getRowValue (Id::toType (Id::graphics), Id::backgroundResolution) };
    const int frameRate { config.getRowValue (Id::toType (Id::graphics), Id::frameRate) };

    return { opacity, resolutionScale, frameRate };
}

void ENDView::setBackground (std::unique_ptr<jam::VulkanShader> shader)
{
    const auto [opacity, resolutionScale, frameRate] { getBackgroundRenderParams (config) };

    background.setShader (std::move (shader), opacity, resolutionScale, frameRate);
}

void ENDView::setBackgroundParams()
{
    const auto [opacity, resolutionScale, frameRate] { getBackgroundRenderParams (config) };

    background.setParams (opacity, resolutionScale, frameRate);
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
