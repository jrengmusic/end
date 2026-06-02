/**
 * @file AppIdentifier.h
 * @brief juce::Identifier constants for the application-level ValueTree.
 *
 * These identifiers define the schema for the three persisted files:
 *
 * window.state  — window geometry only (standalone, cross-instance):
 *     WINDOW (width, height)
 *
 * nexus/\<uuid\>.display  — full display state (daemon client mode only):
 *     END
 *     +-- WINDOW (width, height, zoom, renderer, daemonMode)
 *     +-- TABS (active, position, activePaneID)
 *         +-- TAB
 *             +-- PANES (direction, ratio)
 *                 +-- PANE (uuid) | PANES (nested split)
 *                     +-- SESSION (terminal state, grafted from terminal::Model)
 *                     +-- DOCUMENT (whelmed state, grafted from whelmed::State)
 *
 * nexus/\<uuid\>.nexus  — daemon port only (plain text, no ValueTree).
 *
 * @see AppModel
 * @see terminal::ID (terminal-level identifiers in Source/terminal/data/Identifier.h)
 */

#pragma once

#include <JuceHeader.h>

namespace app
{
/*____________________________________________________________________________*/

    //==========================================================================
    // Renderer type
    //==========================================================================

    enum class RendererType
    {
        gpu,
        cpu
    };

    static constexpr int titleBarHeight { 24 };

namespace id
{
/*____________________________________________________________________________*/

    //==========================================================================
    // Node types
    //==========================================================================

    static const juce::Identifier END        { "END" };
    static const juce::Identifier WINDOW     { "WINDOW" };
    static const juce::Identifier NEXUS      { "NEXUS" };
    static const juce::Identifier SESSIONS { "SESSIONS" };
    static const juce::Identifier SESSION  { "SESSION" };
    static const juce::Identifier LOADING    { "LOADING" };
    static const juce::Identifier OPERATION  { "OPERATION" };
    static const juce::Identifier TABS       { "TABS" };
    static const juce::Identifier TAB        { "TAB" };
    static const juce::Identifier PANES      { "PANES" };
    static const juce::Identifier PANE       { "PANE" };
    static const juce::Identifier DOCUMENT   { "DOCUMENT" };

    //==========================================================================
    // Properties
    //==========================================================================

    static const juce::Identifier zoom               { "zoom" };
    static const juce::Identifier active             { "active" };
    static const juce::Identifier position           { "position" };
    static const juce::Identifier splitVertical      { "splitVertical" };
    static const juce::Identifier activePaneID { "activePaneID" };
    static const juce::Identifier activePaneType { "activePaneType" };
    static const juce::Identifier modalType      { "modalType" };
    static const juce::Identifier selectionType  { "selectionType" };
    static const juce::Identifier fontFamily          { "fontFamily" };
    static const juce::Identifier fontSize            { "fontSize" };
    static const juce::Identifier renderer            { "renderer" };
    static const juce::Identifier gpuAvailable       { "gpuAvailable" };
    static const juce::Identifier daemonMode          { "daemonMode" };
    static const juce::Identifier filePath           { "filePath" };
    static const juce::Identifier displayName        { "displayName" };
    static const juce::Identifier userTabName        { "userTabName" };
    static const juce::Identifier scrollOffset       { "scrollOffset" };
    static const juce::Identifier port              { "port" };
    static const juce::Identifier atlasDirty        { "atlasDirty" };
    static const juce::Identifier scrollbackLines   { "scrollbackLines" };

    /** @brief Monotonic counter incremented by lua::Engine::reload() after each successful config load.
     *  MainComponent's VT listener detects changes and calls applyConfig() + showReloadMessage(). */
    static const juce::Identifier configGeneration  { "configGeneration" };

    /** @brief Path of the .md file pending Whelmed open. Written by LinkManager::dispatch(); consumed
     *  and cleared by Tabs::valueTreePropertyChanged on the WINDOW node. */
    static const juce::Identifier pendingMarkdownFile { "pendingMarkdownFile" };

    /** @brief Path of the image file pending inline open. Written by LinkManager::dispatch(); consumed
     *  and cleared by Tabs::valueTreePropertyChanged on the WINDOW node. */
    static const juce::Identifier pendingImageFile    { "pendingImageFile" };
    static const juce::Identifier cellWidth            { "cellWidth" };
    static const juce::Identifier lineHeight           { "lineHeight" };
    static const juce::Identifier cursorCodepoint      { "cursorCodepoint" };
    static const juce::Identifier cursorStyle          { "cursorStyle" };
    static const juce::Identifier cursorBlinkInterval  { "cursorBlinkInterval" };
    static const juce::Identifier paddingTop           { "paddingTop" };
    static const juce::Identifier paddingRight         { "paddingRight" };
    static const juce::Identifier paddingBottom        { "paddingBottom" };
    static const juce::Identifier paddingLeft          { "paddingLeft" };

    static const juce::Identifier blockCount         { "blockCount" };
    static const juce::Identifier parseComplete      { "parseComplete" };
    static const juce::Identifier totalBlocks        { "totalBlocks" };
    static const juce::Identifier selCursorBlock     { "selCursorBlock" };
    static const juce::Identifier selCursorChar      { "selCursorChar" };
    static const juce::Identifier selAnchorBlock     { "selAnchorBlock" };
    static const juce::Identifier selAnchorChar      { "selAnchorChar" };

    //==========================================================================
    // XML schema attributes (AppParameters.xml)
    //==========================================================================

    static const juce::Identifier type           { "type" };
    static const juce::Identifier defaultValue   { "default" };
    static const juce::Identifier boolType       { "bool" };
    static const juce::Identifier floatType      { "float" };
    static const juce::Identifier stringType     { "string" };

    static const juce::String appMetadata { "AppParameters.xml" };

/**______________________________END OF NAMESPACE______________________________*/
} // namespace id

/**______________________________END OF NAMESPACE______________________________*/
} // namespace app
