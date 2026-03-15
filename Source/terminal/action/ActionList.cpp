/**
 * @file ActionList.cpp
 * @brief Implementation of Terminal::ActionList — fuzzy-searchable command palette.
 *
 * @see ActionList.h
 */

#include "ActionList.h"

namespace Terminal
{

//==============================================================================
ActionList::ActionList()
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (false);
    setInterceptsMouseClicks (true, true);

    // Search box setup
    searchBox.setMultiLine (false);
    searchBox.setReturnKeyStartsNewLine (false);
    searchBox.setScrollbarsShown (false);
    searchBox.setPopupMenuEnabled (false);
    searchBox.setTextToShowWhenEmpty ("Type to search...", juce::Colours::grey);
    searchBox.setWantsKeyboardFocus (true);
    searchBox.addListener (this);

    addAndMakeVisible (searchBox);

    // Result list setup — this component is its own model
    resultList.setModel (this);
    resultList.setRowHeight (rowHeight);
    resultList.setOutlineThickness (0);
    resultList.setMultipleSelectionEnabled (false);

    addAndMakeVisible (resultList);

    setVisible (false);
}

//==============================================================================
void ActionList::show (juce::Component& parent)
{
    if (const auto* action = Action::getContext())
    {
        entries = action->getEntries();

        searchDataset.clear();
        searchDataset.reserve (entries.size());

        for (const auto& entry : entries)
            searchDataset.push_back (entry.name.toStdString());
    }

    filterEntries ({});

    const auto* cfg { Config::getContext() };
    const auto position { cfg->getString (Config::Key::popupPosition) };

    setLookAndFeel (&parent.getLookAndFeel());
    parentComponent = &parent;
    parent.addAndMakeVisible (this);
    applyBounds (parent, position);
    setAlwaysOnTop (true);
    toFront (true);
    setVisible (true);
    searchBox.grabKeyboardFocus();
}

//==============================================================================
void ActionList::hide()
{
    searchBox.setText ({}, juce::dontSendNotification);
    filteredIndices.clear();
    resultList.updateContent();
    setVisible (false);
    setAlwaysOnTop (false);
    setLookAndFeel (nullptr);

    if (parentComponent != nullptr)
    {
        parentComponent->removeChildComponent (this);
        parentComponent->grabKeyboardFocus();
        parentComponent = nullptr;
    }
}

//==============================================================================
bool ActionList::isActive() const noexcept
{
    return isVisible();
}

//==============================================================================
void ActionList::resized()
{
    auto bounds { getLocalBounds() };
    searchBox.setBounds (bounds.removeFromTop (searchBoxHeight));
    resultList.setBounds (bounds);
}

//==============================================================================
void ActionList::paint (juce::Graphics& g)
{
    const auto* cfg { Config::getContext() };
    const auto bgColour { cfg->getColour (Config::Key::windowColour) };
    g.fillAll (bgColour.withAlpha (backgroundAlpha));
}

//==============================================================================
bool ActionList::keyPressed (const juce::KeyPress& key)
{
    const auto upKey    { juce::KeyPress (juce::KeyPress::upKey) };
    const auto downKey  { juce::KeyPress (juce::KeyPress::downKey) };
    const auto escKey   { juce::KeyPress (juce::KeyPress::escapeKey) };
    const auto retKey   { juce::KeyPress (juce::KeyPress::returnKey) };

    bool handled { false };

    if (key == downKey)
    {
        const int current { resultList.getSelectedRow() };
        const int next    { juce::jmin (current + 1, getNumRows() - 1) };
        resultList.selectRow (next);
        handled = true;
    }

    if (not handled and key == upKey)
    {
        const int current { resultList.getSelectedRow() };
        const int prev    { juce::jmax (current - 1, 0) };
        resultList.selectRow (prev);
        handled = true;
    }

    if (not handled and key == retKey)
    {
        executeRow (resultList.getSelectedRow());
        handled = true;
    }

    if (not handled and key == escKey)
    {
        hide();
        handled = true;
    }

    return handled;
}

//==============================================================================
// juce::TextEditor::Listener
//==============================================================================

void ActionList::textEditorTextChanged (juce::TextEditor& editor)
{
    filterEntries (editor.getText());
}

//==============================================================================
void ActionList::textEditorReturnKeyPressed (juce::TextEditor&)
{
    executeRow (resultList.getSelectedRow());
}

//==============================================================================
void ActionList::textEditorEscapeKeyPressed (juce::TextEditor&)
{
    hide();
}

//==============================================================================
// juce::ListBoxModel
//==============================================================================

