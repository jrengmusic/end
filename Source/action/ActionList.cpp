/**
 * @file ActionList.cpp
 * @brief Action::List — lifecycle, configuration, row building, and key dispatch.
 */

#include "ActionList.h"

namespace Action
{ /*____________________________________________________________________________*/

const juce::Identifier List::bindingRowIndexId { "bindingRowIndex" };
const juce::Identifier List::bindingsDirtyId { "bindingsDirty" };

//==============================================================================
List::List (juce::Component& mainWindow, lua::Engine& engine)
    : main (mainWindow)
    , luaEngine (engine)
{
    setOpaque (false);
    setWantsKeyboardFocus (true);
    toFront (true);

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rowContainer, false);
    viewport.setScrollBarsShown (true, false);

    addChildComponent (messageOverlay);

    state.get().setProperty (bindingRowIndexId, -1, nullptr);
    state.get().setProperty (bindingsDirtyId, false, nullptr);
    state.get().addListener (this);

    buildRows();

    const auto* cfg { lua::Engine::getContext() };
    const int padH { cfg->display.actionList.paddingLeft + cfg->display.actionList.paddingRight };
    const int padV { cfg->display.actionList.paddingTop  + cfg->display.actionList.paddingBottom };

    const int proportionalWidth  { jam::toInt (cfg->display.actionList.width  * main.getWidth()  + padH) };
    const int proportionalHeight { jam::toInt (cfg->display.actionList.height * main.getHeight() + padV) };

    const int width { juce::jmax (minimumWidth, proportionalWidth) };
    const int height { juce::jmax (minimumHeight, proportionalHeight) };

    setSize (width, height);

    selectRow (0);

    keyHandler.emplace (KeyHandler::Callbacks { [this]
                                                {
                                                    executeSelected();
                                                },
                                                [this]
                                                {
                                                    if (onDismiss != nullptr)
                                                        onDismiss();
                                                },
                                                [this]
                                                {
                                                    if (auto* searchBox { rows.at (0)->getSearchBox() }; searchBox != nullptr)
                                                        searchBox->setText ("");
                                                },
                                                [this]
                                                {
                                                    enterBindingMode();
                                                },
                                                [this] (int i)
                                                {
                                                    selectRow (i);
                                                },
                                                [this]
                                                {
                                                    return visibleRowCount();
                                                },
                                                [this]
                                                {
                                                    return getSelectedIndex();
                                                } });
}

List::~List()
{
    state.get().removeListener (this);

    if (static_cast<bool> (state.get().getProperty (bindingsDirtyId)))
    {
        luaEngine.load();

        if (auto* registry { Action::Registry::getContext() }; registry != nullptr)
            luaEngine.buildKeyMap (*registry);
    }
}

//==============================================================================
juce::Colour List::getHighlightColour() const
{
    return lua::Engine::getContext()->display.actionList.highlightColour;
}

//==============================================================================
void List::configureSearchBox (juce::TextEditor& editor)
{
    editor.setMultiLine (false);
    editor.setReturnKeyStartsNewLine (false);
    editor.setScrollbarsShown (false);
    editor.setPopupMenuEnabled (false);
    editor.setTextToShowWhenEmpty ("Type to search...", juce::Colours::grey);
    editor.setWantsKeyboardFocus (true);
    editor.setEscapeAndReturnKeysConsumed (false);

    const auto* cfg { lua::Engine::getContext() };
    editor.setColour (
        juce::TextEditor::backgroundColourId,
        cfg->display.window.colour.withAlpha (cfg->display.window.opacity));
    editor.setColour (juce::TextEditor::textColourId,            cfg->display.colours.foreground);
    editor.setColour (juce::CaretComponent::caretColourId,       cfg->display.colours.cursor);
    editor.setColour (juce::TextEditor::outlineColourId,         juce::Colours::transparentBlack);
    editor.setColour (juce::TextEditor::focusedOutlineColourId,  juce::Colours::transparentBlack);

    editor.setFont (juce::Font (juce::FontOptions()
                                    .withName (cfg->display.font.family)
                                    .withPointHeight (cfg->display.font.size)));
}

//==============================================================================
void List::configureActionRow (Row& row)
{
    row.highlightColour = getHighlightColour();

    const auto* cfg { lua::Engine::getContext() };

    if (auto* name { row.getNameLabel() }; name != nullptr)
        name->setColour (juce::Label::textColourId, cfg->display.actionList.nameColour);

    if (auto* shortcut { row.getShortcutLabel() }; shortcut != nullptr)
        shortcut->setColour (juce::Label::textColourId, cfg->display.actionList.shortcutColour);
}

