/**
 * @file shader/Controller.h
 * @brief GL pipeline orchestrator — mirrors juce::OpenGLAppComponent.
 */
#pragma once
#include <JuceHeader.h>
#include "end/Model.h"
#include "config/Config.h"
#include "shader/Program.h"

namespace shader
{
/*____________________________________________________________________________*/

/** @brief GL pipeline orchestrator — mirrors juce::OpenGLAppComponent.
 *
 *  Owns the OpenGLContext lifecycle and config-driven shader loading.
 *  Shader state is accessed via ParameterText atomics on GL thread —
 *  never from VT properties (VT lags by one flush tick).
 *
 *  Listens on TWO models:
 *  - config::Model — ID::background triggers full shader recompile via
 *    loadShaders(background, IDtype::background); ID::postProcessing triggers
 *    post-processing recompile via loadShaders(postProcess, IDtype::postProcessing).
 *    Both reads are config-tree-driven — each config::Shader instance populates
 *    its own subtree (BACKGROUND or POST_PROCESSING) from disk before firing the
 *    property change notification.
 *    Config file watcher coalesces reload notifications: one background property
 *    change = one loadShaders call across all passes.
 *  - end::Model    — ID::size events trigger FBO resize via Resizer.
 *
 *  Resize flow:
 *  - View::resized() packs width + height into end::Size and writes
 *    ID::size as a single int property on the view state tree.
 *  - Parameter\<int\> adapter fires parameterChanged(ID::size).
 *  - ID::size event unpacks via end::Size and calls resizer.set().
 *  - Resizer coalesces rapid changes (16ms timer) and fires the "stop"
 *    trigger, which calls resize() on the GL thread — updates Uniform
 *    viewport and re-initialises FBO pairs at scaled dimensions.
 *
 *  Render loop (multi-pass):
 *  - renderOpenGL() advances uniform state via background.uniform.advance()
 *    and postProcess.uniform.advance().
 *  - When post-processing is active (postProcess compiled, composite initialised):
 *    1. glBlitFramebuffer copies FB 0 (previous frame's bg + JUCE compositing)
 *       into composite writeBuffer. composite.swap() makes it the readBuffer.
 *    2. Post-process renderBuffers + renderImage render to FB 0, reading
 *       composite readBuffer via iScene (GL_TEXTURE4, unit = BufferChannel size).
 *    3. Background buffer passes render at buffer resolution.
 *    4. Background image pass renders into backgroundPass[0] FBO.
 *    5. renderOutput() upscales to FB 0. JUCE composites components. Swap presents.
 *       One frame latency: post-pro shows previous frame's composited scene.
 *  - Without post-processing: background + renderOutput() to default framebuffer.
 *  - iChannel0-3 binding is handled inside Compilation::setChannels().
 *  - iScene binding: sceneTexture GLuint set on postProcess before render (from composite readBuffer),
 *    cleared after. setChannels() binds at unit = BufferChannel::get().size().
 *
 *  Thread contract:
 *  - attach() / detach() / isAttached() : MESSAGE THREAD
 *  - newOpenGLContextCreated / renderOpenGL / openGLContextClosing : GL THREAD
 *  - parameterChanged : MESSAGE THREAD (jam::Model::Listener AsyncUpdater)
 *  - Resizer stop trigger : MESSAGE THREAD (juce::Timer callback)
 */
struct Controller
    : private juce::OpenGLRenderer
    , private jam::Model::Listener
    , private juce::Timer
{
    Controller();
    ~Controller() override;

    /** @brief Detaches the GL context. */
    void shutdownOpenGL();

    /** @brief Attaches the GL context to @p component.
     *  @param component  The component to render into (end::View).
     */
    void attach (juce::Component& component);

    /** @brief Detaches the GL context. */
    void detach();

    /** @brief Returns true if the GL context is currently attached. */
    bool isAttached() const noexcept;

    /** @brief The GL context. Public — mirrors juce::OpenGLAppComponent. */
    juce::OpenGLContext context;

private:
    //==========================================================================
    // juce::OpenGLRenderer — mirrors juce::OpenGLAppComponent

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    //==============================================================================
    void initialise();
    void shutdown();
    void registerEvents();

    /** @brief Compiles shader passes from the config VT subtree identified by @p treeType into @p compilation.
     *         Queues work on GL thread.
     *  @param compilation  Target compilation (background or postProcess).
     *  @param treeType     Tree type to search for under the GRAPHICS child.
     *                      @c IDtype::background selects the BACKGROUND subtree (background passes);
     *                      @c IDtype::postProcessing selects the POST_PROCESSING subtree.
     */
    void loadShaders (Compilation& compilation, const juce::Identifier& treeType);

    /** @brief Compiles vertex + fragment shader, links. Returns nullptr on failure (error sent to message overlay). GL thread only. */
    std::unique_ptr<juce::OpenGLShaderProgram> createProgram (juce::StringRef shaderSource);

    /** @brief Updates viewport uniform and resizes FBO pairs for background passes and output pass. GL thread only. */
    void resize (int screenWidth, int screenHeight);

    /** @brief Upscales output pass FBO to screen resolution with opacity and filter. GL thread only. */
    void renderOutput();

    /** @brief Fires config-driven events with current parameter values.
     *  Ensures Uniform state matches config after construction or reload.
     */
    void refreshParameters();

    //==========================================================================
    // jam::Model::Listener

    void parameterChanged (const juce::Identifier& id, const juce::var& newValue) override;

    //==========================================================================
    // juce::Timer

    void timerCallback() override;

    //==========================================================================
    config::Model& config { *config::Model::getInstance() };
    end::Model& appModel { *end::Model::getInstance() };
    jam::Function::Map<juce::Identifier, void> events;
    Compilation background;

    jam::Resizer resizer;
    std::unique_ptr<Quad> quad;
    /** @brief Background Image pass render target — buffer resolution, single FBO. GL thread only. */
    FrameBuffer backgroundPass;

    /** @brief Output shader — upscales backgroundPass[0] to screen with opacity and filter. GL thread only. */
    std::unique_ptr<juce::OpenGLShaderProgram> outputProgram;

    /** @brief Post-processing compilation — screen resolution, iScene input from composite FBO. GL thread only. */
    Compilation postProcess;

    /** @brief Ping-pong FBO — screen resolution, holds upscaled background + JUCE components for post-processing.
     *         Initialised in resize() when postProcess is compiled. GL thread only. */
    FrameBuffer composite { 2 };

    //==============================================================================
    static inline const juce::String placeholder { "source" };
    static inline const juce::String wrapper { BinaryData::getString ("wrapper.frag") };
    static inline const juce::String screenQuad { BinaryData::getString ("screen.vert") };
    static inline const juce::String outputShader { BinaryData::getString ("output.frag") };
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Controller)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
