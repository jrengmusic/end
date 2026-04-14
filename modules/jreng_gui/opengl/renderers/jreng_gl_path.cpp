namespace jreng
{

static constexpr int maxSubdivisionDepth { 10 };
static constexpr float floatEpsilon { 1.0e-6f };
static constexpr float maxMiterScale { 600.0f };

static void flattenCubic (std::vector<GLPath::LineSegment>& segments,
                           juce::Point<float> p0,
                           juce::Point<float> p1,
                           juce::Point<float> p2,
                           juce::Point<float> p3,
                           float tolerance,
                           int depth = 0) noexcept
{
    if (depth > maxSubdivisionDepth)
    {
        segments.push_back ({ p0, p3 });
        return;
    }

    const auto d { p0.getDistanceFrom (p1) + p1.getDistanceFrom (p2) + p2.getDistanceFrom (p3) };
    const auto straight { p0.getDistanceFrom (p3) };

    if (d - straight < tolerance)
    {
        segments.push_back ({ p0, p3 });
        return;
    }

    const auto p01 { (p0 + p1) * 0.5f };
    const auto p12 { (p1 + p2) * 0.5f };
    const auto p23 { (p2 + p3) * 0.5f };
    const auto p012 { (p01 + p12) * 0.5f };
    const auto p123 { (p12 + p23) * 0.5f };
    const auto p0123 { (p012 + p123) * 0.5f };

    flattenCubic (segments, p0, p01, p012, p0123, tolerance, depth + 1);
    flattenCubic (segments, p0123, p123, p23, p3, tolerance, depth + 1);
}

static void flattenQuadratic (std::vector<GLPath::LineSegment>& segments,
                              juce::Point<float> p0,
                              juce::Point<float> p1,
                              juce::Point<float> p2,
                              float tolerance,
                              int depth = 0) noexcept
{
    if (depth > maxSubdivisionDepth)
    {
        segments.push_back ({ p0, p2 });
        return;
    }

    const auto d { p0.getDistanceFrom (p1) + p1.getDistanceFrom (p2) };
    const auto straight { p0.getDistanceFrom (p2) };

    if (d - straight < tolerance)
    {
        segments.push_back ({ p0, p2 });
        return;
    }

    const auto p01 { (p0 + p1) * 0.5f };
    const auto p12 { (p1 + p2) * 0.5f };
    const auto p012 { (p01 + p12) * 0.5f };

    flattenQuadratic (segments, p0, p01, p012, tolerance, depth + 1);
    flattenQuadratic (segments, p012, p12, p2, tolerance, depth + 1);
}

std::vector<std::vector<GLPath::LineSegment>> GLPath::flattenPath (const juce::Path& path,
                                                                    float tolerance) noexcept
{
    std::vector<std::vector<LineSegment>> subPaths;
    std::vector<LineSegment> current;

    juce::Path::Iterator it { path };
    juce::Point<float> cursor { 0.0f, 0.0f };
    juce::Point<float> subPathStart { 0.0f, 0.0f };

    while (it.next())
    {
        if (it.elementType == juce::Path::Iterator::startNewSubPath)
        {
            if (! current.empty())
            {
                subPaths.push_back (current);
                current.clear();
            }

            cursor = { it.x1, it.y1 };
            subPathStart = cursor;
        }
        else if (it.elementType == juce::Path::Iterator::lineTo)
        {
            const juce::Point<float> next { it.x1, it.y1 };
            current.push_back ({ cursor, next });
            cursor = next;
        }
        else if (it.elementType == juce::Path::Iterator::quadraticTo)
        {
            const juce::Point<float> p1 { it.x1, it.y1 };
            const juce::Point<float> p2 { it.x2, it.y2 };
            flattenQuadratic (current, cursor, p1, p2, tolerance);
            cursor = p2;
        }
        else if (it.elementType == juce::Path::Iterator::cubicTo)
        {
            const juce::Point<float> p1 { it.x1, it.y1 };
            const juce::Point<float> p2 { it.x2, it.y2 };
            const juce::Point<float> p3 { it.x3, it.y3 };
            flattenCubic (current, cursor, p1, p2, p3, tolerance);
            cursor = p3;
        }
        else if (it.elementType == juce::Path::Iterator::closePath)
        {
            if (cursor != subPathStart)
                current.push_back ({ cursor, subPathStart });

            if (! current.empty())
            {
                subPaths.push_back (current);
                current.clear();
            }

            cursor = subPathStart;
        }
    }

    if (! current.empty())
        subPaths.push_back (current);

    return subPaths;
}

juce::Point<float> GLPath::getNormal (const LineSegment& seg) noexcept
{
    const float dx { seg.end.x - seg.start.x };
    const float dy { seg.end.y - seg.start.y };
    const float len { std::sqrt (dx * dx + dy * dy) };

    if (len < floatEpsilon)
        return { 0.0f, 0.0f };

    return { -dy / len, dx / len };
}

std::vector<GLVertex> GLPath::buildTriangleStrip (const std::vector<LineSegment>& segments,
                                                   float halfWidth,
                                                   juce::Colour colour,
                                                   float fringeWidth) noexcept
{
    if (segments.empty())
        return {};

    std::vector<juce::Point<float>> points;
    points.reserve (segments.size() + 1);
    points.push_back (segments.front().start);

    for (const auto& seg : segments)
        points.push_back (seg.end);

    const auto numPoints { points.size() };

    if (numPoints < 2)
        return {};

    std::vector<juce::Point<float>> dm (numPoints);

    for (size_t i { 0 }; i < numPoints; ++i)
    {
        juce::Point<float> dlPrev { 0.0f, 0.0f };
        juce::Point<float> dlNext { 0.0f, 0.0f };
        const bool hasPrev { i > 0 };
        const bool hasNext { i + 1 < numPoints };

        if (hasPrev)
        {
            const float dx { points.at (i).x - points.at (i - 1).x };
            const float dy { points.at (i).y - points.at (i - 1).y };
            const float len { std::sqrt (dx * dx + dy * dy) };

            if (len > floatEpsilon)
                dlPrev = { -dy / len, dx / len };
        }

        if (hasNext)
        {
            const float dx { points.at (i + 1).x - points.at (i).x };
            const float dy { points.at (i + 1).y - points.at (i).y };
            const float len { std::sqrt (dx * dx + dy * dy) };

            if (len > floatEpsilon)
                dlNext = { -dy / len, dx / len };
        }

        if (hasPrev and hasNext)
        {
            float mx { (dlPrev.x + dlNext.x) * 0.5f };
            float my { (dlPrev.y + dlNext.y) * 0.5f };
            const float dmr2 { mx * mx + my * my };

            if (dmr2 > floatEpsilon)
            {
                float scale { 1.0f / dmr2 };

                if (scale > maxMiterScale)
                    scale = maxMiterScale;

                mx *= scale;
                my *= scale;
            }

            dm.at (i) = { mx, my };
        }
        else if (hasNext)
        {
            dm.at (i) = dlNext;
        }
        else if (hasPrev)
        {
            dm.at (i) = dlPrev;
        }
    }

    const float cr { colour.getFloatRed() };
    const float cg { colour.getFloatGreen() };
    const float cb { colour.getFloatBlue() };
    const float ca { colour.getFloatAlpha() };

    auto makeVertex = [cr, cg, cb, ca] (juce::Point<float> p) -> GLVertex
    {
        return { p.x, p.y, cr, cg, cb, ca };
    };

    auto makeFringe = [cr, cg, cb] (juce::Point<float> p) -> GLVertex
    {
        return { p.x, p.y, cr, cg, cb, 0.0f };
    };

    const float coreWidth { halfWidth + fringeWidth };

    std::vector<GLVertex> vertices;
    vertices.reserve (numPoints * (fringeWidth > 0.0f ? 18 : 6));

    for (size_t i { 0 }; i + 1 < numPoints; ++i)
    {
        const auto& p0 { points.at (i) };
        const auto& p1 { points.at (i + 1) };
        const auto& dm0 { dm.at (i) };
        const auto& dm1 { dm.at (i + 1) };

        const auto topLeft     { p0 + dm0 * coreWidth };
        const auto bottomLeft  { p0 - dm0 * coreWidth };
        const auto topRight    { p1 + dm1 * coreWidth };
        const auto bottomRight { p1 - dm1 * coreWidth };

        vertices.push_back (makeVertex (topLeft));
        vertices.push_back (makeVertex (bottomLeft));
        vertices.push_back (makeVertex (topRight));

        vertices.push_back (makeVertex (topRight));
        vertices.push_back (makeVertex (bottomLeft));
        vertices.push_back (makeVertex (bottomRight));

        if (fringeWidth > 0.0f)
        {
            const auto fringeTopLeft     { p0 + dm0 * (coreWidth + fringeWidth) };
            const auto fringeTopRight    { p1 + dm1 * (coreWidth + fringeWidth) };
            const auto fringeBottomLeft  { p0 - dm0 * (coreWidth + fringeWidth) };
            const auto fringeBottomRight { p1 - dm1 * (coreWidth + fringeWidth) };

            vertices.push_back (makeVertex (topLeft));
            vertices.push_back (makeFringe (fringeTopLeft));
            vertices.push_back (makeVertex (topRight));

            vertices.push_back (makeVertex (topRight));
            vertices.push_back (makeFringe (fringeTopLeft));
            vertices.push_back (makeFringe (fringeTopRight));

            vertices.push_back (makeVertex (bottomLeft));
            vertices.push_back (makeVertex (bottomRight));
            vertices.push_back (makeFringe (fringeBottomLeft));

            vertices.push_back (makeVertex (bottomRight));
            vertices.push_back (makeFringe (fringeBottomRight));
            vertices.push_back (makeFringe (fringeBottomLeft));
        }
    }

    return vertices;
}

std::vector<GLVertex> GLPath::tessellateStroke (const juce::Path& path,
                                                 const juce::PathStrokeType& strokeType,
                                                 juce::Colour colour,
                                                 float fringeWidth) noexcept
{
    const auto subPaths { flattenPath (path) };
    const float halfWidth { strokeType.getStrokeThickness() * 0.5f };

    std::vector<GLVertex> vertices;
    vertices.reserve (subPaths.size() * 100);

    for (const auto& segments : subPaths)
    {
        if (segments.empty())
            continue;

        auto strip { buildTriangleStrip (segments, halfWidth, colour, fringeWidth) };
        vertices.insert (vertices.end(), strip.begin(), strip.end());
    }

    return vertices;
}

std::vector<GLVertex> GLPath::tessellateFill (const juce::Path& path,
                                              juce::Colour colour) noexcept
{
    const auto subPaths { flattenPath (path) };
    std::vector<GLVertex> vertices;

    const float cr { colour.getFloatRed() };
    const float cg { colour.getFloatGreen() };
    const float cb { colour.getFloatBlue() };
    const float ca { colour.getFloatAlpha() };

    auto makeVertex = [cr, cg, cb, ca] (juce::Point<float> p) -> GLVertex
    {
        return { p.x, p.y, cr, cg, cb, ca };
    };

    for (const auto& segments : subPaths)
    {
        if (segments.size() < 2)
            continue;

        std::vector<juce::Point<float>> points;
        points.reserve (segments.size() + 1);
        points.push_back (segments.front().start);

        for (const auto& seg : segments)
            points.push_back (seg.end);

        juce::Point<float> centroid { 0.0f, 0.0f };

        for (const auto& p : points)
            centroid += p;

        centroid /= static_cast<float> (points.size());

        // TODO: replace with ear-clipping for non-convex shapes
        for (size_t i { 0 }; i < points.size(); ++i)
        {
            const auto& current { points.at (i) };
            const auto& next { points.at ((i + 1) % points.size()) };

            vertices.push_back (makeVertex (centroid));
            vertices.push_back (makeVertex (current));
            vertices.push_back (makeVertex (next));
        }
    }

    return vertices;
}

} // namespace jreng
