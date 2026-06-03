/**
 * @file ActionList.cpp
 * @brief action::List — lifecycle, configuration, row building, and key dispatch.
 */

#include "ActionList.h"

namespace action
{
/*____________________________________________________________________________*/

const juce::Identifier List::bindingRowIndexId { "bindingRowIndex" };
const juce::Identifier List::bindingsDirtyId { "bindingsDirty" };

//==============================================================================
List::List (juce::Component& mainWindow)
    : main (mainWindow)
{
    setOpaque (false);
    setWantsKeyboardFocus (true);
    toFront (true);

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rowContainer, false);
    viewport.setScrollBarsShown (true, false);

    addChildComponent (messageOverlay);

    state.setTreeProperty (bindingRowIndexId, -1, nullptr);
    state.setTreeProperty (bindingsDirtyId, false, nullptr);
    state.addListener (this);

    buildRows();

    const auto* appState { AppModel::getContext() };
    const int padH { appState->getValue<int> (app::id::DISPLAY_LUA, app::id::actionListPaddingLeft)
                   + appState->getValue<int> (app::id::DISPLAY_LUA, app::id::actionListPaddingRight) };
    const int padV { appState->getValue<int> (app::id::DISPLAY_LUA, app::id::actionListPaddingTop)
                   + appState->getValue<int> (app::id::DISPLAY_LUA, app::id::actionListPaddingBottom) };

    const int proportionalWidth  { jam::toInt (appState->getValue<float> (app::id::DISPLAY_LUA, app::id::actionListWidth)  * main.getWidth()  + static_cast<float> (padH)) };
    const int proportionalHeight { jam::toInt (appState->getValue<float> (app::id::DISPLAY_LUA, app::id::actionListHeight) * main.getHeight() + static_cast<float> (padV)) };

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
    state.removeListener (this);

    if (static_cast<bool> (state.getTreeProperty (bindingsDirtyId)))
    {
        AppModel::getContext()->reload();

        if (auto* registry { action::Registry::getContext() }; registry != nullptr)
            AppModel::getContext()->buildKeyMap (*registry);
    }
}

//==============================================================================
juce::Colour List::getHighlightColour() const
{
    return juce::Colour (static_cast<juce::uint32> (AppModel::getContext()->getValue<int> (app::id::DISPLAY_LUA, app::id::actionListHighlightColour)));
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

    const auto* appState { AppModel::getContext() };
    const juce::Colour windowColour { juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::windowColour))) };
    const float opacity { appState->getValue<float> (app::id::DISPLAY_LUA, app::id::windowOpacity) };
    editor.setColour (
        juce::TextEditor::backgroundColourId,
        windowColour.withAlpha (opacity));
    editor.setColour (juce::TextEditor::textColourId,            juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::foreground))));
    editor.setColour (juce::CaretComponent::caretColourId,       juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::cursorColour))));
    editor.setColour (juce::TextEditor::outlineColourId,         juce::Colours::transparentBlack);
    editor.setColour (juce::TextEditor::focusedOutlineColourId,  juce::Colours::transparentBlack);

    editor.setFont (juce::Font (juce::FontOptions()
                                    .withName (appState->getValue<juce::String> (app::id::DISPLAY_LUA, app::id::fontFamily))
                                    .withPointHeight (appState->getValue<float> (app::id::DISPLAY_LUA, app::id::fontSize))));
}

//==============================================================================
void List::configureActionRow (Row& row)
{
    row.highlightColour = getHighlightColour();

    const auto* appState { AppModel::getContext() };

    if (auto* name { row.getNameLabel() }; name != nullptr)
        name->setColour (juce::Label::textColourId, juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::actionListNameColour))));

    if (auto* shortcut { row.getShortcutLabel() }; shortcut != nullptr)
        shortcut->setColour (juce::Label::textColourId, juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::actionListShortcutColour))));
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
        state.appendChild (node, nullptr);
        jam::Model::attach (state, row.get());

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
            row->actionConfigKey = AppModel::getContext()->getActionLuaKey (entry.id);

            configureActionRow (*row);

            juce::ValueTree node { "ACTION" };
            node.setProperty (jam::ID::id, uuid, nullptr);
            state.appendChild (node, nullptr);
            jam::Model::attach (state, row.get());

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
        state.appendChild (node, nullptr);
        jam::Model::attach (state, row.get());

        rowContainer.addAndMakeVisible (row.get());
        rows.push_back (std::move (row));
    }

    // Prefix key row.
    {
        const juce::String uuid { juce::Uuid().toString() };

        Registry::Entry prefixEntry;
        prefixEntry.id = "prefix";
        prefixEntry.name = "Prefix";
        prefixEntry.shortcut = Registry::parseShortcut (AppModel::getContext()->getPrefixString());

        auto row { std::make_unique<Row> (static_cast<int> (rows.size()), uuid, prefixEntry) };
        row->actionConfigKey = "keys.prefix";

        configureActionRow (*row);

        juce::ValueTree node { "ACTION" };
        node.setProperty (jam::ID::id, uuid, nullptr);
        state.appendChild (node, nullptr);
        jam::Model::attach (state, row.get());

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
            row->actionConfigKey = AppModel::getContext()->getActionLuaKey (entry.id);

            configureActionRow (*row);

            juce::ValueTree node { "ACTION" };
            node.setProperty (jam::ID::id, uuid, nullptr);
            state.appendChild (node, nullptr);
            jam::Model::attach (state, row.get());

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
    const auto* appState { AppModel::getContext() };
    bounds.removeFromTop    (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::actionListPaddingTop));
    bounds.removeFromRight  (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::actionListPaddingRight));
    bounds.removeFromBottom (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::actionListPaddingBottom));
    bounds.removeFromLeft   (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::actionListPaddingLeft));

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
} // namespace action
