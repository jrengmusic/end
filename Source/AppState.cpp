/**
 * @file AppState.cpp
 * @brief Implementation of the application-level ValueTree owner.
 *
 * @see AppState.h
 */

#include "AppState.h"
#include "config/Config.h"
#include "terminal/data/Identifier.h"

AppState::AppState()
    : state (App::ID::END)
{
    initDefaults();
}

AppState::~AppState() = default;

//==============================================================================

juce::ValueTree& AppState::get() noexcept
{
    return state;
}

juce::ValueTree AppState::getWindow() noexcept
{
    return state.getOrCreateChildWithName (App::ID::WINDOW, nullptr);
}

juce::ValueTree AppState::getNexusNode() noexcept
{
    return state.getOrCreateChildWithName (App::ID::NEXUS, nullptr);
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
    return state.getOrCreateChildWithName (App::ID::TABS, nullptr);
}

//==============================================================================

int AppState::getWindowWidth() const noexcept
{
    auto window { state.getChildWithName (App::ID::WINDOW) };
    int result { static_cast<int> (Config::getContext()->getInt (Config::Key::windowWidth)) };

    if (window.isValid() and window.hasProperty (App::ID::width))
        result = static_cast<int> (window.getProperty (App::ID::width));

    return result;
}

int AppState::getWindowHeight() const noexcept
{
    auto window { state.getChildWithName (App::ID::WINDOW) };
    int result { static_cast<int> (Config::getContext()->getInt (Config::Key::windowHeight)) };

    if (window.isValid() and window.hasProperty (App::ID::height))
        result = static_cast<int> (window.getProperty (App::ID::height));

    return result;
}

float AppState::getWindowZoom() const noexcept
{
    auto window { state.getChildWithName (App::ID::WINDOW) };
    float result { Config::zoomMin };

    if (window.isValid() and window.hasProperty (App::ID::zoom))
        result = static_cast<float> (window.getProperty (App::ID::zoom));

    return result;
}

void AppState::setWindowSize (int width, int height)
{
    auto window { getWindow() };
    window.setProperty (App::ID::width, width, nullptr);
    window.setProperty (App::ID::height, height, nullptr);
}

void AppState::setWindowZoom (float zoom)
{
    const float clamped { juce::jlimit (Config::zoomMin, Config::zoomMax, zoom) };
    auto window { getWindow() };
    window.setProperty (App::ID::zoom, clamped, nullptr);
}

App::RendererType AppState::getRendererType() const noexcept
{
    auto window { state.getChildWithName (App::ID::WINDOW) };

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
    const bool gpuAvailable { static_cast<bool> (window.getProperty (App::ID::gpuAvailable, false)) };
    const bool wantsGpu { setting != "false" };
    const juce::String resolved { wantsGpu and gpuAvailable ? App::ID::rendererGpu : App::ID::rendererCpu };
    window.setProperty (App::ID::renderer, resolved, nullptr);
}

void AppState::setGpuAvailable (bool available)
{
    getWindow().setProperty (App::ID::gpuAvailable, available, nullptr);
}

void AppState::setInstanceUuid (const juce::String& uuid)
{
    instanceUuid = uuid;
}

juce::String AppState::getInstanceUuid() const noexcept
{
    return instanceUuid;
}

void AppState::setDaemonMode (bool isDaemon)
{
    getWindow().setProperty (App::ID::daemonMode, isDaemon, nullptr);
}

bool AppState::isDaemonMode() const noexcept
{
    auto window { state.getChildWithName (App::ID::WINDOW) };
    bool result { false };

    if (window.isValid() and window.hasProperty (App::ID::daemonMode))
        result = static_cast<bool> (window.getProperty (App::ID::daemonMode));

    return result;
}

void AppState::setPort (int activePort)
{
    state.setProperty (App::ID::port, activePort, nullptr);

    const juce::File nexusFile { getNexusFile() };
    nexusFile.getParentDirectory().createDirectory();
    nexusFile.replaceWithText (juce::String (activePort));
}

int AppState::getPort() const noexcept
{
    int result { 0 };

    if (state.hasProperty (App::ID::port))
        result = static_cast<int> (state.getProperty (App::ID::port));

    return result;
}

int AppState::getActiveTabIndex() const noexcept
{
    auto tabs { state.getChildWithName (App::ID::TABS) };
    int result { 0 };

    if (tabs.isValid() and tabs.hasProperty (App::ID::active))
        result = static_cast<int> (tabs.getProperty (App::ID::active));

    return result;
}

