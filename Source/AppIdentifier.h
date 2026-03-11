/**
 * @file AppIdentifier.h
 * @brief juce::Identifier constants for the application-level ValueTree.
 *
 * These identifiers define the schema for the application state tree:
 *
 *     END
 *     +-- WINDOW (width, height, zoom)
 *     +-- TABS (active, position)
 *         +-- TAB
 *             +-- PANES
 *                 +-- SESSION (terminal state, grafted from Terminal::State)
 *
 * @see AppState
 * @see Terminal::ID (terminal-level identifiers in Source/terminal/data/Identifier.h)
 */

#pragma once

#include <JuceHeader.h>

namespace App
{
namespace ID
{
    //==========================================================================
    // Node types
    //==========================================================================

    static const juce::Identifier END       { "END" };
    static const juce::Identifier WINDOW    { "WINDOW" };
    static const juce::Identifier TABS      { "TABS" };
    static const juce::Identifier TAB       { "TAB" };
    static const juce::Identifier PANES     { "PANES" };

    //==========================================================================
    // Properties
    //==========================================================================

    static const juce::Identifier width     { "width" };
    static const juce::Identifier height    { "height" };
    static const juce::Identifier zoom      { "zoom" };
    static const juce::Identifier active    { "active" };
    static const juce::Identifier position  { "position" };
    static const juce::Identifier splitVertical { "splitVertical" };
    static const juce::Identifier activeTerminalUuid { "activeTerminalUuid" };
}
}
