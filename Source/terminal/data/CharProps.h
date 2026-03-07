/**
 * @file CharProps.h
 * @brief Packed Unicode character property lookup system for terminal emulation.
 *
 * This file provides O(1) character property lookups for terminal rendering,
 * including character width calculation, emoji detection, and grapheme cluster
 * segmentation per Unicode Standard Annex #29.
 *
 * The data is stored in bit-packed structures to maximize cache efficiency
 * and minimize memory bandwidth during text rendering operations.
 *
 * @sa CharPropsData.h for the lookup table definitions
 * @sa UAX #29: Unicode Text Segmentation
 */

#pragma once

#include <cstdint>
#include "CharPropsData.h"

// MESSAGE THREAD / ANY THREAD (read-only tables, no state)

/**
 * @struct CharProps
 * @brief Packed 32-bit per-codepoint Unicode character properties.
 *
 * This struct provides efficient access to Unicode character properties
 * stored in a single 32-bit word. Bit-packing is used to minimize memory
 * footprint and maximize cache efficiency during terminal text rendering.
 *
 * @par Bit Layout (little-endian, LSB first)
 * @code
 *  Bit  | Field
 *  -----|--------------------------
 *  [0]  | is_emoji_presentation
 *  [8:1]| padding (unused)
 *  [11:9]| shifted_width (3 bits)
 *  [12] | is_emoji
 *  [17:13]| category (5 bits)
 *  [18] | is_emoji_variation_base
 *  [19] | is_invalid
 *  [20] | is_non_rendered
 *  [21] | is_symbol
 *  [22] | is_combining_char
 *  [23] | is_word_char
 *  [24] | is_punctuation
 *  [28:25]| grapheme_break (4 bits)
 *  [30:29]| indic_conjunct_break (2 bits)
 *  [31] | is_extended_pictographic
 *
 * Top 7 bits [31:25] = grapheme_segmentation_property (used for state machine)
 * @endcode
 *
 * @par Why Bit-Packing?
 * - Cache efficiency: One cache line (64 bytes) holds 16 CharProps entries
 * - Memory bandwidth: Single memory access fetches all properties
 * - SIMD-friendly: Multiple properties can be processed in parallel
 *
 * @note This struct is trivially copyable and can be passed by value
 *       without performance penalty.
 *
 * @note Thread safety: This struct and its lookup functions are read-only
 *       after initialization. Any thread may access them without synchronization.
 */
struct CharProps
{
    uint32_t val { 0 };

    /**
     * @brief Returns the terminal display width of this character.
     *
     * Computes the monospace cell width for terminal display. This follows
     * the Unicode Width property (EAW - East Asian Width) where wide characters
     * (East Asian Fullwidth, Wide) have width 2, and most other printable
     * characters have width 1.
     *
     * @return Character width: typically 0 (combining), 1 (regular), or 2 (wide).
     *         Negative values may occur for specific combining marks.
     *
     * @par Example
     * @code
     * CharProps props = charPropsFor('A');  // width() returns 1
     * CharProps props = charPropsFor(0x4E00); // CJK char, width() returns 2
     * CharProps props = charPropsFor(0x0301); // combining accent, width() returns 0
     * @endcode
     */
    int width() const noexcept
    {
        return static_cast<int> ((val >> 9) & 0x7) - WIDTH_SHIFT;
    }

    /**
     * @brief Checks if this character should render as emoji by default.
     *
     * Returns true for characters that have Emoji_Presentation=Yes in Unicode,
     * meaning they should render as emoji glyphs rather than text glyphs
     * when no emoji variation selector is present.
     *
     * @return true if the character has default emoji rendering
     *
     * @par Example
     * @code
     * charPropsFor(0x2764).isEmojiPresentation()  // true (red heart)
     * charPropsFor(0x0041).isEmojiPresentation()  // false (letter A)
     * @endcode
     */
    bool isEmojiPresentation() const noexcept     { return val & 1; }

    /**
     * @brief Checks if this character is in the Unicode Emoji category.
     *
     * Returns true for characters that are officially classified as emoji
     * in the Unicode standard, regardless of whether they have text vs
     * emoji presentation.
     *
     * @return true if the character is an emoji character
     */
    bool isEmoji() const noexcept                  { return (val >> 12) & 1; }

