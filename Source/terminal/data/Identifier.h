/**
 * @file Identifier.h
 * @brief juce::Identifier constants for terminal ValueTree property names.
 *
 * This file provides static Identifier constants used as property keys in the
 * terminal's ValueTree state model. These identifiers enable type-safe property
 * access and are used throughout the terminal emulation layer.
 *
 * The identifiers are organized into logical groups:
 * - Node types: Top-level ValueTree node identifiers
 * - PARAM properties: Generic parameter node properties
 * - Session parameter IDs: Global terminal session state
 * - Mode parameter IDs: DEC terminal mode flags
 * - Per-screen parameter IDs: Screen-specific cursor and scroll state
 *
 * @see State.h for the ValueTree structure that uses these identifiers
 */

#pragma once

#include <JuceHeader.h>

/**
 * @brief Hash specialization for juce::Identifier to enable use in unordered containers.
 *
 * This specialization enables using juce::Identifier as a key type in
 * std::unordered_map and std::unordered_set. The hash is computed from the
 * Identifier's internal character pointer address, which is stable for the
 * lifetime of the Identifier.
 *
 * @par Why Specialize std::hash?
 * By default, std::hash is not defined for juce::Identifier. Without this
 * specialization, Identifiers cannot be used as keys in unordered associative
 * containers. This is useful for:
 * - Fast lookup of terminal modes by identifier name
 * - Building maps of screen parameters by name
 * - Sets of active mode flags
 *
 * @par Hash Quality
 * The hash uses the character pointer address, which provides good distribution
 * for Identifier objects since they intern their strings. Two Identifiers with
 * the same name will have the same pointer, guaranteeing identical hashes.
 *
 * @note Thread Safety: This hash function is read-only and thread-safe.
 *
 * @par Example
 * @code
 * std::unordered_map<juce::Identifier, bool> modeFlags;
 * modeFlags[Terminal::ID::autoWrap] = true;
 * @endcode
 */
namespace std
{
    template <> struct hash<juce::Identifier>
    {
        size_t operator() (const juce::Identifier& id) const noexcept
        {
            return std::hash<const void*>{} (id.getCharPointer().getAddress());
        }
    };
}

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @brief Terminal identifier constants for ValueTree property names.
 *
 * This namespace contains static juce::Identifier constants used as keys
 * when reading and writing terminal state to ValueTree objects. Using
 * static Identifiers avoids string allocation on every property access.
 *
 * @par Naming Convention
 * All identifiers use camelCase to match the ValueTree property names
 * they represent (not the C++ member names in State classes).
 *
 * @par Usage Pattern
 * @code
 * ValueTree session = state.getOrCreateChildWithName(ID::SESSION, nullptr);
 * session.setProperty(ID::cols, 80, nullptr);
 * session.setProperty(ID::visibleRows, 24, nullptr);
 * @endcode
 *
 * @see State.h for the complete state model
 */
namespace ID
{
    //==========================================================================
    // Node types (ValueTree root and child node identifiers)
    //=========================================================================

    /** @brief Root session node identifier. */
    static const juce::Identifier SESSION        { "SESSION" };

    /** @brief Container node for terminal mode states (DEC private modes). */
    static const juce::Identifier MODES          { "MODES" };

    /** @brief Normal screen buffer identifier (DECSC). */
    static const juce::Identifier NORMAL         { "NORMAL" };

    /** @brief Alternate screen buffer identifier (DECSC). */
    static const juce::Identifier ALTERNATE      { "ALTERNATE" };

    /** @brief Generic parameter node identifier for DEC private parameters. */
    static const juce::Identifier PARAM          { "PARAM" };

    //==========================================================================
    // PARAM properties (generic parameter node properties)
    //==========================================================================

    /** @brief Parameter identifier (e.g., DEC private mode number). */
    static const juce::Identifier id             { "id" };

    /** @brief Parameter value (the setting for this parameter). */
    static const juce::Identifier value          { "value" };

    //==========================================================================
    // Session parameter IDs (global terminal session state)
    //==========================================================================

    /** @brief Active screen index (which screen is currently visible). */
    static const juce::Identifier activeScreen   { "activeScreen" };

    /** @brief Number of columns in the terminal (terminal width). */
    static const juce::Identifier cols           { "cols" };

    /** @brief Number of visible rows (terminal height in lines). */
    static const juce::Identifier visibleRows    { "visibleRows" };

    /** @brief Scroll offset for alternate screen buffer (scrollback position). */
    static const juce::Identifier scrollOffset  { "scrollOffset" };

    /** @brief Active modal input type (cast from ModalType enum). 0 = none. */
    static const juce::Identifier modalType     { "modalType" };

    //==========================================================================
    // Display name parameter IDs (tab name sources)
    //==========================================================================

    /** @brief Window title set by OSC 0/2 escape sequences. */
    static const juce::Identifier title              { "title" };

    /** @brief Current working directory path from OSC 7 or OS query. */
    static const juce::Identifier cwd                { "cwd" };

    /** @brief Name of the foreground process running in the terminal. */
    static const juce::Identifier foregroundProcess   { "foregroundProcess" };

