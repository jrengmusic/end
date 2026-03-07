namespace jreng
{
/*____________________________________________________________________________*/
/** -------------------- FUNCTIONS FOR READING SVG ---------------------------*/
const juce::String SVG::Filter::group (juce::XmlElement* svg)
{
    juce::String filtered { svg->toString().replace ("</g>", "") };

    while (filtered.contains ("<g"))
    {
        auto toBeRemoved = filtered
                               .fromFirstOccurrenceOf ("<g", true, true)
                               .upToFirstOccurrenceOf (">", true, true);

        filtered = filtered.replaceFirstOccurrenceOf (toBeRemoved, "", true);
    }

    return filtered;
}

const juce::String SVG::Filter::declaration (juce::XmlElement* svg)
{
    juce::String filtered { svg->toString() };
    juce::String remove { filtered.fromFirstOccurrenceOf ("<?xml", true, true)
                              .upToFirstOccurrenceOf ("<svg", false, true) };

    filtered = filtered.replaceFirstOccurrenceOf (remove, "", true);
    remove = filtered.fromFirstOccurrenceOf (" version", true, true)
                 .upToFirstOccurrenceOf (" style", false, true);

    filtered = filtered.replaceFirstOccurrenceOf (remove, "", true);

    return filtered.trim();
}

//==============================================================================
/** -------------------- FUNCTIONS FOR DRAWING SVG ---------------------------*/

void SVG::draw (juce::Graphics& g, juce::Rectangle<int> area, const juce::File& file)
{
    juce::DrawableImage::createFromSVG (*juce::parseXML (juce::File (file)))->drawWithin (g, area.toFloat(), juce::RectanglePlacement::centred, 1.0f);
}

void SVG::draw (juce::Graphics& g, juce::Rectangle<int> area, juce::XmlElement* e)
{
    juce::DrawableImage::createFromSVG (*e)->drawWithin (g, area.toFloat(), juce::RectanglePlacement::centred, 1.0f);
}

void SVG::draw (juce::Graphics& g, juce::Rectangle<int> area, juce::Drawable* drawable)
{
    drawable->drawWithin (g, area.toFloat(), juce::RectanglePlacement::centred, 1.0f);
}

const juce::Path SVG::getEllipsePath (juce::XmlElement* xml)
{
    juce::Path path;

    for (auto* e : xml->getChildWithTagNameIterator (IDref::ellipse))
    {
        const float cx { static_cast<float> (e->getDoubleAttribute (IDref::cx)) };
        const float cy { static_cast<float> (e->getDoubleAttribute (IDref::cy)) };
        const float rx { static_cast<float> (e->getDoubleAttribute (IDref::rx)) };
        const float ry { static_cast<float> (e->getDoubleAttribute (IDref::ry)) };
        const float x { cx - rx };
        const float y { cy - ry };

        path.addEllipse (x, y, rx * 2, ry * 2);
    }

    return path;
}

const juce::Path SVG::getCirclePath (juce::XmlElement* xml)
{

    
    juce::Path path;

    for (auto* e : xml->getChildWithTagNameIterator (IDref::circle))
    {
        const float cx { static_cast<float> (e->getDoubleAttribute (IDref::cx)) };
        const float cy { static_cast<float> (e->getDoubleAttribute (IDref::cy)) };
        const float r { static_cast<float> (e->getDoubleAttribute (IDref::r)) };
        const float x { cx - r };
        const float y { cy - r };

        path.addEllipse (x, y, r * 2, r * 2);
    }

    return path;
}

const juce::Path SVG::getRectPath (juce::XmlElement* xml)
{
    juce::Path path;

    for (auto* e : xml->getChildWithTagNameIterator (IDref::rect))
    {
        const float x { static_cast<float> (e->getDoubleAttribute (IDref::x)) };
        const float y { static_cast<float> (e->getDoubleAttribute (IDref::y)) };
        const float w { static_cast<float> (e->getDoubleAttribute (IDref::width)) };
        const float h { static_cast<float> (e->getDoubleAttribute (IDref::height)) };

        path.addRectangle (juce::Rectangle<float> (x, y, w, h));
    }

    return path;
}

const juce::Path SVG::getAllFoundPath (juce::XmlElement* xml,
                                       ElementType elementToAdd)
{
    juce::Path path;

    switch (elementToAdd)
    {
        case ElementType::path:
            for (auto* e : xml->getChildWithTagNameIterator (IDref::path))
            {
                path.addPath (juce::DrawableImage::parseSVGPath (e->getStringAttribute (IDref::d)));
            }
            break;

        case ElementType::ellipse:
            path.addPath (getEllipsePath (xml));
            break;

        case ElementType::circle:
            path.addPath (getCirclePath (xml));
            break;

        case ElementType::rect:
            path.addPath (getRectPath (xml));
            break;

        case ElementType::all:
            for (auto* e : xml->getChildWithTagNameIterator (IDref::path))
            {
                path.addPath (juce::DrawableImage::parseSVGPath (e->getStringAttribute (IDref::d)));
            }
            path.addPath (getEllipsePath (xml));
            path.addPath (getCirclePath (xml));
            path.addPath (getRectPath (xml));
            break;
    }

    return path;
}

const juce::Path SVG::getPath (juce::XmlElement* svg,
                               ElementType elementToAdd)
{
    juce::Path path;

    if (svg)
    {
        for (auto* group : svg->getChildWithTagNameIterator (IDref::g))
            path.addPath (getAllFoundPath (group, elementToAdd));

        path.addPath (getAllFoundPath (svg, elementToAdd));
    }

    return path;
}

const juce::Path SVG::getPath (const juce::String& svgString,
                               ElementType elementToAdd)
{
    return getPath (juce::parseXML (svgString).get(), elementToAdd);
}

const juce::Path SVG::getPath (juce::XmlElement* svg,
                               const juce::Rectangle<float>& areaToFit,
                               ElementType elementToAdd)
{
    juce::Path path { getPath (svg, elementToAdd) };

    juce::Rectangle<float> source;

    if (svg->hasAttribute (ID::viewBox))
    {
        source = juce::Rectangle<float>::fromString (svg->getStringAttribute (ID::viewBox));
    }
    else
    {
        float width { static_cast<float> (svg->getDoubleAttribute (ID::width)) };
        float height { static_cast<float> (svg->getDoubleAttribute (ID::height)) };
        source = juce::Rectangle<float> { 0.0f, 0.0f, width, height };
    }

    path.applyTransform (juce::RectanglePlacement().getTransformToFit (source, areaToFit));

    return path;
}

const juce::Path SVG::getPath (const juce::String& svg,
                               const juce::Rectangle<float>& areaToFit,
                               ElementType elementToAdd)
{
    return getPath (juce::parseXML (svg).get(), areaToFit, elementToAdd);
}

const juce::Path SVG::getPath (const juce::String& svg,
                               const juce::Rectangle<int>& areaToFit,
                               ElementType elementToAdd)
{
    return getPath (juce::parseXML (svg).get(), areaToFit.toFloat(), elementToAdd);
}

/*____________________________________________________________________________*/
#if JUCE_MODULE_AVAILABLE_juce_gui_basics
std::unique_ptr<juce::DrawablePath>
SVG::getDrawablePath (juce::XmlElement* svg,
                      const juce::Colour& colour,
                      ElementType elementToAdd,
                      PathStyle style,
                      float strokeWidth)
{
    auto drawable { std::make_unique<juce::DrawablePath>() };

    juce::Path path { getPath (svg, elementToAdd) };

    switch (style)
    {
        case PathStyle::alternate:
            path.setUsingNonZeroWinding (false);
        case PathStyle::stroke:
            drawable->setPath (path);
            drawable->setFill (juce::Colour());
            drawable->setStrokeType (juce::PathStrokeType (strokeWidth, juce::PathStrokeType::JointStyle::mitered));
            drawable->setStrokeFill (colour);
            break;

        case PathStyle::fill:
            drawable->setPath (path);
            drawable->setFill (colour);
            break;
    }

    return drawable;
}

const std::unique_ptr<juce::DrawablePath>
SVG::getDrawablePath (const char* svgString,
                      const juce::Colour& colour,
                      ElementType elementToAdd,
                      PathStyle style,
                      float strokeWidth)
{
    return SVG::getDrawablePath (juce::parseXML (svgString).get(), colour, elementToAdd, style, strokeWidth);
}

const std::unique_ptr<juce::Drawable>
SVG::getDrawable (const char* svgString)
{
    return juce::DrawableImage::createFromSVG (*juce::parseXML (svgString));
}
#endif // JUCE_MODULE_AVAILABLE_juce_gui_basics
//==============================================================================
/** -------------------- FUNCTIONS FOR WRITING SVG ---------------------------*/

const juce::String SVG::Template::declaration {
    R"***(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
<svg width="%%svgWidth%%px" height="%%svgHeight%%px" version="1.1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" xml:space="preserve" xmlns:serif="http://www.serif.com/" style="fill-rule:evenodd;clip-rule:evenodd;stroke-linecap:round;stroke-linejoin:round;stroke-miterlimit:1.5;">
%%svgString%%
</svg>)***"
};

