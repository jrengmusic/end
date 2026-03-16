/**
 * @file ActionList.cpp
 * @brief Command palette — a GlassWindow with search box.
 */

#include "ActionList.h"

namespace Terminal
{ /*____________________________________________________________________________*/

ActionList::ActionList (juce::Component& caller)
    : jreng::GlassWindow (
        new juce::TextEditor(),
        "",
        Config::getContext()->getColour (Config::Key::windowColour),
        Config::getContext()->getFloat (Config::Key::windowOpacity),
        Config::getContext()->getFloat (Config::Key::windowBlurRadius),
        true,
        false)
{
    const int width { static_cast<int> (static_cast<float> (caller.getWidth()) * 0.6f) };

    setSize (width, searchBoxHeight);
    centreAroundComponent (&caller, width, searchBoxHeight);
    enterModalState (true);

    if (auto* editor { dynamic_cast<juce::TextEditor*> (getContentComponent()) })
    {
        editor->setMultiLine (false);
        editor->setReturnKeyStartsNewLine (false);
        editor->setScrollbarsShown (false);
        editor->setPopupMenuEnabled (false);
        editor->setTextToShowWhenEmpty ("Type to search...", juce::Colours::grey);
        editor->setWantsKeyboardFocus (true);

        // Set explicit colours so the editor is legible against the glass backdrop.
        // widgetBackground is intentionally opaque so the caret and selection are
        // visible; text and caret colours are pulled from the terminal theme.
        editor->setColour (juce::TextEditor::backgroundColourId,
            Config::getContext()->getColour (Config::Key::windowColour)
                .withAlpha (Config::getContext()->getFloat (Config::Key::windowOpacity)));
        editor->setColour (juce::TextEditor::textColourId,
            Config::getContext()->getColour (Config::Key::coloursForeground));
        editor->setColour (juce::CaretComponent::caretColourId,
            Config::getContext()->getColour (Config::Key::coloursCursor));
        editor->setColour (juce::TextEditor::outlineColourId,
            juce::Colours::transparentBlack);
        editor->setColour (juce::TextEditor::focusedOutlineColourId,
            juce::Colours::transparentBlack);

        editor->grabKeyboardFocus();
    }
}

void ActionList::closeButtonPressed()
{
    exitModalState (0);
}

bool ActionList::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        exitModalState (0);
        return true;
    }

    return false;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
