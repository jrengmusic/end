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
    load();
}

AppState::~AppState()
{
}

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

juce::ValueTree AppState::getProcessorsNode() noexcept
{
    return getNexusNode().getOrCreateChildWithName (App::ID::PROCESSORS, nullptr);
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
    float result { Config::getContext()->getFloat (Config::Key::windowZoom) };

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

void AppState::setNexusMode (bool isNexus)
{
    getWindow().setProperty (App::ID::nexusMode, isNexus, nullptr);
}

bool AppState::isNexusMode() const noexcept
{
    auto window { state.getChildWithName (App::ID::WINDOW) };
    bool result { false };

    if (window.isValid() and window.hasProperty (App::ID::nexusMode))
        result = static_cast<bool> (window.getProperty (App::ID::nexusMode));

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

void AppState::save (bool isNexusMode)
{
    auto file { getStateFile() };
    file.getParentDirectory().createDirectory();

    if (isNexusMode)
    {
        juce::ValueTree persist { state.createCopy() };
        auto nexusNode { persist.getChildWithName (App::ID::NEXUS) };

        if (nexusNode.isValid())
            persist.removeChild (nexusNode, nullptr);

        if (auto xml { persist.createXml() })
            xml->writeTo (file);
    }
    else
    {
        juce::ValueTree windowOnly { App::ID::END };
        const auto window { state.getChildWithName (App::ID::WINDOW) };

        if (window.isValid())
            windowOnly.appendChild (window.createCopy(), nullptr);

        if (auto xml { windowOnly.createXml() })
            xml->writeTo (file);
    }
}

/**
 * @brief Parses state.xml and replaces the entire in-memory tree.
 *
 * **CRITICAL:** this replaces `state` wholesale via `state = parsed`. ANY
 * property written to AppState before `load()` runs will be lost. The
 * `AppState` ctor calls `load()` so that subsequent code can write freely.
 * Do NOT call `load()` again after construction unless you intend to wipe
 * every runtime-only property on the tree.
 *
 * On parse failure or missing file, falls back to `initDefaults()`.
 *
 * @note MESSAGE THREAD.
 */
void AppState::load()
{
    auto file { getStateFile() };
    bool loaded { false };

    if (file.existsAsFile())
    {
        if (auto xml { juce::parseXML (file) })
        {
            auto parsed { juce::ValueTree::fromXml (*xml) };

            if (parsed.isValid() and parsed.getType() == App::ID::END)
            {
                state = parsed;

                auto staleNexus { state.getChildWithName (App::ID::NEXUS) };

                if (staleNexus.isValid())
                    state.removeChild (staleNexus, nullptr);

                loaded = true;
            }
        }
    }

    if (not loaded)
    {
        initDefaults();
    }
}

juce::File AppState::getStateFile() const
{
    return Config::getContext()->getConfigFile().getParentDirectory().getChildFile ("state.xml");
}

void AppState::initDefaults()
{
    state = juce::ValueTree (App::ID::END);

    auto window { juce::ValueTree (App::ID::WINDOW) };
    auto* cfg { Config::getContext() };
    window.setProperty (App::ID::width, cfg->getInt (Config::Key::windowWidth), nullptr);
    window.setProperty (App::ID::height, cfg->getInt (Config::Key::windowHeight), nullptr);
    window.setProperty (App::ID::zoom, cfg->getFloat (Config::Key::windowZoom), nullptr);

    state.appendChild (window, nullptr);

    auto tabs { juce::ValueTree (App::ID::TABS) };
    tabs.setProperty (App::ID::active, 0, nullptr);
    tabs.setProperty (App::ID::position, cfg->getString (Config::Key::tabPosition), nullptr);
    state.appendChild (tabs, nullptr);
}