const juce::String SVG::Template::parseSVGPath {
    "\t\tjuce::DrawableImage::parseSVGPath (%%path%%),"
};

const juce::String SVG::Template::arrayPath {
    R"***(
    const juce::Array<juce::Path> %%varName%%
    {
%%paths%%
    };)***"
};

const juce::String SVG::Template::Ptr::chars {
    R"***(
    constexpr static const char* const images[]
    {%%pointers%%
    };)***"
};

const juce::String SVG::Template::Ptr::literal {
    R"***(
        R"KuassaSVG(
%%filtered%%
        )KuassaSVG")***"
};

const juce::String SVG::Template::Ptr::createFromSVG {
    "juce::DrawableImage::createFromSVG (*juce::parseXML (%%chars%%))"
};



/*____________________________________________________________________________*/

const juce::String SVG::Format::Ptr::chars (const juce::String& images)
{
    return Template::Ptr::chars.replace ("%%pointers%%", images);
}

const juce::String SVG::Format::Ptr::literal (juce::XmlElement* parsedXML)
{
    return Template::Ptr::literal.replace ("%%filtered%%", Filter::declaration (parsedXML));
}

const juce::String SVG::Format::Ptr::createFromSVG (const juce::String& chars)
{
    return Template::Ptr::createFromSVG.replace ("%%chars%%", chars);
}

