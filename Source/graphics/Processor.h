/**
 * @file graphics/Processor.h
 * @brief GL pipeline orchestrator — analogous to PluginProcessor.
 */
#pragma once
#include <JuceHeader.h>
#include "end/Model.h"
#include "config/Config.h"
#include "graphics/Compositor.h"

namespace graphics
{
/*____________________________________________________________________________*/

/** @brief GL pipeline orchestrator — analogous to PluginProcessor.
 *
 *  Owns the OpenGLContext lifecycle and config-driven event dispatch.
 *  Delegates all rendering to Compositor (analogous to ProcessorChain).
 *
 *  Listens on TWO models:
 *  - config::Model — ID::background and ID::postProcessing trigger
 *    compositor.loadShaders(); ID::frameRate, ID::resolutionScale,
 *    ID::filter, ID::backgroundOpacity dispatch to compositor setters.
 *  - end::Model — ID::size events trigger FBO resize via Resizer.
 *
 *  Thread contract:
 *  - attach() / detach() / isAttached() : MESSAGE THREAD
 *  - newOpenGLContextCreated / renderOpenGL / openGLContextClosing : GL THREAD
 *  - parameterChanged : MESSAGE THREAD (jam::Model::Listener AsyncUpdater)
 *  - Resizer stop trigger : MESSAGE THREAD (juce::Timer callback)
 */
struct Processor
    : private juce::OpenGLRenderer
    , private jam::Model::Listener
    , private juce::Timer
{
    Processor();
    ~Processor() override;

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
    // juce::OpenGLRenderer

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    //==============================================================================
    void registerEvents();

    /** @brief Fires config-driven events with current parameter values.
     *  Ensures compositor state matches config after construction or reload.
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

    Compositor compositor;
    std::unique_ptr<Quad> quad;
    jam::Resizer resizer;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace graphics
