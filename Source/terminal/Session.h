/**
 * @file terminal/Session.h
 * @brief Terminal session — DAW host analog. Owns document, model, and engine.
 */
#pragma once
#include <JuceHeader.h>
#include "terminal/Model.h"
#include "terminal/Processor.h"

namespace terminal
{
/*____________________________________________________________________________*/

/** @brief Terminal session — DAW host per terminal instance.
 *
 *  Owns the document buffer (TextModel), the VT state SSOT (terminal::Model),
 *  and the processing engine (Processor). Nexus
 *  owns Sessions. terminal::View holds a Session reference and parents a
 *  jam::CodeView that renders the owned document.
 *
 *  Construction order: @c model before @c processor — Processor holds a
 *  reference to @c model and registers itself as a jam::Model::Listener in
 *  its constructor.
 *
 *  Phase 4: Resizer, TTY lifecycle, attachInto.
 */
struct Session
{
    explicit Session()
        : processor (model)
    {
        jam::Stamp::getInstance()->addIfNotAlreadyThere (jam::Stamp::Entry {});
    }

    /** @brief Returns the owned TextModel. View drains into this. */
    jam::TextModel& getDocument() noexcept { return document; }

    /** @brief Returns the owned terminal::Model (SSOT). */
    Model& getModel() noexcept { return model; }

    /** @brief Returns the owned Processor. */
    Processor& getProcessor() noexcept { return processor; }

    // Phase 4: start(), stop(), attachInto(), drain()
    // Phase 4: unique_ptr<jam::Resizer> resizer

private:
    /** @brief Document SSOT — 2 screens (normal + alternate). */
    jam::TextModel document { 2 };

    /** @brief VT state SSOT — constructed before @c processor. */
    Model model;

    /** @brief Terminal engine — AudioProcessor analog. Registers as a
     *  jam::Model::Listener on @c model at construction. */
    Processor processor;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
