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

#define IDENTIFIER_PARAMETERS(X) \
    X(parameter,   "parameter") \
    X(param,       "param") \
    X(group,       "group") \
    X(value,       "value") \
    X(state,       "state") \
    X(onState,     "onState") \
    X(bypass,      "bypass") \
    X(master,      "master") \
    X(single,      "single") \
    X(equaliser,   "equaliser") \
    X(versionHint, "versionHint") \
    X(unit,        "unit") \
    X(min,         "min") \
    X(max,         "max")

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
