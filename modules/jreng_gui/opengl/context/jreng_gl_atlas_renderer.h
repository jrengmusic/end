/**
 * @file jreng_gl_atlas_renderer.h
 * @brief GLRenderer subclass with atlas-based glyph and text rendering.
 *
 * `GLAtlasRenderer` extends `GLRenderer` with atlas texture lifecycle
 * management and instanced glyph rendering via `jreng::Glyph::GLContext`.
 * Atlas texture GL handles are stored on `jreng::GlyphAtlas` and accessed via
 * `GlyphAtlas::getContext()`.
 *
 * ### Ownership
 *
 * One `GLAtlasRenderer` instance exists per application window.  It holds a
 * non-owning reference to `Typeface` (CPU bitmaps).  Atlas textures are created
 * lazily on the GL thread (first `uploadStagedBitmaps` call).  The atlas is a
 * process-lifetime resource; `GlyphAtlas::rebuildAtlas()` handles teardown.
 *
 * ### Thread contract
 *
 * Construction and typeface registration happen on the **MESSAGE THREAD**.
 * All GL operations (`contextReady`, `contextClosing`, `renderText`)
 * run on the **GL THREAD**.
 *
 * @see GLRenderer
 * @see jreng::Glyph::GLContext
 * @see jreng::Typeface
 */
#pragma once

namespace jreng
{

/**
 * @class GLAtlasRenderer
 * @brief GLRenderer with atlas-based glyph rendering for text-capable applications.
 *
 * Owns a `jreng::Glyph::GLContext` for instanced glyph rendering (used by
 * `GLGraphics` text commands via the `renderText` hook).  Manages GL atlas
 * texture lifecycle on the typefaces passed at construction.
 *
 * @par Usage
 * @code
 * jreng::GLAtlasRenderer renderer { typeface };
 * renderer.attachTo (targetComponent);
 * @endcode
 *
 * @see GLRenderer
 */
class GLAtlasRenderer : public GLRenderer
{
public:
    /**
     * @brief Constructs the renderer with a typeface.
     *
     * `GlyphAtlas` is accessed via `GlyphAtlas::getContext()` — no reference
     * threading required.
     *
     * @param typeface  Non-owning reference to the typeface whose staged
     *                  bitmaps are uploaded each frame.  Must outlive this
     *                  renderer.
     */
    explicit GLAtlasRenderer (jreng::Typeface& typeface);

    ~GLAtlasRenderer() override = default;

protected:
    void contextReady() override;
    void contextClosing() override;
    void renderText (GLGraphics& g, const juce::Component* target,
                     GLComponent* comp, float totalScale, float vpHeight) override;

private:
    jreng::Typeface&        typefaceRef;
    jreng::Glyph::GLContext glyphContext;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLAtlasRenderer)
};

} // namespace jreng
