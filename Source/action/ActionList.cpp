/**
 * @file ActionList.cpp
 * @brief Command palette — modal glass window with fuzzy search and action list.
 */

#include "ActionList.h"
#include "../Gpu.h"

namespace Action
{ /*____________________________________________________________________________*/

const juce::Identifier List::bindingRowIndexId { "bindingRowIndex" };
const juce::Identifier List::bindingsDirtyId   { "bindingsDirty" };

List::List (juce::Component& caller)
    : jreng::ModalWindow (new juce::Component(), "Action List", true, false)
    , callerRef (caller)
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);

    auto* cfg { Config::getContext() };
    auto* content { getContentComponent() };

    content->addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rowContainer, false);
    viewport.setScrollBarsShown (true, false);

    content->addChildComponent (messageOverlay);

    state.get().setProperty (bindingRowIndexId, -1, nullptr);
    state.get().setProperty (bindingsDirtyId, false, nullptr);

    buildRows();

    setGlass (
        cfg->getColour (Config::Key::windowColour),
        Gpu::resolveOpacity (cfg->getFloat (Config::Key::windowOpacity)),
        cfg->getFloat (Config::Key::windowBlurRadius));

    // Fixed window size from config proportions.
    const int padH { cfg->getInt (Config::Key::actionListPaddingLeft)
                   + cfg->getInt (Config::Key::actionListPaddingRight) };
    const int padV { cfg->getInt (Config::Key::actionListPaddingTop)
                   + cfg->getInt (Config::Key::actionListPaddingBottom) };

    const int width { static_cast<int> (static_cast<float> (caller.getWidth())
                    * cfg->getFloat (Config::Key::actionListWidth)) + padH };
    const int height { static_cast<int> (static_cast<float> (caller.getHeight())
                     * cfg->getFloat (Config::Key::actionListHeight)) + padV };

    setSize (width, height);
    centreAroundComponent (&caller, width, height);
    setVisible (true);
    enterModalState (true);

    selectRow (0);

    keyHandler.emplace (KeyHandler::Callbacks {
        [this]      { executeSelected(); },
        [this]      { jreng::ModalWindow::keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)); },
        [this]      { enterBindingMode(); },
        [this] (int i) { selectRow (i); },
        [this]      { return visibleRowCount(); },
        [this]      { return getSelectedIndex(); }
    });
}

List::~List()
{
    if (static_cast<bool> (state.get().getProperty (bindingsDirtyId)))
    {
        Config::getContext()->reload();
    }

    setLookAndFeel (nullptr);
}

//==============================================================================
juce::Colour List::getHighlightColour() const
{
    return Config::getContext()->getColour (Config::Key::actionListHighlightColour);
}

//==============================================================================
void List::configureSearchBox (juce::TextEditor& editor)
{
    auto* cfg { Config::getContext() };

    editor.setMultiLine (false);
    editor.setReturnKeyStartsNewLine (false);
    editor.setScrollbarsShown (false);
    editor.setPopupMenuEnabled (false);
    editor.setTextToShowWhenEmpty ("Type to search...", juce::Colours::grey);
    editor.setWantsKeyboardFocus (true);
    editor.setEscapeAndReturnKeysConsumed (false);

    editor.setColour (juce::TextEditor::backgroundColourId,
        cfg->getColour (Config::Key::windowColour)
            .withAlpha (cfg->getFloat (Config::Key::windowOpacity)));
    editor.setColour (juce::TextEditor::textColourId,
        cfg->getColour (Config::Key::coloursForeground));
    editor.setColour (juce::CaretComponent::caretColourId,
        cfg->getColour (Config::Key::coloursCursor));
    editor.setColour (juce::TextEditor::outlineColourId,
        juce::Colours::transparentBlack);
    editor.setColour (juce::TextEditor::focusedOutlineColourId,
        juce::Colours::transparentBlack);

    editor.setFont (juce::Font (juce::FontOptions()
        .withName (cfg->getString (Config::Key::fontFamily))
        .withPointHeight (cfg->getFloat (Config::Key::fontSize))));
}

