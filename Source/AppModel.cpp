/**
 * @file AppModel.cpp
 * @brief Implementation of the application-level ValueTree owner.
 *
 * @see AppModel.h
 */

#include "AppModel.h"

AppModel::AppModel()
    : jam::Model (app::id::END)
{
    build (juce::ValueTree::fromXml (*jam::XML::getFromBinary (app::id::appMetadata)));

#if JUCE_WINDOWS
    setValue<juce::String> (app::id::NEXUS_LUA, app::id::shellProgram, juce::String ("powershell.exe"));
#elif JUCE_LINUX
    setValue<juce::String> (app::id::NEXUS_LUA, app::id::shellProgram, juce::String ("bash"));
#endif

    engine.load (*this);

    setValue<int> (jam::ID::width, getValue<int> (app::id::DISPLAY_LUA, app::id::windowWidth));
    setValue<int> (jam::ID::height, getValue<int> (app::id::DISPLAY_LUA, app::id::windowHeight));
    setValue<float> (app::id::zoom, lua::Engine::zoomMin);

    const juce::File configDir { lua::Engine::getConfigPath() };
    watcher.addFolder (configDir);
    watcher.coalesceEvents (300);
    watcher.addListener (this);

    startTimerHz (60);
}

AppModel::~AppModel() { watcher.removeListener (this); }

//==============================================================================

juce::ValueTree AppModel::getWindow() noexcept { return state.getOrCreateChildWithName (app::id::WINDOW, nullptr); }

juce::ValueTree AppModel::getNexusNode() noexcept { return state.getOrCreateChildWithName (app::id::NEXUS, nullptr); }

juce::ValueTree AppModel::getSessionsNode() noexcept
{
    return getNexusNode().getOrCreateChildWithName (app::id::SESSIONS, nullptr);
}

juce::ValueTree AppModel::getTabs() noexcept { return state.getOrCreateChildWithName (app::id::TABS, nullptr); }

//==============================================================================

int AppModel::getWindowWidth() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (state, jam::ID::width).getValue());
}

int AppModel::getWindowHeight() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (state, jam::ID::height).getValue());
}

float AppModel::getWindowZoom() const noexcept
{
    return static_cast<float> (
        static_cast<double> (jam::ValueTree::getValueFromChildWithID (state, app::id::zoom).getValue()));
}

void AppModel::setWindowSize (int width, int height)
{
    setValue (jam::ID::width, width);
    setValue (jam::ID::height, height);
}

void AppModel::setWindowZoom (float zoom)
{
    const float clamped { juce::jlimit (lua::Engine::zoomMin, lua::Engine::zoomMax, zoom) };
    setValue (app::id::zoom, clamped);
}

void AppModel::markAtlasDirty() noexcept { setValue (app::id::atlasDirty, 1); }

app::RendererType AppModel::getRendererType() const noexcept
{
    const auto renderer { jam::ValueTree::getValueFromChildWithID (state, app::id::renderer).getValue().toString() };

    if (Map::Renderer::getContext()->get (renderer) == Map::Renderer::cpu)
        return app::RendererType::cpu;

    return app::RendererType::gpu;
}

void AppModel::setRendererType (const juce::String& setting)
{
    const bool gpuAvailable {
        static_cast<int> (jam::ValueTree::getValueFromChildWithID (state, app::id::gpuAvailable).getValue()) != 0
    };
    const bool wantsGpu { Map::Gpu::getContext()->get (setting) != Map::Gpu::off };
    const juce::String resolved { wantsGpu and gpuAvailable ? Map::Renderer::getContext()->get (Map::Renderer::gpu)
                                                            : Map::Renderer::getContext()->get (Map::Renderer::cpu) };
    setValue (app::id::renderer, resolved);
    jam::BackgroundBlur::setEnabled (getRendererType() == app::RendererType::gpu);
}

void AppModel::setGpuAvailable (bool available) { setValue (app::id::gpuAvailable, available ? 1 : 0); }

void AppModel::setInstanceID (const juce::String& uuid) { state.setProperty (jam::ID::id, uuid, nullptr); }