    /** @brief Derived display name for the tab (computed from foregroundProcess, cwd). */
    static const juce::Identifier displayName         { "displayName" };

    /** @brief Shell program name, stored at session creation for display name filtering. */
    static const juce::Identifier shellProgram        { "shellProgram" };

    //==========================================================================
    // Mode parameter IDs (DEC terminal mode flags)
    //==========================================================================

    /** @brief Origin Mode (DECOM) - relative vs absolute cursor addressing. */
    static const juce::Identifier originMode           { "originMode" };

    /** @brief Auto Wrap Mode (DECAWM) - cursor moves to next line at right margin. */
    static const juce::Identifier autoWrap             { "autoWrap" };

    /** @brief Application Cursor Keys (DECCKM) - arrow key escape sequence mode. */
    static const juce::Identifier applicationCursor    { "applicationCursor" };

    /** @brief Branked Paste Mode (DECBPM) - wraps pasted text with markers. */
    static const juce::Identifier bracketedPaste       { "bracketedPaste" };

    /** @brief Insert Mode (IRM) - inserted characters shift existing content. */
    static const juce::Identifier insertMode           { "insertMode" };

    /** @brief Mouse Tracking Mode (DECXM) - basic mouse button reporting. */
    static const juce::Identifier mouseTracking        { "mouseTracking" };

    /** @brief Mouse Motion Tracking (DECMM) - report mouse movement with button pressed. */
    static const juce::Identifier mouseMotionTracking  { "mouseMotionTracking" };

    /** @brief Mouse All Tracking (DECMAM) - report all mouse movements. */
    static const juce::Identifier mouseAllTracking     { "mouseAllTracking" };

    /** @brief SGR Mouse Encoding (DECSGR) - use SGR escape sequences for mouse. */
    static const juce::Identifier mouseSgr             { "mouseSgr" };

    /** @brief Focus In/Out Events (DECFE) - send escape sequences on window focus. */
    static const juce::Identifier focusEvents          { "focusEvents" };

    /** @brief Application Keypad Mode (DECNKM) - numeric keypad sends application codes. */
    static const juce::Identifier applicationKeypad    { "applicationKeypad" };

    /** @brief Cursor Visibility - whether the cursor is rendered. */
    static const juce::Identifier cursorVisible        { "cursorVisible" };

    /** @brief Reverse Video Mode - swap foreground and background colors. */
    static const juce::Identifier reverseVideo         { "reverseVideo" };

    /** @brief Win32 Input Mode (XTerm 9001) - encode keyboard input as Win32 KEY_EVENT_RECORD. */
    static const juce::Identifier win32InputMode       { "win32InputMode" };

    //==========================================================================
    // Per-screen parameter IDs (screen-specific cursor and scroll state)
    //==========================================================================

    /** @brief Cursor row position (0-based, top row is 0). */
    static const juce::Identifier cursorRow            { "cursorRow" };

    /** @brief Cursor column position (0-based, leftmost column is 0). */
    static const juce::Identifier cursorCol            { "cursorCol" };

    /** @brief Wrap Pending Flag - cursor is at right margin waiting to wrap. */
    static const juce::Identifier wrapPending          { "wrapPending" };

    /** @brief Scroll Top - top margin row for scrolling region (inclusive). */
    static const juce::Identifier scrollTop            { "scrollTop" };

    /** @brief Scroll Bottom - bottom margin row for scrolling region (inclusive). */
    static const juce::Identifier scrollBottom         { "scrollBottom" };

    /** @brief DECSCUSR cursor shape (0=default, 1=blinking block, 2=steady block, 3=blinking underline, 4=steady underline, 5=blinking bar, 6=steady bar). */
    static const juce::Identifier cursorShape         { "cursorShape" };

    /** @brief OSC 12 cursor color red component (0-255). -1 = not set (use config default). */
    static const juce::Identifier cursorColorR         { "cursorColorR" };

    /** @brief OSC 12 cursor color green component (0-255). -1 = not set (use config default). */
    static const juce::Identifier cursorColorG         { "cursorColorG" };

    /** @brief OSC 12 cursor color blue component (0-255). -1 = not set (use config default). */
    static const juce::Identifier cursorColorB         { "cursorColorB" };

    /** @brief Kitty keyboard protocol flags (bitmask). 0 = legacy mode. */
    static const juce::Identifier keyboardFlags        { "keyboardFlags" };

    //==========================================================================
    // Selection state parameter IDs
    //==========================================================================

    /** @brief Active selection type (cast from SelectionType enum). 0 = none. */
    static const juce::Identifier selectionType        { "selectionType" };

    /** @brief Selection cursor row in scrollback-aware grid coordinates. */
    static const juce::Identifier selectionCursorRow   { "selectionCursorRow" };

    /** @brief Selection cursor column in grid coordinates. */
    static const juce::Identifier selectionCursorCol   { "selectionCursorCol" };

    /** @brief Selection anchor row in scrollback-aware grid coordinates. */
    static const juce::Identifier selectionAnchorRow   { "selectionAnchorRow" };

    /** @brief Selection anchor column in grid coordinates. */
    static const juce::Identifier selectionAnchorCol   { "selectionAnchorCol" };
}
/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
