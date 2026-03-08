#pragma once

namespace jreng
{

static constexpr float defaultFlattenTolerance { 0.5f };

struct GLVertex
{
    float x { 0.0f };
    float y { 0.0f };
    float r { 0.0f };
    float g { 0.0f };
    float b { 0.0f };
    float a { 0.0f };
};

class GLPath
{
public:
    GLPath() = default;
    ~GLPath() = default;

    struct LineSegment
    {
        juce::Point<float> start;
        juce::Point<float> end;
    };

    static std::vector<GLVertex> tessellateStroke (const juce::Path& path,
                                                   const juce::PathStrokeType& strokeType,
                                                   juce::Colour colour,
                                                   float fringeWidth = 0.0f) noexcept;

    static std::vector<GLVertex> tessellateFill (const juce::Path& path,
                                                  juce::Colour colour) noexcept;

private:
    static std::vector<std::vector<LineSegment>> flattenPath (const juce::Path& path,
                                                               float tolerance = defaultFlattenTolerance) noexcept;

    static std::vector<GLVertex> buildTriangleStrip (const std::vector<LineSegment>& segments,
                                                      float halfWidth,
                                                      juce::Colour colour,
                                                      float fringeWidth) noexcept;

    static juce::Point<float> getNormal (const LineSegment& seg) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLPath)
};

} // namespace jreng
