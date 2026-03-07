/**
 * @file CSI.h
 * @brief CSI (Control Sequence Introducer) parameter accumulator for VT terminal emulation.
 *
 * CSI sequences are escape sequences that begin with ESC '[' (0x1B 0x5B) and control
 * cursor movement, text attributes, and other terminal functions. They follow the format:
 *
 * @code
 * ESC [ <params> <intermediates> <final>
 * @endcode
 *
 * Where <params> is a semicolon-separated list of decimal numbers, e.g.:
 * - `ESC[1;34m` — set bold (1) and blue foreground (34)
 * - `ESC[10;20H` — move cursor to row 10, column 20
 * - `ESC[38:2:255:0:0m` — set 24-bit RGB color (extended sub-parameter syntax)
 *
 * This struct accumulates parameters as they are parsed character-by-character
 * by the VT state machine. It supports both standard semicolon separators (`;`)
 * and the extended colon sub-separator syntax (`:`) used in ISO 8613-6 / ITU T.416.
 *
 * @see ParserCSI.cpp for CSI dispatch implementation
 * @see https://invisible-island.net/xterm/ctlseqs/ctlseqs.html for xterm CSI reference
 */

#pragma once

#include <JuceHeader.h>
#include <array>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @struct CSI
 * @brief Accumulator for CSI (Control Sequence Introducer) parameters during parsing.
 *
 * The VT parser feeds characters into this accumulator as it processes a CSI sequence.
 * The accumulator builds up numeric parameters digit-by-digit, stores them in a fixed
 * array, and tracks which separators were sub-separators (colon) vs. standard (semicolon).
 *
 * @par Usage Pattern
 * The parser calls methods in this sequence:
 * 1. `addDigit()` — for each numeric character '0'-'9' in the parameter string
 * 2. `addSeparator()` — for each ';' or ':' between parameters
 * 3. `finalize()` — after all parameter characters have been consumed
 * 4. `param()` — during dispatch to retrieve individual parameter values
 * 5. `reset()` — to prepare for the next CSI sequence
 *
 * @par Parameter Separators
 * Standard CSI uses semicolon (`;`) to separate parameters. The extended syntax
 * (ISO 8613-6) allows colon (`:`) as a sub-separator for hierarchical parameters.
 * For example, `38:2:255:0:0` uses colons to group the RGB color sub-parameters.
 * The `subSeparatorBits` field tracks which parameter positions had colons.
 *
 * @par Default Values
 * Omitted parameters default to specific values depending on the command:
 * - `ESC[H` (cursor home) treats missing row/col as 1
 * - `ESC[m` (SGR reset) treats missing params as 0
 * The `param()` method accepts a default value for this reason.
 *
 * @par Memory Layout
 * The struct is designed to be trivially copyable so it can be safely memcpy'd
 * or used in lock-free contexts. Total size is approximately 64 bytes:
 * - `values`: 24 × 2 bytes = 48 bytes
 * - `subSeparatorBits`: 4 bytes
 * - `count`: 1 byte
 * - `accumulator`: 2 bytes
 * - `hasAccumulator`: 1 byte
 * - Padding: ~8 bytes
 *
 * @invariant `count` is always <= MAX_PARAMS
 * @invariant `hasAccumulator` implies `accumulator` holds the current parameter value
 * @invariant After `finalize()`, `hasAccumulator` is always false
 *
 * @note Thread Safety: This struct is not thread-safe. It is used exclusively
 *       by the READER THREAD during parsing.
 *
 * @see Parser for the state machine that drives this accumulator
 * @see ParserCSI.cpp for CSI sequence dispatch
 */
struct CSI
{
    /**
     * @brief Maximum number of parameters that can be stored.
     *
     * xterm supports up to 16 parameters for some commands, but we allocate 24
     * to handle edge cases and future extensions. This matches common terminal
     * emulator implementations.
     */
    static constexpr uint8_t MAX_PARAMS { 24 };

    /**
     * @brief Array of parameter values accumulated during parsing.
     *
     * Each element holds a parsed numeric parameter. The array is zero-initialized.
     * Unused slots (index >= count) contain zero but should not be accessed.
     *
     * @sa count for the number of valid entries
     * @sa param() for safe access with default values
     */
    std::array<uint16_t, MAX_PARAMS> values {};

