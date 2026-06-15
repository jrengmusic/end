/**
 * @file shader/Controller.h
 * @brief Shader engine orchestrator — owns the GL pipeline for background rendering.
 */
#pragma once
#include <JuceHeader.h>
#include "shader/Quad.h"
#include "shader/Pass.h"
#include "shader/Buffer.h"

namespace shader
{
/*____________________________________________________________________________*/

/** @brief Shader engine orchestrator — owns the GL pipeline for background rendering.
 *
 *  Implements juce::OpenGLRenderer. Owns the OpenGL context, fullscreen quad,
 *  background pass, and 4 ping-pong buffer slots.
 *
 *  Component painting is enabled — JUCE composites components natively on top
 *  of the background rendered here. Controller only draws behind components.
 *
 *  Owned by end::View as a single member. View delegates all GL lifecycle
 *  to this class and stays clean.
 *
 *  Thread contract:
 *  - attach() / detach() / resize() : MESSAGE THREAD
 *  - renderOpenGL / newOpenGLContextCreated / openGLContextClosing : GL THREAD
 */
struct Controller : public juce::OpenGLRenderer
{
    Controller() = default;
    ~Controller() override;

    /** @brief Attaches the GL context to the given component.
     *
     *  Enables JUCE native component painting, disables continuous repainting
     *  (render on demand via triggerRepaint), registers this as the renderer,
     *  then attaches the context.
     *
     *  @param component  The component to attach to (end::View).
     */
    void attach (juce::Component& component);

    /** @brief Detaches the GL context (CPU mode or shutdown).
     *
     *  Triggers openGLContextClosing on the GL thread before returning.
     */
    void detach();

    /** @brief Returns true if the GL context is currently attached. */
    bool isAttached() const noexcept;

    /** @brief Notifies the controller of a component resize.
     *
     *  Stores new dimensions atomically — buffer resize happens on the GL thread
     *  at the start of the next renderOpenGL() call.
     *
     *  @param width   New width in pixels.
     *  @param height  New height in pixels.
     */
    void resize (int width, int height);

    //==========================================================================
    // juce::OpenGLRenderer

    /** @brief Initialises GL resources on the GL thread (context just became active). */
    void newOpenGLContextCreated() override;

    /** @brief Renders one frame on the GL thread. */
    void renderOpenGL() override;

    /** @brief Releases all GL resources on the GL thread (context about to be destroyed). */
    void openGLContextClosing() override;

private:
    juce::OpenGLContext context;
    Quad quad;
    Pass background;
    std::array<Buffer, 4> buffers;

    // Render state (GL thread only)
    int viewportWidth { 0 };
    int viewportHeight { 0 };
    float startTime { 0.0f };
    float lastFrameTime { 0.0f };
    int frameCount { 0 };
    float mouse[4] { 0.0f, 0.0f, 0.0f, 0.0f };

    // Pending resize dimensions written on message thread, read on GL thread
    std::atomic<int> pendingWidth { 0 };
    std::atomic<int> pendingHeight { 0 };

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Controller)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
