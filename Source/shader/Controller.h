/**
 * @file shader/Controller.h
 * @brief GL pipeline orchestrator — mirrors juce::OpenGLAppComponent + end::LookAndFeel.
 */
#pragma once
#include <JuceHeader.h>
#include "shader/Quad.h"
#include "shader/Compiler.h"
#include "config/Config.h"

namespace shader
{
/*____________________________________________________________________________*/

/** @brief GL pipeline orchestrator — mirrors juce::OpenGLAppComponent + end::LookAndFeel.
 *
 *  GL lifecycle mirrors juce::OpenGLAppComponent: public openGLContext,
 *  getFrameCounter(), shutdownOpenGL(), private OpenGLRenderer virtuals.
 *  attach()/detach() extend for runtime GPU/CPU switching.
 *
 *  Config-driven loading mirrors end::LookAndFeel: loadShaders() mirrors
 *  loadGraphics(), the passes map mirrors the graphics map. Shader loading
 *  is driven by config::File::Shaders bimap iteration.
 *
 *  Each Pass is a fully resolved artifact — program + baked uniform setter +
 *  optional FBO. No per-frame dispatch, no per-uniform if-checks. The setter
 *  is built at load time by Compiler::buildUniformSetter and captures all
 *  active uniform locations.
 *
 *  valueTreePropertyChanged receives the config::Shader state tree directly.
 *  Source extraction happens on the message thread; compilation and Pass
 *  construction happen on the GL thread via executeOnGLThread.
 *
 *  Thread contract:
 *  - attach() / detach() / isAttached() / valueTreePropertyChanged() : MESSAGE THREAD
 *  - newOpenGLContextCreated / renderOpenGL / openGLContextClosing : GL THREAD
 *  - loadShaders() / render() : GL THREAD
 */
struct Controller : private juce::OpenGLRenderer
                  , private juce::ValueTree::Listener
{
    /** @brief Fully resolved renderable pass — built at load time, used at render time.
     *
     *  program owns the linked shader. setUniforms is a baked lambda from
     *  Compiler::buildUniformSetter — captures all active uniform locations,
     *  calls the right glUniform* per known Shadertoy uniform. fbo is non-null
     *  for buffer passes (bufferA–D); null for image (renders to screen).
     */
    struct Pass
    {
        std::unique_ptr<juce::OpenGLShaderProgram>  program;
        std::unique_ptr<juce::OpenGLFrameBuffer>    fbo;
        Compiler::UniformSetter                     setUniforms;
    };

    Controller() = default;
    ~Controller() override;

    /** @brief Returns the number of frames rendered since context creation. */
    int getFrameCounter() const noexcept { return frameCounter; }

    /** @brief Detaches the GL context. */
    void shutdownOpenGL();

    /** @brief Attaches the GL context to @p component.
     *  @param component  The component to render into (end::View).
     */
    void attach (juce::Component& component);

    /** @brief Detaches the GL context and removes the config listener. */
    void detach();

    /** @brief Returns true if the GL context is currently attached. */
    bool isAttached() const noexcept;

    /** @brief The GL context. Public — mirrors juce::OpenGLAppComponent. */
    juce::OpenGLContext openGLContext;

private:
    //==========================================================================
    // juce::OpenGLRenderer — mirrors juce::OpenGLAppComponent

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    void initialise();
    void shutdown();
    void render();

    //==========================================================================
    // Config listener

    /** @brief Routes config::Shader state changes to loadShaders on the GL thread.
     *
     *  Reads pass sources from @p tree children on the message thread, captures
     *  assembled fragment strings, posts compilation to the GL thread.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree,
                                   const juce::Identifier& property) override;

    /** @brief Compiles shader passes and builds the passes map. GL THREAD only.
     *
     *  Mirrors LookAndFeel::loadGraphics(). Takes pre-extracted fragment sources
     *  keyed by pass stem. For each entry, compiles via Compiler::build, bakes
     *  uniform setter via Compiler::buildUniformSetter, creates FBO for buffer
     *  passes. Stores each Pass in passes.
     *
     *  @param sources  Pass stem → assembled fragment source string.
     */
    void loadShaders (const jam::HashMap<juce::Identifier, juce::String>& sources);

    //==========================================================================

    inline static const juce::String vertexSource { BinaryData::getString ("quad.vert") };
    inline static const juce::String wrapper { BinaryData::getString ("shader.frag") };

    int frameCounter { 0 };
    Quad quad;

    /** @brief Pass table — one entry per compiled shader pass.
     *  Mirrors LookAndFeel::graphics. Rebuilt by loadShaders().
     */
    jam::HashMap<juce::Identifier, Pass> passes;

    config::Shader& shaderConfig { config::Model::getInstance()->getShader() };

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Controller)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
