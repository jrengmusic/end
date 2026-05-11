/**
 * @file AppState.cpp
 * @brief Implementation of the application-level ValueTree owner.
 *
 * @see AppState.h
 */

#include "AppState.h"
#include "AppLayout.h"
#include "component/LookAndFeel.h"
#include "lua/Engine.h"
#include "terminal/data/Identifier.h"

AppState::AppState()
    : jam::ValueTree (App::ID::END)
{
    initDefaults();
}

AppState::~AppState() = default;

//==============================================================================

bool AppState::flush() noexcept
{
    if (needsFlushAtom->exchangeAcquire (0) != 0)
    {
        flushGroup (App::ID::WINDOW);
        flushGroup (App::ID::TABS);
        flushGroup (App::ID::END);
        return true;
    }

    return false;
}

void AppState::loadParamValue (const juce::ValueTree& parsedNode,
                                const juce::Identifier& groupId,
                                const juce::Identifier& paramId) noexcept
{
    auto param { jam::ValueTree::getChildWithID (parsedNode, paramId.toString()) };

    if (param.isValid())
    {
        storeValue (groupId, paramId,
                    static_cast<int> (param.getProperty (jam::ID::value)));
    }
}

juce::ValueTree AppState::getWindow() noexcept
{
    return get().getOrCreateChildWithName (App::ID::WINDOW, nullptr);
}

juce::ValueTree AppState::getNexusNode() noexcept
{
    return get().getOrCreateChildWithName (App::ID::NEXUS, nullptr);
}

juce::ValueTree AppState::getSessionsNode() noexcept
{
    return getNexusNode().getOrCreateChildWithName (App::ID::SESSIONS, nullptr);
}

juce::ValueTree AppState::getLoadingNode() noexcept
{
    return getNexusNode().getOrCreateChildWithName (App::ID::LOADING, nullptr);
}

juce::ValueTree AppState::getTabs() noexcept
{
    return get().getOrCreateChildWithName (App::ID::TABS, nullptr);
}

//==============================================================================

static int getParamInt (const juce::ValueTree& root,
                        const juce::Identifier& groupId,
                        const juce::Identifier& paramId,
                        int defaultValue = 0) noexcept
{
    auto groupNode { root.getChildWithName (groupId) };
    int result { defaultValue };

    if (groupNode.isValid())
    {
        auto param { jam::ValueTree::getChildWithID (groupNode, paramId.toString()) };

        if (param.isValid())
        {
            result = static_cast<int> (param.getProperty (jam::ID::value));
        }
    }

    return result;
}

int AppState::getWindowWidth() const noexcept
{
    return getParamInt (get(), App::ID::WINDOW, App::ID::width);
}

int AppState::getWindowHeight() const noexcept
{
    return getParamInt (get(), App::ID::WINDOW, App::ID::height);
}

float AppState::getWindowZoom() const noexcept
{
    auto window { get().getChildWithName (App::ID::WINDOW) };
    float result { lua::Engine::zoomMin };

    if (window.isValid() and window.hasProperty (App::ID::zoom))
        result = static_cast<float> (window.getProperty (App::ID::zoom));

    return result;
}

void AppState::setWindowSize (int width, int height)
{
    storeValue (App::ID::WINDOW, App::ID::width, width);
    storeValue (App::ID::WINDOW, App::ID::height, height);
}

void AppState::setWindowZoom (float zoom)
{
    const float clamped { juce::jlimit (lua::Engine::zoomMin, lua::Engine::zoomMax, zoom) };
    auto window { getWindow() };
    window.setProperty (App::ID::zoom, clamped, nullptr);
}

juce::String AppState::getFontFamily() const noexcept
{
    auto window { get().getChildWithName (App::ID::WINDOW) };
    juce::String result { lua::Engine::getContext()->display.font.family };

    if (window.isValid() and window.hasProperty (App::ID::fontFamily))
        result = window.getProperty (App::ID::fontFamily).toString();

    return result;
}

void AppState::setFontFamily (const juce::String& family)
{
    auto window { getWindow() };
    window.setProperty (App::ID::fontFamily, family, nullptr);
}

float AppState::getFontSize() const noexcept
{
    auto window { get().getChildWithName (App::ID::WINDOW) };
    float result { static_cast<float> (lua::Engine::getContext()->dpiCorrectedFontSize()) };

    if (window.isValid() and window.hasProperty (App::ID::fontSize))
        result = static_cast<float> (window.getProperty (App::ID::fontSize));

    return result;
}

void AppState::setFontSize (float size)
{
    auto window { getWindow() };
    window.setProperty (App::ID::fontSize, size, nullptr);
}

