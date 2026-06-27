/**
 * @file graphics/Compositor.h
 * @brief Shader renderer — background and post-processing pipelines.
 */
#pragma once
#include <JuceHeader.h>
#include "graphics/Program.h"

namespace graphics
{
/*____________________________________________________________________________*/

/** @brief Shader renderer — background and post-processing pipelines.
 *
 *  Dumb renderer with zero upstream visibility. Receives shader source trees and
 *  parameter values through its public API from Processor. Does not access
 *  config::Model or end::Model.
 *
 *  Renders background shader (multi-pass Shadertoy) to FB 0. When post-processing
 *  is active, intercepts component painting via CachedImage: composites background
 *  and components into a scene FBO, feeds it to the post-processing Compilation
 *  (shaders[ID::postProcessing]), then outputs the result to JUCE's cachedImageFrameBuffer.
 *
 *  Surface API mirrors TETRIS contract:
 *  - prepare() — initialise GL resources
 *  - process() — per-frame render loop
 *  - reset()   — release GL resources
 *
 *  @par Thread contract
 *  prepare() / process() / reset() : GL THREAD.
 *  attach() / detach() / isAttached() / triggerRepaint() : MESSAGE THREAD.
 *  compileShaders() / resize() dispatch to GL thread internally — message thread safe.
 *  Setters are message-thread safe (single-value writes).
 */
struct Compositor
{
    explicit Compositor (juce::OpenGLRenderer& rendererRef);
    ~Compositor() = default;

    //==============================================================================
    // Lifecycle — owns GL context

    /** @brief Configures the GL context, attaches it to @p view, and installs
     *  CachedImage on @p cacheTarget.
     *
     *  Sets OpenGL 4.1, enables multisampling, registers the stored renderer,
     *  attaches to @p view. Message thread.
     *
     *  @param view         The GL-attached root component (end::View).
     *  @param cacheTarget  The child whose painting gets intercepted for post-processing.
     */
    void attach (juce::Component& view, juce::Component& cacheTarget);

    /** @brief Detaches the GL context. Message thread. */
    void detach();

    /** @brief Returns true if the GL context is currently attached. */
    bool isAttached() const noexcept;

    /** @brief Triggers a GL repaint cycle. Message thread. */
    void triggerRepaint();

    //==============================================================================
    // TETRIS API — GL thread only

    /** @brief Creates the fullscreen Quad and compiles the output program.
     *         Initialises background FBOs from target component dimensions.
     *         GL thread only.
     */
    void prepare();

    /** @brief Per-frame render loop — renders background to FB 0 (if compiled).
     *         Components are painted by JUCE after this returns.
     *         GL thread only.
     */
    void process();

    /** @brief Releases all GL resources. GL thread only. */
    void reset();

    //==============================================================================
    /** @brief CachedComponentImage subclass installed on the cache target component.
     *  Intercepts painting for post-processing. Passthrough when post-processing is inactive.
     */
    struct CachedImage : juce::CachedComponentImage
    {
        Compositor& compositor;
        juce::Component& owner;

        CachedImage (Compositor& c, juce::Component& o) : compositor (c), owner (o) {}

        void paint (juce::Graphics& g) override
        {
            if (compositor.isAttached() and compositor.isPostProcessing())
                compositor.renderPost (owner, g);
            else
                owner.paintEntireComponent (g, false);
        }

        bool invalidateAll() override { return true; }
        bool invalidate (const juce::Rectangle<int>&) override { return true; }
        void releaseResources() override {}
    };

    /** @brief Returns true if post-processing shaders are compiled. */
    bool isPostProcessing() const noexcept;

    /** @brief Renders the post-processing pipeline. Called from CachedImage::paint.
     *  @param owner  The component whose subtree is being post-processed.
     *  @param g      The Graphics context targeting JUCE's cachedImageFrameBuffer.
     */
    void renderPost (juce::Component& owner, juce::Graphics& g);

    //==============================================================================
    // Config-driven — dispatch to GL thread internally

