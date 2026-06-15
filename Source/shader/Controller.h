/**
 * @file shader/Controller.h
 * @brief GL pipeline orchestrator — mirrors juce::OpenGLAppComponent API with attach/detach.
 */
#pragma once
#include <JuceHeader.h>
#include "shader/Quad.h"
#include "shader/Pass.h"

namespace shader
{
/*____________________________________________________________________________*/

/** @brief GL pipeline orchestrator — mirrors juce::OpenGLAppComponent with attach/detach.
 *
 *  Identical API to juce::OpenGLAppComponent: initialise/shutdown/render virtuals,
 *  getFrameCounter(), shutdownOpenGL(). Adds attach/detach for runtime GPU/CPU switching.
 *
 *  Owned by end::View as a single member. View delegates all GL lifecycle to this class.
 *
 *  Thread contract:
 *  - attach() / detach() / isAttached() : MESSAGE THREAD
 *  - newOpenGLContextCreated / renderOpenGL / openGLContextClosing : GL THREAD
 */
struct Controller : private juce::OpenGLRenderer
{
    Controller() = default;
    ~Controller() override;

    /** @brief Returns the number of frames rendered since context creation. */
    int getFrameCounter() const noexcept { return frameCounter; }

    /** @brief Detaches the GL context. Call from subclass destructor if needed. */
    void shutdownOpenGL();

    /** @brief Attaches the GL context to the given component.
     *
     *  Enables JUCE native component painting, disables continuous repainting,
     *  registers this as the renderer, then attaches the context.
     *
     *  @param component  The component to attach to (end::View).
     */
    void attach (juce::Component& component);

    /** @brief Detaches the GL context (CPU mode or shutdown). */
    void detach();

    /** @brief Returns true if the GL context is currently attached. */
    bool isAttached() const noexcept;

    /** @brief The GL context. Public, same as OpenGLAppComponent. */
    juce::OpenGLContext openGLContext;

private:
    //==========================================================================
    // juce::OpenGLRenderer — identical to OpenGLAppComponent internals

    /** @brief Initialises GL resources on the GL thread. */
    void newOpenGLContextCreated() override;

    /** @brief Renders one frame — increments counter, calls render(). */
    void renderOpenGL() override;

    /** @brief Releases all GL resources on the GL thread. */
    void openGLContextClosing() override;

    //==========================================================================
    // Render implementation

    /** @brief Called from newOpenGLContextCreated — creates GL resources. */
    void initialise();

    /** @brief Called from openGLContextClosing — releases GL resources. */
    void shutdown();

    /** @brief Called from renderOpenGL — draws one frame. */
    void render();

    //==========================================================================

    int frameCounter { 0 };
    Quad quad;
    Pass background;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Controller)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
