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
 *  Shader state is accessed via config::Model API (getChildWithName) —
 *  never by storing a raw ValueTree snapshot.
 *
 *  Listens on TWO models:
 *  - config::Model — per-pass property IDs (Common, Image, BufferA-D) trigger shader recompile.
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
 *  Render loop (single Image pass):
 *  - renderOpenGL() clears, advances uniform state, sets viewport.
 *  - Looks up the Image program in the programs HashMap.
 *  - If found: use(), dispatches iResolution/iTime/iTimeDelta/iFrame via
 *    uniform.set(), draws fullscreen quad.
 *  - Multi-pass rendering (buffer passes) is not yet implemented.
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

    //==========================================================================
    // jam::Model::Listener

    void parameterChanged (const juce::Identifier& id, const juce::var& newValue) override;

    //==========================================================================
    config::Model& config { *config::Model::getInstance() };
    config::Shaders& files { *config::Shaders::getInstance() };
    end::Model& appModel { *end::Model::getInstance() };
    jam::Function::Map<juce::Identifier, void> events;
    jam::HashMap<juce::Identifier, std::unique_ptr<Pass>> programs;

    //==============================================================================
    struct Quad
    {
        Quad()
        {
            using namespace ::juce::gl;

            glGenVertexArrays (1, &vao);
            glBindVertexArray (vao);

            glGenBuffers (1, &vbo);
            glBindBuffer (GL_ARRAY_BUFFER, vbo);

            static constexpr float vertices[] { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
            glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_STATIC_DRAW);

            glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            glEnableVertexAttribArray (0);

            glBindVertexArray (0);
        }

        ~Quad()
        {
            using namespace ::juce::gl;

            glDeleteBuffers (1, &vbo);
            glDeleteVertexArrays (1, &vao);
        }

        void draw() const noexcept
        {
            using namespace ::juce::gl;

            glBindVertexArray (vao);
            glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray (0);
        }

        GLuint vao { 0 };
        GLuint vbo { 0 };

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Quad)
    };

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
