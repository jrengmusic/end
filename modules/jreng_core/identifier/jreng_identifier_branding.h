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

#define IDENTIFIER_BRANDING(X) \
    X(companyName,      "companyName") \
    X(copyright,        "copyright") \
    X(daw,              "daw") \
    X(developer,        "developer") \
    X(jreng,            "JRENG!") \
    X(kuassa,           "kuassa") \
    X(legalCompanyName, "legalCompanyName") \
    X(logo,             "logo") \
    X(product,          "product") \
    X(productName,      "productName") \
    X(trademark,        "trademark") \
    X(version,          "version") \
    X(versionString,    "versionString") \
    X(website,          "website") \
    X(year,             "year")


/**_____________________________END OF NAMESPACE______________________________*/
} // namespace jreng