    /**
     * @brief Checks if this character can serve as an emoji variation base.
     *
     * Returns true for characters that can be followed by variation selectors
     * to switch between text and emoji presentation (U+FE0E/0xFE0F).
     *
     * @return true if the character can be used with emoji variation selectors
     */
    bool isEmojiVariationBase() const noexcept  { return (val >> 18) & 1; }

    /**
     * @brief Checks if this codepoint is invalid (unassigned or surrogate).
     *
     * Returns true for codepoints that are either:
     * - Surrogate code points (U+D800-U+DFFF)
     * - Unassigned codepoints in the current Unicode version
     *
     * @return true if the codepoint should not be rendered
     */
    bool isInvalid() const noexcept                { return (val >> 19) & 1; }

    /**
     * @brief Checks if this character should not be visually rendered.
     *
     * Returns true for format control characters (CF category), unassigned
     * codepounds, and other non-rendering characters that should be hidden
     * in the terminal display.
     *
     * @return true if the character should not be displayed
     */
    bool isNonRendered() const noexcept            { return (val >> 20) & 1; }

    /**
     * @brief Checks if this character is a symbol.
     *
     * Returns true for characters in Unicode categories:
     * - Sm (Math symbols)
     * - Sc (Currency symbols)
     * - Sk (Modifier symbols)
     * - So (Other symbols)
     *
     * @return true if the character is a symbol
     */
    bool isSymbol() const noexcept                 { return (val >> 21) & 1; }

    /**
     * @brief Checks if this character is a combining character.
     *
     * Returns true for characters that modify other characters, including:
     * - Mn (Mark, Nonspacing)
     * - Mc (Mark, Spacing Combining)
     * - Me (Mark, Enclosing)
     *
     * These characters typically have zero width but affect the
     * appearance of preceding base characters.
     *
     * @return true if the character is a combining mark
     */
    bool isCombiningChar() const noexcept          { return (val >> 22) & 1; }

    /**
     * @brief Checks if this character is a word-forming character.
     *
     * Returns true for characters that should be treated as word characters
     * for word-wise navigation and selection in the terminal. This includes
     * letters, numbers, and underscore from all scripts.
     *
     * @return true if the character participates in word boundaries
     */
    bool isWordChar() const noexcept               { return (val >> 23) & 1; }

    /**
     * @brief Checks if this character is punctuation.
     *
     * Returns true for characters in Unicode punctuation categories:
     * - Pc (Punctuation, Connector)
     * - Pd (Punctuation, Dash)
     * - Ps (Punctuation, Open)
     * - Pe (Punctuation, Close)
     * - Pi (Punctuation, Initial quote)
     * - Pf (Punctuation, Final quote)
     * - Po (Punctuation, Other)
     *
     * @return true if the character is punctuation
     */
    bool isPunctuation() const noexcept            { return (val >> 24) & 1; }

    /**
     * @brief Checks if this character is extended pictographic.
     *
     * Returns true for characters listed in the Unicode
     * Extended_Pictographic property, used for emoji and icon display.
     * This is a broader set than isEmoji() as it includes legacy emoji
     * and pictographic symbols.
     *
     * @return true if the character is extended pictographic
     */
    bool isExtendedPictographic() const noexcept   { return (val >> 31) & 1; }

    /**
     * @brief Returns the 7-bit grapheme segmentation property.
     *
     * This is the key used for grapheme cluster boundary determination
     * per UAX #29. It combines the GraphemeBreakProperty (4 bits) with
     * additional flags for efficient state machine lookup.
     *
     * @return 7-bit value encoding the grapheme segmentation property
     */
    uint8_t graphemeSegmentationProperty() const noexcept
    {
        return static_cast<uint8_t> (val >> 25);
    }

