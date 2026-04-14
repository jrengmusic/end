#pragma once

namespace jreng
{

/**
 * @namespace GLVertexLayout
 * @brief Stateless utility for configuring OpenGL vertex attribute pointers.
 *
 * Provides the `Attribute` descriptor struct and the `setupAttributes()`
 * factory function.  All operations require an active OpenGL context and a
 * bound VAO.
 */
namespace GLVertexLayout
{
    struct Attribute
    {
        GLuint location;
        GLint size;
        GLenum type;
        GLsizei stride;
        size_t offset;
        GLuint divisor;
    };

    void setupAttributes (const Attribute* attributes, int count) noexcept;
}

} // namespace jreng