/*____________________________________________________________________________*/

const juce::String SVG::Format::pathToString (const juce::Path& path)
{
    if (path.toString().isNotEmpty())
    {
        auto tokenize { juce::StringArray::fromTokens (path.toString().toUpperCase(), true) };

        for (auto& token : tokenize)
        {
            if (token.containsAnyOf ("ABCDEFGHIJKLMNOPQRSTUVWXYZ"))
            {
                token = '*' + token + '*';
            }
            else if (token.containsAnyOf ("0123456789"))
            {
                token = '|' + token + '|';
            }
        }

        return tokenize.joinIntoString ("")
            .replace ("*", "")
            .replace ("|*", "")
            .replace ("*|", "")
            .replace ("||", ", ")
            .replace ("|", "");
    }

    return juce::String();
}

const juce::String SVG::Format::stroke (const juce::Path& path,
                                        const juce::Colour& colour,
                                        float strokeWidth,
                                        const juce::String& pathId)
{
    auto d = pathToString (path);
    if (d.isEmpty())
        return {};

    // Extract RGB hex (#RRGGBB) and alpha separately
    const juce::String rgb = juce::String::formatted ("#%02x%02x%02x",
                                                      colour.getRed(),
                                                      colour.getGreen(),
                                                      colour.getBlue());
    const juce::String alpha = juce::String (colour.getFloatAlpha(), 1).replaceCharacter (',', '.');

    juce::String xml;
    xml << "\n\t\t<path"
        << (pathId.isNotEmpty() ? " id=" + pathId.quoted() : juce::String())
        << " d=\"" << d << "\""
        << " fill=\"none\""
        << " stroke=\"" << rgb << "\""
        << " stroke-width=\"" << juce::String (strokeWidth) << "px\""
        << " stroke-opacity=\"" << alpha << "\""
        << "/>\n";

    return xml;
}

const juce::String SVG::Format::fill (const juce::Path& path,
                                      const juce::Colour& colour,
                                      const juce::String& pathId)
{
    auto d = pathToString (path);
    if (d.isEmpty())
        return {};

    const juce::String rgb = juce::String::formatted ("#%02x%02x%02x",
                                                      colour.getRed(),
                                                      colour.getGreen(),
                                                      colour.getBlue());
    const juce::String alpha = juce::String (colour.getFloatAlpha(), 1).replaceCharacter (',', '.');

    juce::String xml;
    xml << "\n\t\t<path"
        << (pathId.isNotEmpty() ? " id=" + pathId.quoted() : juce::String())
        << " d=\"" << d << "\""
        << " fill=\"" << rgb << "\""
        << " fill-opacity=\"" << alpha << "\""
        << " stroke=\"none\""
        << "/>\n";

    return xml;
}