void AppState::setActiveTabIndex (int index)
{
    auto tabs { getTabs() };
    tabs.setProperty (App::ID::active, index, nullptr);
}

juce::String AppState::getTabPosition() const noexcept
{
    auto tabs { state.getChildWithName (App::ID::TABS) };
    juce::String result { Config::getContext()->getString (Config::Key::tabPosition) };

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
    auto tabs { state.getChildWithName (App::ID::TABS) };
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
    auto tabs { state.getChildWithName (App::ID::TABS) };
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
    auto tabs { getTabs() };

    if (tabs.isValid())
    {
        tabs.setProperty (App::ID::modalType, type, nullptr);
    }
}

int AppState::getModalType() const noexcept
{
    auto tabs { state.getChildWithName (App::ID::TABS) };
    int result { 0 };

    if (tabs.isValid() and tabs.hasProperty (App::ID::modalType))
        result = static_cast<int> (tabs.getProperty (App::ID::modalType));

    return result;
}

void AppState::setSelectionType (int type)
{
    auto tabs { getTabs() };

    if (tabs.isValid())
    {
        tabs.setProperty (App::ID::selectionType, type, nullptr);
    }
}

int AppState::getSelectionType() const noexcept
{
    auto tabs { state.getChildWithName (App::ID::TABS) };
    int result { 0 };

    if (tabs.isValid() and tabs.hasProperty (App::ID::selectionType))
        result = static_cast<int> (tabs.getProperty (App::ID::selectionType));

    return result;
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

    juce::ValueTree persist { state.createCopy() };
    auto nexusNode { persist.getChildWithName (App::ID::NEXUS) };

    if (nexusNode.isValid())
        persist.removeChild (nexusNode, nullptr);

    persist.removeProperty (App::ID::port, nullptr);

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

                    if (parsedWindow.hasProperty (App::ID::width))
                        currentWindow.setProperty (App::ID::width, parsedWindow.getProperty (App::ID::width), nullptr);

                    if (parsedWindow.hasProperty (App::ID::height))
                        currentWindow.setProperty (App::ID::height, parsedWindow.getProperty (App::ID::height), nullptr);

                    if (parsedWindow.hasProperty (App::ID::zoom))
                        currentWindow.setProperty (App::ID::zoom, parsedWindow.getProperty (App::ID::zoom), nullptr);

                    if (parsedWindow.hasProperty (App::ID::renderer))
                        currentWindow.setProperty (App::ID::renderer, parsedWindow.getProperty (App::ID::renderer), nullptr);
                }

                auto parsedTabs { parsed.getChildWithName (App::ID::TABS) };

                if (parsedTabs.isValid())
                {
                    auto existingTabs { state.getChildWithName (App::ID::TABS) };

                    if (existingTabs.isValid())
                        state.removeChild (existingTabs, nullptr);

                    state.appendChild (parsedTabs.createCopy(), nullptr);
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
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile (".config/end/nexus/" + instanceUuid + ".display");
}

juce::File AppState::getNexusFile() const
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile (".config/end/nexus/" + instanceUuid + ".nexus");
}

juce::File AppState::getWindowState() const
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile (".config/end/window.state");
}

void AppState::saveWindowState()
{
    const juce::File file { getWindowState() };
    file.getParentDirectory().createDirectory();

    auto window { state.getChildWithName (App::ID::WINDOW) };

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
                auto currentWindow { getWindow() };

                if (parsed.hasProperty (App::ID::width))
                    currentWindow.setProperty (App::ID::width, parsed.getProperty (App::ID::width), nullptr);

                if (parsed.hasProperty (App::ID::height))
                    currentWindow.setProperty (App::ID::height, parsed.getProperty (App::ID::height), nullptr);
            }
        }
    }
}

void AppState::initDefaults()
{
    state = juce::ValueTree (App::ID::END);

    auto window { juce::ValueTree (App::ID::WINDOW) };
    auto* cfg { Config::getContext() };
    window.setProperty (App::ID::width, cfg->getInt (Config::Key::windowWidth), nullptr);
    window.setProperty (App::ID::height, cfg->getInt (Config::Key::windowHeight), nullptr);
    window.setProperty (App::ID::zoom, Config::zoomMin, nullptr);

    state.appendChild (window, nullptr);

    auto tabs { juce::ValueTree (App::ID::TABS) };
    tabs.setProperty (App::ID::active, 0, nullptr);
    tabs.setProperty (App::ID::position, cfg->getString (Config::Key::tabPosition), nullptr);
    state.appendChild (tabs, nullptr);
}
