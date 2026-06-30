/**
 * @file terminal/Session.h
 * @brief Terminal session — DAW host analog. Owns document, view, and engine.
 */
#pragma once
#include <JuceHeader.h>
#include "terminal/Processor.h"

namespace terminal
{
/*____________________________________________________________________________*/

/** @brief Terminal session — DAW host per terminal instance.
 *
 *  Owns the document buffer (CodeModel) and processing engine (Processor).
 *  Nexus owns Sessions. terminal::View holds Session reference.
 *
 *  STUB: CodeView removed — glyph pipeline (jam::Font, glyph::Arrangement,
 *  glyph::Graphics) deleted. No terminal content rendered until new pipeline
 *  is wired.
 *
 *  Phase 4: terminal::Model, Resizer, TTY lifecycle, graftInto.
 */
struct Session
{
    explicit Session()
    {
        jam::Stamp::getInstance()->addIfNotAlreadyThere (jam::Stamp::Entry {});
    }

    // STUB: getTextEditor() removed — jam::CodeView depends on deleted glyph pipeline.

    /** @brief Returns the owned CodeModel. View drains into this. */
    jam::CodeModel& getDocument() noexcept { return document; }

    /** @brief Returns the owned Processor. */
    Processor& getProcessor() noexcept { return processor; }

    // Phase 4: start(), stop(), graftInto(), drain()
    // Phase 4: terminal::Model model, unique_ptr<jam::Resizer> resizer

    /** @brief Document SSOT — 2 screens (normal + alternate). */
    jam::CodeModel document { 2 };

private:
    /** @brief Terminal engine — AudioProcessor analog. */
    Processor processor;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
