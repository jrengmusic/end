/**
 * @file AppState.cpp
 * @brief Implementation of the application-level ValueTree owner.
 *
 * @see AppState.h
 */

#include "AppState.h"
#include "component/LookAndFeel.h"
#include "lua/Engine.h"
#include "terminal/data/Identifier.h"

AppState::AppState()
    : jam::ValueTree (App::ID::END)
{
    auto xml { jam::XML::getFromBinary (App::ID::appMetadata) };
    jassert (xml != nullptr);

    AppLayout::build (*xml, *this);

    // Overlay Lua runtime defaults.
    const auto* cfg { lua::Engine::getContext() };
    setValue (App::ID::width,      cfg->display.window.width);
    setValue (App::ID::height,     cfg->display.window.height);
    setValue (App::ID::zoom,       static_cast<double> (lua::Engine::zoomMin));
    setValue (App::ID::fontFamily, cfg->display.font.family);
    setValue (App::ID::fontSize,   static_cast<double> (cfg->dpiCorrectedFontSize()));
    setValue (App::ID::position,   cfg->display.tab.position);

    startTimerHz (60);
}

AppState::~AppState() = default;

//==============================================================================

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

int AppState::getWindowWidth() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), App::ID::width).getValue());
}

int AppState::getWindowHeight() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), App::ID::height).getValue());
}

float AppState::getWindowZoom() const noexcept
{
    return static_cast<float> (static_cast<double> (jam::ValueTree::getValueFromChildWithID (get(), App::ID::zoom).getValue()));
}

void AppState::setWindowSize (int width, int height)
{
    setValue (App::ID::width, width);
    setValue (App::ID::height, height);
}

void AppState::setWindowZoom (float zoom)
{
    const float clamped { juce::jlimit (lua::Engine::zoomMin, lua::Engine::zoomMax, zoom) };
    setValue (App::ID::zoom, static_cast<double> (clamped));
}

juce::String AppState::getFontFamily() const noexcept
{
    return jam::ValueTree::getValueFromChildWithID (get(), App::ID::fontFamily).getValue().toString();
}

void AppState::setFontFamily (const juce::String& family)
{
    setValue (App::ID::fontFamily, family);
}

float AppState::getFontSize() const noexcept
{
    return static_cast<float> (static_cast<double> (jam::ValueTree::getValueFromChildWithID (get(), App::ID::fontSize).getValue()));
}

void AppState::setFontSize (float size)
{
    setValue (App::ID::fontSize, static_cast<double> (size));
}

void AppState::markAtlasDirty() noexcept
{
    params.get<jam::Parameter<int>> (App::ID::atlasDirty)->storeRelease (1);
}

bool AppState::consumeAtlasDirty() noexcept
{
    return params.get<jam::Parameter<int>> (App::ID::atlasDirty)->exchangeAcquire (0) != 0;
}

App::RendererType AppState::getRendererType() const noexcept
{
    const auto renderer { jam::ValueTree::getValueFromChildWithID (get(), App::ID::renderer).getValue().toString() };

    if (renderer == App::ID::rendererCpu)
        return App::RendererType::cpu;

    return App::RendererType::gpu;
}

void AppState::setRendererType (const juce::String& setting)
{
    const bool gpuAvailable { static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), App::ID::gpuAvailable).getValue()) != 0 };
    const bool wantsGpu { setting != "false" };
    const juce::String resolved { wantsGpu and gpuAvailable ? App::ID::rendererGpu : App::ID::rendererCpu };
    setValue (App::ID::renderer, resolved);
    jam::BackgroundBlur::setEnabled (getRendererType() == App::RendererType::gpu);
}

void AppState::setGpuAvailable (bool available)
{
    setValue (App::ID::gpuAvailable, available ? 1 : 0);
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
    setValue (App::ID::daemonMode, isDaemon ? 1 : 0);
}

bool AppState::isDaemonMode() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), App::ID::daemonMode).getValue()) != 0;
}

void AppState::setPort (int activePort)
{
    setValue (App::ID::port, activePort);

    const juce::File nexusFile { getNexusFile() };
    nexusFile.getParentDirectory().createDirectory();
    nexusFile.replaceWithText (juce::String (activePort));
}

int AppState::getPort() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), App::ID::port).getValue());
}

int AppState::getActiveTabIndex() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), App::ID::active).getValue());
}

void AppState::setActiveTabIndex (int index)
{
    setValue (App::ID::active, index);
}

juce::String AppState::getTabPosition() const noexcept
{
    return jam::ValueTree::getValueFromChildWithID (get(), App::ID::position).getValue().toString();
}

void AppState::setTabPosition (const juce::String& position)
{
    setValue (App::ID::position, position);
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
    return jam::ValueTree::getValueFromChildWithID (get(), App::ID::activePaneID).getValue().toString();
}

void AppState::setActivePaneID (const juce::String& uuid)
{
    setValue (App::ID::activePaneID, uuid);
}

juce::String AppState::getActivePaneType() const noexcept
{
    return jam::ValueTree::getValueFromChildWithID (get(), App::ID::activePaneType).getValue().toString();
}

void AppState::setActivePaneType (const juce::String& type)
{
    setValue (App::ID::activePaneType, type);
}

void AppState::setModalType (int type)
{
    setValue (App::ID::modalType, type);
}

int AppState::getModalType() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), App::ID::modalType).getValue());
}

void AppState::setSelectionType (int type)
{
    setValue (App::ID::selectionType, type);
}

int AppState::getSelectionType() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), App::ID::selectionType).getValue());
}

juce::String AppState::getPwd() const noexcept
{
    juce::String result { juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName() };
    const auto cwd { activeSession.getProperty (Terminal::ID::cwd).toString() };

    if (cwd.isNotEmpty())
        result = cwd;

    return result;
}

void AppState::setPwd (juce::ValueTree sessionTree)
{
    activeSession = sessionTree;
}

//==============================================================================

void AppState::save()
{
    flush();

    const juce::File file { getStateFile() };
    file.getParentDirectory().createDirectory();

    juce::ValueTree persist { get().createCopy() };
    auto nexusNode { persist.getChildWithName (App::ID::NEXUS) };

    if (nexusNode.isValid())
        persist.removeChild (nexusNode, nullptr);

    if (auto xml { persist.createXml() })
        xml->writeTo (file);
}

void AppState::load()
{
    const juce::File file { getStateFile() };

    if (file.existsAsFile())
    {
        if (auto xml { juce::parseXML (file) })
        {
            auto parsed { juce::ValueTree::fromXml (*xml) };

            if (parsed.isValid() and parsed.getType() == App::ID::END)
                replaceState (parsed);
        }
    }
}

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
    flush();

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
                for (int i { 0 }; i < parsed.getNumChildren(); ++i)
                {
                    auto child { parsed.getChild (i) };

                    if (child.getType() == jam::ValueTree::PARAM)
                    {
                        const juce::Identifier paramId { child.getProperty (jam::ID::id).toString() };
                        const auto paramValue { child.getProperty (jam::ID::value) };
                        setValue (paramId, paramValue);
                    }
                }
            }
        }
    }
}