juce::String AppModel::getInstanceID() const noexcept { return state.getProperty (jam::ID::id).toString(); }

void AppModel::setDaemonMode (bool isDaemon) { setValue (app::id::daemonMode, isDaemon ? 1 : 0); }

bool AppModel::isDaemonMode() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (state, app::id::daemonMode).getValue()) != 0;
}

void AppModel::setPort (int activePort)
{
    setValue (app::id::port, activePort);

    const juce::File nexusFile { getNexusFile() };
    nexusFile.getParentDirectory().createDirectory();
    nexusFile.replaceWithText (juce::String (activePort));
}

int AppModel::getPort() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (state, app::id::port).getValue());
}

void AppModel::setActiveTabIndex (int index) { setValue (app::id::active, index); }

//==============================================================================

juce::ValueTree AppModel::addTab()
{
    auto tabs { getTabs() };
    juce::ValueTree tab (app::id::TAB);

    juce::ValueTree panes (app::id::PANES);
    tab.appendChild (panes, nullptr);

    tabs.appendChild (tab, nullptr);
    return tab;
}

void AppModel::removeTab (int index)
{
    auto tabs { getTabs() };
    int tabIndex { 0 };
    bool found { false };

    for (int i { 0 }; not found and i < tabs.getNumChildren(); ++i)
    {
        if (tabs.getChild (i).getType() == app::id::TAB)
        {
            if (tabIndex == index)
            {
                tabs.removeChild (i, nullptr);
                found = true;
            }
            else
            {
                ++tabIndex;
            }
        }
    }
}

juce::ValueTree AppModel::getTab (int index) noexcept
{
    auto tabs { getTabs() };
    juce::ValueTree result {};
    int tabIndex { 0 };

    for (int i { 0 }; not result.isValid() and i < tabs.getNumChildren(); ++i)
    {
        if (tabs.getChild (i).getType() == app::id::TAB)
        {
            if (tabIndex == index)
                result = tabs.getChild (i);
            else
                ++tabIndex;
        }
    }

    return result;
}

juce::String AppModel::getActivePaneID() const noexcept
{
    return jam::ValueTree::getValueFromChildWithID (state, app::id::activePaneID).getValue().toString();
}

void AppModel::setActivePaneID (const juce::String& uuid) { setValue (app::id::activePaneID, uuid); }

juce::String AppModel::getActivePaneType() const noexcept
{
    return jam::ValueTree::getValueFromChildWithID (state, app::id::activePaneType).getValue().toString();
}

void AppModel::setActivePaneType (const juce::String& type) { setValue (app::id::activePaneType, type); }

void AppModel::setModalType (int type) { setValue (app::id::modalType, type); }

int AppModel::getModalType() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (state, app::id::modalType).getValue());
}

void AppModel::setSelectionType (int type) { setValue (app::id::selectionType, type); }

int AppModel::getSelectionType() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (state, app::id::selectionType).getValue());
}

juce::String AppModel::getPwd() const noexcept
{
    juce::String result { juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName() };
    const auto cwd { activeSession.getProperty (terminal::id::cwd).toString() };

    if (cwd.isNotEmpty())
        result = cwd;

    return result;
}

void AppModel::setPwd (juce::ValueTree sessionTree) { activeSession = sessionTree; }

//==============================================================================

void AppModel::save()
{
    flush();

    const juce::File file { getStateFile() };
    file.getParentDirectory().createDirectory();

    juce::ValueTree persist { state.createCopy() };
    auto nexusNode { persist.getChildWithName (app::id::NEXUS) };

    if (nexusNode.isValid())
        persist.removeChild (nexusNode, nullptr);

    auto configChild { persist.getChildWithName (app::id::CONFIG) };

    if (configChild.isValid())
        persist.removeChild (configChild, nullptr);

    if (auto xml { persist.createXml() })
        xml->writeTo (file);
}

