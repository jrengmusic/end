/**
 * @file shader/Quad.h
 * @brief Fullscreen quad VBO — shared geometry for all shader passes.
 */
#pragma once
#include <JuceHeader.h>

namespace shader
{
/*____________________________________________________________________________*/

/** @brief Fullscreen quad VBO — shared geometry for all shader passes.
 *
 *  Owns a VBO with 4 vertices in a triangle strip covering [-1,1] NDC.
 *
 *  GL resource lifecycle: create() allocates VBO on the GL thread
 *  (call from newOpenGLContextCreated). destroy() releases it
 *  (call from openGLContextClosing).
 *
 *  Thread contract: ALL methods GL THREAD only.
 */
struct Quad
{
    /** @brief Allocates the VBO. */
    void create();

    /** @brief Releases the VBO. Safe to call if not created. */
    void destroy();

    /** @brief Binds the VBO, enables the position attribute, draws, disables, unbinds. */
    void draw();

private:
    /** @brief VBO handle. Zero when not created. */
    GLuint vbo { 0 };

    /** @brief Position attribute location in the vertex shader. */
    static constexpr GLuint positionAttribute { 0 };
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace shader
