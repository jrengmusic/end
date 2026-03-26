/**
 * @file LinkManager.h
 * @brief Owns viewport link scanning, hit-testing, hint assignment, and dispatch.
 *
 * LinkManager centralises all link-related logic:
 *
 * - **scan()** — O(rows×cols) viewport scan for file and URL tokens, plus OSC 8
 *   hyperlink merging from the State HYPERLINKS ValueTree node.
 * - **scanForHints()** — full-viewport scan used on open-file mode entry; assigns
 *   single-character hint labels for the overlay.
 * - **hitTest()** — returns the first `LinkSpan` at the given (row, col) cell, or
 *   `nullptr` if none.
 * - **dispatch()** — opens URLs in the browser or sends the file path to the
 *   configured editor via the write callback.
 *
 * @see LinkSpan
 * @see LinkDetector
 */

#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include "LinkSpan.h"

namespace Terminal
{ /*____________________________________________________________________________*/

class Grid;
class State;

/**
 * @class LinkManager
 * @brief Owns link detection, hint labelling, hit-testing, and dispatch for one terminal session.
 *
 * Constructed with a `State&`, a `const Grid&`, and a write callback.  All
 * methods are MESSAGE THREAD only unless noted.
 *
 * @par Thread context
 * All public methods must be called on the **MESSAGE THREAD**.
 */
class LinkManager : public juce::ValueTree::Listener
{
public:
    /**
     * @brief Constructs a LinkManager with direct references to state and grid.
     *
     * @param state      Terminal parameter store — used for block bounds and screen queries.
     * @param grid       Terminal grid — read-only source for cell content.
     * @param writeToPty Callback that delivers raw bytes to the PTY writer.
     * @note MESSAGE THREAD.
     */
    LinkManager (State& state,
                 const Grid& grid,
                 std::function<void (const char*, int)> writeToPty) noexcept;
    ~LinkManager() override;

    /**
     * @brief Scans the viewport for file-path and URL tokens.
     *
     * Populates `clickableLinks` with all detected spans.
     *
     * @param cwd             Shell current working directory for relative-path resolution.
     * @param outputRowsOnly  When `true`, only rows inside the OSC 133 output block
     *                        are scanned (hover underline mode).  When `false`, all
     *                        visible rows are scanned (open-file hint mode).
     * @note MESSAGE THREAD.
     */
    void scan (const juce::String& cwd, bool outputRowsOnly);

    /**
     * @brief Scans the full viewport, paginates into 26-link pages, and labels page 0.
     *
     * Populates `hintLinks` with all detected spans (unlabeled), then computes
     * total pages, resets the page index to 0, and calls `assignCurrentPage()`
     * to label the first page into `activeHints`.
     *
     * @param cwd  Shell current working directory for relative-path resolution.
     * @note MESSAGE THREAD.
     */
    void scanForHints (const juce::String& cwd);

    /**
     * @brief Clears all hint-mode link spans and resets page state.
     *
     * Called when open-file mode exits.
     *
     * @note MESSAGE THREAD.
     */
    void clearHints() noexcept;

