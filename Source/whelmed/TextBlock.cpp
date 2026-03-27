#include "TextBlock.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

TextBlock::TextBlock()
{
    editor.setMultiLine (true, true);
    editor.setReadOnly (true);
    editor.setScrollbarsShown (false);
    editor.setCaretVisible (false);
    editor.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    editor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    editor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    editor.setOpaque (false);
    editor.setBorder (juce::BorderSize<int> (0, 0, 0, 0));
    editor.setIndents (0, 0);
    addAndMakeVisible (editor);
}

void TextBlock::appendStyledText (const juce::String& text,
                                   const juce::FontOptions& fontOptions,
                                   juce::Colour colour)
{
    editor.setFont (fontOptions);
    editor.setColour (juce::TextEditor::textColourId, colour);
    editor.insertTextAtCaret (text);
}

void TextBlock::resized()
{
    editor.setBounds (getLocalBounds());
    preferredHeight = editor.getTextHeight();
}

int TextBlock::getPreferredHeight() const noexcept
{
    return preferredHeight;
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
