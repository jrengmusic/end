#pragma once
#include <JuceHeader.h>

namespace Whelmed
{ /*____________________________________________________________________________*/

/** Tokenizes code and returns a styled AttributedString with syntax colours.
    Uses JUCE's CodeTokeniser standalone (no CodeEditorComponent).
    Unknown languages return plain monospace text with default code colour. */
juce::AttributedString tokenize (const juce::String& code,
                                  const juce::String& language);

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
