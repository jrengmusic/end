/**
 * @file ActionList.cpp
 * @brief Command palette — modal glass window with fuzzy search and action list.
 */

#include "ActionList.h"
#include "../Gpu.h"

namespace Action
{ /*____________________________________________________________________________*/

List::List (juce::Component& caller)
    : jreng::ModalWindow (new juce::Component(), "Action List", true, false)
{
    setLookAndFeel (&lookAndFeel);

    auto* cfg { Config::getContext() };
    auto* content { getContentComponent() };

    content->addAndMakeVisible (searchBox);
    setupSearchBox();

    content->addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rowContainer, false);
    viewport.setScrollBarsShown (true, false);

    content->addAndMakeVisible (remapDialog);

    buildRows();

    searchBox.onTextChange = [this] { filterRows (searchBox.getText()); };

    remapDialog.onCommit = [this] (const juce::String& shortcut, bool /*isModal*/)
    {
        if (activeRemapRow != nullptr and activeRemapRow->actionConfigKey.isNotEmpty())
        {
            Config::getContext()->patchKey (activeRemapRow->actionConfigKey, shortcut);
            Config::getContext()->reload();
        }

        activeRemapRow = nullptr;
        closeButtonPressed();
    };

    remapDialog.onCancel = [this]
    {
        activeRemapRow = nullptr;
        remapDialog.setVisible (false);
        searchBox.grabKeyboardFocus();
    };

    setGlass (
        cfg->getColour (Config::Key::windowColour),
        Gpu::resolveOpacity (cfg->getFloat (Config::Key::windowOpacity)),
        cfg->getFloat (Config::Key::windowBlurRadius));

    const int padH { cfg->getInt (Config::Key::actionListPaddingLeft)
                   + cfg->getInt (Config::Key::actionListPaddingRight) };
    const int padV { cfg->getInt (Config::Key::actionListPaddingTop)
                   + cfg->getInt (Config::Key::actionListPaddingBottom) };

    const int width { static_cast<int> (static_cast<float> (caller.getWidth()) * 0.6f) + padH };
    const int visibleRows { juce::jmin (visibleRowCount(), maxVisibleRows) };
    const int height { searchBoxHeight + visibleRows * rowHeight + padV };

    setSize (width, height);
    centreAroundComponent (&caller, width, height);
    setVisible (true);
    enterModalState (true);

    searchBox.grabKeyboardFocus();
}

List::~List()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void List::setupSearchBox()
{
    auto* cfg { Config::getContext() };

    searchBox.setMultiLine (false);
    searchBox.setReturnKeyStartsNewLine (false);
    searchBox.setScrollbarsShown (false);
    searchBox.setPopupMenuEnabled (false);
    searchBox.setTextToShowWhenEmpty ("Type to search...", juce::Colours::grey);
    searchBox.setWantsKeyboardFocus (true);

    searchBox.setColour (juce::TextEditor::backgroundColourId,
        cfg->getColour (Config::Key::windowColour)
            .withAlpha (cfg->getFloat (Config::Key::windowOpacity)));
    searchBox.setColour (juce::TextEditor::textColourId,
        cfg->getColour (Config::Key::coloursForeground));
    searchBox.setColour (juce::CaretComponent::caretColourId,
        cfg->getColour (Config::Key::coloursCursor));
    searchBox.setColour (juce::TextEditor::outlineColourId,
        juce::Colours::transparentBlack);
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId,
        juce::Colours::transparentBlack);

    searchBox.setFont (juce::Font (juce::FontOptions()
        .withName (cfg->getString (Config::Key::fontFamily))
        .withPointHeight (cfg->getFloat (Config::Key::fontSize))));
}

//==============================================================================
void List::handleShortcutClicked (Row* clickedRow)
{
    if (clickedRow != nullptr and clickedRow->actionConfigKey.isNotEmpty())
    {
        activeRemapRow = clickedRow;

        bool isModal { false };
        auto* registry { Registry::getContext() };
        const auto& entries { registry->getEntries() };
        bool found { false };

        for (const auto& entry : entries)
        {
            if (not found and entry.name == clickedRow->nameLabel.getText())
            {
                isModal = entry.isModal;
                found = true;
            }
        }

        remapDialog.show (clickedRow->shortcutLabel.getText(),
                          isModal,
                          clickedRow->nameLabel.getText());
    }
}