void AppModel::load()
{
    const juce::File file { getStateFile() };

    if (file.existsAsFile())
    {
        if (auto xml { juce::parseXML (file) })
        {
            auto parsed { juce::ValueTree::fromXml (*xml) };

            if (parsed.isValid() and parsed.getType() == app::id::END)
            {
                // Restore WINDOW and TABS from the saved state, preserving CONFIG
                // (which was excluded from save and is not in the parsed file).
                // replaceState would destroy CONFIG and orphan its Parameters.
                auto savedWindow { parsed.getChildWithName (app::id::WINDOW) };

                if (savedWindow.isValid())
                {
                    auto windowNode { state.getOrCreateChildWithName (app::id::WINDOW, nullptr) };
                    jam::ValueTree::loadState (windowNode, savedWindow);
                }

                auto savedTabs { parsed.getChildWithName (app::id::TABS) };

                if (savedTabs.isValid())
                {
                    auto tabsNode { state.getOrCreateChildWithName (app::id::TABS, nullptr) };
                    jam::ValueTree::loadState (tabsNode, savedTabs);
                }

                restoreValues();
            }
        }
    }
}

void AppModel::deleteNexusFile() { getNexusFile().deleteFile(); }

juce::File AppModel::getStateFile() const
{
    return lua::Engine::getConfigPath().getChildFile ("nexus/" + getInstanceID() + ".display");
}

juce::File AppModel::getNexusFile() const
{
    return lua::Engine::getConfigPath().getChildFile ("nexus/" + getInstanceID() + ".nexus");
}

juce::File AppModel::getWindowState() const { return lua::Engine::getConfigPath().getChildFile ("window.state"); }

void AppModel::saveWindowState()
{
    flush();

    const juce::File file { getWindowState() };
    file.getParentDirectory().createDirectory();

    auto window { state.getChildWithName (app::id::WINDOW) };

    if (window.isValid())
    {
        if (auto xml { window.createXml() })
            xml->writeTo (file);
    }
}

void AppModel::loadWindowState()
{
    const juce::File file { getWindowState() };

    if (file.existsAsFile())
    {
        if (auto xml { juce::parseXML (file) })
        {
            auto parsed { juce::ValueTree::fromXml (*xml) };

            if (parsed.isValid() and parsed.getType() == app::id::WINDOW)
            {
                for (int i { 0 }; i < parsed.getNumChildren(); ++i)
                {
                    auto child { parsed.getChild (i) };

                    if (child.getType() == jam::Model::PARAM)
                    {
                        const juce::Identifier paramId { child.getProperty (jam::ID::id).toString() };
                        const juce::var value { child.getProperty (jam::ID::value) };
                        auto paramNode { jam::ValueTree::getChildWithID (state, paramId.toString()) };

                        if (paramNode.isValid())
                            paramNode.setProperty (jam::ID::value, value, nullptr);
                    }
                }

                restoreValues();
            }
        }
    }
}

//==============================================================================
// Config authority (D8.1)
//==============================================================================

void AppModel::fileChanged (const juce::File& file, jam::File::Watcher::Event event)
{
    if (event == jam::File::Watcher::Event::fileUpdated and file.hasFileExtension ("lua")
        and getValue<int> (app::id::NEXUS_LUA, app::id::autoReload) != 0)
    {
        reload();
    }
}

juce::ValueTree AppModel::getConfig() const noexcept { return state.getChildWithName (app::id::CONFIG); }

