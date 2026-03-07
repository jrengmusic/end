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

#define IDENTIFIER_DATA(X) \
    X(data,         "data") \
    X(table_data,   "table_data") \
    X(table,        "table") \
    X(item,         "item") \
    X(headers,      "headers") \
    X(choices,      "choices") \
    X(idx,          "idx") \
    X(list,         "list") \


/**_____________________________END OF NAMESPACE______________________________*/
} // namespace jreng
