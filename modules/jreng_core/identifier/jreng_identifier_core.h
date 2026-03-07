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

#define IDENTIFIER_CORE(X) \
    X(attribute,   "attribute") \
    X(info,        "info") \
    X(init,        "init") \
    X(interface,   "interface") \
    X(isDirty,     "isDirty") \
    X(mode,        "mode") \
    X(name,        "name") \
    X(page,        "page") \
    X(project,     "project") \
    X(prompt,      "prompt") \
    X(settings,    "settings") \
    X(type,        "type") \
    X(whitespace,  "whitespace") \
    X(drawing,     "drawing") \
    X(boilerplate, "boilerplate")


/**_____________________________END OF NAMESPACE______________________________*/
} // namespace jreng
