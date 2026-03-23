/**
 * @file jreng_glyph_key.h
 * @brief Unique identity of a rasterized glyph in the atlas.
 */

#pragma once

#include <cstdint>
#include <functional>

namespace jreng::Glyph
{

/**
 * @struct Key
 * @brief Unique identity of a rasterized glyph in the atlas.
 *
 * A Key fully identifies one rasterized glyph instance.  Two glyphs with
 * the same codepoint but different font faces, sizes, or spans produce
 * distinct keys and are stored as separate atlas entries.
 *
 * @note Must be used on the **MESSAGE THREAD** only.
 *
 * @see LRUCache
 * @see GlyphAtlas
 */
// MESSAGE THREAD
struct Key
{
    /** @brief FreeType / CoreText glyph index within the font face. */
    uint32_t glyphIndex { 0 };

    /**
     * @brief Opaque font handle.
     *
     * On macOS this is a `CTFontRef`; on Linux/Windows it is an `FT_Face`.
     * Stored as `void*` to keep the header platform-agnostic.
     */
    void* fontFace { nullptr };

    /** @brief Logical point size at which the glyph was rasterized. */
    float fontSize { 0.0f };

    /**
     * @brief Number of cells/units the glyph spans horizontally.
     *
     * Wide characters (e.g. CJK) span 2 cells; most glyphs span 1.
     * Nerd Font icons may span 1 or 2 depending on their constraint.
     */
    uint8_t span { 0 };

    /**
     * @brief Equality comparison for use as an unordered_map key.
     * @param other The key to compare against.
     * @return `true` if all four fields are identical.
     */
    bool operator== (const Key& other) const noexcept
    {
        return glyphIndex == other.glyphIndex
            and fontFace == other.fontFace
            and fontSize == other.fontSize
            and span == other.span;
    }
};

} // namespace jreng::Glyph

namespace std
{
    /**
     * @brief std::hash specialization for jreng::Glyph::Key.
     *
     * Combines hashes of all four fields with bit-shifted XOR to reduce
     * collisions across the typical glyph-key distribution.
     */
    template<>
    struct hash<jreng::Glyph::Key>
    {
        /**
         * @brief Compute hash for a jreng::Glyph::Key.
         * @param key The key to hash.
         * @return Combined hash value.
         */
        size_t operator() (const jreng::Glyph::Key& key) const noexcept
        {
            return std::hash<uint32_t>{}(key.glyphIndex)
                 ^ (std::hash<const void*>{}(key.fontFace) << 1)
                 ^ (std::hash<float>{}(key.fontSize) << 2)
                 ^ (std::hash<uint8_t>{}(key.span) << 3);
        }
    };
}
