#include "CodeBlock.h"
#include "config/Config.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

CodeBlock::CodeBlock (const juce::String& code, const juce::String& language)
{
    // Select tokeniser based on language
    if (language == "cpp" or language == "c" or language == "h" or language == "cc" or language == "cxx")
        tokeniser = std::make_unique<juce::CPlusPlusCodeTokeniser>();
    else if (language == "lua")
        tokeniser = std::make_unique<juce::LuaTokeniser>();
    else if (language == "xml" or language == "html" or language == "svg")
        tokeniser = std::make_unique<juce::XmlTokeniser>();

    editor = std::make_unique<juce::CodeEditorComponent> (document, tokeniser.get());
    editor->setReadOnly (true);
    editor->setLineNumbersShown (false);
    editor->setScrollbarThickness (0);

    // Apply config
    const auto* config { Whelmed::Config::getContext() };
    const juce::String fontFamily { config->getString (Whelmed::Config::Key::codeFamily) };
    const float fontSize { config->getFloat (Whelmed::Config::Key::codeSize) };
    editor->setFont (juce::Font (juce::FontOptions().withName (fontFamily).withPointHeight (fontSize).withStyle ("Book")));

    const auto bgColour { config->getColour (Whelmed::Config::Key::codeFenceBackground) };
    editor->setColour (juce::CodeEditorComponent::backgroundColourId, bgColour);
    editor->setColour (juce::CodeEditorComponent::lineNumberBackgroundId, bgColour);

    const auto textColour { config->getColour (Whelmed::Config::Key::bodyColour) };
    editor->setColour (juce::CodeEditorComponent::defaultTextColourId, textColour);
    editor->setColour (juce::CaretComponent::caretColourId, juce::Colours::transparentBlack);

    // Build colour scheme from config
    juce::CodeEditorComponent::ColourScheme colourScheme;
    colourScheme.set ("Error",             config->getColour (Whelmed::Config::Key::tokenError));
    colourScheme.set ("Comment",           config->getColour (Whelmed::Config::Key::tokenComment));
    colourScheme.set ("Keyword",           config->getColour (Whelmed::Config::Key::tokenKeyword));
    colourScheme.set ("Operator",          config->getColour (Whelmed::Config::Key::tokenOperator));
    colourScheme.set ("Identifier",        config->getColour (Whelmed::Config::Key::tokenIdentifier));
    colourScheme.set ("Integer",           config->getColour (Whelmed::Config::Key::tokenInteger));
    colourScheme.set ("Float",             config->getColour (Whelmed::Config::Key::tokenFloat));
    colourScheme.set ("String",            config->getColour (Whelmed::Config::Key::tokenString));
    colourScheme.set ("Bracket",           config->getColour (Whelmed::Config::Key::tokenBracket));
    colourScheme.set ("Punctuation",       config->getColour (Whelmed::Config::Key::tokenPunctuation));
    colourScheme.set ("Preprocessor Text", config->getColour (Whelmed::Config::Key::tokenPreprocessor));
    editor->setColourScheme (colourScheme);

    document.replaceAllContent (code);

    addAndMakeVisible (editor.get());
}

CodeBlock::~CodeBlock() = default;

void CodeBlock::paint (juce::Graphics& g)
{
    const auto bgColour { Whelmed::Config::getContext()->getColour (Whelmed::Config::Key::codeFenceBackground) };
    g.fillAll (bgColour);
}

void CodeBlock::resized()
{
    editor->setBounds (getLocalBounds());

    const int lineHeight { editor->getLineHeight() };
    const int lineCount { document.getNumLines() };
    preferredHeight = (lineCount * lineHeight) + kVerticalPadding;
}

int CodeBlock::getPreferredHeight() const noexcept
{
    return preferredHeight;
}

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
