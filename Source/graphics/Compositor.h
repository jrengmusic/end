/**
 * @file graphics/Compositor.h
 * @brief Render pipeline compositor — analogous to ProcessorChain.
 */
#pragma once
#include <JuceHeader.h>
#include "end/Model.h"
#include "config/Config.h"
#include "graphics/Program.h"

namespace graphics
{
/*____________________________________________________________________________*/

/** @brief Render pipeline compositor — analogous to ProcessorChain.
 *
 *  Owns two Compilation instances (background and post-processing),
 *  pipeline framebuffers, and the output shader. Orchestrates the
 *  three-layer render pipeline: background → JUCE components → post-processing.
 *
 *  Surface API mirrors TETRIS contract:
 *  - prepare() — initialise GL resources (= ProcessorChain::prepare)
 *  - process() — per-frame render loop (= ProcessorChain::process)
 *  - reset()   — release GL resources (= ProcessorChain::reset)
 *
 *  @par Thread contract
 *  All methods must be called on the **GL THREAD** unless noted otherwise.
 *  Setters (setOpacity, setTextureFilter, etc.) are called from the
 *  message thread — they write single values read by the GL thread.
 */
struct Compositor
{
    Compositor() = default;
    ~Compositor() = default;

    /** @brief Initialises GL resources — creates output shader program.
     *  @param ctx  Active GL context (stored for createProgram).
     */
    void prepare (juce::OpenGLContext& ctx);

    /** @brief Per-frame render loop — renders all layers to the default framebuffer.
     *
     *  Without post-processing: background → renderOutput (upscale to FB 0).
     *  With post-processing: background → compositeScene (bg + prev JUCE → sceneCapture)
     *  → post-process sceneCapture → FB 0.
     *
     *  @param quad  Fullscreen quad for drawing.
     */
    void process (Quad& quad);

    /** @brief Releases all GL resources — compilations, FBOs, output program. */
    void reset();

    /** @brief Compiles shader passes from the config subtree identified by @p treeType.
     *
     *  Selects the background or postProcess compilation based on treeType,
     *  reads shader source from config, compiles and links programs.
     *  Must be called on GL thread.
     *
     *  @param treeType  @c IDtype::background or @c IDtype::postProcessing.
     */
    void loadShaders (const juce::Identifier& treeType);

    /** @brief Updates viewport uniform and resizes all FBOs. GL thread only.
     *  @param screenWidth   Screen width in pixels (after DPI scaling).
     *  @param screenHeight  Screen height in pixels (after DPI scaling).
     */
    void resize (int screenWidth, int screenHeight);

    /** @brief Returns current screen dimensions from the background uniform. */
    juce::Point<int> getScreenSize() const { return background.uniform.getScreenSize(); }

    //==============================================================================
    // Setters — message thread safe (single-value writes read by GL thread)

    void setOpacity (float value);
    void setTextureFilter (GLenum value);
    void setResolutionScale (float value);
    void setFrameRate (int fps);

private:
    /** @brief Compiles vertex + fragment shader, links. Returns nullptr on failure. GL thread only. */
    std::unique_ptr<juce::OpenGLShaderProgram> createProgram (juce::StringRef shaderSource);

    /** @brief Upscales backgroundPass to FB 0 with opacity and filter. GL thread only. */
    void renderOutput (Quad& quad);

    /** @brief Composites background + previous frame JUCE components into sceneCapture.
     *
     *  Renders background texture into sceneCapture FBO with opacity blend,
     *  then blits FB 0 (previous frame's JUCE component painting) on top.
     *  Result: sceneCapture contains the composited scene for post-processing.
     *
     *  @param quad  Fullscreen quad for drawing.
     */
    void compositeScene (Quad& quad);

    /** @brief Renders post-processing passes from sceneCapture to FB 0. GL thread only.
     *  @param quad  Fullscreen quad for drawing.
     */
    void renderPostProcess (Quad& quad);

    //==============================================================================
    config::Model& config { *config::Model::getInstance() };
    end::Model& appModel { *end::Model::getInstance() };
    juce::OpenGLContext* context { nullptr };

    Compilation background;
    Compilation postProcess;

    /** @brief Background Image pass render target — buffer resolution, single FBO. GL thread only. */
    FrameBuffer backgroundPass;

    /** @brief Scene capture FBO — screen resolution, holds composited bg + JUCE for post-processing.
     *         Written in compositeScene(), read by post-processing as iScene texture. GL thread only. */
    FrameBuffer sceneCapture;

    /** @brief Output shader — upscales backgroundPass to screen with opacity and filter. GL thread only. */
    std::unique_ptr<juce::OpenGLShaderProgram> outputProgram;

    //==============================================================================
    static inline const juce::String placeholder { "source" };
    static inline const juce::String wrapper { BinaryData::getString ("wrapper.frag") };
    static inline const juce::String screenQuad { BinaryData::getString ("screen.vert") };
    static inline const juce::String outputShader { BinaryData::getString ("output.frag") };
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Compositor)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace graphics