void AppState::markAtlasDirty() noexcept
{
    params.get<jam::AnyMap> (App::ID::WINDOW)->get<jam::Atom<int>> (App::ID::atlasDirty)->storeRelease (1);
}

bool AppState::consumeAtlasDirty() noexcept
{
    return params.get<jam::AnyMap> (App::ID::WINDOW)->get<jam::Atom<int>> (App::ID::atlasDirty)->exchangeAcquire (0) != 0;
}

App::RendererType AppState::getRendererType() const noexcept
{
    auto window { get().getChildWithName (App::ID::WINDOW) };

    if (window.isValid() and window.hasProperty (App::ID::renderer)
        and window.getProperty (App::ID::renderer).toString() == App::ID::rendererCpu)
    {
        return App::RendererType::cpu;
    }

    return App::RendererType::gpu;
}

void AppState::setRendererType (const juce::String& setting)
{
    auto window { getWindow() };
    const bool gpuAvailable { getParamInt (get(), App::ID::WINDOW, App::ID::gpuAvailable) != 0 };
    const bool wantsGpu { setting != "false" };
    const juce::String resolved { wantsGpu and gpuAvailable ? App::ID::rendererGpu : App::ID::rendererCpu };
    window.setProperty (App::ID::renderer, resolved, nullptr);
    jam::BackgroundBlur::setEnabled (getRendererType() == App::RendererType::gpu);
}

void AppState::setGpuAvailable (bool available)
{
    storeValue (App::ID::WINDOW, App::ID::gpuAvailable, available ? 1 : 0);
}

void AppState::setInstanceUuid (const juce::String& uuid)
{
    get().setProperty (jam::ID::id, uuid, nullptr);
}

juce::String AppState::getInstanceUuid() const noexcept
{
    return get().getProperty (jam::ID::id).toString();
}

void AppState::setDaemonMode (bool isDaemon)
{
    storeValue (App::ID::WINDOW, App::ID::daemonMode, isDaemon ? 1 : 0);
}

bool AppState::isDaemonMode() const noexcept
{
    return getParamInt (get(), App::ID::WINDOW, App::ID::daemonMode) != 0;
}

void AppState::setPort (int activePort)
{
    storeValue (App::ID::END, App::ID::port, activePort);

    const juce::File nexusFile { getNexusFile() };
    nexusFile.getParentDirectory().createDirectory();
    nexusFile.replaceWithText (juce::String (activePort));
}

int AppState::getPort() const noexcept
{
    auto param { jam::ValueTree::getChildWithID (get(), App::ID::port.toString()) };
    int result { 0 };

    if (param.isValid())
        result = static_cast<int> (param.getProperty (jam::ID::value));

    return result;
}

int AppState::getActiveTabIndex() const noexcept
{
    return getParamInt (get(), App::ID::TABS, App::ID::active);
}

void AppState::setActiveTabIndex (int index)
{
    storeValue (App::ID::TABS, App::ID::active, index);
}

juce::String AppState::getTabPosition() const noexcept
{
    auto tabs { get().getChildWithName (App::ID::TABS) };
    juce::String result { lua::Engine::getContext()->display.tab.position };

    if (tabs.isValid() and tabs.hasProperty (App::ID::position))
        result = tabs.getProperty (App::ID::position).toString();

    return result;
}

void AppState::setTabPosition (const juce::String& position)
{
    auto tabs { getTabs() };
    tabs.setProperty (App::ID::position, position, nullptr);
}

//==============================================================================

juce::ValueTree AppState::addTab()
{
    auto tabs { getTabs() };
    juce::ValueTree tab (App::ID::TAB);

    juce::ValueTree panes (App::ID::PANES);
    tab.appendChild (panes, nullptr);

    tabs.appendChild (tab, nullptr);
    return tab;
}

