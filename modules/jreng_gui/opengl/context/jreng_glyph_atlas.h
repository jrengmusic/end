/**
 * @file jreng_glyph_atlas.h
 * @brief GL texture handle holder for the mono and emoji glyph atlases.
 *
 * `GlyphAtlas` is a pure resource holder: it stores the two GL texture handles
 * (`monoAtlas`, `emojiAtlas`) and the active `AtlasSize` preset.  It performs
 * no GL operations except in `rebuildAtlas()`, which deletes and zeroes both
 * handles on the GL thread.
 *
 * ### Ownership
 * A single `GlyphAtlas` instance lives on `MainComponent` and is shared by
 * all `GLAtlasRenderer` instances.  CPU-side image data stays on `Typeface`;
 * this class holds GL handles only.
 *
 * Inherits `jreng::Context<GlyphAtlas>` so any subsystem can call
 * `GlyphAtlas::getContext()` without threading references through ctors.
 *
 * ### Thread contract
 * All accessors and mutators: **MESSAGE THREAD**.
 * `rebuildAtlas()`:          **GL THREAD** only.
 */
#pragma once

namespace jreng
{ /*____________________________________________________________________________*/

/**
 * @class GlyphAtlas
 * @brief Holds the mono and emoji GL texture handles and the atlas size preset.
 *
 * Pure resource holder — no atomics, no listeners, no decisions, no history.
 * Lifetime is bound to its owner (`MainComponent`).
 *
 * Inherits `jreng::Context<GlyphAtlas>` — one instance registered at a time,
 * accessible via `GlyphAtlas::getContext()`.
 */
class GlyphAtlas : public jreng::Context<GlyphAtlas>
{
public:
    /**
     * @brief Constructor. Registers this instance as the active context.
     *        Both GL handles are zero (no texture allocated yet).
     */
    GlyphAtlas() noexcept = default;

    /** @brief Destructor. Unregisters this instance from the context. No GL teardown — call `rebuildAtlas()` on the GL thread first. */
    ~GlyphAtlas() override = default;

    /** @brief Returns the mono atlas GL texture handle. */
    uint32_t getMonoAtlas() const noexcept  { return monoAtlas; }

    /** @brief Returns the emoji atlas GL texture handle. */
    uint32_t getEmojiAtlas() const noexcept { return emojiAtlas; }

    /**
     * @brief Sets the mono atlas GL texture handle.
     * @param handle  GL texture name returned by `glGenTextures`.
     */
    void setMonoAtlas (uint32_t handle) noexcept  { monoAtlas = handle; }

    /**
     * @brief Sets the emoji atlas GL texture handle.
     * @param handle  GL texture name returned by `glGenTextures`.
     */
    void setEmojiAtlas (uint32_t handle) noexcept { emojiAtlas = handle; }

    /**
     * @brief Sets the atlas size preset used for the next rebuild.
     * @param size  `AtlasSize::standard` (4096) or `AtlasSize::compact` (2048).
     */
    void setAtlasSize (jreng::Glyph::AtlasSize size) noexcept { atlasSize = size; }

    /** @brief Returns the current atlas size preset. */
    jreng::Glyph::AtlasSize getAtlasSize() const noexcept { return atlasSize; }

    /**
     * @brief Deletes both GL textures and zeroes the handles.
     *
     * For each non-zero handle, calls `glDeleteTextures (1, &handle)` then
     * sets the handle to 0.  After this call both handles are 0 and new
     * textures must be allocated before rendering.
     *
     * @note **GL THREAD** — must be called from within a JUCE GL context callback.
     */
    void rebuildAtlas() noexcept;

private:
    uint32_t monoAtlas  { 0 };
    uint32_t emojiAtlas { 0 };
    jreng::Glyph::AtlasSize atlasSize { jreng::Glyph::AtlasSize::standard };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlyphAtlas)
};

} // namespace jreng
