/**
 * @file jreng_glyph_atlas.cpp
 * @brief Implementation of `jreng::GlyphAtlas`.
 */

namespace jreng
{

void GlyphAtlas::rebuildAtlas() noexcept
{
    if (monoAtlas != 0)
    {
        juce::gl::glDeleteTextures (1, &monoAtlas);
        monoAtlas = 0;
    }

    if (emojiAtlas != 0)
    {
        juce::gl::glDeleteTextures (1, &emojiAtlas);
        emojiAtlas = 0;
    }
}

} // namespace jreng
