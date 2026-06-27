/**
 * @file terminal/Processor.h
 * @brief Terminal engine — AudioProcessor analog. Owns pipeline.
 */
#pragma once
#include <JuceHeader.h>

namespace terminal
{
/*____________________________________________________________________________*/

/** @brief Terminal engine — owns processing pipeline.
 *
 *  AudioProcessor analog in the MVP (Model-View-Processor) pattern.
 *  Will own Model, Parser, Video, CellFifo, and TTY in Phase 4.
 *  Buffer (CodeModel) is owned by Session, not Processor.
 *
 *  Phase 3: lean stub. Correct shape, built layer by layer.
 */
struct Processor
{
    Processor() = default;

    // Phase 4: terminal::Model, Parser, Video, CellFifo, TTY

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
