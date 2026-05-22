/**
 * @file AppState.cpp
 * @brief Implementation of the application-level ValueTree owner.
 *
 * @see AppState.h
 */

#include "AppState.h"

AppState::AppState()
    : jam::ValueTree (app::id::END)
{
    auto xml { jam::XML::getFromBinary (app::id::appMetadata) };
    jassert (xml != nullptr);

    build (*xml);

    // Overlay Lua runtime defaults.
    const auto* cfg { lua::Engine::getContext() };
    setValue (jam::ID::width,      cfg->display.window.width);
    setValue (jam::ID::height,     cfg->display.window.height);
    setValue (app::id::zoom,       static_cast<double> (lua::Engine::zoomMin));
    setValue (app::id::fontFamily, cfg->display.font.family);
    setValue (app::id::fontSize,   static_cast<double> (cfg->dpiCorrectedFontSize()));
    setValue (app::id::position,        cfg->display.tab.position);
    setValue (app::id::scrollbackLines,    cfg->nexus.terminal.scrollbackLines);
    setValue (app::id::cellWidth,           cfg->display.font.cellWidth);
    setValue (app::id::lineHeight,          cfg->display.font.lineHeight);
    setValue (app::id::cursorCodepoint,     static_cast<int> (cfg->display.cursor.codepoint));
    setValue (app::id::cursorStyle,         cfg->display.cursor.style);
    setValue (app::id::cursorBlinkInterval, cfg->display.cursor.blinkInterval);
    setValue (app::id::paddingTop,          cfg->nexus.terminal.paddingTop);
    setValue (app::id::paddingRight,        cfg->nexus.terminal.paddingRight);
    setValue (app::id::paddingBottom,       cfg->nexus.terminal.paddingBottom);
    setValue (app::id::paddingLeft,         cfg->nexus.terminal.paddingLeft);

    startTimerHz (60);
}

AppState::~AppState() = default;

//==============================================================================

juce::ValueTree AppState::getWindow() noexcept
{
    return get().getOrCreateChildWithName (app::id::WINDOW, nullptr);
}

juce::ValueTree AppState::getNexusNode() noexcept
{
    return get().getOrCreateChildWithName (app::id::NEXUS, nullptr);
}

juce::ValueTree AppState::getSessionsNode() noexcept
{
    return getNexusNode().getOrCreateChildWithName (app::id::SESSIONS, nullptr);
}

juce::ValueTree AppState::getLoadingNode() noexcept
{
    return getNexusNode().getOrCreateChildWithName (app::id::LOADING, nullptr);
}

juce::ValueTree AppState::getTabs() noexcept
{
    return get().getOrCreateChildWithName (app::id::TABS, nullptr);
}

//==============================================================================

int AppState::getWindowWidth() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), jam::ID::width).getValue());
}

int AppState::getWindowHeight() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), jam::ID::height).getValue());
}

float AppState::getWindowZoom() const noexcept
{
    return static_cast<float> (static_cast<double> (jam::ValueTree::getValueFromChildWithID (get(), app::id::zoom).getValue()));
}

void AppState::setWindowSize (int width, int height)
{
    setValue (jam::ID::width, width);
    setValue (jam::ID::height, height);
}

void AppState::setWindowZoom (float zoom)
{
    const float clamped { juce::jlimit (lua::Engine::zoomMin, lua::Engine::zoomMax, zoom) };
    setValue (app::id::zoom, static_cast<double> (clamped));
}

juce::String AppState::getFontFamily() const noexcept
{
    return jam::ValueTree::getValueFromChildWithID (get(), app::id::fontFamily).getValue().toString();
}

void AppState::setFontFamily (const juce::String& family)
{
    setValue (app::id::fontFamily, family);
}

float AppState::getFontSize() const noexcept
{
    return static_cast<float> (static_cast<double> (jam::ValueTree::getValueFromChildWithID (get(), app::id::fontSize).getValue()));
}

void AppState::setFontSize (float size)
{
    setValue (app::id::fontSize, static_cast<double> (size));
}

void AppState::setScrollbackLines (int lines)
{
    setValue (app::id::scrollbackLines, lines);
}

int AppState::getCellWidth() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::cellWidth).getValue());
}

void AppState::setCellWidth (int width)
{
    setValue (app::id::cellWidth, width);
}

int AppState::getLineHeight() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::lineHeight).getValue());
}

