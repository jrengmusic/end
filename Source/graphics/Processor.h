/**
 * @file graphics/Processor.h
 * @brief GL pipeline pure listener/adapter — bridges config events to Compositor API.
 */
#pragma once
#include <JuceHeader.h>
#include "end/Model.h"
#include "config/Config.h"
#include "graphics/Compositor.h"

namespace graphics
{
/*____________________________________________________________________________*/

/** @brief GL pipeline pure listener/adapter — owns Compositor, bridges config
 *         events to Compositor API. Never touches the GL context directly.
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

    /** @brief Attaches the GL context to @p view and installs CachedImage on @p cacheTarget.
     *  @param view         The component to render into (end::View).
     *  @param cacheTarget  The child component whose painting gets post-processed (Tabs).
     */
    void attach (juce::Component& view, juce::Component& cacheTarget);

    /** @brief Detaches the GL context. */
    void detach();

    /** @brief Returns true if the GL context is currently attached. */
    bool isAttached() const noexcept;

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

    /** @brief Extracts shader pass sources from the config tree for the given type.
     *  @param treeType  Config tree type (IDtype::background or IDtype::postProcessing).
     *  @return HashMap of pass Identifier to GLSL source string.
     */
    jam::HashMap<juce::Identifier, juce::String> extractShaderSources (const juce::Identifier& treeType);

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

    Compositor compositor { *this };
    jam::Resizer resizer;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace graphics