    /**
     * @brief Bitmask tracking which parameters were preceded by a colon sub-separator.
     *
     * Bit N is set if the separator before parameter N was ':' rather than ';'.
     * For example, in `38:2:255:0:0`, bits 1, 2, 3, 4 would be set (indices 1-4
     * had colons before them).
     *
     * This enables dispatch handlers to distinguish between:
     * - `38;5;1` (indexed color: 38 ; 5 ; palette_index)
     * - `38:2::255:0:0` (RGB color: 38 : 2 : : R : G : B)
     *
     * @sa isSubSeparator() to check a specific parameter position
     */
    uint32_t subSeparatorBits { 0 };

    /**
     * @brief Number of parameters currently stored in `values`.
     *
     * This count includes parameters that were implicitly specified by a trailing
     * separator. For example, `ESC[1;;3m` produces three parameters: 1, 0, 3.
     *
     * @invariant count <= MAX_PARAMS
     */
    uint8_t  count { 0 };

    /**
     * @brief Temporary accumulator for building the current parameter value.
     *
     * As digits are fed via `addDigit()`, this value grows: '1' -> 1, '12' -> 12, etc.
     * When a separator or final byte is encountered, this value is copied to `values`
     * and the accumulator is reset.
     *
     * @sa hasAccumulator indicates whether any digits have been accumulated
     */
    uint16_t accumulator { 0 };

    /**
     * @brief Flag indicating whether `accumulator` contains valid data.
     *
     * This is false until the first digit is added. It distinguishes between:
     * - No parameter specified (hasAccumulator = false) -> use default
     * - Parameter is zero (hasAccumulator = true, accumulator = 0)
     *
     * For example, in `ESC[;5m`, the first parameter has no digits, so
     * hasAccumulator is false when the separator is encountered.
     */
    bool     hasAccumulator { false };

    /**
     * @brief Adds a digit to the current parameter accumulator.
     *
     * Called by the parser for each numeric character '0'-'9' in the CSI parameter
     * string. The digit is incorporated into `accumulator` using base-10 arithmetic.
     *
     * @par Overflow Behavior
     * If the accumulated value exceeds 65535 (uint16_t max), it wraps modulo 65536.
     * This matches xterm behavior for excessively large parameters.
     *
     * @param digit  The numeric value of the digit character (0-9).
     *               The parser is responsible for converting '0'-'9' to 0-9.
     *
     * @note READER THREAD only.
     *
     * @par Example
     * @code
     * // Parsing "123"
     * addDigit(1);  // accumulator = 1
     * addDigit(2);  // accumulator = 12
     * addDigit(3);  // accumulator = 123
     * @endcode
     */
    void addDigit (uint8_t digit) noexcept
    {
        accumulator = static_cast<uint16_t> (accumulator * 10 + digit);
        hasAccumulator = true;
    }

    /**
     * @brief Records a parameter separator and stores the accumulated value.
     *
     * Called when the parser encounters ';' or ':' in the CSI parameter string.
     * If a value was being accumulated, it is stored in `values`. The separator
     * type (colon vs. semicolon) is recorded in `subSeparatorBits`.
     *
     * @par Empty Parameters
     * If `hasAccumulator` is false (no digits since last separator), the value
     * stored is zero (from initialization). This handles sequences like `ESC[;;m`
     * where parameters are omitted.
     *
     * @par Parameter Limit
     * If `count` is already at MAX_PARAMS, the separator and any accumulated
     * value are silently discarded. This prevents buffer overflow on malformed
     * or maliciously long sequences.
     *
     * @param separatorByte  The separator character: ';' for standard separator,
     *                       ':' for sub-separator (extended syntax).
     *
     * @note READER THREAD only.
     *
     * @par Example
     * @code
     * // Parsing "1;34"
     * addDigit(1);         // accumulator = 1
     * addSeparator(';');   // values[0] = 1, count = 1
     * addDigit(3);         // accumulator = 3
     * addDigit(4);         // accumulator = 34
     * // finalize() will store values[1] = 34
     * @endcode
     *
     * @par Sub-separator Example
     * @code
     * // Parsing "38:2"
     * addDigit(3); addDigit(8);  // accumulator = 38
     * addSeparator(':');         // values[0] = 38, bit 0 set
     * addDigit(2);               // accumulator = 2
     * // isSubSeparator(0) returns true
     * @endcode
     */
    void addSeparator (uint8_t separatorByte) noexcept
    {
        if (count < MAX_PARAMS)
        {
            if (hasAccumulator)
            {
                values.at (count) = accumulator;
            }

            if (separatorByte == ':')
            {
                subSeparatorBits |= (1u << count);
            }

            ++count;
            accumulator = 0;
            hasAccumulator = false;
        }
    }

