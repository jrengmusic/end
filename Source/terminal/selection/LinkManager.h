/**
 * @file LinkManager.h
 * @brief Owns viewport link scanning, hit-testing, hint assignment, and dispatch.
 *
 * LinkManager centralises all link-related logic that was previously scattered
 * across TerminalComponent:
 *
 * - **scan()** — O(rows×cols) viewport scan for file and URL tokens, plus OSC 8
 *   hyperlink merging.  Calls `session.getParser().clearOsc8Links()` internally
 *   (tell-don't-ask).
 * - **scanForHints()** — full-viewport scan used on open-file mode entry; assigns
 *   single-character hint labels for the overlay.
 * - **hitTest()** — returns the first `LinkSpan` at the given (row, col) cell, or
 *   `nullptr` if none.
 * - **dispatch()** — opens URLs in the browser or sends the file path to the
 *   configured editor via the PTY.
 *
 * @see LinkSpan
 * @see LinkDetector
 * @see Terminal::Session
 */

#pragma once

#include <JuceHeader.h>
#include <vector>
#include "LinkSpan.h"

namespace Terminal
{ /*____________________________________________________________________________*/

class Grid;
class Session;

/**
 * @class LinkManager
 * @brief Owns link detection, hint labelling, hit-testing, and dispatch for one terminal session.
 *
 * Constructed with a `Session&` — the session reference is held for the lifetime
 * of the manager.  All methods are MESSAGE THREAD only unless noted.
 *
 * @par Thread context
 * All public methods must be called on the **MESSAGE THREAD**.
 */
class LinkManager
{
public:
    /**
     * @brief Constructs a LinkManager bound to the given session.
     * @param session  The terminal session owning the grid and parser to scan.
     * @note MESSAGE THREAD.
     */
    explicit LinkManager (Session& session) noexcept;

    /**
     * @brief Scans the viewport for file-path and URL tokens.
     *
     * Calls `session.getParser().clearOsc8Links()` before scanning so that stale
     * OSC 8 spans from the previous scan are discarded.  Populates
     * `clickableLinks` with all detected spans.
     *
     * @param cwd             Shell current working directory for relative-path resolution.
     * @param outputRowsOnly  When `true`, only rows inside the OSC 133 output block
     *                        are scanned (hover underline mode).  When `false`, all
     *                        visible rows are scanned (open-file hint mode).
     * @note MESSAGE THREAD.
     */
    void scan (const juce::String& cwd, bool outputRowsOnly);

    /**
     * @brief Scans the full viewport and assigns single-character hint labels.
     *
     * Used on open-file mode entry.  Populates `hintLinks` with spans labelled
     * `'a'`–`'z'` using a character from the token's own filename where possible.
     *
     * @param cwd  Shell current working directory for relative-path resolution.
     * @note MESSAGE THREAD.
     */
    void scanForHints (const juce::String& cwd);

    /**
     * @brief Clears all hint-mode link spans.
     *
     * Called when open-file mode exits.
     *
     * @note MESSAGE THREAD.
     */
    void clearHints() noexcept;

    /**
     * @brief Marks the scan cache as stale so the next `scan()` call re-runs.
     * @note MESSAGE THREAD.
     */
    void invalidate() noexcept;

    /**
     * @brief Returns `true` when the click-link cache is stale and needs rescanning.
     * @return `true` if a new scan is required.
     * @note MESSAGE THREAD.
     */
    bool needsScan() const noexcept;

    /**
     * @brief Returns the first clickable link at the given visible-row cell, or `nullptr`.
     *
     * @param row  Visible row index (0 = topmost visible row).
     * @param col  Zero-based column index.
     * @return Pointer into `clickableLinks`, or `nullptr` if no span covers the cell.
     * @note MESSAGE THREAD.  Pointer is valid until the next `scan()` call.
     */
    const LinkSpan* hitTest (int row, int col) const noexcept;

    /**
     * @brief Returns the first hint-mode link matching the given label character,
     *        or `nullptr`.
     *
     * @param label  Single lowercase letter `'a'`–`'z'` to match against hint labels.
     * @return Pointer into `hintLinks`, or `nullptr` if no span matches.
     * @note MESSAGE THREAD.  Pointer is valid until the next `scanForHints()` call.
     */
    const LinkSpan* hitTestHint (char label) const noexcept;

    /**
     * @brief Dispatches the link: opens URLs in the browser or writes the file
     *        path to the PTY editor command.
     *
     * File dispatch reads the configured editor from Config and constructs a
     * `"<editor> <path>\r"` command written to the session PTY.  URL dispatch
     * calls `juce::URL::launchInDefaultBrowser()`.
     *
     * @param span  The link to dispatch.
     * @note MESSAGE THREAD.
     */
    void dispatch (const LinkSpan& span) const;

    /**
     * @brief Returns the current clickable-link spans (for hover underline).
     * @return Read-only reference to the internal vector.
     * @note MESSAGE THREAD.
     */
    const std::vector<LinkSpan>& getClickableLinks() const noexcept;

    /**
     * @brief Returns the current hint-mode link spans.
     * @return Read-only reference to the internal vector.
     * @note MESSAGE THREAD.
     */
    const std::vector<LinkSpan>& getHintLinks() const noexcept;

private:
    /**
     * @brief Shared scan implementation.
     *
     * Scans `grid` for file/URL tokens and OSC 8 hyperlinks.  Optionally
     * restricted to the OSC 133 output block.  Returns the collected spans
     * without hint label assignment.
     *
     * @param grid            Terminal grid to scan (read-only).
     * @param cwd             Shell current working directory.
     * @param outputRowsOnly  When `true`, restrict to the OSC 133 block.
     * @return Vector of detected spans without hint labels.
     */
    std::vector<LinkSpan> scanViewport (const Grid& grid,
                                        const juce::String& cwd,
                                        bool outputRowsOnly) const;

    /**
     * @brief Assigns unique single-character hint labels to `spans` in-place.
     *
     * Each span is given a character from its own token text (first available
     * unique lowercase letter).  Spans for which no unique letter can be found
     * receive a null label.
     *
     * @param spans  Spans to label.  Modified in-place.
     * @param grid   Terminal grid (needed to read cell codepoints for label selection).
     */
    static void assignHintLabels (std::vector<LinkSpan>& spans, const Grid& grid) noexcept;

    /** @brief Session reference — provides grid, parser, state, and PTY write access. */
    Session& session;

    /** @brief Link spans for hover-underline mode.  Refreshed lazily. */
    std::vector<LinkSpan> clickableLinks;

    /** @brief Link spans for open-file hint-label mode.  Set on mode entry. */
    std::vector<LinkSpan> hintLinks;

    /** @brief `true` when the click-link cache is stale and needs rescanning. */
    bool scanNeeded { true };
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
