namespace jreng::Mermaid
{ /*____________________________________________________________________________*/

using jreng::IDref;

// Phase 1: CSS Parsing
juce::Array<Graphic::CSSRule> Graphic::extractCSSRules (juce::XmlElement* styleElement)
{
    juce::Array<CSSRule> rules;

    if (styleElement != nullptr)
    {
        juce::String cssText { styleElement->getAllSubText() };

        int pos { 0 };
        bool hasBraces { true };
        while (pos < cssText.length() and hasBraces)
        {
            int braceOpen { cssText.indexOf (pos, "{") };
            if (braceOpen <= 0)
            {
                hasBraces = false;
            }
            else
            {
                juce::String selector { cssText.substring (pos, braceOpen).trim() };

                int braceClose { cssText.indexOf (braceOpen, "}") };
                if (braceClose <= 0)
                {
                    hasBraces = false;
                }
                else
                {
                    juce::String propertiesText { cssText.substring (braceOpen + 1, braceClose).trim() };

                    CSSRule rule;
                    rule.selector = selector;

                    juce::StringArray propertyPairs;
                    propertyPairs.addTokens (propertiesText, ";", "");

                    for (const auto& pair : propertyPairs)
                    {
                        juce::String trimmedPair { pair.trim() };
                        if (trimmedPair.isNotEmpty())
                        {
                            int colonPos { trimmedPair.indexOf (":") };
                            if (colonPos > 0)
                            {
                                juce::String key { trimmedPair.substring (0, colonPos).trim() };
                                juce::String value { trimmedPair.substring (colonPos + 1).trim() };
                                rule.properties.set (key, value);
                            }
                        }
                    }

                    rules.add (rule);
                    pos = braceClose + 1;
                }
            }
        }
    }

    return rules;
}

juce::StringPairArray Graphic::parseInlineStyle (const juce::String& styleAttr)
{
    juce::StringPairArray result;

    if (styleAttr.isNotEmpty())
    {
        juce::StringArray propertyPairs;
        propertyPairs.addTokens (styleAttr, ";", "");

        for (const auto& pair : propertyPairs)
        {
            juce::String trimmedPair { pair.trim() };
            if (trimmedPair.isNotEmpty())
            {
                int colonPos { trimmedPair.indexOf (":") };
                if (colonPos > 0)
                {
                    juce::String key { trimmedPair.substring (0, colonPos).trim() };
                    juce::String value { trimmedPair.substring (colonPos + 1).trim() };
                    result.set (key, value);
                }
            }
        }
    }

    return result;
}

juce::Colour Graphic::resolveColour (const juce::String& colourStr)
{
    juce::Colour colour { juce::Colours::magenta };

    if (colourStr.isNotEmpty())
    {
        juce::Colour parsed { juce::Colour::fromString (colourStr) };
        if (parsed != juce::Colour() and parsed.getAlpha() != 0)
            colour = parsed;
    }

    return colour;
}

float Graphic::resolveFloat (const juce::String& numericStr)
{
    float result { 1.0f };

    if (numericStr.isNotEmpty())
    {
        juce::String stripped { numericStr.trim() };

        if (stripped.endsWith ("px") or stripped.endsWith ("em") or stripped.endsWith ("%") or stripped.endsWith ("pt"))
            stripped = stripped.substring (0, stripped.length() - 2);

        float parsed { stripped.getFloatValue() };
        if (parsed > 0)
            result = parsed;
    }

    return result;
}

juce::Array<Graphic::CSSRule> Graphic::matchCSSSelectors (
    const juce::String& classAttr,
    const juce::String& idAttr,
    const juce::Array<CSSRule>& allCSSRules)
{
    juce::Array<CSSRule> matchingRules;

    for (const auto& rule : allCSSRules)
    {
        juce::String selector { rule.selector.trim() };

        if (selector.startsWith ("."))
        {
            juce::String className { selector.substring (1) };
            if (classAttr.contains (className))
                matchingRules.add (rule);
        }
        else if (selector.startsWith ("#"))
        {
            juce::String idName { selector.substring (1) };
            if (idAttr == idName)
                matchingRules.add (rule);
        }
    }

    return matchingRules;
}

juce::StringPairArray Graphic::mergeStyles (
    const juce::Array<CSSRule>& cssRules,
    const juce::StringPairArray& inlineStyle)
{
    juce::StringPairArray result;

    for (const auto& rule : cssRules)
    {
        for (int i { 0 }; i < rule.properties.size(); ++i)
        {
            juce::String key { rule.properties.getAllKeys().getReference (i) };
            juce::String value { rule.properties[key] };
            result.set (key, value);
        }
    }

    for (int i { 0 }; i < inlineStyle.size(); ++i)
    {
        juce::String key { inlineStyle.getAllKeys().getReference (i) };
        juce::String value { inlineStyle[key] };
        result.set (key, value);
    }

    return result;
}

// Phase 2: Element Extraction
Graphic::Diagram Graphic::extractPath (
    juce::XmlElement* pathElement,
    const juce::StringPairArray& resolvedStyles)
{
    jassert (pathElement != nullptr);

    juce::String pathData { pathElement->getStringAttribute (IDref::d) };
    jassert (not pathData.isEmpty());

    juce::Path path { juce::Drawable::parseSVGPath (pathData) };
    juce::Colour colour { resolveColour (resolvedStyles.getValue (IDref::fill, "")) };
    float strokeWidth { resolveFloat (resolvedStyles.getValue ("stroke-width", "")) };

    Diagram diagram;
    diagram.type = Diagram::StrokePath;
    diagram.path = path;
    diagram.colour = colour;
    diagram.strokeWidth = strokeWidth;

    return diagram;
}

Graphic::Diagram Graphic::extractRect (
    juce::XmlElement* rectElement,
    const juce::StringPairArray& resolvedStyles)
{
    jassert (rectElement != nullptr);

    float x { static_cast<float> (rectElement->getDoubleAttribute (IDref::x, 0.0)) };
    float y { static_cast<float> (rectElement->getDoubleAttribute (IDref::y, 0.0)) };
    float w { static_cast<float> (rectElement->getDoubleAttribute (IDref::width, 0.0)) };
    float h { static_cast<float> (rectElement->getDoubleAttribute (IDref::height, 0.0)) };

    juce::Path path;
    path.addRectangle (x, y, w, h);

    juce::Colour colour { resolveColour (resolvedStyles.getValue (IDref::fill, "")) };
    float strokeWidth { resolveFloat (resolvedStyles.getValue ("stroke-width", "")) };

    Diagram diagram;
    diagram.type = Diagram::FillPath;
    diagram.path = path;
    diagram.colour = colour;
    diagram.strokeWidth = strokeWidth;

    return diagram;
}

Graphic::Diagram Graphic::extractCircle (
    juce::XmlElement* circleElement,
    const juce::StringPairArray& resolvedStyles)
{
    jassert (circleElement != nullptr);

    float cx { static_cast<float> (circleElement->getDoubleAttribute (IDref::cx, 0.0)) };
    float cy { static_cast<float> (circleElement->getDoubleAttribute (IDref::cy, 0.0)) };
    float r { static_cast<float> (circleElement->getDoubleAttribute (IDref::r, 0.0)) };

    jassert (r > 0);

    juce::Path path;
    path.addEllipse (cx - r, cy - r, r * 2, r * 2);

    juce::Colour colour { resolveColour (resolvedStyles.getValue (IDref::fill, "")) };
    float strokeWidth { resolveFloat (resolvedStyles.getValue ("stroke-width", "")) };

    Diagram diagram;
    diagram.type = Diagram::FillPath;
    diagram.path = path;
    diagram.colour = colour;
    diagram.strokeWidth = strokeWidth;

    return diagram;
}

Graphic::Diagram Graphic::extractEllipse (
    juce::XmlElement* ellipseElement,
    const juce::StringPairArray& resolvedStyles)
{
    jassert (ellipseElement != nullptr);

    float cx { static_cast<float> (ellipseElement->getDoubleAttribute (IDref::cx, 0.0)) };
    float cy { static_cast<float> (ellipseElement->getDoubleAttribute (IDref::cy, 0.0)) };
    float rx { static_cast<float> (ellipseElement->getDoubleAttribute (IDref::rx, 0.0)) };
    float ry { static_cast<float> (ellipseElement->getDoubleAttribute (IDref::ry, 0.0)) };

    jassert (rx > 0 and ry > 0);

    juce::Path path;
    path.addEllipse (cx - rx, cy - ry, rx * 2, ry * 2);

    juce::Colour colour { resolveColour (resolvedStyles.getValue (IDref::fill, "")) };
    float strokeWidth { resolveFloat (resolvedStyles.getValue ("stroke-width", "")) };

    Diagram diagram;
    diagram.type = Diagram::FillPath;
    diagram.path = path;
    diagram.colour = colour;
    diagram.strokeWidth = strokeWidth;

    return diagram;
}

Graphic::Diagram Graphic::extractText (
    juce::XmlElement* textElement,
    const juce::StringPairArray& resolvedStyles)
{
    jassert (textElement != nullptr);

    Diagram diagram;

    juce::String text { textElement->getAllSubText() };
    if (text.isNotEmpty())
    {
        float x { static_cast<float> (textElement->getDoubleAttribute (IDref::x, 0.0)) };
        float y { static_cast<float> (textElement->getDoubleAttribute (IDref::y, 0.0)) };

        juce::String fontFamily { resolvedStyles.getValue (IDref::font_family, "sans-serif") };
        float fontSize { resolveFloat (resolvedStyles.getValue (IDref::font_size, "16")) };
        juce::FontOptions font (fontFamily, fontSize, juce::Font::plain);

        juce::Colour colour { resolveColour (resolvedStyles.getValue (IDref::fill, "")) };

        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, text, x, y);

        diagram.type = Diagram::DrawText;
        diagram.glyphs = glyphs;
        diagram.colour = colour;
    }

