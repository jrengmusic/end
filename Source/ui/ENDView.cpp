#include "ui/ENDView.h"

ENDView::ENDView()
    : jam::Model::Component<ENDView> (ENDModel::getInstance()->getChildWithName (jam::Format::toType (Id::window)))
    , messageOverlay (model.getChildWithName (jam::Format::toType (Id::overlay)), [] { return ENDLookAndFeel::getInstance()->getOverlayFont(); })
{
    setOpaque (false);
    toFront (true);

    registerActions();
    registerEvents();

    commandManager->registerAllCommandsForTarget (this);
    commandManager->setFirstCommandTarget (this);

    addAndMakeVisible (background);
    addMouseListener (&background, true);

    addChildComponent (messageOverlay);

    createAndAttachParameters();

    config.addListener (this);
    model.addListener (this);

    juce::MessageManager::callAsync (
        [this]
        {
            setMouseConfig();

            addSession();
            commandManager->setKeyPresses (config.getChildWithName (jam::Format::toType (Id::keys)));
            grabKeyboardFocus();
        });
}

ENDView::~ENDView()
{
    model.removeListener (this);
    config.removeListener (this);
    commandManager->setFirstCommandTarget (nullptr);
}

void ENDView::resized()
{
    setViewState (jam::Size<int16_t> (getWidth(), getHeight()));

    background.setBounds (getLocalBounds());
    messageOverlay.setBounds (getLocalBounds());

    if (auto* sessionView { getActiveSessionView() })
        sessionView->setBounds (getLocalBounds());
}

bool ENDView::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return commandManager->invokeKey (key, state, false);
}

void ENDView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    const auto key { ConfigModel::getEventKey (events, tree, property) };

    if (events.contains (key))
        events.get (key, tree);
}

juce::ApplicationCommandTarget* ENDView::getNextCommandTarget()
{
    return getActiveTabView();
}

void ENDView::getAllCommands (juce::Array<juce::CommandID>& commands)
{
    jam::CommandManager::addCommands (commands, *commandId);
}

void ENDView::getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result)
{
    jam::CommandManager::setInfo (result, commandId->get (commandID).toString());
}

bool ENDView::perform (const InvocationInfo& info)
{
    const auto isPerformed { commands.contains (info.commandID) };

    if (isPerformed)
        commands.get (info.commandID);

    return isPerformed;
}

