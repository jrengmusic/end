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

    /** @brief Number of rows currently used in the scrollback buffer. */
    static const juce::Identifier scrollbackUsed { "scrollbackUsed" };

    /** @brief Current hint page index (0-based). Updated on open-file mode entry and spacebar. */
    static const juce::Identifier hintPage       { "hintPage" };

    /** @brief Total number of hint pages. Updated on open-file mode entry. */
    static const juce::Identifier hintTotalPages { "hintTotalPages" };

    //==========================================================================
    // Display name parameter IDs (tab name sources)
    //==========================================================================

    /** @brief Window title set by OSC 0/2 escape sequences. */
    static const juce::Identifier title              { "title" };

    /** @brief Current working directory path from OSC 7 or OS query. */
    static const juce::Identifier cwd                { "cwd" };

    /** @brief Name of the foreground process running in the terminal. */
    static const juce::Identifier foregroundProcess   { "foregroundProcess" };

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

    /** @brief Progressive keyboard protocol flags (CSI u bitmask). 0 = legacy mode. */
    static const juce::Identifier keyboardFlags        { "keyboardFlags" };

    //==========================================================================
    // Selection state parameter IDs
    //==========================================================================

    /** @brief Selection cursor row in scrollback-aware grid coordinates. */
    static const juce::Identifier selectionCursorRow   { "selectionCursorRow" };

    /** @brief Selection cursor column in grid coordinates. */
    static const juce::Identifier selectionCursorCol   { "selectionCursorCol" };

    /** @brief Selection anchor row in scrollback-aware grid coordinates. */
    static const juce::Identifier selectionAnchorRow   { "selectionAnchorRow" };

    /** @brief Selection anchor column in grid coordinates. */
    static const juce::Identifier selectionAnchorCol   { "selectionAnchorCol" };

    /** @brief Mouse drag anchor row in absolute (scrollback-aware) grid coordinates. */
    static const juce::Identifier dragAnchorRow        { "dragAnchorRow" };

    /** @brief Mouse drag anchor column in grid coordinates. */
    static const juce::Identifier dragAnchorCol        { "dragAnchorCol" };

    /** @brief True when the drag threshold has been crossed and a drag selection is active. */
    static const juce::Identifier dragActive           { "dragActive" };

    //==========================================================================
    // Transient session atomics (moved from stray members)
    //==========================================================================

    /** @brief Remaining paste echo bytes expected from the PTY (gate suppresses repaint). */
    static const juce::Identifier pasteEchoRemaining   { "pasteEchoRemaining" };

    /** @brief True while synchronized output (mode 2026) is active. */
    static const juce::Identifier syncOutputActive     { "syncOutputActive" };

    /** @brief True when a same-size PTY resize is requested on next drain. */
    static const juce::Identifier syncResizePending    { "syncResizePending" };

    /** @brief First visible row of the current OSC 133 command output block. -1 = none. */
    static const juce::Identifier outputBlockTop       { "outputBlockTop" };

    /** @brief Last visible row of the current OSC 133 command output block. -1 = none. */
    static const juce::Identifier outputBlockBottom    { "outputBlockBottom" };

    /** @brief True between OSC 133 C and D while output is being produced. */
    static const juce::Identifier outputScanActive     { "outputScanActive" };

    /** @brief Cursor row of the most-recently received OSC 133 A prompt marker. -1 = none. */
    static const juce::Identifier promptRow            { "promptRow" };

    //==========================================================================
    // Flush / repaint signal atomics (moved from stray members)
    //==========================================================================

    /** @brief True when any atomic parameter has been written since the last flush. */
    static const juce::Identifier needsFlush           { "needsFlush" };

    /** @brief True when new cell data has been written to the grid since the last repaint. */
    static const juce::Identifier snapshotDirty        { "snapshotDirty" };

    /** @brief Non-zero when all visible rows must be rebuilt on the next frame. */
    static const juce::Identifier fullRebuild          { "fullRebuild" };

    //==========================================================================
    // Cursor blink state (message-thread only, moved from stray members)
    //==========================================================================

    /** @brief Current blink phase: 1.0 = visible half, 0.0 = hidden half. */
    static const juce::Identifier cursorBlinkOn        { "cursorBlinkOn" };

    /** @brief Milliseconds accumulated since the last blink phase toggle. */
    static const juce::Identifier cursorBlinkElapsed   { "cursorBlinkElapsed" };

    /** @brief Last-flushed cursor row used to detect cursor movement for blink reset. */
    static const juce::Identifier prevFlushedCursorRow { "prevFlushedCursorRow" };

    /** @brief Last-flushed cursor column used to detect cursor movement for blink reset. */
    static const juce::Identifier prevFlushedCursorCol { "prevFlushedCursorCol" };

    /** @brief Blink half-period in milliseconds (from cursor.blink_interval config). */
    static const juce::Identifier cursorBlinkInterval  { "cursorBlinkInterval" };

    /** @brief Whether cursor blinking is enabled (from cursor.blink config). */
    static const juce::Identifier cursorBlinkEnabled   { "cursorBlinkEnabled" };

    /** @brief Whether the terminal component currently has keyboard focus. */
    static const juce::Identifier cursorFocused        { "cursorFocused" };

    //==========================================================================
    // IPC subscriber seqno tracking
    //==========================================================================

    /** @brief Container node for per-subscriber seqno tracking (children of SESSION). */
    static const juce::Identifier SUBSCRIBERS          { "SUBSCRIBERS" };

    /** @brief Child node type for one subscriber entry (children of SUBSCRIBERS). */
    static const juce::Identifier SUBSCRIBER           { "SUBSCRIBER" };

    /** @brief Property on a SUBSCRIBER node: the subscriber's stable UUID string. */
    static const juce::Identifier subscriberId         { "subscriberId" };

    /** @brief Property on a SUBSCRIBER node: the last seqno delivered to this subscriber. Stored as int64. */
    static const juce::Identifier lastKnownSeqno       { "lastKnownSeqno" };

    /** @brief Root-level SESSION property: the last seqno applied to this State (proxy/client side). */
    static const juce::Identifier lastKnownSeqnoRoot   { "lastKnownSeqnoRoot" };

}
/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
