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

#define IDENTIFIER_USER(X) \
    X(user,    "user") \
    X(email,   "email") \
    X(support, "support")

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace jreng