//==============================================================================
void List::buildRows()
{
    auto* registry { Registry::getContext() };
    jassert (registry != nullptr);

    // Row 0: search box.
    {
        const juce::String uuid { juce::Uuid().toString() };
        auto row { std::make_unique<Row> (0, uuid) };

        auto* searchBox { row->getSearchBox() };
        jassert (searchBox != nullptr);

        configureSearchBox (*searchBox);
        searchBox->onTextChange = [this, searchBox]
        {
            filterRows (searchBox->getText());
        };

        juce::ValueTree node { "ACTION" };
        node.setProperty (jam::ID::id, uuid, nullptr);
        state.get().appendChild (node, nullptr);
        jam::ValueTree::attach (state, row.get());

        addAndMakeVisible (row.get());
        rows.push_back (std::move (row));
    }

    const auto& entries { registry->getEntries() };

    // Global action rows (non-modal).
    for (const auto& entry : entries)
    {
        if (not entry.isModal)
        {
            const juce::String uuid { juce::Uuid().toString() };
            auto row { std::make_unique<Row> (static_cast<int> (rows.size()), uuid, entry) };
            row->actionConfigKey = luaEngine.getActionLuaKey (entry.id);

            configureActionRow (*row);

            juce::ValueTree node { "ACTION" };
            node.setProperty (jam::ID::id, uuid, nullptr);
            state.get().appendChild (node, nullptr);
            jam::ValueTree::attach (state, row.get());

            rowContainer.addAndMakeVisible (row.get());
            rows.push_back (std::move (row));
        }
    }

    // Separator between global and modal groups.
    {
        const juce::String uuid { juce::Uuid().toString() };
        auto row { std::make_unique<Row> (static_cast<int> (rows.size()), uuid, RowKind::separator) };
        row->highlightColour = getHighlightColour();

        juce::ValueTree node { "ACTION" };
        node.setProperty (jam::ID::id, uuid, nullptr);
        state.get().appendChild (node, nullptr);
        jam::ValueTree::attach (state, row.get());

        rowContainer.addAndMakeVisible (row.get());
        rows.push_back (std::move (row));
    }

    // Prefix key row.
    {
        const juce::String uuid { juce::Uuid().toString() };

        Registry::Entry prefixEntry;
        prefixEntry.id = "prefix";
        prefixEntry.name = "Prefix";
        prefixEntry.shortcut = Registry::parseShortcut (luaEngine.getPrefixString());

        auto row { std::make_unique<Row> (static_cast<int> (rows.size()), uuid, prefixEntry) };
        row->actionConfigKey = "keys.prefix";

        configureActionRow (*row);

        juce::ValueTree node { "ACTION" };
        node.setProperty (jam::ID::id, uuid, nullptr);
        state.get().appendChild (node, nullptr);
        jam::ValueTree::attach (state, row.get());

        rowContainer.addAndMakeVisible (row.get());
        rows.push_back (std::move (row));
    }

    // Modal action rows.
    for (const auto& entry : entries)
    {
        if (entry.isModal)
        {
            const juce::String uuid { juce::Uuid().toString() };
            auto row { std::make_unique<Row> (static_cast<int> (rows.size()), uuid, entry) };
            row->actionConfigKey = luaEngine.getActionLuaKey (entry.id);

            configureActionRow (*row);

            juce::ValueTree node { "ACTION" };
            node.setProperty (jam::ID::id, uuid, nullptr);
            state.get().appendChild (node, nullptr);
            jam::ValueTree::attach (state, row.get());

            rowContainer.addAndMakeVisible (row.get());
            rows.push_back (std::move (row));
        }
    }

    layoutRows();
}

//==============================================================================
void List::resized()
{
    auto bounds { getLocalBounds() };
    const auto* cfg { lua::Engine::getContext() };
    bounds.removeFromTop    (cfg->display.actionList.paddingTop);
    bounds.removeFromRight  (cfg->display.actionList.paddingRight);
    bounds.removeFromBottom (cfg->display.actionList.paddingBottom);
    bounds.removeFromLeft   (cfg->display.actionList.paddingLeft);

    if (not rows.empty())
        rows.at (0)->setBounds (bounds.removeFromTop (rowHeight));

    viewport.setBounds (bounds);
    messageOverlay.setBounds (getLocalBounds());
    layoutRows();
}

//==============================================================================
bool List::keyPressed (const juce::KeyPress& key)
{
    bool handled { false };

    if (getBindingRowIndex() >= 1)
    {
        handled = handleBindingKey (key);
    }
    else
    {
        handled = keyHandler->handleKey (key);
    }

    return handled;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Action