//==============================================================================
void List::configureActionRow (Row& row)
{
    auto* cfg { Config::getContext() };

    row.highlightColour = getHighlightColour();

    if (auto* name { row.getNameLabel() }; name != nullptr)
        name->setColour (juce::Label::textColourId,
                         cfg->getColour (Config::Key::actionListNameColour));

    if (auto* shortcut { row.getShortcutLabel() }; shortcut != nullptr)
        shortcut->setColour (juce::Label::textColourId,
                             cfg->getColour (Config::Key::actionListShortcutColour));
}

//==============================================================================
void List::handleValueChanged()
{
    auto* cfg      { Config::getContext() };
    auto* registry { Action::Registry::getContext() };

    for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows.at (static_cast<std::size_t> (i))->actionConfigKey.isNotEmpty())
        {
            if (auto* label { rows.at (static_cast<std::size_t> (i))->getShortcutLabel() }; label != nullptr)
            {
                const auto currentShortcut { label->getText() };
                const auto configShortcut  { cfg->getString (rows.at (static_cast<std::size_t> (i))->actionConfigKey) };

                if (currentShortcut != configShortcut)
                {
                    cfg->patchKey (rows.at (static_cast<std::size_t> (i))->actionConfigKey, currentShortcut);
                    registry->buildKeyMap();
                    state.get().setProperty (bindingsDirtyId, true, nullptr);
                }
            }
        }
    }
}

//==============================================================================
void List::buildRows()
{
    auto* content { getContentComponent() };
    auto* registry { Registry::getContext() };
    jassert (registry != nullptr);

    // Row 0: search box.
    {
        const juce::String uuid { juce::Uuid().toString() };
        auto row { std::make_unique<Row> (0, uuid) };

        auto* searchBox { row->getSearchBox() };
        jassert (searchBox != nullptr);

        configureSearchBox (*searchBox);
        searchBox->onTextChange = [this, searchBox] { filterRows (searchBox->getText()); };

        juce::ValueTree node { "ACTION" };
        node.setProperty (jreng::ID::id, uuid, nullptr);
        state.get().appendChild (node, nullptr);
        jreng::ValueTree::attach (state, row.get());

        content->addAndMakeVisible (row.get());
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

            configureActionRow (*row);

            juce::ValueTree node { "ACTION" };
            node.setProperty (jreng::ID::id, uuid, nullptr);
            state.get().appendChild (node, nullptr);
            jreng::ValueTree::attach (state, row.get());

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
        node.setProperty (jreng::ID::id, uuid, nullptr);
        state.get().appendChild (node, nullptr);
        jreng::ValueTree::attach (state, row.get());

        rowContainer.addAndMakeVisible (row.get());
        rows.push_back (std::move (row));
    }

    // Prefix key row.
    {
        auto* cfg { Config::getContext() };
        const juce::String uuid { juce::Uuid().toString() };

        Registry::Entry prefixEntry;
        prefixEntry.id = "prefix";
        prefixEntry.name = "Prefix";
        prefixEntry.shortcut = Registry::parseShortcut (cfg->getString (Config::Key::keysPrefix));

        auto row { std::make_unique<Row> (static_cast<int> (rows.size()), uuid, prefixEntry) };
        row->actionConfigKey = Config::Key::keysPrefix;

        configureActionRow (*row);

        juce::ValueTree node { "ACTION" };
        node.setProperty (jreng::ID::id, uuid, nullptr);
        state.get().appendChild (node, nullptr);
        jreng::ValueTree::attach (state, row.get());

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

            configureActionRow (*row);

            juce::ValueTree node { "ACTION" };
            node.setProperty (jreng::ID::id, uuid, nullptr);
            state.get().appendChild (node, nullptr);
            jreng::ValueTree::attach (state, row.get());

            rowContainer.addAndMakeVisible (row.get());
            rows.push_back (std::move (row));
        }
    }

    state.onValueChanged = [this] { handleValueChanged(); };

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
            // Simple containsIgnoreCase check — FuzzySearch integration preserved below.
            if (rows.at (static_cast<std::size_t> (i))->getKind() != RowKind::separator)
                rows.at (static_cast<std::size_t> (i))->setVisible (false);
        }
    }

    if (query.isNotEmpty())
    {
        jreng::FuzzySearch::Data::vector dataset;
        dataset.reserve (rows.size() - 1);

        for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
        {
            if (auto* label { rows.at (static_cast<std::size_t> (i))->getNameLabel() }; label != nullptr)
                dataset.push_back (label->getText().toStdString());
        }

        const auto results { jreng::FuzzySearch::getResult (query, dataset) };

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
void List::layoutRows()
{
    const int rowWidth { viewport.getWidth() - viewport.getScrollBarThickness() };
    int yPos { 0 };

    for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows.at (static_cast<std::size_t> (i))->isVisible())
        {
            const int height { rows.at (static_cast<std::size_t> (i))->getKind() == RowKind::separator
                                   ? separatorRowHeight : rowHeight };
            rows.at (static_cast<std::size_t> (i))->setBounds (0, yPos, rowWidth, height);
            yPos += height;
        }
    }

    rowContainer.setSize (rowWidth, yPos);
}

