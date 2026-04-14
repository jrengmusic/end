#pragma once

namespace jreng
{

class GLGraphics
{
public:
    enum class CommandType
    {
        draw,
        drawLineStrip,
        pushClip,
        popClip
    };

    struct Command
    {
        CommandType type { CommandType::draw };
        std::vector<GLVertex> vertices;
    };

    struct TextCommand
    {
        explicit TextCommand (const jreng::Font& f) noexcept : font (f) {}

        jreng::Font font;
        std::vector<uint16_t> glyphCodes;
        std::vector<juce::Point<float>> positions;
        int numGlyphs { 0 };
    };

    explicit GLGraphics (float viewportWidth, float viewportHeight, float scale = 1.0f);
    ~GLGraphics() = default;

    void setColour (juce::Colour newColour) noexcept;
    void setFont (jreng::Font& font) noexcept;
    void drawGlyphs (const uint16_t* glyphCodes,
                     const juce::Point<float>* positions,
                     int numGlyphs) noexcept;

    void strokePath (const juce::Path& path,
                     const juce::PathStrokeType& strokeType,
                     bool antiAliased = false);

    void fillPath (const juce::Path& path);

    void fillAll (juce::Colour colour);

    void fillRect (juce::Rectangle<float> area);

    void fillLinearGradient (juce::Rectangle<float> area,
                             juce::Colour outerColour,
                             juce::Colour innerColour,
                             juce::Point<float> outerPoint,
                             juce::Point<float> innerPoint);

    void drawText (const juce::String& text,
                   juce::Rectangle<int> area,
                   juce::Justification justification,
                   bool useEllipsesIfTooBig = true) noexcept;

    void drawText (const juce::String& text,
                   juce::Rectangle<float> area,
                   juce::Justification justification,
                   bool useEllipsesIfTooBig = true) noexcept;

    void drawFittedText (const juce::String& text,
                         int x, int y, int width, int height,
                         juce::Justification justification,
                         int maximumNumberOfLines,
                         float minimumHorizontalScale = 0.0f) noexcept;

    void reduceClipRegion (const juce::Path& clipPath);
    void restoreClipRegion();

    const std::vector<Command>& getCommands() const noexcept;
    const std::vector<TextCommand>& getTextCommands() const noexcept;
    bool hasContent() const noexcept { return not commands.empty() or not textCommands.empty(); }

    void clear() noexcept;

private:
    float width { 0.0f };
    float height { 0.0f };
    float renderingScale { 1.0f };
    juce::Colour currentColour { juce::Colours::white };
    jreng::Font* currentFont { nullptr };
    std::vector<Command> commands;
    std::vector<TextCommand> textCommands;

    void addDrawVertices (std::vector<GLVertex>&& vertices);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLGraphics)
};

} // namespace jreng