//==============================================================================
void List::buildRows()
{
    auto* registry { Registry::getContext() };
    jassert (registry != nullptr);

    const auto& entries { registry->getEntries() };

    for (const auto& entry : entries)
    {
        const juce::String uuid { juce::Uuid().toString() };
        auto row { std::make_unique<Row> (entry, uuid) };

        row->onExecuted = [this] (Row*)
        {
            if (Config::getContext()->getBool (Config::Key::keysActionListCloseOnRun))
                closeButtonPressed();
        };

        row->onShortcutClicked = [this] (Row* clickedRow) { handleShortcutClicked (clickedRow); };

        rowContainer.addAndMakeVisible (row.get());
        rows.push_back (std::move (row));
    }

    layoutRows();
}

//==============================================================================
void List::filterRows (const juce::String& query)
{
    if (query.isEmpty())
    {
        for (auto& row : rows)
            row->setVisible (true);
    }
    else
    {
        // Build dataset of action names for fuzzy search.
        jreng::FuzzySearch::Data::vector dataset;
        dataset.reserve (rows.size());

        for (const auto& row : rows)
            dataset.push_back (row->nameLabel.getText().toStdString());

        const auto results { jreng::FuzzySearch::getResult (query, dataset) };

        // Build a set of matched names for O(1) lookup.
        std::unordered_set<std::string> matchedNames;

        for (const auto& result : results)
            matchedNames.insert (result.second);

        for (auto& row : rows)
        {
            const bool isMatch { matchedNames.count (row->nameLabel.getText().toStdString()) > 0 };
            row->setVisible (isMatch);
        }
    }

    selectedIndex = -1;
    layoutRows();

    // Auto-select first visible row if any exist.
    if (visibleRowCount() > 0)
        selectRow (0);
}

//==============================================================================
void List::layoutRows()
{
    const int rowWidth { viewport.getMaximumVisibleWidth() };
    int yPos { 0 };

    for (auto& row : rows)
    {
        if (row->isVisible())
        {
            row->setBounds (0, yPos, rowWidth, rowHeight);
            yPos += rowHeight;
        }
    }

    rowContainer.setSize (rowWidth, yPos);
}

//==============================================================================
void List::selectRow (int index)
{
    const int total { visibleRowCount() };

    if (total > 0)
    {
        int targetVisible { index };

        if (targetVisible < 0)
            targetVisible = total - 1;

        if (targetVisible >= total)
            targetVisible = 0;

        // Deselect all rows.
        for (auto& row : rows)
            row->setAlpha (1.0f);

        // Find and highlight the target visible row.
        int visibleCount { 0 };

        for (auto& row : rows)
        {
            if (row->isVisible())
            {
                if (visibleCount == targetVisible)
                {
                    row->setAlpha (0.7f);
                    selectedIndex = targetVisible;

                    // Ensure selected row is visible in viewport.
                    viewport.setViewPosition (0,
                        juce::jmax (0, row->getY() - viewport.getHeight() / 2));
                }

                ++visibleCount;
            }
        }
    }
    else
    {
        selectedIndex = -1;
    }
}

//==============================================================================
void List::executeSelected()
{
    if (selectedIndex >= 0)
    {
        int visibleCount { 0 };
        bool found { false };

        for (auto& row : rows)
        {
            if (row->isVisible() and not found)
            {
                if (visibleCount == selectedIndex)
                {
                    found = true;

                    if (row->run != nullptr)
                        row->run();

                    if (Config::getContext()->getBool (Config::Key::keysActionListCloseOnRun))
                        closeButtonPressed();
                }

                ++visibleCount;
            }
        }
    }
}

//==============================================================================
int List::visibleRowCount() const noexcept
{
    int count { 0 };

    for (const auto& row : rows)
    {
        if (row->isVisible())
            ++count;
    }

    return count;
}

//==============================================================================
bool List::keyPressed (const juce::KeyPress& key)
{
    bool handled { false };

    if (remapDialog.isVisible() and remapDialog.isLearning())
    {
        handled = remapDialog.handleLearnKey (key);
    }
    else if (remapDialog.isVisible() and key == juce::KeyPress::escapeKey)
    {
        remapDialog.onCancel();
        handled = true;
    }
    else if (key == juce::KeyPress::escapeKey)
    {
        handled = jreng::ModalWindow::keyPressed (key);
    }
    else if (key == juce::KeyPress (juce::KeyPress::upKey))
    {
        selectRow (selectedIndex - 1);
        handled = true;
    }
    else if (key == juce::KeyPress (juce::KeyPress::downKey))
    {
        selectRow (selectedIndex + 1);
        handled = true;
    }
    else if (key == juce::KeyPress (juce::KeyPress::returnKey))
    {
        executeSelected();
        handled = true;
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

        searchBox.setBounds (bounds.removeFromTop (searchBoxHeight));
        viewport.setBounds (bounds);
        remapDialog.setBounds (content->getLocalBounds());
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
