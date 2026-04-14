/**
 * @file jreng_gl_atlas_renderer.cpp
 * @brief GLAtlasRenderer GL-thread method implementations.
 *
 * @see jreng_gl_atlas_renderer.h
 */
namespace jreng
{ /*____________________________________________________________________________*/

GLAtlasRenderer::GLAtlasRenderer (std::initializer_list<jreng::Typeface*> typefaces)
    : managedTypefaces (typefaces)
{
}

GLAtlasRenderer::~GLAtlasRenderer() = default;

void GLAtlasRenderer::contextReady()
{
    glyphContext.createContext();
}

void GLAtlasRenderer::contextClosing()
{
    glyphContext.closeContext();

    for (auto* typeface : managedTypefaces)
    {
        if (typeface != nullptr)
        {
            auto monoHandle { static_cast<GLuint> (typeface->getGlMonoAtlas()) };
            auto emojiHandle { static_cast<GLuint> (typeface->getGlEmojiAtlas()) };

            if (monoHandle != 0)
                juce::gl::glDeleteTextures (1, &monoHandle);

            if (emojiHandle != 0)
                juce::gl::glDeleteTextures (1, &emojiHandle);

            typeface->resetGlAtlas();
        }
    }
}

void GLAtlasRenderer::renderText (GLGraphics& g, const juce::Component* target,
                                   GLComponent* comp, float totalScale, float vpHeight)
{
    if (not g.getTextCommands().empty() and glyphContext.isReady())
    {
        const auto localBounds { comp->getLocalBounds() };
        const float compWidth { static_cast<float> (localBounds.getWidth()) };
        const float compHeight { static_cast<float> (localBounds.getHeight()) };

        const auto origin { target->getLocalPoint (comp, juce::Point<float> (0.0f, 0.0f)) };
        const int destX { static_cast<int> (origin.x * totalScale) };
        const int destY { static_cast<int> (origin.y * totalScale) };
        const int physW { static_cast<int> (compWidth * totalScale) };
        const int physH { static_cast<int> (compHeight * totalScale) };
        const int fullH { static_cast<int> (vpHeight) };

        glyphContext.push (destX, destY, physW, physH, fullH);

        for (const auto& textCmd : g.getTextCommands())
        {
            jreng::Font fontCopy { textCmd.font };
            glyphContext.uploadStagedBitmaps (fontCopy.getTypeface());
            glyphContext.setFont (fontCopy);
            glyphContext.drawGlyphs (textCmd.glyphCodes.data(),
                                     textCmd.positions.data(),
                                     textCmd.numGlyphs);
        }

        glyphContext.pop();
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace jreng
