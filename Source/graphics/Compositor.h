/**
 * @file graphics/Compositor.h
 * @brief Background shader renderer — owns GL context, renders background layer only.
 */
#pragma once
#include <JuceHeader.h>
#include "graphics/Program.h"

namespace graphics
{
/*____________________________________________________________________________*/

/** @brief Background shader renderer — owns the GL context and background rendering resources.
 *
 *  Dumb renderer with zero upstream visibility. Receives shader source trees and
 *  parameter values through its public API from Processor. Does not access
 *  config::Model or end::Model.
 *
 *  Renders background shader (multi-pass Shadertoy) to FB 0. JUCE handles
 *  component painting via its own CachedImage + drawComponentBuffer after
 *  renderOpenGL returns. Components always render on top of background.
 *
 *  Surface API mirrors TETRIS contract:
 *  - prepare() — initialise GL resources
 *  - process() — per-frame render loop
 *  - reset()   — release GL resources
 *
 *  @par Thread contract
 *  prepare() / process() / reset() : GL THREAD.
 *  attach() / detach() / isAttached() / triggerRepaint() : MESSAGE THREAD.
 *  loadShaders() / resize() dispatch to GL thread internally — message thread safe.
 *  Setters are message-thread safe (single-value writes).
 */
struct Compositor
{
    Compositor() = default;
    ~Compositor() = default;

    //==============================================================================
    // Lifecycle — owns GL context

    /** @brief Configures the GL context and attaches it to a component.
     *
     *  Sets OpenGL 4.1, enables multisampling, registers @p renderer,
     *  attaches to @p component. Component painting left enabled (JUCE default).
     *  Message thread.
     *
     *  @param renderer   The OpenGLRenderer implementation (Processor).
     *  @param component  The component to render into (end::View).
     */
    void attach (juce::OpenGLRenderer& renderer, juce::Component& component);

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
    // Config-driven — dispatch to GL thread internally

    /** @brief Compiles background shader passes from the given source tree.
     *
     *  Dispatches compilation to the GL thread. After compilation, resizes
     *  FBOs to match the current target component dimensions. On failure,
     *  calls reportError (if set) from the GL thread.
     *  Message thread safe.
     *
     *  @param shaderTree  ValueTree containing pass source properties (Image, BufferA, etc.).
     */
    void loadShaders (const juce::ValueTree& shaderTree);

    /** @brief Dispatches FBO resize to the GL thread.
     *         Takes logical pixel dimensions, applies rendering scale internally.
     *         Message thread safe.
     *  @param width   Logical pixel width.
     *  @param height  Logical pixel height.
     */
    void resize (int width, int height);

    //==============================================================================
    // Message-thread setters

    /** @brief Sets the background opacity. @param value Opacity in [0, 1]. */
    void setOpacity (float value);

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
    juce::Point<int> getScreenSize() const { return background.uniform.getScreenSize(); }

    //==============================================================================
    // Error reporting — called from GL thread on shader compilation failure.

    /** @brief Called with the error string when createProgram fails. Set by Processor. */
    std::function<void (const juce::String&)> reportError;

private:
    /** @brief Resizes background FBOs at the given pixel dimensions. GL thread only. */
    void resizeBuffers (int pixelWidth, int pixelHeight);

    /** @brief Compiles vertex + fragment shader. Returns nullptr on failure.
     *         On failure, calls reportError (if set) with the error message. GL thread only.
     *  @param shaderSource  GLSL fragment shader source.
     */
    std::unique_ptr<juce::OpenGLShaderProgram> createProgram (juce::StringRef shaderSource);

    /** @brief Renders background to FB 0 with opacity blend. GL thread only.
     *  @param quad          Fullscreen quad for drawing.
     *  @param screenWidth   Viewport width in pixels.
     *  @param screenHeight  Viewport height in pixels.
     */
    void renderOutput (Quad& quad, int screenWidth, int screenHeight);

    //==============================================================================
    juce::OpenGLContext context;

    Compilation background;

    /** @brief Output shader — renders background output to screen with opacity and filter. GL thread only. */
    std::unique_ptr<juce::OpenGLShaderProgram> outputProgram;

    /** @brief Fullscreen quad VAO+VBO. Created in prepare(). */
    std::unique_ptr<Quad> quad;

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