lua::Engine::Theme AppModel::buildTheme() const { return engine.buildTheme(); }
const lua::Engine::SelectionKeys& AppModel::getSelectionKeys() const { return engine.getSelectionKeys(); }
bool AppModel::isClickableExtension (const juce::String& ext) const noexcept
{
    return engine.isClickableExtension (ext);
}
juce::String AppModel::getHandler (const juce::String& ext) const noexcept { return engine.getHandler (ext); }
float AppModel::dpiCorrectedFontSize() const noexcept { return engine.dpiCorrectedFontSize(); }
void AppModel::registerActions (action::Registry& r) { engine.registerActions (r); }
void AppModel::buildKeyMap (action::Registry& r) { engine.buildKeyMap (r); }
void AppModel::registerApiTable() { engine.registerApiTable(); }
void AppModel::setDisplayCallbacks (lua::Engine::DisplayCallbacks c) { engine.setDisplayCallbacks (std::move (c)); }
void AppModel::setPopupCallbacks (lua::Engine::PopupCallbacks c) { engine.setPopupCallbacks (std::move (c)); }
const juce::String& AppModel::getLoadError() const { return engine.getLoadError(); }
juce::String AppModel::getShortcutString (const juce::String& k) const { return engine.getShortcutString (k); }
juce::String AppModel::getActionLuaKey (const juce::String& a) const { return engine.getActionLuaKey (a); }
juce::String AppModel::getPrefixString() const noexcept { return engine.getPrefixString(); }
bool AppModel::isKeyFileRemappable() const noexcept { return engine.isKeyFileRemappable(); }
void AppModel::overrideShortcut (const juce::String& key, const juce::String& value) { engine.patchKey (key, value); }
void AppModel::reload()
{
    engine.load (*this);

    // Font mutation requires GL context detached — same pattern as setRenderer().
    // AppModel does not own the GL context; font mutation is deferred to the
    // configGeneration listener in MainComponent which detaches before mutating.
    markAtlasDirty();

    auto configNode { getConfig() };
    const int current { static_cast<int> (configNode.getProperty (app::id::configGeneration, 0)) };
    configNode.setProperty (app::id::configGeneration, current + 1, nullptr);
}

//==============================================================================

void AppModel::build (const juce::ValueTree& source)
{
    // Pass 1: structural copy — every child of source becomes a child of state.
    for (int i { 0 }; i < source.getNumChildren(); ++i)
        state.appendChild (source.getChild (i).createCopy(), nullptr);

    // Pass 2: walk the tree, create and add each parameter.
    jam::ValueTree::applyFunctionRecursively (state,
                                              [this] (const juce::ValueTree& node)
                                              {
                                                  return createAndAddParameter (node);
                                              });
}

bool AppModel::createAndAddParameter (const juce::ValueTree& node)
{
    if (node.getType() == jam::Model::PARAM)
    {
        const juce::Identifier id { node.getProperty (jam::ID::id).toString() };
        const auto typeStr { node.getProperty (app::id::type).toString() };
        const auto defaultStr { node.getProperty (app::id::defaultValue).toString() };
        const auto maxlen { static_cast<int> (node.getProperty ("maxlen", 256)) };
        auto parentNode { node.getParent() };

        // Default to root params. Only use a group AnyMap if one is registered
        // for the parent's type (CONFIG subtree: NEXUS_LUA, DISPLAY_LUA, etc.).
        // WINDOW and TABS params have non-root parents but no registered group.
        jam::AnyMap* group { &params };

        if (parentNode != state and params.contains (parentNode.getType()))
            group = params.get<jam::AnyMap> (parentNode.getType());

        jassert (group != nullptr);

        if (typeStr == app::id::stringType.toString())
        {
            addTextParameter (id, defaultStr, maxlen, *group, parentNode);
        }
        else if (typeStr == app::id::colourType.toString())
        {
            const auto hexStr { defaultStr.startsWith ("0x") ? defaultStr.substring (2) : defaultStr };
            const auto argb { static_cast<juce::uint32> (std::stoul (hexStr.toStdString(), nullptr, 16)) };
            addParameter<int> (id, static_cast<int> (argb), *group, parentNode);
        }
        else if (typeStr == app::id::boolType.toString())
        {
            addParameter<int> (id, Map::Bool::getContext()->get (defaultStr) ? 1 : 0, *group, parentNode);
        }
        else if (typeStr == app::id::floatType.toString())
        {
            addParameter<float> (id, static_cast<float> (defaultStr.getDoubleValue()), *group, parentNode);
        }
        else
        {
            addParameter<int> (id, defaultStr.getIntValue(), *group, parentNode);
        }

        return false;// leaf — no children, continue walk to subsequent nodes
    }

    // Group node — pre-register AnyMap for CONFIG subtree type nodes.
    if (node.getParent().isValid())
    {
        const auto parentType { node.getParent().getType() };

        if (parentType == app::id::CONFIG and not params.contains (node.getType()))
            params.add<jam::AnyMap> (node.getType());
    }

    return false;// continue recursion into children
}