    /**
     * @brief Returns the Unicode Grapheme Break property.
     *
     * Returns the GraphemeBreakProperty enum value indicating how this
     * character behaves in grapheme cluster boundary determination.
     * This follows Unicode Standard Annex #29.
     *
     * @return GraphemeBreakProperty value (AtStart, Control, Extend, ZWJ, etc.)
     *
     * @sa UAX #29: Unicode Text Segmentation
     */
    GraphemeBreakProperty graphemeBreak() const noexcept
    {
        return static_cast<GraphemeBreakProperty> ((val >> 25) & 0xF);
    }

    /**
     * @brief Returns the Indic Conjunct Break property.
     *
     * Returns the IndicConjunctBreak enum value used for complex
     * script rendering in Indic scripts (Devanagari, Bengali, etc.).
     *
     * @return IndicConjunctBreak value (None, Linker, Consonant, Extend)
     */
    IndicConjunctBreak indicConjunctBreak() const noexcept
    {
        return static_cast<IndicConjunctBreak> ((val >> 29) & 0x3);
    }

    /**
     * @brief Returns the Unicode General Category.
     *
     * Returns the UnicodeCategory enum value representing the
     * general category of this character (Lu, Ll, Nd, Po, etc.).
     *
     * @return UnicodeCategory value
     */
    UnicodeCategory category() const noexcept
    {
        return static_cast<UnicodeCategory> ((val >> 13) & 0x1F);
    }
};

// Ensure CharProps remains exactly 32 bits to maintain memory layout guarantees
static_assert (sizeof (CharProps) == sizeof (uint32_t), "CharProps must be 32 bits");

/**
 * @brief Performs 3-level multistage table lookup for character properties.
 *
 * This function implements an O(1) lookup from a Unicode codepoint to its
 * packed CharProps. A 3-level table structure is used instead of a direct
 * 1.1M-entry array to conserve memory while maintaining constant-time access.
 *
 * @par Multistage Table Structure
 * - Level 1 (T1): 4352 entries - coarse index from top 8 bits (cp >> 8)
 * - Level 2 (T2): 46592 entries - medium index combining T1 result + lower bits
 * - Level 3 (T3): Final CharProps value
 *
 * The lookup formula:
 * @code
 * T3[T2[(T1[cp >> 8] << 8) | (cp & 0xFF)]]
 * @endcode
 *
 * @par Branchless Clamp
 * Invalid codepoints (greater than MAX_UNICODE = 0x10FFFF) are clamped to 0
 * using a branchless technique:
 * @code
 * mask = (cp - (MAX_UNICODE + 1)) >> 63  // 0xFFFFFFFF if invalid, 0 if valid
 * cp = cp & mask                          // 0 if invalid, unchanged if valid
 * @endcode
 * This avoids branch misprediction penalties on hot paths.
 *
 * @param cp Unicode codepoint (0 to 0x10FFFF, or invalid values clamped)
 * @return CharProps for the given codepoint, or default properties for invalid codepoints
 *
 * @par Performance
 * - Time complexity: O(1), typically 3 array accesses
 * - Branchless: No conditional branches in the hot path
 * - Cache-friendly: T1 fits in L1 cache, T2/T3 accessed sequentially
 *
 * @sa CharPropsData.h for table definitions
 * @sa MAX_UNICODE for valid codepoint range
 */
static inline CharProps charPropsFor (uint32_t cp) noexcept
{
    // Branchless clamp: if cp > MAX_UNICODE, set to 0
    const int64_t diff { static_cast<int64_t> (cp) - static_cast<int64_t> (MAX_UNICODE + 1u) };
    const uint32_t mask { static_cast<uint32_t> (diff >> 63) };
    cp = cp & mask;

    return CharProps {
        charPropsT3[
            charPropsT2[
                (static_cast<uint32_t> (charPropsT1[cp >> CHAR_PROPS_SHIFT]) << CHAR_PROPS_SHIFT)
                + (cp & CHAR_PROPS_MASK)
            ]
        ]
    };
}

