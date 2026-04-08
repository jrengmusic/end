/**
 * @file LinkSpan.h
 * @brief Data struct representing a single detected link span on the terminal viewport.
 *
 * A LinkSpan records the grid coordinates, resolved URI, type, and hint label
 * for one file path or URL found during an on-demand viewport scan.  It is
 * produced by `Terminal::Display::scanViewportForLinks()` and stored in
 * `Terminal::Display::activeLinks` for the duration of an open-file modal
 * session.
 *
 * @see LinkDetector
 * @see Terminal::Display::scanViewportForLinks
 */

#pragma once

#include <JuceHeader.h>
#include "LinkDetector.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @struct LinkSpan
 * @brief A detected link (file path or URL) found on the terminal viewport.
 *
 * Stores the grid location of the token's first character, its length in
 * columns, the resolved URI string, the classification type, and a short
 * hint label (one or two ASCII letters) shown in the open-file overlay.
 *
 * Hint labels are assigned by `scanViewportForLinks()` after all spans have
 * been collected:
 * - First 26 spans:   single character `'a'` … `'z'`.
 * - Spans 27 onward:  two characters `"aa"` … `"zz"`.
 *
 * @note Header-only — no linkage required.
 * @see LinkDetector::LinkType
 * @see Terminal::Display::scanViewportForLinks
 */
struct LinkSpan
{
    /** @brief Zero-based visible row index of the token's first character. */
    int row { 0 };

    /** @brief Zero-based column index of the token's first character. */
    int col { 0 };

    /** @brief Zero-based column index where the hint label is rendered. */
    int labelCol { 0 };

    /**
     * @brief Number of terminal columns the token occupies.
     *
     * Equal to the number of cells scanned (one codepoint each); wide
     * characters are not present in link tokens so each codepoint maps to
     * exactly one column.
     */
    int length { 0 };

    /**
     * @brief Resolved URI for this span.
     *
     * - For `LinkType::file`: an absolute `file:///` URI constructed by
     *   resolving the token against the shell's current working directory.
     * - For `LinkType::url`:  the raw token text (already a full URL).
     */
    juce::String uri;

    /** @brief Classification result from `LinkDetector::classify()`. */
    LinkDetector::LinkType type { LinkDetector::LinkType::none };

    /**
     * @brief Single-character hint label from the filename itself.
     *
     * The label is a character from the token's own name. First char is
     * preferred; if already taken by another span, subsequent chars are
     * tried until a unique one is found. `labelCol` indicates which column
     * the label character occupies. Null-terminated.
     */
    char hintLabel[2] {};
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