    /**
     * @brief Finalizes parameter accumulation after all parameter characters are consumed.
     *
     * Called by the parser after the CSI parameter string is complete (typically
     * when the final byte is encountered). If a value was being accumulated, it
     * is stored as the final parameter.
     *
     * This method is idempotent — calling it multiple times has no additional
     * effect after the first call (until `reset()` is called).
     *
     * @par Empty Sequences
     * If no parameters were specified (e.g., `ESC[m`), this method does nothing
     * and `count` remains zero. Dispatch handlers typically treat this as a
     * default case (e.g., SGR reset).
     *
     * @par Single Parameter
     * For `ESC[5m`, after `addDigit(5)`, `finalize()` stores the value:
     * @code
     * // Before: hasAccumulator = true, accumulator = 5, count = 0
     * finalize();
     * // After: hasAccumulator = false, values[0] = 5, count = 1
     * @endcode
     *
     * @note READER THREAD only.
     * @note Must be called before `param()` is used during dispatch.
     */
    void finalize() noexcept
    {
        if (hasAccumulator and count < MAX_PARAMS)
        {
            values.at (count) = accumulator;
            ++count;
            accumulator = 0;
            hasAccumulator = false;
        }
    }

    /**
     * @brief Resets the accumulator to initial state for a new CSI sequence.
     *
     * Called after a CSI sequence has been fully processed (dispatched) to prepare
     * for the next sequence. All fields are reset to their default values.
     *
     * @par Performance
     * This method uses `std::array::fill()` which is typically optimized to
     * a memset or vectorized store. The struct is small enough that this
     * is negligible in the parsing path.
     *
     * @note READER THREAD only.
     * @note Must be called before reusing the accumulator for a new sequence.
     */
    void reset() noexcept
    {
        values.fill (uint16_t { 0 });
        subSeparatorBits = 0;
        count = 0;
        accumulator = 0;
        hasAccumulator = false;
    }

    /**
     * @brief Retrieves a parameter value by index with a default fallback.
     *
     * Used by CSI dispatch handlers to safely access parameter values. If the
     * requested index is out of range (>= count), the default value is returned.
     *
     * @par Default Value Semantics
     * The default value depends on the specific CSI command:
     * - Cursor positioning (CUP, HVP): default is 1 (first row/column)
     * - SGR (text attributes): default is 0 (reset/normal)
     * - Erase (ED, EL): default is 0 (erase below/to right)
     *
     * The caller (dispatch handler) is responsible for choosing the appropriate
     * default value based on the command being processed.
     *
     * @param index         Zero-based parameter index (0 = first parameter).
     * @param defaultValue  Value to return if the parameter was not specified.
     *
     * @return The parameter value at `index`, or `defaultValue` if index >= count.
     *
     * @note READER THREAD only.
     * @note `finalize()` must be called before using this method.
     *
     * @par Example
     * @code
     * // For "ESC[5m", count = 1, values[0] = 5
     * param(0, 0);  // returns 5
     * param(1, 0);  // returns 0 (default, index out of range)
     *
     * // For "ESC[m", count = 0
     * param(0, 0);  // returns 0 (default)
     * @endcode
     */
    uint16_t param (uint8_t index, uint16_t defaultValue) const noexcept
    {
        uint16_t result { defaultValue };

        if (index < count)
        {
            result = values.at (index);
        }

        return result;
    }

    /**
     * @brief Checks whether a parameter position was preceded by a colon sub-separator.
     *
     * Used to distinguish between standard semicolon-separated parameters and
     * the extended colon-separated sub-parameter syntax (ISO 8613-6).
     *
     * @par Extended Color Example
     * @code
     * // "38:2:255:0:0" - 24-bit RGB foreground
     * // isSubSeparator(0) = false (38 was first, no separator before it)
     * // isSubSeparator(1) = true  (colon before 2)
     * // isSubSeparator(2) = true  (colon before 255)
     * // isSubSeparator(3) = true  (colon before 0)
     * // isSubSeparator(4) = true  (colon before 0)
     * @endcode
     *
     * @param index  Zero-based parameter index to check.
     *
     * @return true if the separator before this parameter was ':', false if it was
     *         ';' or if this is the first parameter (index 0).
     *
     * @note READER THREAD only.
     *
     * @see subSeparatorBits for the underlying bitmask
     */
    bool isSubSeparator (uint8_t index) const noexcept
    {
        return (subSeparatorBits & (1u << index)) != 0;
    }
};

/**
 * @brief Static assertion that CSI is trivially copyable.
 *
 * This ensures the struct can be safely memcpy'd, stored in lock-free
 * data structures, and has no hidden constructors or destructors that
 * could cause surprising behavior.
 */
static_assert (std::is_trivially_copyable_v<CSI>,
               "CSI must be trivially copyable");

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