/**
 * @struct GraphemeSegmentationResult
 * @brief Packed 16-bit result from grapheme segmentation state machine.
 *
 * This struct holds the output of one step in the grapheme cluster
 * boundary determination state machine per UAX #29. It encodes both
 * the new state and whether the current character should be added
 * to the current cell (grapheme cluster) or start a new one.
 *
 * @par Bit Layout
 * @code
 *  Bit  | Field
 *  -----|--------------------------
 *  [9:0]| new_state (grapheme_break:4 + 5 boolean flags + padding)
 *  [10] | add_to_current_cell
 * [15:11]| unused
 * @endcode
 *
 * @par Grapheme Clusters
 * Grapheme clusters are user-perceived characters. They are important
 * for terminal applications because:
 * - Combining marks should not be counted as separate characters
 * - Emoji ZWJ sequences should stay together (family + ZWJ + person)
 * - Indic ligatures should be treated as units
 *
 * @note Thread safety: This struct is read-only after initialization.
 */
struct GraphemeSegmentationResult
{
    uint16_t val { 0 };

    /**
     * @brief Checks if this character belongs to the current grapheme cluster.
     *
     * @return true if the character should be added to the current cell;
     *         false if it starts a new grapheme cluster
     */
    bool addToCurrentCell() const noexcept { return (val >> 10) & 1; }

    /**
     * @brief Returns the state machine state after processing this character.
     *
     * @return 10-bit state value for the next state machine transition
     */
    uint16_t state() const noexcept { return val & 0x3FF; }
};

// Ensure GraphemeSegmentationResult remains exactly 16 bits
static_assert (sizeof (GraphemeSegmentationResult) == sizeof (uint16_t),
               "GraphemeSegmentationResult must be 16 bits");

/**
 * @brief Performs one state machine transition for grapheme segmentation.
 *
 * This function implements one step of the Unicode grapheme cluster
 * boundary determination algorithm per UAX #29. Given the previous state
 * and the current character's properties, it returns the new state and
 * whether a cluster boundary occurs.
 *
 * @par State Machine
 * The state machine uses a multistage lookup similar to charPropsFor():
 * - Key = (prev_state << 7) | graphemeSegmentationProperty
 * - Lookup in 3-level tables to get new state + boundary flag
 *
 * @param prev Previous grapheme segmentation state
 * @param cp CharProps for the current codepoint
 * @return GraphemeSegmentationResult with new state and addToCurrentCell flag
 *
 * @par Example
 * @code
 * auto state = graphemeSegmentationInit();
 * for (char32_t c : string) {
 *     auto result = graphemeSegmentationStep(state, charPropsFor(c));
 *     if (!result.addToCurrentCell()) {
 *         // Start new cell here - previous cluster is complete
 *     }
 *     state = result;
 * }
 * @endcode
 *
 * @sa UAX #29: Unicode Text Segmentation
 * @sa GraphemeBreakProperty for the possible character classes
 */
static inline GraphemeSegmentationResult graphemeSegmentationStep (
    GraphemeSegmentationResult prev, CharProps cp) noexcept
{
    const uint32_t key {
        (static_cast<uint32_t> (prev.state()) << 7)
        | static_cast<uint32_t> (cp.graphemeSegmentationProperty())
    };

    return GraphemeSegmentationResult {
        graphemeSegT3[
            graphemeSegT2[
                (static_cast<uint32_t> (graphemeSegT1[key >> GRAPHEME_SEG_SHIFT]) << GRAPHEME_SEG_SHIFT)
                + (key & GRAPHEME_SEG_MASK)
            ]
        ]
    };
}

/**
 * @brief Returns the initial state for grapheme segmentation.
 *
 * Creates the starting state for processing a new text run. The initial
 * state corresponds to "Positioning at Start" in UAX #29, where the
 * previous character is treated as if it were GBP=Any (matches everything)
 * to ensure the first character always starts a new cluster.
 *
 * @return GraphemeSegmentationResult in the initial "AtStart" state
 *
 * @par Initial State Value
 * The value 0 encodes:
 * - state() = 0 (AtStart state index)
 * - addToCurrentCell() = false (first char always starts new cell)
 *
 * @sa UAX #29: Unicode Text Segmentation
 */
static inline GraphemeSegmentationResult graphemeSegmentationInit() noexcept
{
    // AtStart state = GBP index 0, all flags false, add_to_current_cell = false
    return GraphemeSegmentationResult { 0 };
}
