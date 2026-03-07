/**
 * @file GLVertexLayout.cpp
 * @brief Implementation of `GLVertexLayout::setupAttributes()`.
 *
 * Iterates an array of `GLVertexLayout::Attribute` descriptors and issues the
 * three OpenGL calls required to configure each vertex attribute:
 *
 * 1. `glEnableVertexAttribArray(location)` — activates the attribute slot.
 * 2. `glVertexAttribPointer(...)` — binds the current `GL_ARRAY_BUFFER` to
 *    the attribute with the specified component count, type, stride, and
 *    byte offset.  Normalisation is always `GL_FALSE`.
 * 3. `glVertexAttribDivisor(location, divisor)` — sets the instancing rate.
 *    A divisor of `1` means the attribute advances once per instance; `0`
 *    means once per vertex.
 *
 * ### Preconditions
 *
 * - A VAO must be bound (`glBindVertexArray`) before calling this function.
 * - The instance VBO must be bound to `GL_ARRAY_BUFFER` so that
 *   `glVertexAttribPointer` records the correct buffer binding in the VAO.
 *
 * @note This file must only be compiled into the GL THREAD call path.
 *
 * @see GLVertexLayout.h
 * @see Terminal::Render::OpenGL::drawInstances()
 * @see Terminal::Render::OpenGL::drawBackgrounds()
 */
#include "GLVertexLayout.h"

/**
 * @brief Configure a list of vertex attributes on the currently bound VAO.
 *
 * For each `Attribute` in @p attributes[0..count-1], calls:
 * - `glEnableVertexAttribArray(attr.location)`
 * - `glVertexAttribPointer(attr.location, attr.size, attr.type, GL_FALSE,
 *       attr.stride, reinterpret_cast<const void*>(attr.offset))`
 * - `glVertexAttribDivisor(attr.location, attr.divisor)`
 *
 * The `reinterpret_cast` on `attr.offset` is the standard idiom for passing
 * a byte offset as the `pointer` argument to `glVertexAttribPointer` when
 * using VBO-backed attributes.
 *
 * @param attributes  Pointer to the first `Attribute` descriptor.
 *                    Must not be `nullptr` when @p count > 0.
 * @param count       Number of descriptors to process.
 *
 * @note **GL THREAD** only.  VAO and instance VBO must be bound.
 * @see GLVertexLayout::Attribute
 */
void GLVertexLayout::setupAttributes (const Attribute* attributes, int count) noexcept
{
    for (int i { 0 }; i < count; ++i)
    {
        const Attribute& attr { attributes[i] };
        juce::gl::glEnableVertexAttribArray (attr.location);
        juce::gl::glVertexAttribPointer (attr.location,
                                         attr.size,
                                         attr.type,
                                         juce::gl::GL_FALSE,
                                         attr.stride,
                                         reinterpret_cast<const void*> (attr.offset));
        juce::gl::glVertexAttribDivisor (attr.location, attr.divisor);
    }
}