void AppState::setLineHeight (int height)
{
    setValue (app::id::lineHeight, height);
}

int AppState::getCursorCodepoint() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::cursorCodepoint).getValue());
}

void AppState::setCursorCodepoint (int codepoint)
{
    setValue (app::id::cursorCodepoint, codepoint);
}

int AppState::getCursorStyle() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::cursorStyle).getValue());
}

void AppState::setCursorStyle (int style)
{
    setValue (app::id::cursorStyle, style);
}

int AppState::getCursorBlinkInterval() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::cursorBlinkInterval).getValue());
}

void AppState::setCursorBlinkInterval (int ms)
{
    setValue (app::id::cursorBlinkInterval, ms);
}

int AppState::getPaddingTop() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::paddingTop).getValue());
}

int AppState::getPaddingRight() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::paddingRight).getValue());
}

int AppState::getPaddingBottom() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::paddingBottom).getValue());
}

int AppState::getPaddingLeft() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::paddingLeft).getValue());
}

void AppState::setPaddingTop (int value)
{
    setValue (app::id::paddingTop, value);
}

void AppState::setPaddingRight (int value)
{
    setValue (app::id::paddingRight, value);
}

void AppState::setPaddingBottom (int value)
{
    setValue (app::id::paddingBottom, value);
}

void AppState::setPaddingLeft (int value)
{
    setValue (app::id::paddingLeft, value);
}

void AppState::markAtlasDirty() noexcept
{
    params.get<jam::Parameter<int>> (app::id::atlasDirty)->storeRelease (1);
}

bool AppState::consumeAtlasDirty() noexcept
{
    return params.get<jam::Parameter<int>> (app::id::atlasDirty)->exchangeAcquire (0) != 0;
}

app::RendererType AppState::getRendererType() const noexcept
{
    const auto renderer { jam::ValueTree::getValueFromChildWithID (get(), app::id::renderer).getValue().toString() };

    if (Map::Renderer::getContext()->get (renderer) == Map::Renderer::cpu)
        return app::RendererType::cpu;

    return app::RendererType::gpu;
}

void AppState::setRendererType (const juce::String& setting)
{
    const bool gpuAvailable { static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::gpuAvailable).getValue()) != 0 };
    const bool wantsGpu { Map::Gpu::getContext()->get (setting) != Map::Gpu::off };
    const juce::String resolved { wantsGpu and gpuAvailable ? Map::Renderer::getContext()->get (Map::Renderer::gpu)
                                                             : Map::Renderer::getContext()->get (Map::Renderer::cpu) };
    setValue (app::id::renderer, resolved);
    jam::BackgroundBlur::setEnabled (getRendererType() == app::RendererType::gpu);
}

void AppState::setGpuAvailable (bool available)
{
    setValue (app::id::gpuAvailable, available ? 1 : 0);
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
    setValue (app::id::daemonMode, isDaemon ? 1 : 0);
}

bool AppState::isDaemonMode() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::daemonMode).getValue()) != 0;
}

void AppState::setPort (int activePort)
{
    setValue (app::id::port, activePort);

    const juce::File nexusFile { getNexusFile() };
    nexusFile.getParentDirectory().createDirectory();
    nexusFile.replaceWithText (juce::String (activePort));
}

int AppState::getPort() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::port).getValue());
}

int AppState::getActiveTabIndex() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::active).getValue());
}

void AppState::setActiveTabIndex (int index)
{
    setValue (app::id::active, index);
}

juce::String AppState::getTabPosition() const noexcept
{
    return jam::ValueTree::getValueFromChildWithID (get(), app::id::position).getValue().toString();
}

void AppState::setTabPosition (const juce::String& position)
{
    setValue (app::id::position, position);
}

//==============================================================================

juce::ValueTree AppState::addTab()
{
    auto tabs { getTabs() };
    juce::ValueTree tab (app::id::TAB);

    juce::ValueTree panes (app::id::PANES);
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

juce::ValueTree AppState::getTab (int index) noexcept
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

juce::String AppState::getActivePaneID() const noexcept
{
    return jam::ValueTree::getValueFromChildWithID (get(), app::id::activePaneID).getValue().toString();
}

void AppState::setActivePaneID (const juce::String& uuid)
{
    setValue (app::id::activePaneID, uuid);
}

juce::String AppState::getActivePaneType() const noexcept
{
    return jam::ValueTree::getValueFromChildWithID (get(), app::id::activePaneType).getValue().toString();
}

void AppState::setActivePaneType (const juce::String& type)
{
    setValue (app::id::activePaneType, type);
}

void AppState::setModalType (int type)
{
    setValue (app::id::modalType, type);
}

int AppState::getModalType() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::modalType).getValue());
}