const juce::String SVG::Format::group (juce::StringRef svgString, const juce::String& groupId)
{
    juce::String format { "\t<g%%groupId%%>\t%%svgString%%\t</g>" };

    format = format.replace ("%%groupId%%", groupId.isNotEmpty() ? " id=" + groupId.quoted() : groupId);

    return format.replace ("%%svgString%%", svgString);
}

const juce::String SVG::Format::rect (const juce::Rectangle<int>& rectangle,
                                      const juce::String& rectId,
                                      const juce::Colour& colour)
{
    enum
    {
        x,
        y,
        width,
        height
    };

    juce::StringArray keywords {
        "%%x%%",
        "%%y%%",
        "%%width%%",
        "%%height%%",
    };

    juce::String formatted {
        "\t<rect id=\"" + String::toValidID (rectId) + "\" x=\"" + keywords[x] + "\" y=\"" + keywords[y] + "\" width=\"" + keywords[width] + "\" height=\"" + keywords[height] + "\" style=\"fill:none;stroke:" + colour.toString().replaceSection (0, 2, "#") + ";\"/>"
    };

    juce::StringArray bounds;
    bounds.addTokens (rectangle.toString(), false);

    for (int index { x }; index < bounds.size(); ++index)
    {
        formatted = formatted.replace (keywords[index], bounds[index]);
    }

    return formatted;
}

const juce::StringArray SVG::Format::getStringArrayPath (juce::XmlElement* svg,
                                                         const juce::String& varName)
{
    juce::StringArray paths;

    if (auto xml = juce::parseXML (Filter::group (svg)))
    {
        for (auto* e : xml->getChildWithTagNameIterator (IDref::path))
        {
            paths.add (Template::parseSVGPath.replace ("%%path%%", e->getStringAttribute (IDref::d)));
        }

        for (auto* e : xml->getChildWithTagNameIterator (IDref::ellipse))
        {
            const float cx { static_cast<float> (e->getDoubleAttribute (IDref::cx)) };
            const float cy { static_cast<float> (e->getDoubleAttribute (IDref::cy)) };
            const float rx { static_cast<float> (e->getDoubleAttribute (IDref::rx)) };
            const float ry { static_cast<float> (e->getDoubleAttribute (IDref::ry)) };
            const float x { cx - rx };
            const float y { cy - ry };

            juce::Path path;
            path.addEllipse (x, y, 2 * rx, 2 * ry);

            paths.add (Template::parseSVGPath.replace ("%%path%%", Format::pathToString (path)));
        }

        for (auto* e : xml->getChildWithTagNameIterator (IDref::circle))
        {
            const float cx { static_cast<float> (e->getDoubleAttribute (IDref::cx)) };
            const float cy { static_cast<float> (e->getDoubleAttribute (IDref::cy)) };
            const float r { static_cast<float> (e->getDoubleAttribute (IDref::r)) };
            const float x { cx - r };
            const float y { cy - r };

            juce::Path path;
            path.addEllipse (x, y, 2 * r, 2 * r);

            paths.add (Template::parseSVGPath.replace ("%%path%%", Format::pathToString (path)));
        }

        for (auto* e : xml->getChildWithTagNameIterator (IDref::rect))
        {
            if (! e->getStringAttribute (IDref::style).equalsIgnoreCase ("fill:none;"))
            {
                const float x { static_cast<float> (e->getDoubleAttribute (IDref::x)) };
                const float y { static_cast<float> (e->getDoubleAttribute (IDref::y)) };
                const float w { static_cast<float> (e->getDoubleAttribute (IDref::width)) };
                const float h { static_cast<float> (e->getDoubleAttribute (IDref::height)) };

                juce::Path path;
                path.addRectangle (x, y, w, h);

                paths.add (Template::parseSVGPath.replace ("%%path%%", Format::pathToString (path)));
            }
        }
    }

    return paths;
}
/*____________________________________________________________________________*/

const juce::String SVG::File::getStringToWrite (int width, int height, juce::StringRef svgString)
{
    return Template::declaration
        .replace ("%%svgWidth%%", juce::String (width))
        .replace ("%%svgHeight%%", juce::String (height))
        .replace ("%%svgString%%", svgString);
}

/*____________________________________________________________________________*/

const int SVG::getSVGWidth (juce::XmlElement* svg)
{
    return svg->getIntAttribute (IDref::width);
}

const int SVG::getSVGHeight (juce::XmlElement* svg)
{
    return svg->getIntAttribute (IDref::height);
}

const auto SVG::getSVGSize (juce::XmlElement* svg)
{
    return std::make_pair (getSVGWidth (svg), getSVGHeight (svg));
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
