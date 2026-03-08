namespace jreng
{

GLGraphics::GLGraphics (float viewportWidth, float viewportHeight, float scale)
    : width (viewportWidth)
    , height (viewportHeight)
    , renderingScale (scale)
{
}

void GLGraphics::setColour (juce::Colour newColour) noexcept
{
    currentColour = newColour;
}

void GLGraphics::strokePath (const juce::Path& path,
                              const juce::PathStrokeType& strokeType,
                              bool antiAliased)
{
    juce::Path scaled;
    scaled.addPath (path, juce::AffineTransform::scale (renderingScale));

    if (strokeType.getStrokeThickness() <= 1.0f)
    {
        juce::PathFlatteningIterator it (scaled);

        const float r { currentColour.getFloatRed() };
        const float g { currentColour.getFloatGreen() };
        const float b { currentColour.getFloatBlue() };
        const float a { currentColour.getFloatAlpha() };

        std::vector<GLVertex> vertices;

        if (it.next())
        {
            vertices.push_back ({ it.x1, it.y1, r, g, b, a });
            vertices.push_back ({ it.x2, it.y2, r, g, b, a });

            while (it.next())
                vertices.push_back ({ it.x2, it.y2, r, g, b, a });
        }

        if (not vertices.empty())
        {
            Command cmd;
            cmd.type = CommandType::drawLineStrip;
            cmd.vertices = std::move (vertices);
            commands.push_back (std::move (cmd));
        }

        return;
    }

    const float physicalWidth { std::max (strokeType.getStrokeThickness(), 1.0f) };
    const juce::PathStrokeType physical { physicalWidth,
                                          strokeType.getJointStyle(),
                                          strokeType.getEndStyle() };
    const float fringe { antiAliased ? 1.0f : 0.0f };
    addDrawVertices (GLPath::tessellateStroke (scaled, physical, currentColour, fringe));
}

void GLGraphics::fillPath (const juce::Path& path)
{
    juce::Path scaled;
    scaled.addPath (path, juce::AffineTransform::scale (renderingScale));

    addDrawVertices (GLPath::tessellateFill (scaled, currentColour));
}

void GLGraphics::fillAll (juce::Colour colour)
{
    juce::Path rect;
    rect.addRectangle (0.0f, 0.0f, width * renderingScale, height * renderingScale);
    setColour (colour);

    addDrawVertices (GLPath::tessellateFill (rect, currentColour));
}

void GLGraphics::fillRect (juce::Rectangle<float> area)
{
    juce::Path rect;
    rect.addRectangle (area * renderingScale);

    addDrawVertices (GLPath::tessellateFill (rect, currentColour));
}

void GLGraphics::fillLinearGradient (juce::Rectangle<float> area,
                                      juce::Colour outerColour,
                                      juce::Colour innerColour,
                                      juce::Point<float> outerPoint,
                                      juce::Point<float> innerPoint)
{
    const float s { renderingScale };
    const float x0 { area.getX() * s };
    const float y0 { area.getY() * s };
    const float x1 { area.getRight() * s };
    const float y1 { area.getBottom() * s };

    const float ox { outerPoint.x * s };
    const float oy { outerPoint.y * s };
    const float ix { innerPoint.x * s };
    const float iy { innerPoint.y * s };

    const float or_ { outerColour.getFloatRed() };
    const float og { outerColour.getFloatGreen() };
    const float ob { outerColour.getFloatBlue() };
    const float oa { outerColour.getFloatAlpha() };

    const float ir { innerColour.getFloatRed() };
    const float ig { innerColour.getFloatGreen() };
    const float ib { innerColour.getFloatBlue() };
    const float ia { innerColour.getFloatAlpha() };

    const bool isVertical { std::abs (ox - ix) < 0.001f };

    std::vector<GLVertex> vertices;
    vertices.reserve (6);

    if (isVertical)
    {
        const bool outerOnTop { oy < iy };
        const float topR { outerOnTop ? or_ : ir };
        const float topG { outerOnTop ? og : ig };
        const float topB { outerOnTop ? ob : ib };
        const float topA { outerOnTop ? oa : ia };
        const float botR { outerOnTop ? ir : or_ };
        const float botG { outerOnTop ? ig : og };
        const float botB { outerOnTop ? ib : ob };
        const float botA { outerOnTop ? ia : oa };

        vertices.push_back ({ x0, y0, topR, topG, topB, topA });
        vertices.push_back ({ x1, y0, topR, topG, topB, topA });
        vertices.push_back ({ x1, y1, botR, botG, botB, botA });

        vertices.push_back ({ x0, y0, topR, topG, topB, topA });
        vertices.push_back ({ x1, y1, botR, botG, botB, botA });
        vertices.push_back ({ x0, y1, botR, botG, botB, botA });
    }
    else
    {
        const bool outerOnLeft { ox < ix };
        const float leftR { outerOnLeft ? or_ : ir };
        const float leftG { outerOnLeft ? og : ig };
        const float leftB { outerOnLeft ? ob : ib };
        const float leftA { outerOnLeft ? oa : ia };
        const float rightR { outerOnLeft ? ir : or_ };
        const float rightG { outerOnLeft ? ig : og };
        const float rightB { outerOnLeft ? ib : ob };
        const float rightA { outerOnLeft ? ia : oa };

        vertices.push_back ({ x0, y0, leftR, leftG, leftB, leftA });
        vertices.push_back ({ x1, y0, rightR, rightG, rightB, rightA });
        vertices.push_back ({ x1, y1, rightR, rightG, rightB, rightA });

        vertices.push_back ({ x0, y0, leftR, leftG, leftB, leftA });
        vertices.push_back ({ x1, y1, rightR, rightG, rightB, rightA });
        vertices.push_back ({ x0, y1, leftR, leftG, leftB, leftA });
    }

    addDrawVertices (std::move (vertices));
}

void GLGraphics::reduceClipRegion (const juce::Path& clipPath)
{
    juce::Path scaled;
    scaled.addPath (clipPath, juce::AffineTransform::scale (renderingScale));

    Command cmd;
    cmd.type = CommandType::pushClip;
    cmd.vertices = GLPath::tessellateFill (scaled, juce::Colours::white);
    commands.push_back (std::move (cmd));
}

void GLGraphics::restoreClipRegion()
{
    Command cmd;
    cmd.type = CommandType::popClip;
    commands.push_back (std::move (cmd));
}

const std::vector<GLGraphics::Command>& GLGraphics::getCommands() const noexcept
{
    return commands;
}

bool GLGraphics::hasContent() const noexcept
{
    return not commands.empty();
}

void GLGraphics::clear() noexcept
{
    commands.clear();
}

void GLGraphics::addDrawVertices (std::vector<GLVertex>&& vertices)
{
    if (not vertices.empty())
    {
        Command cmd;
        cmd.type = CommandType::draw;
        cmd.vertices = std::move (vertices);
        commands.push_back (std::move (cmd));
    }
}

} // namespace jreng
