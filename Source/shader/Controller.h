/**
 * @file shader/Controller.h
 * @brief GL pipeline orchestrator — mirrors juce::OpenGLAppComponent.
 */
#pragma once
#include <JuceHeader.h>
#include "config/Config.h"

namespace shader
{
/*____________________________________________________________________________*/

/** @brief GL pipeline orchestrator — mirrors juce::OpenGLAppComponent.
 *
 *  Skeleton: OpenGLAppComponent lifecycle + runtime GPU switching via
 *  attach()/detach(). Config-driven shader loading will be built on top
 *  of this skeleton to mirror end::LookAndFeel's loadGraphics() pattern.
 *
 *  Thread contract:
 *  - attach() / detach() / isAttached() : MESSAGE THREAD
 *  - newOpenGLContextCreated / renderOpenGL / openGLContextClosing : GL THREAD
 */
struct Controller
    : private juce::OpenGLRenderer
    , private juce::ValueTree::Listener
{
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
    void loadShaders();
    void buildQuad();
    //==========================================================================
    // Config listener

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    //==========================================================================

    int frameCounter { 0 };

    juce::ValueTree shaders { jam::Model::getChildWithName (
        config::Model::getInstance()->state, IDtype::shader) };

    struct Program
    {
        std::unique_ptr<juce::OpenGLShaderProgram> p;
        std::optional<juce::OpenGLFrameBuffer> fbo;
    };

    jam::HashMap<juce::Identifier, std::unique_ptr<Program>> programs;
    static inline const juce::String wrapper { BinaryData::getString ("wrapper.frag") };
    static inline const juce::String screenQuad { BinaryData::getString ("screen.vert") };
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Controller)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