//==============================================================================
void List::selectRow (int index)
{
    const int count { static_cast<int> (rows.size()) };

    int target { juce::jlimit (0, count - 1, index) };

    // Skip non-selectable rows (separators) in the direction of movement.
    if (target > 0 and not rows.at (static_cast<std::size_t> (target))->isSelectable())
    {
        const int current { getSelectedIndex() };
        const int direction { target >= current ? 1 : -1 };

        while (target > 0 and target < count
               and not rows.at (static_cast<std::size_t> (target))->isSelectable())
        {
            target += direction;
        }

        target = juce::jlimit (0, count - 1, target);
    }

    // If target row (beyond 0) is hidden, stay at 0.
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

    // Row 0 = search. Enter from search executes first visible action.
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

        if (Config::getContext()->getBool (Config::Key::keysActionListCloseOnRun))
            closeButtonPressed();
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
int List::getBindingRowIndex() const
{
    return static_cast<int> (state.get().getProperty (bindingRowIndexId));
}

void List::setBindingRowIndex (int index)
{
    state.get().setProperty (bindingRowIndexId, index, nullptr);
}

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
    jreng::Animator::toggleFade (&messageOverlay, false);
    grabKeyboardFocus();
}

bool List::handleBindingKey (const juce::KeyPress& key)
{
    bool handled { false };

    if (getBindingRowIndex() >= 1)
    {
        // Ctrl+C cancels binding mode.
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
                auto* cfg { Config::getContext() };
                auto* registry { Registry::getContext() };
                const auto shortcutString { Registry::shortcutToString (key) };

                if (rows.at (static_cast<std::size_t> (targetIndex))->actionConfigKey.isNotEmpty())
                {
                    cfg->patchKey (rows.at (static_cast<std::size_t> (targetIndex))->actionConfigKey, shortcutString);
                    registry->buildKeyMap();
                    state.get().setProperty (bindingsDirtyId, true, nullptr);

                    if (auto* label { rows.at (static_cast<std::size_t> (targetIndex))->getShortcutLabel() }; label != nullptr)
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

//==============================================================================
void List::resized()
{
    juce::DocumentWindow::resized();

    if (auto* content { getContentComponent() }; content != nullptr)
    {
        auto* cfg { Config::getContext() };

        auto bounds { content->getLocalBounds() };
        bounds.removeFromTop (cfg->getInt (Config::Key::actionListPaddingTop));
        bounds.removeFromRight (cfg->getInt (Config::Key::actionListPaddingRight));
        bounds.removeFromBottom (cfg->getInt (Config::Key::actionListPaddingBottom));
        bounds.removeFromLeft (cfg->getInt (Config::Key::actionListPaddingLeft));

        // Row 0 fixed at top, outside viewport.
        if (not rows.empty())
            rows.at (0)->setBounds (bounds.removeFromTop (rowHeight));

        viewport.setBounds (bounds);
        messageOverlay.setBounds (content->getLocalBounds());
        layoutRows();
    }
}

//==============================================================================
int List::getDesktopWindowStyleFlags() const
{
    return juce::ComponentPeer::windowIsTemporary
         | juce::ComponentPeer::windowHasTitleBar
         | juce::ComponentPeer::windowHasDropShadow;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Action
