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
 *  - config::Model — ID::background triggers full shader recompile (loadShaders).
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
 *  - renderOpenGL() advances uniform state.
 *  - Buffer passes (BufferA-D): iterate in HashMap insertion order, render
 *    each into its writeBuffer() FBO, swap ping-pong after each pass.
 *  - Image pass: renders to default framebuffer after all buffer passes.
 *  - All passes receive iChannel0-3 bound to BufferA-D read textures via
 *    setChannels() / unbindChannels().
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

    /** @brief Compiles all shader passes from config VT. Queues work on GL thread. */
    void loadShaders();

    /** @brief Compiles vertex + fragment shader, links. Returns nullptr on failure (error sent to message overlay). GL thread only. */
    std::unique_ptr<juce::OpenGLShaderProgram> createProgram (juce::StringRef shaderSource);

    /** @brief Updates viewport uniform and resizes FBO pairs for all non-Image programs. GL thread only. */
    void resize (int w, int h);

    /** @brief Sets iChannel sampler uniforms and binds buffer pass read textures to corresponding texture units.
     *         Only binds channels with existing buffer passes. GL thread only.
     *  @param program  The active GL program (must be use()'d first).
     */
    void setChannels (juce::OpenGLShaderProgram& program);

    /** @brief Unbinds texture units 0-3. GL thread only. */
    void unbindChannels();

    //==========================================================================
    // jam::Model::Listener

    void parameterChanged (const juce::Identifier& id, const juce::var& newValue) override;

    //==========================================================================
    config::Model& config { *config::Model::getInstance() };
    file::Shaders& files { *file::Shaders::getInstance() };
    end::Model& appModel { *end::Model::getInstance() };
    jam::Function::Map<juce::Identifier, void> events;
    jam::HashMap<juce::Identifier, std::unique_ptr<Pass>> programs;

    jam::Resizer resizer;
    std::unique_ptr<Quad> quad;
    Uniform uniform;
    //==============================================================================
    static inline const juce::String placeholder { "source" };
    static inline const juce::String wrapper { BinaryData::getString ("wrapper.frag") };
    static inline const juce::String screenQuad { BinaryData::getString ("screen.vert") };
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Controller)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