    /** @brief Compiles shader passes into the Compilation identified by @p id.
     *
     *  Dispatches compilation to the GL thread. After compilation, resizes
     *  FBOs to match the current target component dimensions. On failure,
     *  calls reportError (if set) from the GL thread.
     *  Message thread safe.
     *
     *  @param id       Compilation identifier (ID::background or ID::postProcessing).
     *  @param sources  Pass sources keyed by Identifier (Image, BufferA, etc.).
     */
    void compileShaders (const juce::Identifier& id,
                         const jam::HashMap<juce::Identifier, juce::String>& sources);

    /** @brief Dispatches FBO resize to the GL thread.
     *         Reads current component dimensions at execution time.
     *         Message thread safe.
     */
    void resize();

    //==============================================================================
    // Message-thread setters

    /** @brief Sets the background opacity. @param value Opacity in [0, 1]. */
    void setOpacity (float value);

    /** @brief Sets the post-processing effect intensity. @param value 0.0 = original, 1.0 = full effect. */
    void setPostOpacity (float value);

    /** @brief Sets the background texture filter. @param value GL_LINEAR or GL_NEAREST. */
    void setTextureFilter (GLenum value);

    /** @brief Sets the background resolution scale and dispatches a resize.
     *  @param value Scale factor (e.g. 0.5 = half resolution).
     */
    void setResolutionScale (float value);

    /** @brief Sets the target frame rate. @param fps Frames per second. */
    void setFrameRate (int fps);

    //==============================================================================
    // Query

    /** @brief Returns current screen dimensions from background uniform. */
    juce::Point<int> getScreenSize() const { return shaders.at (ID::background)->uniform.getScreenSize(); }

    //==============================================================================
    // Error reporting — called from GL thread on shader compilation failure.

    /** @brief Called with the error string when createProgram fails. Set by Processor. */
    std::function<void (const juce::String&)> reportError;

private:
    /** @brief Returns the target component's pixel dimensions (logical × rendering scale).
     *  @param component  The target component (must not be null).
     */
    end::Size getSize (const juce::Component& component) const;

    /** @brief Resizes all FBOs from the current target component dimensions. GL thread only. */
    void resizeBuffers();

    /** @brief Compiles vertex + fragment shader. Returns nullptr on failure.
     *         On failure, calls reportError (if set) with the error message. GL thread only.
     *  @param shaderSource  GLSL fragment shader source.
     */
    std::unique_ptr<juce::OpenGLShaderProgram> createProgram (juce::StringRef shaderSource);

    /** @brief Renders a Compilation's output to the current FBO.
     *  Sets viewport, reads texture/opacity/filter from the Compilation's uniform. GL thread only.
     *  @param id      Compilation identifier in the shaders map.
     *  @param width   Viewport width in pixels.
     *  @param height  Viewport height in pixels.
     */
    void renderTexture (const juce::Identifier& id, int width, int height);

    //==============================================================================
    juce::OpenGLRenderer& renderer;
    juce::OpenGLContext context;

    jam::HashMap<juce::Identifier, std::unique_ptr<Compilation>> shaders;

    /** @brief Composited scene (background + components) for post-processing input. Screen resolution. GL thread only. */
    juce::OpenGLFrameBuffer componentCapture;

    /** @brief Output shader — renders background output to screen with opacity and filter. GL thread only. */
    std::unique_ptr<juce::OpenGLShaderProgram> outputProgram;

    /** @brief Fullscreen quad VAO+VBO. Created in prepare(). */
    std::unique_ptr<Quad> quad;

    //==============================================================================
    /** @brief Compilation pipeline identifiers — drives prepare() and any future per-pipeline iteration. */
    static inline const juce::Identifier compilationIDs[] { ID::background, ID::postProcessing };

    static inline const juce::String placeholder { "source" };
    static inline const juce::String wrapper { BinaryData::getString ("wrapper.frag") };
    static inline const juce::String screenQuad { BinaryData::getString ("screen.vert") };
    static inline const juce::String outputShader { BinaryData::getString ("output.frag") };
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Compositor)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace graphics