void AppState::removeTab (int index)
{
    auto tabs { getTabs() };
    int tabIndex { 0 };
    bool found { false };

    for (int i { 0 }; not found and i < tabs.getNumChildren(); ++i)
    {
        if (tabs.getChild (i).getType() == App::ID::TAB)
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

juce::ValueTree AppState::getTab (int index) noexcept
{
    auto tabs { getTabs() };
    juce::ValueTree result {};
    int tabIndex { 0 };

    for (int i { 0 }; not result.isValid() and i < tabs.getNumChildren(); ++i)
    {
        if (tabs.getChild (i).getType() == App::ID::TAB)
        {
            if (tabIndex == index)
                result = tabs.getChild (i);
            else
                ++tabIndex;
        }
    }

    return result;
}

juce::String AppState::getActivePaneID() const noexcept
{
    auto tabs { get().getChildWithName (App::ID::TABS) };
    juce::String result {};

    if (tabs.isValid() and tabs.hasProperty (App::ID::activePaneID))
        result = tabs.getProperty (App::ID::activePaneID).toString();

    return result;
}

void AppState::setActivePaneID (const juce::String& uuid)
{
    auto tabs { getTabs() };

    if (tabs.isValid())
    {
        tabs.setProperty (App::ID::activePaneID, uuid, nullptr);
    }
}

juce::String AppState::getActivePaneType() const noexcept
{
    auto tabs { get().getChildWithName (App::ID::TABS) };
    juce::String result { App::ID::paneTypeTerminal };

    if (tabs.isValid() and tabs.hasProperty (App::ID::activePaneType))
        result = tabs.getProperty (App::ID::activePaneType).toString();

    return result;
}

void AppState::setActivePaneType (const juce::String& type)
{
    auto tabs { getTabs() };

    if (tabs.isValid())
    {
        tabs.setProperty (App::ID::activePaneType, type, nullptr);
    }
}

void AppState::setModalType (int type)
{
    storeValue (App::ID::TABS, App::ID::modalType, type);
}

int AppState::getModalType() const noexcept
{
    return getParamInt (get(), App::ID::TABS, App::ID::modalType);
}

void AppState::setSelectionType (int type)
{
    storeValue (App::ID::TABS, App::ID::selectionType, type);
}

int AppState::getSelectionType() const noexcept
{
    return getParamInt (get(), App::ID::TABS, App::ID::selectionType);
}

juce::String AppState::getPwd() const noexcept
{
    const auto cwd { pwdValue.toString() };
    juce::String result { juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName() };

    if (cwd.isNotEmpty())
        result = cwd;

    return result;
}

void AppState::setPwd (juce::ValueTree sessionTree)
{
    pwdValue.referTo (sessionTree.getPropertyAsValue (Terminal::ID::cwd, nullptr));
}

//==============================================================================

/**
 * @brief Writes the full state (WINDOW + TABS) to `nexus/<uuid>.display`.
 *
 * Daemon client mode only.  The NEXUS subtree is excluded — rebuilt live on
 * reconnect.  Port is NOT written here — port lives in `<uuid>.nexus`.
 * Daemon never calls this — daemon only writes its port via setPort().
 *
 * @note MESSAGE THREAD.
 */
void AppState::save()
{
    const juce::File file { getStateFile() };
    file.getParentDirectory().createDirectory();

    juce::ValueTree persist { get().createCopy() };
    auto nexusNode { persist.getChildWithName (App::ID::NEXUS) };

    if (nexusNode.isValid())
        persist.removeChild (nexusNode, nullptr);

    if (auto xml { persist.createXml() })
        xml->writeTo (file);
}

/**
 * @brief Reads the full state from `nexus/<uuid>.display` into the in-memory tree.
 *
 * Daemon client mode only.  Merges WINDOW and TABS subtrees.  Falls back
 * silently to initDefaults() values on parse failure or missing file.
 * Port is NOT read here — read as plain text from `<uuid>.nexus` during startup.
 * The NEXUS subtree from the file is discarded — rebuilt live on reconnect.
 *
 * @note MESSAGE THREAD.
 */
void AppState::load()
{
    const juce::File file { getStateFile() };

    if (file.existsAsFile())
    {
        if (auto xml { juce::parseXML (file) })
        {
            auto parsed { juce::ValueTree::fromXml (*xml) };

            if (parsed.isValid() and parsed.getType() == App::ID::END)
            {
                auto parsedWindow { parsed.getChildWithName (App::ID::WINDOW) };

                if (parsedWindow.isValid())
                {
                    auto currentWindow { getWindow() };

                    // Direct properties (float/string) — copy from parsed
                    if (parsedWindow.hasProperty (App::ID::zoom))
                        currentWindow.setProperty (App::ID::zoom, parsedWindow.getProperty (App::ID::zoom), nullptr);

                    if (parsedWindow.hasProperty (App::ID::renderer))
                        currentWindow.setProperty (App::ID::renderer, parsedWindow.getProperty (App::ID::renderer), nullptr);

                    // PARAM children (int/bool) — find by id, update Atom
                    loadParamValue (parsedWindow, App::ID::WINDOW, App::ID::width);
                    loadParamValue (parsedWindow, App::ID::WINDOW, App::ID::height);
                }

                auto parsedTabs { parsed.getChildWithName (App::ID::TABS) };

                if (parsedTabs.isValid())
                {
                    auto currentTabs { getTabs() };

                    // Direct properties (string) — copy from parsed
                    if (parsedTabs.hasProperty (App::ID::position))
                        currentTabs.setProperty (App::ID::position, parsedTabs.getProperty (App::ID::position), nullptr);

                    if (parsedTabs.hasProperty (App::ID::activePaneID))
                        currentTabs.setProperty (App::ID::activePaneID, parsedTabs.getProperty (App::ID::activePaneID), nullptr);

                    if (parsedTabs.hasProperty (App::ID::activePaneType))
                        currentTabs.setProperty (App::ID::activePaneType, parsedTabs.getProperty (App::ID::activePaneType), nullptr);

                    // PARAM children (int) — find by id, update Atom
                    loadParamValue (parsedTabs, App::ID::TABS, App::ID::active);

                    // Dynamic TAB children — remove existing, copy from parsed
                    for (int i { currentTabs.getNumChildren() - 1 }; i >= 0; --i)
                    {
                        if (currentTabs.getChild (i).getType() == App::ID::TAB)
                            currentTabs.removeChild (i, nullptr);
                    }

                    for (int i { 0 }; i < parsedTabs.getNumChildren(); ++i)
                    {
                        if (parsedTabs.getChild (i).getType() == App::ID::TAB)
                            currentTabs.appendChild (parsedTabs.getChild (i).createCopy(), nullptr);
                    }
                }
            }
        }
    }
}

/**
 * @brief Deletes `~/.config/end/nexus/<uuid>.nexus`.
 *
 * Called by daemon on clean exit (`onAllSessionsExited` lambda in Main.cpp).
 *
 * @note MESSAGE THREAD.
 */
void AppState::deleteNexusFile()
{
    getNexusFile().deleteFile();
}

juce::File AppState::getStateFile() const
{
    return lua::Engine::getConfigPath().getChildFile ("nexus/" + getInstanceUuid() + ".display");
}

juce::File AppState::getNexusFile() const
{
    return lua::Engine::getConfigPath().getChildFile ("nexus/" + getInstanceUuid() + ".nexus");
}

juce::File AppState::getWindowState() const
{
    return lua::Engine::getConfigPath().getChildFile ("window.state");
}

void AppState::saveWindowState()
{
    const juce::File file { getWindowState() };
    file.getParentDirectory().createDirectory();

    auto window { get().getChildWithName (App::ID::WINDOW) };

    if (window.isValid())
    {
        if (auto xml { window.createXml() })
            xml->writeTo (file);
    }
}

void AppState::loadWindowState()
{
    const juce::File file { getWindowState() };

    if (file.existsAsFile())
    {
        if (auto xml { juce::parseXML (file) })
        {
            auto parsed { juce::ValueTree::fromXml (*xml) };

            if (parsed.isValid() and parsed.getType() == App::ID::WINDOW)
            {
                // Width and height are PARAM children in the new format,
                // direct properties in the old format. Handle both.
                if (parsed.hasProperty (App::ID::width))
                {
                    // Old format — direct property
                    storeValue (App::ID::WINDOW, App::ID::width,
                                static_cast<int> (parsed.getProperty (App::ID::width)));
                }
                else
                {
                    // New format — PARAM child
                    loadParamValue (parsed, App::ID::WINDOW, App::ID::width);
                }

                if (parsed.hasProperty (App::ID::height))
                {
                    storeValue (App::ID::WINDOW, App::ID::height,
                                static_cast<int> (parsed.getProperty (App::ID::height)));
                }
                else
                {
                    loadParamValue (parsed, App::ID::WINDOW, App::ID::height);
                }
            }
        }
    }
}

void AppState::initDefaults()
{
    auto xml { jam::XML::getFromBinary (App::ID::appMetadata) };
    jassert (xml != nullptr);

    AppLayout::build (*xml, *this);
    needsFlushAtom = params.get<jam::AnyMap> (App::ID::END)->get<jam::Atom<int>> (App::ID::needsFlush);

    // Overlay Lua config runtime defaults onto XML-declared static defaults.
    const auto* cfg { lua::Engine::getContext() };

    // Int params — store in Atoms (width, height are Atom<int>)
    storeValue (App::ID::WINDOW, App::ID::width,  cfg->display.window.width);
    storeValue (App::ID::WINDOW, App::ID::height, cfg->display.window.height);

    // Float/string params — set VT properties directly
    auto window { getWindow() };
    window.setProperty (App::ID::zoom,       lua::Engine::zoomMin,       nullptr);
    window.setProperty (App::ID::fontFamily, cfg->display.font.family,   nullptr);
    window.setProperty (App::ID::fontSize,   static_cast<float> (cfg->dpiCorrectedFontSize()), nullptr);

    auto tabs { getTabs() };
    tabs.setProperty (App::ID::position, cfg->display.tab.position,  nullptr);

    flush();
    startTimerHz (60);
}
