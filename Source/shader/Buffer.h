#pragma once
#include <JuceHeader.h>
#include "shader/Pass.h"

namespace shader
{
/*____________________________________________________________________________*/

/** @brief Ping-pong double-buffered FBO — one Shadertoy compute buffer slot.
 *
 *  Owns two juce::OpenGLFrameBuffer instances (read/write) and a shader::Pass.
 *  Each frame, render() binds the write FBO, runs the pass, then swaps
 *  read↔write so the next frame reads this frame's output.
 *
 *  Used for Shadertoy Buffer A-D (feedback effects, particle sims, fluid).
 *
 *  Thread contract: ALL methods GL THREAD only.
 */
struct Buffer
{
    /** @brief Initialises both FBOs at the given dimensions.
     *  @param context  The active OpenGL context.
     *  @param width    Width in pixels.
     *  @param height   Height in pixels.
     *  @return         True if both FBOs initialised successfully.
     */
    bool resize (juce::OpenGLContext& context, int width, int height);

    /** @brief Renders the buffer pass into the write FBO, then swaps.
     *
     *  Binds the write FBO as render target, delegates to pass.render(),
     *  releases the FBO, and swaps read↔write indices.
     *
     *  @param quad        Shared fullscreen quad.
     *  @param width       Viewport width.
     *  @param height      Viewport height.
     *  @param time        iTime.
     *  @param timeDelta   iTimeDelta.
     *  @param frame       iFrame.
     *  @param mouse       iMouse (4 floats).
     */
    void render (Quad& quad, int width, int height,
                 float time, float timeDelta, int frame,
                 const float mouse[4]);

    /** @brief Returns the previous-frame texture ID (the read FBO).
     *  Other passes bind this as an iChannel to sample this buffer's output.
     */
    GLuint getReadTexture() const noexcept;

    /** @brief Returns true if the pass has a compiled shader. */
    bool isLoaded() const noexcept { return pass.isLoaded(); }

    /** @brief Releases both FBOs and the shader pass. */
    void release();

    /** @brief The shader pass for this buffer. Public for compile access. */
    Pass pass;

private:
    /** @brief FBO pair. Index 0 and 1, swapped each frame. */
    juce::OpenGLFrameBuffer fbos[2];

    /** @brief Current write index (0 or 1). Read index is 1 - writeIndex. */
    int writeIndex { 0 };
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace shader
