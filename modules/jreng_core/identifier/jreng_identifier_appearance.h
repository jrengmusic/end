#pragma once

namespace jreng
{
/*____________________________________________________________________________*/
// ============================================================================
// MACROS are evil. Yet, in the pursuit of one source of truth for convenience
// and consistency in the land of C++, this necessary horror spares us from
// repeating ourselves. May God forgive our sins.
//
// JRENG!
// ============================================================================

#define IDENTIFIER_APPEARANCE(X) \
    X(appearance,      "appearance") \
    X(baseline,        "baseline") \
    X(colour,          "colour") \
    X(colours,         "colours") \
    X(common,          "common") \
    X(bold,            "bold") \
    X(dark,            "dark") \
    X(light,           "light") \
    X(kerning,         "kerning") \
    X(lookAndFeel,     "LookAndFeel") \
    X(maxHeight,       "max_height") \
    X(maxWidth,        "max_width") \
    X(scrollingText,   "scrollingText") \
    X(scrambledText,   "scrambledText") \
    X(size,            "size") \
    X(title,           "title") \
    X(fonts,           "fonts") \
    X(customStyleSheet,"customstylesheet")


/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
