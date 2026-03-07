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

#define IDENTIFIER_MATH(X) \
    X(Default,       "default") \
    X(choice,        "choice") \
    X(centre,        "centre") \
    X(minLabel,      "minLabel") \
    X(maxLabel,      "maxLabel") \
    X(floatingPoint, "float") \
    X(integer,       "int") \
    X(boolean,       "bool") \
    X(interval,      "interval") \
    X(A,             "A") \
    X(B,             "B") \
    X(numbers,       "number") \
    X(number,        "numbers") \
    X(taper,         "taper") \
    X(skew,          "skew") \
    X(log1,          "log1") \
    X(log2,          "log2") \
    X(log3,          "log3") \
    X(log4,          "log4") \
    X(log5,          "log5") \
    X(log10,         "log10") \
    X(log15,         "log15") \
    X(log20,         "log20") \
    X(log25,         "log25") \
    X(log30,         "log30") \
    X(log35,         "log35") \
    X(log40,         "log40") \
    X(log45,         "log45") \
    X(linear,        "linear") \
    X(dB,            "dB") \
    X(oversampling,  "oversampling") \
    X(samplerate,    "samplerate") \
    X(blocksize,     "blocksize")


/**_____________________________END OF NAMESPACE______________________________*/
} // namespace jreng
