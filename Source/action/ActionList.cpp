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
List::List (juce::Component& mainWindow, Scripting::Engine& engine)
    : main (mainWindow)
    , scriptingEngine (engine)
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

    const int padH { config.getInt (Config::Key::actionListPaddingLeft)
                     + config.getInt (Config::Key::actionListPaddingRight) };
    const int padV { config.getInt (Config::Key::actionListPaddingTop)
                     + config.getInt (Config::Key::actionListPaddingBottom) };

    const int proportionalWidth { jam::toInt (config.getFloat (Config::Key::actionListWidth)
                                  * main.getWidth() + padH) };
    const int proportionalHeight { jam::toInt (config.getFloat (Config::Key::actionListHeight)
                                   * main.getHeight() + padV) };

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
        scriptingEngine.load();

        if (auto* registry { Action::Registry::getContext() }; registry != nullptr)
            scriptingEngine.buildKeyMap (*registry);
    }
}

//==============================================================================
juce::Colour List::getHighlightColour() const
{
    return Config::getContext()->getColour (Config::Key::actionListHighlightColour);
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

    editor.setColour (
        juce::TextEditor::backgroundColourId,
        config.getColour (Config::Key::windowColour).withAlpha (config.getFloat (Config::Key::windowOpacity)));
    editor.setColour (juce::TextEditor::textColourId, config.getColour (Config::Key::coloursForeground));
    editor.setColour (juce::CaretComponent::caretColourId, config.getColour (Config::Key::coloursCursor));
    editor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    editor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);

    editor.setFont (juce::Font (juce::FontOptions()
                                    .withName (config.getString (Config::Key::fontFamily))
                                    .withPointHeight (config.getFloat (Config::Key::fontSize))));
}

//==============================================================================
void List::configureActionRow (Row& row)
{
    row.highlightColour = getHighlightColour();

    if (auto* name { row.getNameLabel() }; name != nullptr)
        name->setColour (juce::Label::textColourId, config.getColour (Config::Key::actionListNameColour));

    if (auto* shortcut { row.getShortcutLabel() }; shortcut != nullptr)
        shortcut->setColour (juce::Label::textColourId, config.getColour (Config::Key::actionListShortcutColour));
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
                const auto engineShortcut { scriptingEngine.getShortcutString (rows.at (static_cast<std::size_t> (i))->actionConfigKey) };

                if (currentShortcut != engineShortcut)
                {
                    scriptingEngine.patchKey (rows.at (static_cast<std::size_t> (i))->actionConfigKey, currentShortcut);
                    scriptingEngine.load();

                    if (registry != nullptr)
                        scriptingEngine.buildKeyMap (*registry);

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
            row->actionConfigKey = scriptingEngine.getActionLuaKey (entry.id);

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
        prefixEntry.shortcut = Registry::parseShortcut (scriptingEngine.getPrefixString());

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
            row->actionConfigKey = scriptingEngine.getActionLuaKey (entry.id);

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
    bounds.removeFromTop (config.getInt (Config::Key::actionListPaddingTop));
    bounds.removeFromRight (config.getInt (Config::Key::actionListPaddingRight));
    bounds.removeFromBottom (config.getInt (Config::Key::actionListPaddingBottom));
    bounds.removeFromLeft (config.getInt (Config::Key::actionListPaddingLeft));

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

    if (target > 0 and not rows.at (static_cast<std::size_t> (target))->isSelectable())
    {
        const int current { getSelectedIndex() };
        const int direction { target >= current ? 1 : -1 };

        while (target > 0 and target < count and not rows.at (static_cast<std::size_t> (target))->isSelectable())
        {
            target += direction;
        }

        target = juce::jlimit (0, count - 1, target);
    }

    if (target > 0 and not rows.at (static_cast<std::size_t> (target))->isVisible())
        target = 0;

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

        if (config.getBool (Config::Key::actionListCloseOnRun))
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
                    scriptingEngine.patchKey (rows.at (static_cast<std::size_t> (targetIndex))->actionConfigKey, shortcutString);
                    scriptingEngine.load();
                    scriptingEngine.buildKeyMap (*registry);
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

