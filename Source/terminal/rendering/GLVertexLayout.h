/**
 * @file GLVertexLayout.h
 * @brief Vertex attribute layout utility for OpenGL instanced rendering.
 *
 * `GLVertexLayout` is a stateless utility namespace that encapsulates the
 * repetitive `glEnableVertexAttribArray` / `glVertexAttribPointer` /
 * `glVertexAttribDivisor` sequence required to configure per-instance vertex
 * attributes for instanced draw calls.
 *
 * ### Design rationale
 *
 * The terminal renderer uses a single VAO and two VBOs (quad + instance).
 * Attribute layout varies between the glyph pass (`Render::Glyph`) and the
 * background pass (`Render::Background`).  Rather than duplicating the setup
 * code in each draw function, callers build a small stack-allocated array of
 * `GLVertexLayout::Attribute` descriptors and pass it to `setupAttributes()`.
 *
 * ### Usage
 *
 * @code
 * // GL THREAD — inside drawInstances()
 * const GLVertexLayout::Attribute attrs[]
 * {
 *     { 1, 2, juce::gl::GL_FLOAT, sizeof (Glyph), 0,                               1 },
 *     { 2, 2, juce::gl::GL_FLOAT, sizeof (Glyph), sizeof (juce::Point<float>),     1 },
 *     { 3, 4, juce::gl::GL_FLOAT, sizeof (Glyph), 2 * sizeof (juce::Point<float>), 1 },
 *     { 4, 4, juce::gl::GL_FLOAT, sizeof (Glyph), 2 * sizeof (juce::Point<float>) + sizeof (juce::Rectangle<float>), 1 }
 * };
 * GLVertexLayout::setupAttributes (attrs, 4);
 * @endcode
 *
 * @note All functions in this namespace must be called on the **GL THREAD**
 *       with a VAO already bound.
 *
 * @see Terminal::Render::OpenGL::drawInstances()
 * @see Terminal::Render::OpenGL::drawBackgrounds()
 * @see GLShaderCompiler
 */
#pragma once
#include <JuceHeader.h>

/**
 * @namespace GLVertexLayout
 * @brief Stateless utility for configuring OpenGL vertex attribute pointers.
 *
 * Provides the `Attribute` descriptor struct and the `setupAttributes()`
 * factory function.  All operations require an active OpenGL context and a
 * bound VAO.
 *
 * @note **GL THREAD** only.
 *
 * @see setupAttributes()
 * @see Attribute
 */
// Utility for setting up OpenGL vertex attribute pointers
// GL THREAD ONLY
namespace GLVertexLayout
{
    /**
     * @struct Attribute
     * @brief Descriptor for a single OpenGL vertex attribute.
     *
     * Encapsulates all parameters needed to configure one vertex attribute
     * via `glEnableVertexAttribArray`, `glVertexAttribPointer`, and
     * `glVertexAttribDivisor`.
     *
     * @par Example — per-instance `vec2` at location 1 within a `Glyph` struct
     * @code
     * GLVertexLayout::Attribute attr { 1, 2, juce::gl::GL_FLOAT, sizeof (Glyph), 0, 1 };
     * @endcode
     *
     * @see setupAttributes()
     */
    // Describes a single vertex attribute
    struct Attribute
    {
        /** @brief Shader attribute location index (matches `layout(location = N)` in GLSL). */
        GLuint location;

        /**
         * @brief Number of scalar components per attribute element.
         *
         * For example, `2` for a `vec2`, `4` for a `vec4`.
         */
        GLint size;

        /**
         * @brief OpenGL data type of each component.
         *
         * Typically `GL_FLOAT` for all terminal renderer attributes.
         */
        GLenum type;

        /**
         * @brief Byte stride between successive attribute elements in the VBO.
         *
         * Set to `sizeof(InstanceStruct)` so that OpenGL steps by one full
         * instance record between consecutive instances.
         */
        GLsizei stride;

        /**
         * @brief Byte offset of this attribute within the instance struct.
         *
         * Passed as the `pointer` argument to `glVertexAttribPointer` via
         * `reinterpret_cast<const void*>(offset)`.
         */
        size_t offset;

        /**
         * @brief Instancing divisor passed to `glVertexAttribDivisor`.
         *
         * - `0` — attribute advances once per vertex (non-instanced; used for
         *         the unit-quad VBO at location 0).
         * - `1` — attribute advances once per instance (used for all
         *         per-instance attributes at locations 1–4).
         */
        GLuint divisor;
    };

    /**
     * @brief Configure a list of vertex attributes on the currently bound VAO.
     *
     * Iterates @p attributes[0..count-1] and for each entry calls:
     * 1. `glEnableVertexAttribArray(attr.location)`
     * 2. `glVertexAttribPointer(attr.location, attr.size, attr.type,
     *        GL_FALSE, attr.stride, (const void*)attr.offset)`
     * 3. `glVertexAttribDivisor(attr.location, attr.divisor)`
     *
     * @param attributes  Pointer to the first `Attribute` descriptor.
     *                    Must not be `nullptr` when @p count > 0.
     * @param count       Number of descriptors in @p attributes.
     *
     * @note **GL THREAD** only.  A VAO must be bound before calling this
     *       function, and the instance VBO must be bound to `GL_ARRAY_BUFFER`.
     *
     * @see Terminal::Render::OpenGL::drawInstances()
     * @see Terminal::Render::OpenGL::drawBackgrounds()
     */
    // Set up a list of vertex attributes
    // Assumes VAO is already bound
    void setupAttributes (const Attribute* attributes, int count) noexcept;
}