int ActionList::getNumRows()
{
    return static_cast<int> (filteredIndices.size());
}

//==============================================================================
void ActionList::paintListBoxItem (int rowNumber,
                                   juce::Graphics& g,
                                   int width,
                                   int height,
                                   bool rowIsSelected)
{
    const auto* cfg { Config::getContext() };

    if (rowIsSelected)
    {
        const auto selColour { cfg->getColour (Config::Key::coloursSelection) };
        g.fillAll (selColour);
    }

    const bool validRow { rowNumber >= 0
                          and rowNumber < static_cast<int> (filteredIndices.size()) };

    if (validRow)
    {
        const auto& entry { entries.at (static_cast<std::size_t> (filteredIndices.at (static_cast<std::size_t> (rowNumber)))) };

        const auto fgColour { cfg->getColour (Config::Key::coloursForeground) };
        g.setColour (fgColour);

        const juce::Font font { juce::FontOptions (
            cfg->getString (Config::Key::overlayFamily),
            10.0f,
            juce::Font::plain) };
        g.setFont (font);

        const int nameW { static_cast<int> (static_cast<float> (width) * nameColumnFraction) };
        const int descW { static_cast<int> (static_cast<float> (width) * descColumnFraction) };
        const int shortW { width - nameW - descW };

        const juce::Rectangle<int> nameBounds  { columnPadding,
                                                  0,
                                                  nameW - columnPadding,
                                                  height };
        const juce::Rectangle<int> descBounds  { nameW + columnPadding,
                                                  0,
                                                  descW - columnPadding,
                                                  height };
        const juce::Rectangle<int> shortBounds { nameW + descW,
                                                  0,
                                                  shortW - columnPadding,
                                                  height };

        g.drawText (entry.name,
                    nameBounds,
                    juce::Justification::centredLeft,
                    true);

        g.setColour (fgColour.withAlpha (0.7f));
        g.drawText (entry.description,
                    descBounds,
                    juce::Justification::centredLeft,
                    true);

        const juce::String shortcutText { entry.shortcut.isValid()
                                          ? entry.shortcut.getTextDescription()
                                          : juce::String{} };

        g.setColour (fgColour.withAlpha (0.5f));
        g.drawText (shortcutText,
                    shortBounds,
                    juce::Justification::centredRight,
                    true);
    }
}

//==============================================================================
void ActionList::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    executeRow (row);
}

//==============================================================================
// Internal helpers
//==============================================================================

void ActionList::filterEntries (const juce::String& query)
{
    filteredIndices.clear();

    if (query.isEmpty())
    {
        filteredIndices.reserve (entries.size());

        for (int i { 0 }; i < static_cast<int> (entries.size()); ++i)
            filteredIndices.push_back (i);
    }
    else
    {
        const auto results { jreng::FuzzySearch::getResult (query, searchDataset) };

        filteredIndices.reserve (results.size());

        for (const auto& result : results)
        {
            const juce::String matchedName { result.second };

            for (int i { 0 }; i < static_cast<int> (entries.size()); ++i)
            {
                if (entries.at (static_cast<std::size_t> (i)).name == matchedName)
                {
                    filteredIndices.push_back (i);
                    break;
                }
            }
        }
    }

    resultList.updateContent();

    if (not filteredIndices.empty())
        resultList.selectRow (0);
}

//==============================================================================
void ActionList::executeRow (int row)
{
    const bool validRow { row >= 0
                          and row < static_cast<int> (filteredIndices.size()) };

    if (validRow)
    {
        const auto& entry { entries.at (static_cast<std::size_t> (filteredIndices.at (static_cast<std::size_t> (row)))) };

        if (entry.execute != nullptr)
        {
            hide();
            entry.execute();
        }
    }
}

//==============================================================================
void ActionList::applyBounds (juce::Component& parent, const juce::String& position)
{
    const int parentW { parent.getWidth() };
    const int parentH { parent.getHeight() };

    const int paletteW { static_cast<int> (static_cast<float> (parentW) * widthFraction) };
    const int paletteX { (parentW - paletteW) / 2 };

    const int maxListH  { static_cast<int> (static_cast<float> (parentH) * maxListHeightFraction) };
    const int numRows   { static_cast<int> (filteredIndices.size()) };
    const int listH     { juce::jmin (numRows * rowHeight, maxListH) };
    const int totalH    { searchBoxHeight + listH };

    const bool isBottom { position == "bottom" };

    const int paletteY { isBottom ? (parentH - totalH) : 0 };

    setBounds (paletteX, paletteY, paletteW, totalH);
}

} // namespace Terminal
