/**
 * @file AppState.cpp
 * @brief Implementation of the application-level ValueTree owner.
 *
 * @see AppState.h
 */

#include "AppState.h"
#include "config/Config.h"

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

juce::ValueTree AppState::getTabs() noexcept
{
    return state.getOrCreateChildWithName (App::ID::TABS, nullptr);
}

//==============================================================================

int AppState::getWindowWidth() const noexcept
{
    auto window { state.getChildWithName (App::ID::WINDOW) };

    if (window.isValid() and window.hasProperty (App::ID::width))
        return static_cast<int> (window.getProperty (App::ID::width));

    return static_cast<int> (Config::getContext()->getInt (Config::Key::windowWidth));
}

int AppState::getWindowHeight() const noexcept
{
    auto window { state.getChildWithName (App::ID::WINDOW) };

    if (window.isValid() and window.hasProperty (App::ID::height))
        return static_cast<int> (window.getProperty (App::ID::height));

    return static_cast<int> (Config::getContext()->getInt (Config::Key::windowHeight));
}

float AppState::getWindowZoom() const noexcept
{
    auto window { state.getChildWithName (App::ID::WINDOW) };

    if (window.isValid() and window.hasProperty (App::ID::zoom))
        return static_cast<float> (window.getProperty (App::ID::zoom));

    return Config::getContext()->getFloat (Config::Key::windowZoom);
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

int AppState::getActiveTabIndex() const noexcept
{
    auto tabs { state.getChildWithName (App::ID::TABS) };

    if (tabs.isValid() and tabs.hasProperty (App::ID::active))
        return static_cast<int> (tabs.getProperty (App::ID::active));

    return 0;
}

void AppState::setActiveTabIndex (int index)
{
    auto tabs { getTabs() };
    tabs.setProperty (App::ID::active, index, nullptr);
}

juce::String AppState::getTabPosition() const noexcept
{
    auto tabs { state.getChildWithName (App::ID::TABS) };

    if (tabs.isValid() and tabs.hasProperty (App::ID::position))
        return tabs.getProperty (App::ID::position).toString();

    return Config::getContext()->getString (Config::Key::tabPosition);
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

    for (int i = 0; i < tabs.getNumChildren(); ++i)
    {
        if (tabs.getChild (i).getType() == App::ID::TAB)
        {
            if (tabIndex == index)
            {
                tabs.removeChild (i, nullptr);
                return;
            }

            ++tabIndex;
        }
    }
}

juce::ValueTree AppState::getTab (int index) noexcept
{
    auto tabs { getTabs() };
    int tabIndex { 0 };

    for (int i = 0; i < tabs.getNumChildren(); ++i)
    {
        if (tabs.getChild (i).getType() == App::ID::TAB)
        {
            if (tabIndex == index)
                return tabs.getChild (i);

            ++tabIndex;
        }
    }

    return juce::ValueTree();
}

juce::String AppState::getActiveTerminalUuid() const noexcept
{
    auto tabs { state.getChildWithName (App::ID::TABS) };

    if (tabs.isValid() and tabs.hasProperty (App::ID::activeTerminalUuid))
    {
        return tabs.getProperty (App::ID::activeTerminalUuid).toString();
    }

    return {};
}

void AppState::setActiveTerminalUuid (const juce::String& uuid)
{
    auto tabs { getTabs() };

    if (tabs.isValid())
    {
        tabs.setProperty (App::ID::activeTerminalUuid, uuid, nullptr);
    }
}

//==============================================================================

void AppState::save()
{
    auto file { getStateFile() };
    file.getParentDirectory().createDirectory();

    if (auto xml { state.createXml() })
        xml->writeTo (file);
}

void AppState::load()
{
    auto file { getStateFile() };

    if (file.existsAsFile())
    {
        if (auto xml { juce::parseXML (file) })
        {
            auto loaded { juce::ValueTree::fromXml (*xml) };

            if (loaded.isValid() and loaded.getType() == App::ID::END)
            {
                state = loaded;
                return;
            }
        }
    }

    initDefaults();
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
