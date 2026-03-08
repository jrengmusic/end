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

    explicit GLGraphics (float viewportWidth, float viewportHeight, float scale = 1.0f);
    ~GLGraphics() = default;

    void setColour (juce::Colour newColour) noexcept;

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

    void reduceClipRegion (const juce::Path& clipPath);
    void restoreClipRegion();

    const std::vector<Command>& getCommands() const noexcept;
    bool hasContent() const noexcept;

    void clear() noexcept;

private:
    float width { 0.0f };
    float height { 0.0f };
    float renderingScale { 1.0f };
    juce::Colour currentColour { juce::Colours::white };
    std::vector<Command> commands;

    void addDrawVertices (std::vector<GLVertex>&& vertices);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLGraphics)
};

} // namespace jreng