    return diagram;
}

juce::AffineTransform Graphic::parseTransform (const juce::String& transformAttr)
{
    juce::AffineTransform transform;
    juce::StringArray transformParts;
    transformParts.addTokens (transformAttr, " ", "");

    for (const auto& part : transformParts)
    {
        juce::String trimmedPart { part.trim() };
        if (trimmedPart.isNotEmpty())
        {
            if (trimmedPart.startsWith ("translate("))
            {
                juce::String content { trimmedPart.substring (juce::String ("translate(").length(), trimmedPart.length() - 1) };
                juce::StringArray values;
                values.addTokens (content, ",", "");

                if (values.size() >= 1)
                {
                    float tx { values.getReference (0).trim().getFloatValue() };
                    float ty { values.size() >= 2 ? values.getReference (1).trim().getFloatValue() : 0.0f };
                    transform = transform.translated (tx, ty);
                }
            }
            else if (trimmedPart.startsWith ("scale("))
            {
                juce::String content { trimmedPart.substring (juce::String ("scale(").length(), trimmedPart.length() - 1) };
                juce::StringArray values;
                values.addTokens (content, ",", "");

                if (values.size() >= 1)
                {
                    float sx { values.getReference (0).trim().getFloatValue() };
                    float sy { values.size() >= 2 ? values.getReference (1).trim().getFloatValue() : sx };
                    transform = transform.scaled (sx, sy);
                }
            }
            else if (trimmedPart.startsWith ("rotate("))
            {
                juce::String content { trimmedPart.substring (juce::String ("rotate(").length(), trimmedPart.length() - 1) };
                float angle { content.trim().getFloatValue() };
                transform = transform.rotated (angle * (juce::MathConstants<float>::pi / 180.0f));
            }
            else if (trimmedPart.startsWith ("matrix("))
            {
                juce::String content { trimmedPart.substring (juce::String ("matrix(").length(), trimmedPart.length() - 1) };
                juce::StringArray values;
                values.addTokens (content, ",", "");

                if (values.size() >= 6)
                {
                    float a { values.getReference (0).trim().getFloatValue() };
                    float b { values.getReference (1).trim().getFloatValue() };
                    float c { values.getReference (2).trim().getFloatValue() };
                    float d { values.getReference (3).trim().getFloatValue() };
                    float e { values.getReference (4).trim().getFloatValue() };
                    float f { values.getReference (5).trim().getFloatValue() };

                    transform = juce::AffineTransform (a, b, c, d, e, f);
                }
            }
        }
    }

    return transform;
}

