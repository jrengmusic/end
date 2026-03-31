/**
 * @file AppIdentifier.h
 * @brief juce::Identifier constants for the application-level ValueTree.
 *
 * These identifiers define the schema for the application state tree:
 *
 *     END
 *     +-- WINDOW (width, height, zoom, renderer)
 *     +-- TABS (active, position, activePaneID)
 *         +-- TAB
 *             +-- PANES (direction, ratio)
 *                 +-- PANE (uuid) | PANES (nested split)
 *                     +-- SESSION (terminal state, grafted from Terminal::State)
 *                     +-- DOCUMENT (whelmed state, grafted from Whelmed::State)
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
    static const juce::Identifier PANE      { "PANE" };
    static const juce::Identifier DOCUMENT  { "DOCUMENT" };

    //==========================================================================
    // Properties
    //==========================================================================

    static const juce::Identifier width              { "width" };
    static const juce::Identifier height             { "height" };
    static const juce::Identifier zoom               { "zoom" };
    static const juce::Identifier active             { "active" };
    static const juce::Identifier position           { "position" };
    static const juce::Identifier splitVertical      { "splitVertical" };
    static const juce::Identifier activePaneID { "activePaneID" };
    static const juce::Identifier activePaneType { "activePaneType" };
    static const juce::Identifier renderer           { "renderer" };
    static const juce::Identifier filePath           { "filePath" };
    static const juce::Identifier displayName        { "displayName" };
    static const juce::Identifier scrollOffset       { "scrollOffset" };
    static const juce::Identifier blockCount         { "blockCount" };
    static const juce::Identifier parseComplete      { "parseComplete" };
    static const juce::Identifier pendingPrefix      { "pendingPrefix" };
    static const juce::Identifier totalBlocks        { "totalBlocks" };
}
}
