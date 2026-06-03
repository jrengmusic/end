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

    // CONFIG schema — filename constants (SSOT for all config path + node id resolution).
    static const juce::String endLua      { "end.lua" };
    static const juce::String nexusLua    { "nexus.lua" };
    static const juce::String displayLua  { "display.lua" };
    static const juce::String whelmedLua  { "whelmed.lua" };
    static const juce::String keysLua     { "keys.lua" };
    static const juce::String popupsLua   { "popups.lua" };
    static const juce::String actionsLua  { "actions.lua" };

    // CONFIG node types — derived from filenames via jam::Text::toValidID.
    // Unique by construction (no collision with terminal::id::DISPLAY or runtime NEXUS).
    static const juce::Identifier CONFIG       { "CONFIG" };
    static const juce::Identifier NEXUS_LUA    { jam::Text::toValidID (nexusLua, true) };
    static const juce::Identifier DISPLAY_LUA  { jam::Text::toValidID (displayLua, true) };
    static const juce::Identifier WHELMED_LUA  { jam::Text::toValidID (whelmedLua, true) };
    static const juce::Identifier KEYS_LUA     { jam::Text::toValidID (keysLua, true) };
    static const juce::Identifier POPUPS_LUA   { jam::Text::toValidID (popupsLua, true) };
    static const juce::Identifier ACTIONS_LUA  { jam::Text::toValidID (actionsLua, true) };

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
    /** @brief CONFIG / NEXUS scalar properties. */
    //==========================================================================

    static const juce::Identifier gpu                      { "gpu" };
    static const juce::Identifier daemon                   { "daemon" };
    static const juce::Identifier autoReload               { "autoReload" };
    static const juce::Identifier shellProgram             { "shellProgram" };
    static const juce::Identifier shellArgs                { "shellArgs" };
    static const juce::Identifier shellIntegration         { "shellIntegration" };
    static const juce::Identifier scrollStep               { "scrollStep" };
    static const juce::Identifier dropMultifiles           { "dropMultifiles" };
    static const juce::Identifier dropQuoted               { "dropQuoted" };
    static const juce::Identifier hyperlinkEditor          { "hyperlinkEditor" };
    static const juce::Identifier imageAtlasBudgetBytes    { "imageAtlasBudgetBytes" };
    static const juce::Identifier imageCols                { "imageCols" };
    static const juce::Identifier imageRows                { "imageRows" };
    static const juce::Identifier imagePadding             { "imagePadding" };
    static const juce::Identifier imageAtlasDimension      { "imageAtlasDimension" };
    static const juce::Identifier imageBorder              { "imageBorder" };

    //==========================================================================
    /** @brief CONFIG / DISPLAY — Window properties. */
    //==========================================================================

    static const juce::Identifier windowTitle              { "windowTitle" };
    static const juce::Identifier windowWidth              { "windowWidth" };
    static const juce::Identifier windowHeight             { "windowHeight" };
    static const juce::Identifier windowColour             { "windowColour" };
    static const juce::Identifier windowOpacity            { "windowOpacity" };
    static const juce::Identifier windowBlurRadius         { "windowBlurRadius" };
    static const juce::Identifier windowAlwaysOnTop        { "windowAlwaysOnTop" };
    static const juce::Identifier windowButtons            { "windowButtons" };
    static const juce::Identifier windowForceDwm           { "windowForceDwm" };
    static const juce::Identifier windowSaveSize           { "windowSaveSize" };
    static const juce::Identifier windowConfirmationOnExit { "windowConfirmationOnExit" };

    /** @brief CONFIG / DISPLAY — Font properties. */
    static const juce::Identifier fontLigatures            { "fontLigatures" };
    static const juce::Identifier fontEmbolden             { "fontEmbolden" };
    static const juce::Identifier fontDesktopScale         { "fontDesktopScale" };

    /** @brief CONFIG / DISPLAY — Cursor properties. */
    static const juce::Identifier cursorBlink              { "cursorBlink" };
    static const juce::Identifier cursorForce              { "cursorForce" };

    /** @brief CONFIG / DISPLAY — Colour properties. Shared identifiers (foreground,
     *  background, scrollbarThumb, scrollbarTrack) are also used by CONFIG / WHELMED. */
    static const juce::Identifier foreground               { "foreground" };
    static const juce::Identifier background               { "background" };
    static const juce::Identifier cursorColour             { "cursorColour" };
    static const juce::Identifier selectionColour          { "selectionColour" };
    static const juce::Identifier selectionCursorColour    { "selectionCursorColour" };
    static const juce::Identifier ansi0                    { "ansi0" };
    static const juce::Identifier ansi1                    { "ansi1" };
    static const juce::Identifier ansi2                    { "ansi2" };
    static const juce::Identifier ansi3                    { "ansi3" };
    static const juce::Identifier ansi4                    { "ansi4" };
    static const juce::Identifier ansi5                    { "ansi5" };
    static const juce::Identifier ansi6                    { "ansi6" };
    static const juce::Identifier ansi7                    { "ansi7" };
    static const juce::Identifier ansi8                    { "ansi8" };
    static const juce::Identifier ansi9                    { "ansi9" };
    static const juce::Identifier ansi10                   { "ansi10" };
    static const juce::Identifier ansi11                   { "ansi11" };
    static const juce::Identifier ansi12                   { "ansi12" };
    static const juce::Identifier ansi13                   { "ansi13" };
    static const juce::Identifier ansi14                   { "ansi14" };
    static const juce::Identifier ansi15                   { "ansi15" };
    static const juce::Identifier editorBackground         { "editorBackground" };
    static const juce::Identifier editorOutline            { "editorOutline" };
    static const juce::Identifier statusBarColour          { "statusBarColour" };
    static const juce::Identifier statusBarLabelBg         { "statusBarLabelBg" };
    static const juce::Identifier statusBarLabelFg         { "statusBarLabelFg" };
    static const juce::Identifier statusBarSpinner         { "statusBarSpinner" };
    static const juce::Identifier hintLabelBg              { "hintLabelBg" };
    static const juce::Identifier hintLabelFg              { "hintLabelFg" };
    static const juce::Identifier scrollbarThumb           { "scrollbarThumb" };
    static const juce::Identifier scrollbarTrack           { "scrollbarTrack" };

    /** @brief CONFIG / DISPLAY — Tab properties. */
    static const juce::Identifier tabFamily                { "tabFamily" };
    static const juce::Identifier tabSize                  { "tabSize" };
    static const juce::Identifier tabForeground            { "tabForeground" };
    static const juce::Identifier tabInactive              { "tabInactive" };
    static const juce::Identifier tabPosition              { "tabPosition" };
    static const juce::Identifier tabLine                  { "tabLine" };
    static const juce::Identifier tabActive                { "tabActive" };
    static const juce::Identifier tabIndicator             { "tabIndicator" };
    static const juce::Identifier tabButtonSvg             { "tabButtonSvg" };

    /** @brief CONFIG / DISPLAY — Pane properties. */
    static const juce::Identifier paneBarColour            { "paneBarColour" };
    static const juce::Identifier paneBarHighlight         { "paneBarHighlight" };

    /** @brief CONFIG / DISPLAY — Overlay properties. */
    static const juce::Identifier overlayFamily            { "overlayFamily" };
    static const juce::Identifier overlaySize              { "overlaySize" };
    static const juce::Identifier overlayColour            { "overlayColour" };

    /** @brief CONFIG / DISPLAY — Menu properties. */
    static const juce::Identifier menuOpacity              { "menuOpacity" };

    /** @brief CONFIG / DISPLAY — ActionList properties. */
    static const juce::Identifier actionListCloseOnRun        { "actionListCloseOnRun" };
    static const juce::Identifier actionListPosition          { "actionListPosition" };
    static const juce::Identifier actionListNameFamily        { "actionListNameFamily" };
    static const juce::Identifier actionListNameStyle         { "actionListNameStyle" };
    static const juce::Identifier actionListNameSize          { "actionListNameSize" };
    static const juce::Identifier actionListShortcutFamily    { "actionListShortcutFamily" };
    static const juce::Identifier actionListShortcutStyle     { "actionListShortcutStyle" };
    static const juce::Identifier actionListShortcutSize      { "actionListShortcutSize" };
    static const juce::Identifier actionListPaddingTop        { "actionListPaddingTop" };
    static const juce::Identifier actionListPaddingRight      { "actionListPaddingRight" };
    static const juce::Identifier actionListPaddingBottom     { "actionListPaddingBottom" };
    static const juce::Identifier actionListPaddingLeft       { "actionListPaddingLeft" };
    static const juce::Identifier actionListNameColour        { "actionListNameColour" };
    static const juce::Identifier actionListShortcutColour    { "actionListShortcutColour" };
    static const juce::Identifier actionListWidth             { "actionListWidth" };
    static const juce::Identifier actionListHeight            { "actionListHeight" };
    static const juce::Identifier actionListHighlightColour   { "actionListHighlightColour" };

    /** @brief CONFIG / DISPLAY — StatusBar properties. */
    static const juce::Identifier statusBarPosition        { "statusBarPosition" };
    static const juce::Identifier statusBarFontFamily      { "statusBarFontFamily" };
    static const juce::Identifier statusBarFontSize        { "statusBarFontSize" };
    static const juce::Identifier statusBarFontStyle       { "statusBarFontStyle" };

    /** @brief CONFIG / DISPLAY — PopupBorder properties. */
    static const juce::Identifier popupBorderColour        { "popupBorderColour" };
    static const juce::Identifier popupBorderWidth         { "popupBorderWidth" };

    /** @brief CONFIG / DISPLAY — top-level scrollbar dimension. */
    static const juce::Identifier scrollbarWidth           { "scrollbarWidth" };

    //==========================================================================
    /** @brief CONFIG / WHELMED properties. Identifiers shared with CONFIG / DISPLAY
     *  (fontFamily, fontSize, lineHeight, foreground, background, paddingTop/Right/Bottom/Left,
     *  scrollStep, scrollbarThumb, scrollbarTrack, selectionColour) are already declared above. */
    //==========================================================================

    static const juce::Identifier fontStyle                { "fontStyle" };
    static const juce::Identifier codeFamily               { "codeFamily" };
    static const juce::Identifier codeSize                 { "codeSize" };
    static const juce::Identifier codeStyle                { "codeStyle" };
    static const juce::Identifier h1Size                   { "h1Size" };
    static const juce::Identifier h2Size                   { "h2Size" };
    static const juce::Identifier h3Size                   { "h3Size" };
    static const juce::Identifier h4Size                   { "h4Size" };
    static const juce::Identifier h5Size                   { "h5Size" };
    static const juce::Identifier h6Size                   { "h6Size" };
    static const juce::Identifier bodyColour               { "bodyColour" };
    static const juce::Identifier codeColour               { "codeColour" };
    static const juce::Identifier linkColour               { "linkColour" };
    static const juce::Identifier h1Colour                 { "h1Colour" };
    static const juce::Identifier h2Colour                 { "h2Colour" };
    static const juce::Identifier h3Colour                 { "h3Colour" };
    static const juce::Identifier h4Colour                 { "h4Colour" };
    static const juce::Identifier h5Colour                 { "h5Colour" };
    static const juce::Identifier h6Colour                 { "h6Colour" };
    static const juce::Identifier codeFenceBackground      { "codeFenceBackground" };
    static const juce::Identifier progressBackground       { "progressBackground" };
    static const juce::Identifier progressForeground       { "progressForeground" };
    static const juce::Identifier progressTextColour       { "progressTextColour" };
    static const juce::Identifier progressSpinnerColour    { "progressSpinnerColour" };
    static const juce::Identifier tokenError               { "tokenError" };
    static const juce::Identifier tokenComment             { "tokenComment" };
    static const juce::Identifier tokenKeyword             { "tokenKeyword" };
    static const juce::Identifier tokenOperator            { "tokenOperator" };
    static const juce::Identifier tokenIdentifier          { "tokenIdentifier" };
    static const juce::Identifier tokenInteger             { "tokenInteger" };
    static const juce::Identifier tokenFloat               { "tokenFloat" };
    static const juce::Identifier tokenString              { "tokenString" };
    static const juce::Identifier tokenBracket             { "tokenBracket" };
    static const juce::Identifier tokenPunctuation         { "tokenPunctuation" };
    static const juce::Identifier tokenPreprocessor        { "tokenPreprocessor" };
    static const juce::Identifier tableBackground          { "tableBackground" };
    static const juce::Identifier tableHeaderBackground    { "tableHeaderBackground" };
    static const juce::Identifier tableRowAlt              { "tableRowAlt" };
    static const juce::Identifier tableBorderColour        { "tableBorderColour" };
    static const juce::Identifier tableHeaderText          { "tableHeaderText" };
    static const juce::Identifier tableCellText            { "tableCellText" };
    static const juce::Identifier scrollbarBackground      { "scrollbarBackground" };
    static const juce::Identifier scrollDown               { "scrollDown" };
    static const juce::Identifier scrollUp                 { "scrollUp" };
    static const juce::Identifier scrollTop                { "scrollTop" };
    static const juce::Identifier scrollBottom             { "scrollBottom" };

    //==========================================================================
    /** @brief CONFIG / KEYS properties. */
    //==========================================================================

    static const juce::Identifier prefix                   { "prefix" };
    static const juce::Identifier prefixTimeout            { "prefixTimeout" };

    //==========================================================================
    /** @brief CONFIG / POPUPS properties. */
    //==========================================================================

    static const juce::Identifier defaultCols              { "defaultCols" };
    static const juce::Identifier defaultRows              { "defaultRows" };
    static const juce::Identifier defaultPosition          { "defaultPosition" };

    //==========================================================================
    // XML schema attributes (AppParameters.xml)
    //==========================================================================

    static const juce::Identifier type           { "type" };
    static const juce::Identifier defaultValue   { "default" };
    static const juce::Identifier boolType       { "bool" };
    static const juce::Identifier floatType      { "float" };
    static const juce::Identifier stringType     { "string" };
    static const juce::Identifier colourType     { "colour" };

    static const juce::String appMetadata { "AppParameters.xml" };

/**______________________________END OF NAMESPACE______________________________*/
} // namespace id

/**______________________________END OF NAMESPACE______________________________*/
} // namespace app