// Phase 3: Main Extraction
void Graphic::processElement (
    juce::XmlElement* element,
    const juce::Array<CSSRule>& allCSSRules,
    Diagrams& outDiagrams)
{
    juce::String tag { element->getTagName() };
    juce::String classAttr { element->getStringAttribute ("class", "") };
    juce::String idAttr { element->getStringAttribute (IDref::id, "") };
    juce::String styleAttr { element->getStringAttribute (IDref::style, "") };
    juce::String transformAttr { element->getStringAttribute ("transform", "") };

    auto cssRules { matchCSSSelectors (classAttr, idAttr, allCSSRules) };
    auto inlineStyle { parseInlineStyle (styleAttr) };
    auto resolvedStyles { mergeStyles (cssRules, inlineStyle) };
    auto transform { parseTransform (transformAttr) };

    if (tag == IDref::path)
    {
        auto diagram { extractPath (element, resolvedStyles) };
        diagram.transform = transform;
        outDiagrams.push_back (diagram);
    }
    else if (tag == IDref::rect)
    {
        auto diagram { extractRect (element, resolvedStyles) };
        diagram.transform = transform;
        outDiagrams.push_back (diagram);
    }
    else if (tag == IDref::circle)
    {
        auto diagram { extractCircle (element, resolvedStyles) };
        diagram.transform = transform;
        outDiagrams.push_back (diagram);
    }
    else if (tag == IDref::ellipse)
    {
        auto diagram { extractEllipse (element, resolvedStyles) };
        diagram.transform = transform;
        outDiagrams.push_back (diagram);
    }
    else if (tag == IDref::text)
    {
        auto diagram { extractText (element, resolvedStyles) };
        diagram.transform = transform;
        outDiagrams.push_back (diagram);
    }
    // Ignore: <style>, <marker>, <defs>, <g>, etc.
}

Graphic::Diagrams Graphic::extractFromSVG (const juce::String& svgString)
{
    auto svg { juce::parseXML (svgString) };
    jassert (svg != nullptr);

    auto styleElement { jreng::XML::getChildByName (svg.get(), IDref::style) };
    auto cssRules { extractCSSRules (styleElement) };

    Diagrams diagrams;
    jreng::XML::applyFunctionRecursively (svg.get(), [&] (juce::XmlElement* element)
                                          {
                                              processElement (element, cssRules, diagrams);
                                          });

    return diagrams;
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng::Mermaid
