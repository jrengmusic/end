/**
 * @file ActionList.cpp
 * @brief Command palette — a ModalWindow with search box.
 */

#include "ActionList.h"
#include "../../Gpu.h"

namespace Terminal
{ /*____________________________________________________________________________*/

ActionList::ActionList (juce::Component& caller)
    : jreng::ModalWindow (
        new juce::TextEditor(),
        "",
        true,
        false)
{
    auto* cfg { Config::getContext() };

    setGlass (
        cfg->getColour (Config::Key::windowColour),
        Gpu::resolveOpacity (cfg->getFloat (Config::Key::windowOpacity)),
        cfg->getFloat (Config::Key::windowBlurRadius));

    const int width { static_cast<int> (static_cast<float> (caller.getWidth()) * 0.6f) };

    setSize (width, searchBoxHeight);
    centreAroundComponent (&caller, width, searchBoxHeight);
    setVisible (true);
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

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
