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
 *  Owns the document buffer (CodeModel), text renderer (CodeView), and
 *  processing engine (Processor). Nexus owns Sessions. terminal::View
 *  parents CodeView for rendering and holds Session reference.
 *
 *  Phase 3: stub — CodeView with dummy text, empty lifecycle.
 *  Phase 4: terminal::Model, Resizer, TTY lifecycle, graftInto.
 */
struct Session
{
    explicit Session (const jam::Font& font)
        : textEditor (font, document)
    {
        jam::Stamp::getInstance()->addIfNotAlreadyThere (jam::Stamp::Entry {});
    }

    /** @brief Returns the owned CodeView. View parents this for rendering. */
    jam::CodeView& getTextEditor() noexcept { return textEditor; }

    /** @brief Returns the owned CodeModel. View drains into this. */
    jam::CodeModel& getDocument() noexcept { return document; }

    /** @brief Returns the owned Processor. */
    Processor& getProcessor() noexcept { return processor; }

    // Phase 4: start(), stop(), graftInto(), drain()
    // Phase 4: terminal::Model model, unique_ptr<jam::Resizer> resizer

    /** @brief Document SSOT — 2 screens (normal + alternate). Declared before textEditor. */
    jam::CodeModel document { 2 };

private:
    /** @brief Terminal text renderer — references document. Constructed after document. */
    jam::CodeView textEditor;

    /** @brief Terminal engine — AudioProcessor analog. */
    Processor processor;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
