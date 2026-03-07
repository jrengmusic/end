#pragma once
#include "jreng_identifier_appearance.h"
#include "jreng_identifier_branding.h"
#include "jreng_identifier_core.h"
#include "jreng_identifier_data.h"
#include "jreng_identifier_engine.h"
#include "jreng_identifier_evaluation.h"
#include "jreng_identifier_files.h"
#include "jreng_identifier_math.h"
#include "jreng_identifier_misc.h"
#include "jreng_identifier_layout.h"
#include "jreng_identifier_parameters.h"
#include "jreng_identifier_style.h"
#include "jreng_identifier_svg.h"
#include "jreng_identifier_ui.h"
#include "jreng_identifier_user.h"

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

// Expansion helpers
#define AS_IDENTIFIER(name, str) inline static const juce::Identifier name { str };
#define AS_STRINGREF(name, str)  inline static const juce::StringRef name { str };
#define AS_UPPER(name, str)      inline static const juce::String name { juce::String(str).toUpperCase() };
#define AS_TYPE(name, str)      inline static const juce::Identifier name { juce::String(str).toUpperCase() };

#define MAKE_VIEW(ViewName, EXPANDER) \
struct ViewName { \
    IDENTIFIER_APPEARANCE(EXPANDER) \
    IDENTIFIER_BRANDING(EXPANDER) \
    IDENTIFIER_CORE(EXPANDER) \
    IDENTIFIER_DATA(EXPANDER) \
    IDENTIFIER_ENGINE(EXPANDER) \
    IDENTIFIER_EVALUATION(EXPANDER) \
    IDENTIFIER_FILES(EXPANDER) \
    IDENTIFIER_LAYOUT(EXPANDER) \
    IDENTIFIER_MATH(EXPANDER) \
    IDENTIFIER_MISCELLANEOUS(EXPANDER) \
    IDENTIFIER_PARAMETERS(EXPANDER) \
    IDENTIFIER_STYLE(EXPANDER) \
    IDENTIFIER_SVG(EXPANDER) \
    IDENTIFIER_UI_COMPONENTS(EXPANDER) \
    IDENTIFIER_USER(EXPANDER) \
};

MAKE_VIEW(ID,    AS_IDENTIFIER)
MAKE_VIEW(IDref, AS_STRINGREF)
MAKE_VIEW(IDtag, AS_UPPER)
MAKE_VIEW(IDtype, AS_TYPE)


/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