    /**
     * @brief Advances to the next hint page, wrapping around, and re-labels.
     *
     * Increments the page index modulo total pages, writes it to State,
     * and calls `assignCurrentPage()` to repopulate `activeHints`.
     *
     * @note MESSAGE THREAD.
     */
    void advanceHintPage() noexcept;

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
     * Searches `activeHints` (the current page's labeled subset).
     *
     * @param label  Single lowercase letter `'a'`–`'z'` to match against hint labels.
     * @return Pointer into `activeHints`, or `nullptr` if no span matches.
     * @note MESSAGE THREAD.  Pointer is valid until the next `scanForHints()` or `advanceHintPage()` call.
     */
    const LinkSpan* hitTestHint (char label) const noexcept;

    /**
     * @brief Dispatches the link: opens URLs in the browser or writes the file
     *        path to the PTY editor command.
     *
     * File dispatch reads the configured editor from Config and constructs a
     * `"<editor> <path>\r"` command delivered through the stored write callback.
     * URL dispatch calls `juce::URL::launchInDefaultBrowser()`.
     *
     * @param span  The link to dispatch.
     * @note MESSAGE THREAD.
     */
    void dispatch (const LinkSpan& span) const;

    /**
     * @brief Callback invoked when a .md file link is activated.
     *
     * When set, intercepts file dispatch for `.md` extensions before the
     * default editor-launch logic runs.
     *
     * @note MESSAGE THREAD.
     */
    mutable std::function<void (const juce::File&)> onOpenMarkdown;

    /**
     * @brief Returns the current clickable-link spans (for hover underline).
     * @return Read-only reference to the internal vector.
     * @note MESSAGE THREAD.
     */
    const std::vector<LinkSpan>& getClickableLinks() const noexcept;

    /**
     * @brief Returns the current hint-mode link spans (full unfiltered list).
     * @return Read-only reference to the internal vector.
     * @note MESSAGE THREAD.
     */
    const std::vector<LinkSpan>& getHintLinks() const noexcept;

    /**
     * @brief Returns the current page's labeled hint spans.
     *
     * These are the spans passed to `Screen::setHintOverlay()`.  Valid until
     * the next `scanForHints()` or `advanceHintPage()` call.
     *
     * @return Read-only reference to the active-page hint vector.
     * @note MESSAGE THREAD.
     */
    const LinkSpan* getActiveHintsData() const noexcept;
    int getActiveHintsCount() const noexcept;

private:
    /**
     * @brief Shared scan implementation.
     *
     * Scans `grid` for file/URL tokens and OSC 8 hyperlinks from the State
     * HYPERLINKS ValueTree.  Optionally restricted to the OSC 133 output block.
     * Returns the collected spans without hint label assignment.
     *
     * @param cwd             Shell current working directory.
     * @param outputRowsOnly  When `true`, restrict to the OSC 133 block.
     * @return Vector of detected spans without hint labels.
     */
    std::vector<LinkSpan> scanViewport (const juce::String& cwd,
                                        bool outputRowsOnly) const;

    /**
     * @brief Assigns unique single-character hint labels to `spans` in-place.
     *
     * Each span is given a character from its own token text (first available
     * unique lowercase letter).  Spans for which no unique letter can be found
     * receive a null label (to be filled by `assignCurrentPage`'s fallback pass).
     *
     * @param spans  Spans to label.  Modified in-place.
     */
    void assignHintLabels (std::vector<LinkSpan>& spans) noexcept;

    /**
     * @brief Builds page boundaries from `hintLinks` based on character availability.
     *
     * Greedily assigns labels from each span's own characters. When all 26
     * letters are exhausted, starts a new page. Stores boundaries in `pageBreaks`.
     *
     * @note MESSAGE THREAD.
     */
    void buildPages() noexcept;

    /**
     * @brief Labels the current page's spans and stores them in `activeHints`.
     *
     * Reads page index from State, slices `hintLinks` using `pageBreaks`,
     * and assigns labels via `assignHintLabels`.
     *
     * @note MESSAGE THREAD.
     */
    void assignCurrentPage() noexcept;

    /** @brief Terminal parameter store — provides block bounds and screen queries. */
    State& state;

    /** @brief Terminal grid — read-only source for cell content during scans. */
    const Grid& grid;

    /** @brief Delivers raw bytes to the PTY (used by `dispatch()` for file links). */
    std::function<void (const char*, int)> writeToPty;

    /** @brief Link spans for hover-underline mode.  Refreshed by ValueTree listener. */
    std::vector<LinkSpan> clickableLinks;

    /** @brief All link spans for open-file hint-label mode.  Full unfiltered list set on mode entry. */
    std::vector<LinkSpan> hintLinks;

    /** @brief Start index of the current page in `hintLinks`. */
    int activeStart { 0 };

    /** @brief Number of spans in the current page. */
    int activeCount { 0 };

    /** @brief Page boundary indices into `hintLinks`.  Built by `buildPages()`. */
    std::vector<int> pageBreaks;

    /** @brief Cached reference to the promptRow PARAM node for direct listening. */
    juce::ValueTree promptRowNode;

    /** @brief Cached reference to the activeScreen PARAM node for direct listening. */
    juce::ValueTree activeScreenNode;

    /** @brief Cached reference to the HYPERLINKS container node for direct listening. */
    juce::ValueTree hyperlinksNode;

    /** @brief Cached reference to the scrollOffset PARAM node for direct listening. */
    juce::ValueTree scrollOffsetNode;

    /** @brief Cached reference to the outputBlockBottom PARAM node for rescan during output. */
    juce::ValueTree outputBlockBottomNode;

    // =========================================================================
    // ValueTree::Listener
    // =========================================================================

    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
