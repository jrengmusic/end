/**
 * @file shader/Quad.h
 * @brief Fullscreen quad VAO/VBO — shared geometry for all shader passes.
 */
#pragma once
#include <JuceHeader.h>

namespace shader
{
/*____________________________________________________________________________*/

/** @brief Fullscreen quad VAO/VBO — shared geometry for all shader passes.
 *
 *  Owns a VAO and a VBO with 4 vertices in a triangle strip covering [-1,1] NDC.
 *  The VAO captures the VBO binding and vertex attrib pointer setup at create() time,
 *  so draw() only needs to bind the VAO — compatible with core profile contexts.
 *
 *  GL resource lifecycle: create() allocates VAO + VBO on the GL thread
 *  (call from newOpenGLContextCreated). destroy() releases them
 *  (call from openGLContextClosing).
 *
 *  Thread contract: ALL methods GL THREAD only.
 */
struct Quad
{
    /** @brief Allocates the VAO and VBO. */
    void create();

    /** @brief Releases the VAO and VBO. Safe to call if not created. */
    void destroy();

    /** @brief Binds the VAO and draws. */
    void draw();

private:
    /** @brief VAO handle. Zero when not created. */
    GLuint vao { 0 };

    /** @brief VBO handle. Zero when not created. */
    GLuint vbo { 0 };

    /** @brief Position attribute location in the vertex shader. */
    static constexpr GLuint positionAttribute { 0 };
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace shader
