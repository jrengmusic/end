/**
 * @file ActionList.cpp
 * @brief Command palette — self-contained component with fuzzy search and action list.
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
void List::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    auto* registry { Action::Registry::getContext() };

    for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows.at (static_cast<std::size_t> (i))->actionConfigKey.isNotEmpty())
        {
            if (auto* label { rows.at (static_cast<std::size_t> (i))->getShortcutLabel() }; label != nullptr)
            {
                const auto currentShortcut { label->getText() };
                const auto engineShortcut { luaEngine.getShortcutString (rows.at (static_cast<std::size_t> (i))->actionConfigKey) };

                if (currentShortcut != engineShortcut)
                {
                    luaEngine.patchKey (rows.at (static_cast<std::size_t> (i))->actionConfigKey, currentShortcut);
                    luaEngine.load();

                    if (registry != nullptr)
                        luaEngine.buildKeyMap (*registry);

                    state.get().setProperty (bindingsDirtyId, true, nullptr);
                }
            }
        }
    }
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
void List::layoutRows()
{
    const int rowWidth { viewport.getWidth() - viewport.getScrollBarThickness() };
    int yPos { 0 };

    for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows.at (static_cast<std::size_t> (i))->isVisible())
        {
            const int height { rows.at (static_cast<std::size_t> (i))->getKind() == RowKind::separator
                                   ? separatorRowHeight
                                   : rowHeight };
            rows.at (static_cast<std::size_t> (i))->setBounds (0, yPos, rowWidth, height);
            yPos += height;
        }
    }

    rowContainer.setSize (rowWidth, yPos);
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
void List::filterRows (const juce::String& query)
{
    // Row 0 is always visible (it IS the search box).
    for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
    {
        if (query.isEmpty())
        {
            rows.at (static_cast<std::size_t> (i))->setVisible (true);
        }
        else
        {
            if (rows.at (static_cast<std::size_t> (i))->getKind() != RowKind::separator)
                rows.at (static_cast<std::size_t> (i))->setVisible (false);
        }
    }

    if (query.isNotEmpty())
    {
        jam::FuzzySearch::Data::vector dataset;
        dataset.reserve (rows.size() - 1);

        for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
        {
            if (auto* label { rows.at (static_cast<std::size_t> (i))->getNameLabel() }; label != nullptr)
                dataset.push_back (label->getText().toStdString());
        }

        const auto results { jam::FuzzySearch::getResult (query, dataset) };

        std::unordered_set<std::string> matchedNames;

        for (const auto& result : results)
            matchedNames.insert (result.second);

        for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
        {
            if (auto* label { rows.at (static_cast<std::size_t> (i))->getNameLabel() }; label != nullptr)
            {
                const bool isMatch { matchedNames.count (label->getText().toStdString()) > 0 };
                rows.at (static_cast<std::size_t> (i))->setVisible (isMatch);
            }
        }
    }

    layoutRows();
    selectRow (0);
}

//==============================================================================
void List::selectRow (int index)
{
    const int count { static_cast<int> (rows.size()) };

    int target { juce::jlimit (0, count - 1, index) };

    if (target > 0 and (not rows.at (static_cast<std::size_t> (target))->isSelectable()
                        or not rows.at (static_cast<std::size_t> (target))->isVisible()))
    {
        const int current { getSelectedIndex() };
        const int direction { target >= current ? 1 : -1 };

        while (target > 0 and target < count
               and (not rows.at (static_cast<std::size_t> (target))->isSelectable()
                    or not rows.at (static_cast<std::size_t> (target))->isVisible()))
        {
            target += direction;
        }

        target = juce::jlimit (0, count - 1, target);
    }

    if (target > 0 and (not rows.at (static_cast<std::size_t> (target))->isSelectable()
                        or not rows.at (static_cast<std::size_t> (target))->isVisible()))
    {
        target = 0;
    }

    for (auto& row : rows)
        row->getValueObject().setValue (false);

    if (target >= 0 and target < count)
    {
        rows.at (static_cast<std::size_t> (target))->getValueObject().setValue (true);

        if (target > 0)
        {
            grabKeyboardFocus();

            const int rowY { rows.at (static_cast<std::size_t> (target))->getY() };
            const int viewTop { viewport.getViewPositionY() };
            const int viewBottom { viewTop + viewport.getViewHeight() };

            if (rowY < viewTop)
                viewport.setViewPosition (0, rowY);

            if (rowY + rowHeight > viewBottom)
                viewport.setViewPosition (0, rowY + rowHeight - viewport.getViewHeight());
        }
    }
}

//==============================================================================
void List::executeSelected()
{
    int target { getSelectedIndex() };

    if (target == 0)
    {
        for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
        {
            if (rows.at (static_cast<std::size_t> (i))->isVisible() and target == 0)
                target = i;
        }
    }

    if (target > 0 and target < static_cast<int> (rows.size()))
    {
        if (rows.at (static_cast<std::size_t> (target))->run != nullptr)
            rows.at (static_cast<std::size_t> (target))->run();

        if (lua::Engine::getContext()->display.actionList.closeOnRun)
        {
            if (onActionRun != nullptr)
                onActionRun();
        }
    }
}

//==============================================================================
int List::visibleRowCount() const noexcept
{
    int count { 0 };

    for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows.at (static_cast<std::size_t> (i))->isVisible()
            and rows.at (static_cast<std::size_t> (i))->isSelectable())
            ++count;
    }

    return count;
}

//==============================================================================
int List::getSelectedIndex() const noexcept
{
    int result { 0 };

    for (int i { 0 }; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows.at (static_cast<std::size_t> (i))->isSelected() and result == 0)
            result = i;
    }

    return result;
}

//==============================================================================
int List::getBindingRowIndex() const { return static_cast<int> (state.get().getProperty (bindingRowIndexId)); }

void List::setBindingRowIndex (int index) { state.get().setProperty (bindingRowIndexId, index, nullptr); }

//==============================================================================
void List::enterBindingMode()
{
    const int selected { getSelectedIndex() };

    if (selected > 0)
    {
        setBindingRowIndex (selected);
        messageOverlay.showMessage ("Type key to remap", bindingModeTimeoutMs);
    }
}

void List::exitBindingMode()
{
    setBindingRowIndex (-1);
    jam::Animator::toggleFade (&messageOverlay, false);
    grabKeyboardFocus();
}

bool List::handleBindingKey (const juce::KeyPress& key)
{
    bool handled { false };

    if (getBindingRowIndex() >= 1)
    {
        if (key.getKeyCode() == 'C' and key.getModifiers().isCtrlDown())
        {
            exitBindingMode();
            handled = true;
        }
        else
        {
            const int targetIndex { getBindingRowIndex() };

            if (targetIndex < static_cast<int> (rows.size()))
            {
                auto* registry { Registry::getContext() };
                const auto shortcutString { Registry::shortcutToString (key) };

                if (rows.at (static_cast<std::size_t> (targetIndex))->actionConfigKey.isNotEmpty())
                {
                    luaEngine.patchKey (rows.at (static_cast<std::size_t> (targetIndex))->actionConfigKey, shortcutString);
                    luaEngine.load();
                    luaEngine.buildKeyMap (*registry);
                    state.get().setProperty (bindingsDirtyId, true, nullptr);

                    if (auto* label { rows.at (static_cast<std::size_t> (targetIndex))->getShortcutLabel() };
                        label != nullptr)
                        label->setText (shortcutString, juce::dontSendNotification);
                }
            }

            exitBindingMode();
            handled = true;
        }
    }

    return handled;
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

