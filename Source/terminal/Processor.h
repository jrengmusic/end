/**
 * @file terminal/Processor.h
 * @brief Terminal engine — AudioProcessor analog. Owns pipeline.
 */
#pragma once
#include <JuceHeader.h>
#include "terminal/Model.h"

namespace terminal
{
/*____________________________________________________________________________*/

/** @brief Terminal engine — owns processing pipeline.
 *
 *  AudioProcessor analog in the MVP (Model-View-Processor) pattern.
 *  Will own Parser, Video, CellFifo, and TTY. Buffer (CodeModel)
 *  is owned by Session, not Processor.
 *
 *  Holds a reference to Session's terminal::Model — registered as a
 *  jam::Model::Listener for Direction B.
 *  parameterChanged() fires on the CALLING thread (jam_Model.h:36-48), i.e.
 *  the message thread — it is the WAKE seam only, never reader-owned state.
 *  Nudging the TTY poll wake fd completes once the reader thread exists.
 *
 *  Lean stub. Correct shape, built layer by layer.
 */
struct Processor : public jam::Model::Listener
{
    /** @brief Constructs the Processor and registers it as a listener on
     *  @p terminalModel.
     *  @param terminalModel  Session's owned terminal::Model. Must outlive
     *                        this Processor (Session declares model before
     *                        processor — construction/destruction order).
     */
    explicit Processor (Model& terminalModel)
        : model (terminalModel)
    {
        model.addListener (this);
    }

    ~Processor() override
    {
        model.removeListener (this);
    }

    // Parser, Video, CellFifo, TTY

    /** @brief Direction B wake seam.
     *  @note Fires on the MESSAGE thread (jam_Model.h:36-48). Touches no
     *  reader-owned state — nudging the TTY poll wake fd completes this,
     *  once the reader thread exists; the reader consumes the changed
     *  atomic at its loop top.
     */
    void parameterChanged (const juce::Identifier& id, const juce::var& newValue) override
    {
        juce::ignoreUnused (id, newValue);
    }

private:
    Model& model;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