void ENDView::createAndAttachParameters()
{
    const auto windowSize { config.getRowValue (jam::Format::toType (Id::display), Id::size) };
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

SessionView* ENDView::getActiveSessionView() const noexcept
{
    auto* focusedSessionParameter { model.getParameter<jam::Parameter<int64_t>> (
        jam::Format::toType (Id::sessions), Id::focusedSession) };
    jassert (focusedSessionParameter != nullptr);

    const jam::UUID sessionUuid { focusedSessionParameter->getValue() };

    return sessions.contains (sessionUuid) ? sessions.at (sessionUuid).get() : nullptr;
}

TabView* ENDView::getActiveTabView() const noexcept
{
    if (auto* sessionView { getActiveSessionView() })
        return sessionView->getActiveTabView();

    return nullptr;
}

void ENDView::addSession()
{
    auto& session { nexus.createSession() };
    const jam::UUID sessionUuid { static_cast<int64_t> (session.state.getProperty (Id::id)) };

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

    commandManager->invokeDirectly (map::CommandId::newTab, false);
}

void ENDView::addPane (const juce::Identifier& edge, const juce::String& directory)
{
#if JAM_USING_PROFILER
    const jam::Profiler::Zone zone { Id::newPane };
    panePrompt.emplace (Id::panePrompt);
#endif
    if (auto* tabView { getActiveTabView() })
        tabView->addPane (edge, directory);
}

void ENDView::registerApplicationActions()
{
    commands.add (map::CommandId::quit,
                  [] (const juce::CommandID&)
                  {
                      juce::JUCEApplication::getInstance()->systemRequestedQuit();
                  });

    commands.add (map::CommandId::reload,
                  [this] (const juce::CommandID&)
                  {
                      config.loadFromPath();
                  });
}

void ENDView::registerTabActions()
{
    commands.add (map::CommandId::newTab,
                  [this] (const juce::CommandID&)
                  {
                      if (auto* sessionView { getActiveSessionView() })
                      {
                          const auto directory { getFocusedDirectory() };
                          jam::UUID uuid {};
                          sessionView->add (uuid);

                          const juce::Identifier edge {};
                          addPane (edge, directory);
                      }
                  });

    commands.add (map::CommandId::nextTab,
                  [this] (const juce::CommandID&)
                  {
                      if (auto* sessionView { getActiveSessionView() })
                          sessionView->nextTab();
                  });

    commands.add (map::CommandId::prevTab,
                  [this] (const juce::CommandID&)
                  {
                      if (auto* sessionView { getActiveSessionView() })
                          sessionView->prevTab();
                  });
}

#if JAM_USING_PROFILER
void ENDView::registerProfilerActions()
{
    const auto manifestFile { Nexus::getManifestFile() };
    const auto manifest { jam::ConfigDocument::parse (manifestFile.loadFileAsString(), manifestFile.getFullPathName()).getValueTree (Id::manifest) };
    const auto profilerTable { manifest.getChildWithName (jam::Format::toType (Id::profiler)) };
    const int panes { static_cast<int> (profilerTable[jam::Format::toType (Id::panes)]) };

    nexus.onPromptPainted = [this, panes]
    {
        panePrompt.reset();

        if (auto* tabView { getActiveTabView() })
        {
            if (tabView->getNumChildren() < panes)
                commandManager->invokeDirectly (map::CommandId::splitVertical, false);
            else
                commandManager->invokeDirectly (map::CommandId::quit, false);
        }
    };
}
#endif

void ENDView::registerZoomActions()
{
    for (const juce::CommandID command : { map::CommandId::zoomIn, map::CommandId::zoomOut, map::CommandId::zoomReset })
    {
        commands.add (command,
                      [this] (const juce::CommandID& command)
                      {
                          const float step { config.getRowValue (jam::Format::toType (Id::display), Id::zoomStep) };

                          if (auto* sessionView { getActiveSessionView() })
                          {
                              if (static_cast<bool> (config.getRowValue (jam::Format::toType (Id::display), Id::exclusiveZoom)))
                              {
                                  if (auto* tabView { sessionView->getActiveTabView() })
                                      tabView->getFocusedPane().setZoom (command, step);
                              }
                              else
                              {
                                  sessionView->setZoom (command, step);
                              }
                          }
                      });
    }
}

void ENDView::registerJoinActions()
{
    for (const auto& [direction, actionId] : TabView::getJoinCommands())
    {
        commands.add (actionId,
                      [this, direction = direction] (const juce::CommandID&)
                      {
                          if (auto* tabView { getActiveTabView() })
                              tabView->join (direction);
                      });
    }
}

void ENDView::registerSwapActions()
{
    static const std::array<std::pair<juce::CommandID, juce::Identifier>, 4> swapDirections {{
        { map::CommandId::swapLeft, Id::left },
        { map::CommandId::swapDown, Id::bottom },
        { map::CommandId::swapUp, Id::top },
        { map::CommandId::swapRight, Id::right },
    }};

    for (const auto& [actionId, direction] : swapDirections)
    {
        commands.add (actionId,
                      [this, direction = direction] (const juce::CommandID&)
                      {
                          if (auto* tabView { getActiveTabView() })
                              tabView->swap (direction);
                      });
    }
}

void ENDView::registerResizeActions()
{
    static const std::array<std::tuple<juce::CommandID, juce::Identifier, PaneResize>, 4> paneResizes {{
        { map::CommandId::reducePaneWidth, Id::width, &TabView::reducePane },
        { map::CommandId::reducePaneHeight, Id::height, &TabView::reducePane },
        { map::CommandId::expandPaneWidth, Id::width, &TabView::expandPane },
        { map::CommandId::expandPaneHeight, Id::height, &TabView::expandPane },
    }};

    for (const auto& [actionId, axis, resize] : paneResizes)
    {
        commands.add (actionId,
                      [this, axis = axis, resize = resize] (const juce::CommandID&)
                      {
                          if (auto* tabView { getActiveTabView() })
                          {
                              const float step { config.getRowValue (jam::Format::toType (Id::display), Id::paneStep) };
                              (tabView->*resize) (tabView->getFocusedChild(), axis, step);
                          }
                      });
    }
}

void ENDView::registerModeActions()
{
    static const std::array<std::pair<juce::CommandID, int>, 2> paneModes {{
        { map::CommandId::normal, map::EditorMode::normal },
        { map::CommandId::open, map::EditorMode::open },
    }};

    for (const auto& [actionId, mode] : paneModes)
    {
        commands.add (actionId,
                      [this, mode = mode] (const juce::CommandID&)
                      {
                          if (auto* tabView { getActiveTabView() })
                              tabView->getFocusedPane().setMode (mode);
                      });
    }
}

void ENDView::registerSplitActions()
{
    static const std::array<std::pair<juce::CommandID, juce::Identifier>, 2> splitEdges {{
        { map::CommandId::splitHorizontal, Id::bottom },
        { map::CommandId::splitVertical, Id::right },
    }};

    for (const auto& [actionId, edge] : splitEdges)
    {
        commands.add (actionId,
                      [this, edge = edge] (const juce::CommandID&)
                      {
                          addPane (edge, getFocusedDirectory());
                      });
    }
}

void ENDView::registerLoadPluginActions()
{
    commands.add (map::CommandId::loadPlugin,
                  [this] (const juce::CommandID&)
                  {
                      if (auto* tabView { getActiveTabView() })
                          tabView->loadPlugin();
                  });
}

void ENDView::registerCloseActions()
{
    for (const juce::CommandID command : { map::CommandId::closeTerminal, map::CommandId::closePane, map::CommandId::closeTerminalPane })
    {
        commands.add (command,
                      [this] (const juce::CommandID& command)
                      {
                          if (auto* tabView { getActiveTabView() })
                              tabView->getFocusedPane().close (command);
                      });
    }
}

juce::String ENDView::getFocusedDirectory() const
{
    if (auto* tabView { getActiveTabView() })
        return tabView->getFocusedPane().getValueTree().getChildWithName (jam::Format::toType (Id::pluginState)).getProperty (Id::cwd).toString();

    return juce::String {};
}

void ENDView::registerActions()
{
    registerApplicationActions();
    registerTabActions();
    registerZoomActions();
    registerModeActions();
    registerSplitActions();
    registerLoadPluginActions();
    registerCloseActions();
    registerJoinActions();
    registerSwapActions();
    registerResizeActions();
#if JAM_USING_PROFILER
    registerProfilerActions();
#endif
}

void ENDView::registerGraphicsEvents()
{
    for (const auto& id : { jam::Format::toType (Id::backgroundOpacity), jam::Format::toType (Id::frameRate), jam::Format::toType (Id::backgroundResolution) })
        events.add (id, [this] (const juce::Identifier&, juce::ValueTree&) { setBackgroundParams(); });
}

void ENDView::registerMouseEvents()
{
    for (const auto& id : { jam::Format::toType (Id::enabled), jam::Format::toType (Id::imouse), jam::Format::toType (Id::orbit), jam::Format::toType (Id::reset) })
        events.add (id, [this] (const juce::Identifier&, juce::ValueTree&) { setMouseConfig(); });
}

void ENDView::registerEvents()
{
    registerGraphicsEvents();
    registerMouseEvents();

    events.add (jam::Format::toType (Id::keys), [this] (const juce::Identifier&, juce::ValueTree& tree) { commandManager->setKeyPresses (tree.getParent()); });
}

void ENDView::setBackground (std::unique_ptr<jam::VulkanShader> shader)
{
    background.setShader (std::move (shader),
                          static_cast<float> (config.getRowValue (jam::Format::toType (Id::graphics), Id::backgroundOpacity)),
                          static_cast<float> (config.getRowValue (jam::Format::toType (Id::graphics), Id::backgroundResolution)),
                          static_cast<int> (config.getRowValue (jam::Format::toType (Id::graphics), Id::frameRate)));
}

void ENDView::setBackgroundParams()
{
    background.setParams (static_cast<float> (config.getRowValue (jam::Format::toType (Id::graphics), Id::backgroundOpacity)),
                          static_cast<float> (config.getRowValue (jam::Format::toType (Id::graphics), Id::backgroundResolution)),
                          static_cast<int> (config.getRowValue (jam::Format::toType (Id::graphics), Id::frameRate)));
}

void ENDView::setMouseConfig()
{
    const bool enabled { config.getRowValue (jam::Format::toType (Id::mouse), Id::enabled) };
    const auto imouseButton { static_cast<map::MouseButton::value> (map::MouseButton::getInstance()->get (
        config.getRowValue (jam::Format::toType (Id::mouse), Id::imouse).toString())) };
    const auto orbitButton { static_cast<map::MouseButton::value> (map::MouseButton::getInstance()->get (
        config.getRowValue (jam::Format::toType (Id::mouse), Id::orbit).toString())) };
    const auto resetButton { static_cast<map::MouseButton::value> (map::MouseButton::getInstance()->get (
        config.getRowValue (jam::Format::toType (Id::mouse), Id::reset).toString())) };

    background.setMouseConfig (enabled, imouseButton, orbitButton, resetButton);
}
