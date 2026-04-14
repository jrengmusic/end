/**
 * @file jreng_gl_atlas_renderer.h
 * @brief GLRenderer subclass with atlas-based glyph and text rendering.
 *
 * `GLAtlasRenderer` extends `GLRenderer` with atlas texture lifecycle
 * management and instanced glyph rendering via `jreng::Glyph::GLContext`.
 * Atlas textures are stored on `jreng::Typeface` (mirroring the CPU atlas
 * pattern) and managed exclusively by this renderer.
 *
 * ### Ownership
 *
 * One `GLAtlasRenderer` instance exists per application window.  It owns
 * the GL atlas lifecycle for all typefaces passed at construction.
 * Atlas textures are created lazily on the GL thread (first
 * `uploadStagedBitmaps` call) and deleted in `contextClosing()`.
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
 * jreng::GLAtlasRenderer renderer { &monoTypeface, &bodyTypeface };
 * renderer.attachTo (targetComponent);
 * @endcode
 *
 * @see GLRenderer
 */
class GLAtlasRenderer : public GLRenderer
{
public:
    /**
     * @brief Constructs the renderer with a set of managed typefaces.
     *
     * Each typeface's GL atlas texture handles are managed by this renderer:
     * created lazily on the GL thread, deleted in `contextClosing()`.
     *
     * @param typefaces  Non-owning pointers to typefaces whose GL atlas
     *                   lifecycle this renderer manages.  Must outlive
     *                   this renderer (enforced by member declaration order).
     */
    GLAtlasRenderer (std::initializer_list<jreng::Typeface*> typefaces);

    ~GLAtlasRenderer() override;

protected:
    void contextReady() override;
    void contextClosing() override;
    void renderText (GLGraphics& g, const juce::Component* target,
                     GLComponent* comp, float totalScale, float vpHeight) override;

private:
    jreng::Glyph::GLContext glyphContext;
    std::vector<jreng::Typeface*> managedTypefaces;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLAtlasRenderer)
};

} // namespace jreng