void AppState::setSelectionType (int type)
{
    setValue (app::id::selectionType, type);
}

int AppState::getSelectionType() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), app::id::selectionType).getValue());
}

juce::String AppState::getPwd() const noexcept
{
    juce::String result { juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName() };
    const auto cwd { activeSession.getProperty (terminal::id::cwd).toString() };

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
    auto nexusNode { persist.getChildWithName (app::id::NEXUS) };

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

            if (parsed.isValid() and parsed.getType() == app::id::END)
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

    auto window { get().getChildWithName (app::id::WINDOW) };

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

            if (parsed.isValid() and parsed.getType() == app::id::WINDOW)
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

//==============================================================================

juce::var AppState::resolveAppLayoutDefault (const juce::XmlElement& elem) noexcept
{
    const auto typeStr    { elem.getStringAttribute (app::id::type.toString()) };
    const auto defaultStr { elem.getStringAttribute (app::id::defaultValue.toString()) };
    juce::var result {};

    if (typeStr == app::id::boolType.toString())
    {
        result = Map::Bool::getContext()->get (defaultStr);
    }
    else if (typeStr == app::id::floatType.toString())
    {
        result = elem.getDoubleAttribute (app::id::defaultValue.toString());
    }
    else if (typeStr == app::id::stringType.toString())
    {
        result = defaultStr;
    }
    else
    {
        result = elem.getIntAttribute (app::id::defaultValue.toString());
    }

    return result;
}

void AppState::build (const juce::XmlElement& xml)
{
    // Root VT node — already constructed in AppState (app::id::END).
    juce::ValueTree rootNode { get() };

    // Walk XML children — dispatch on tag name.
    for (auto* child : xml.getChildIterator())
    {
        const auto& tag { child->getTagName() };

        if (tag == jam::ValueTree::PARAM.toString())
        {
            // Root-level parameter → flat params, root VT node.
            const juce::Identifier id { child->getStringAttribute (jam::ID::id.toString()) };
            const auto typeStr { child->getStringAttribute (app::id::type.toString()) };

            if (typeStr == app::id::floatType.toString() or typeStr == app::id::stringType.toString())
            {
                // Float/string: PARAM child only, no Parameter.
                juce::ValueTree param { jam::ValueTree::PARAM };
                param.setProperty (jam::ID::id, id.toString(), nullptr);
                param.setProperty (jam::ID::value, resolveAppLayoutDefault (*child), nullptr);
                rootNode.appendChild (param, nullptr);
            }
            else
            {
                // int/bool: Parameter<int> + PARAM child via addParameter.
                addParameter (id,
                              static_cast<int> (resolveAppLayoutDefault (*child)),
                              params,
                              rootNode);
            }
        }
        else
        {
            // Group tag (WINDOW, TABS) — create structural VT child, recurse into PARAMs.
            const juce::Identifier groupId { tag };
            juce::ValueTree groupNode { groupId };
            rootNode.appendChild (groupNode, nullptr);

            for (auto* groupChild : child->getChildIterator())
            {
                const juce::Identifier paramId { groupChild->getStringAttribute (jam::ID::id.toString()) };
                const auto paramTypeStr { groupChild->getStringAttribute (app::id::type.toString()) };

                if (paramTypeStr == app::id::floatType.toString() or paramTypeStr == app::id::stringType.toString())
                {
                    // Float/string: PARAM child only, no Parameter.
                    juce::ValueTree param { jam::ValueTree::PARAM };
                    param.setProperty (jam::ID::id, paramId.toString(), nullptr);
                    param.setProperty (jam::ID::value, resolveAppLayoutDefault (*groupChild), nullptr);
                    groupNode.appendChild (param, nullptr);
                }
                else
                {
                    // int/bool: Parameter<int> + PARAM child, flat AnyMap.
                    addParameter (paramId,
                                  static_cast<int> (resolveAppLayoutDefault (*groupChild)),
                                  params,
                                  groupNode);
                }
            }
        }
    }
}

