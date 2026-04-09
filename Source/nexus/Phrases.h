/**
 * @file Phrases.h
 * @brief Random verb pool for nexus loading overlay messages.
 *
 * @see LoaderOverlay
 */

#pragma once
#include <JuceHeader.h>

namespace Nexus
{

struct Phrases
{
    /** @brief Returns a random verb from the loading phrase pool. */
    static juce::String pick()
    {
        static const juce::StringArray pool {
            // Arcane
            "Invoking", "Conjuring", "Summoning", "Manifesting",
            "Kindling", "Igniting", "Sealing", "Attuning to",
            "Awakening", "Weaving", "Interweaving", "Forging", "Reticulating",
            // Abstract sonic / signal
            "Tuning", "Resonating", "Oscillating", "Modulating",
            "Phasing", "Harmonising", "Synthesising", "Patching",
            "Sampling", "Sequencing",
            // Terminal / systems ops
            "Spawning", "Forking", "Mounting", "Attaching",
            "Binding", "Probing", "Negotiating", "Handshaking",
            "Arbitrating", "Reconciling", "Rehydrating", "Bootstrapping",
        };

        return pool[juce::Random::getSystemRandom().nextInt (pool.size())];
    }
};

} // namespace Nexus
