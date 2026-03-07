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

#define IDENTIFIER_ENGINE(X) \
    /* Crossover Parameters */ \
    X(CROSSOVER_LOW_MID,      "CROSSOVER_LOW_MID") \
    X(CROSSOVER_MID_HIGH,     "CROSSOVER_MID_HIGH") \
    \
    /* Low Band Parameters */ \
    X(LOW_ATTACK,             "LOW_ATTACK") \
    X(LOW_ATTACK_STRENGTH,    "LOW_ATTACK_STRENGTH") \
    X(LOW_DECAY,              "LOW_DECAY") \
    X(LOW_DECAY_STRENGTH,     "LOW_DECAY_STRENGTH") \
    X(LOW_INPUT_GAIN,         "LOW_INPUT_GAIN") \
    X(LOW_OUTPUT_GAIN,        "LOW_OUTPUT_GAIN") \
    X(LOW_COMPENSATION,       "LOW_COMPENSATION") \
    X(LOW_DETECTION_MODE,     "LOW_DETECTION_MODE") \
    \
    /* Mid Band Parameters */ \
    X(MID_ATTACK,             "MID_ATTACK") \
    X(MID_ATTACK_STRENGTH,    "MID_ATTACK_STRENGTH") \
    X(MID_DECAY,              "MID_DECAY") \
    X(MID_DECAY_STRENGTH,     "MID_DECAY_STRENGTH") \
    X(MID_INPUT_GAIN,         "MID_INPUT_GAIN") \
    X(MID_OUTPUT_GAIN,        "MID_OUTPUT_GAIN") \
    X(MID_COMPENSATION,       "MID_COMPENSATION") \
    X(MID_DETECTION_MODE,     "MID_DETECTION_MODE") \
    \
    /* High Band Parameters */ \
    X(HIGH_ATTACK,            "HIGH_ATTACK") \
    X(HIGH_ATTACK_STRENGTH,   "HIGH_ATTACK_STRENGTH") \
    X(HIGH_DECAY,             "HIGH_DECAY") \
    X(HIGH_DECAY_STRENGTH,    "HIGH_DECAY_STRENGTH") \
    X(HIGH_INPUT_GAIN,        "HIGH_INPUT_GAIN") \
    X(HIGH_OUTPUT_GAIN,       "HIGH_OUTPUT_GAIN") \
    X(HIGH_COMPENSATION,      "HIGH_COMPENSATION") \
    X(HIGH_DETECTION_MODE,    "HIGH_DETECTION_MODE")

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
