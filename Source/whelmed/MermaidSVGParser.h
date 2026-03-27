#pragma once

#include <JuceHeader.h>

namespace Whelmed
{ /*____________________________________________________________________________*/

//==============================================================================
/**
    A single renderable shape primitive parsed from a mermaid SVG.

    Build a flat list of these, then iterate to render — fill first, then stroke,
    based on hasFill/hasStroke flags. Order preserves SVG painter's algorithm.
*/
struct SVGPrimitive
{
    juce::Path             path;
    juce::Colour           fillColour   { juce::Colours::transparentBlack };
    juce::Colour           strokeColour { juce::Colours::transparentBlack };
    float                  strokeWidth  { 1.0f };
    bool                   hasFill      { false };
    bool                   hasStroke    { false };
};

//==============================================================================
/**
    A single text primitive parsed from a mermaid SVG.

    Pre-built at parse time. Render with g.drawText / g.drawFittedText — zero
    allocation at render time.
*/
struct SVGTextPrimitive
{
    juce::String           text;
    juce::Rectangle<float> bounds;
    juce::Colour           colour       { juce::Colours::black };
    float                  fontSize     { 14.0f };
    juce::Justification    justification{ juce::Justification::centred };
};

//==============================================================================
/**
    Result of a parse operation.
*/
struct MermaidParseResult
{
    std::vector<SVGPrimitive>     primitives;
    std::vector<SVGTextPrimitive> texts;
    juce::Rectangle<float>        viewBox;
    bool                          ok { false };
};

//==============================================================================
/**
    Parses mermaid.js SVG string output into flat lists of SVGPrimitive and
    SVGTextPrimitive, ready for juce::Graphics rendering.

    Usage:
    @code
        auto result = MermaidSVGParser::parse (svgString);
        if (result.ok)
        {
            for (auto& p : result.primitives)
            {
                if (p.hasFill)
                {
                    g.setColour (p.fillColour);
                    g.fillPath (p.path);
                }
                if (p.hasStroke)
                {
                    g.setColour (p.strokeColour);
                    g.strokePath (p.path, juce::PathStrokeType (p.strokeWidth));
                }
            }
            for (auto& t : result.texts)
            {
                g.setColour (t.colour);
                g.setFont (t.fontSize);
                g.drawText (t.text, t.bounds, t.justification, true);
            }
        }
    @endcode
*/
class MermaidSVGParser
{
public:
    //==========================================================================
    static MermaidParseResult parse (const juce::String& svgString)
    {
        MermaidParseResult result;

        auto xml { juce::XmlDocument::parse (svgString) };

        if (xml != nullptr and xml->getTagName().toLowerCase() == "svg")
        {
            result.viewBox = parseViewBox (*xml);

            // 1. Parse <style> block into class->paint map
            StyleMap classStyles;
            parseStyleBlock (*xml, classStyles);

            // 2. Pre-index <marker> elements from <defs> by id
            MarkerMap markers;
            parseDefsMarkers (*xml, markers);

            // 3. Walk the element tree, accumulating primitives
            juce::AffineTransform identity;
            PaintState rootPaint;
            rootPaint.fill        = juce::Colours::black;
            rootPaint.stroke      = juce::Colours::transparentBlack;
            rootPaint.strokeWidth = 1.0f;

            walkElement (*xml, identity, rootPaint, classStyles, markers,
                         result.primitives, result.texts);

            result.ok = true;
        }

        return result;
    }

private:
    //==========================================================================
    // Internal paint state — inherited through <g> hierarchy
    struct PaintState
    {
        juce::Colour fill        { juce::Colours::black };
        juce::Colour stroke      { juce::Colours::transparentBlack };
        float        strokeWidth { 1.0f };
        bool         hasFill     { true };
        bool         hasStroke   { false };
        float        fontSize    { 14.0f };
    };

    using StyleMap  = std::unordered_map<std::string, PaintState>;
    using MarkerMap = std::unordered_map<std::string, const juce::XmlElement*>;

    //==========================================================================
    // --- Colour parsing ------------------------------------------------------

    static juce::Colour parseColour (const juce::String& raw, const juce::Colour& currentColour)
    {
        const auto s { raw.trim().toLowerCase() };

        if (s.isEmpty() or s == "none")
            return juce::Colours::transparentBlack;

        if (s == "currentcolor")
            return currentColour;

        if (s == "transparent")
            return juce::Colours::transparentBlack;

        // Named colours — handle the small set mermaid actually emits
        if (s == "black")  return juce::Colours::black;
        if (s == "white")  return juce::Colours::white;

        // hsl(...) — mermaid sequence diagram uses this
        if (s.startsWith ("hsl"))
            return parseHSL (s);

        // rgba(...)
        if (s.startsWith ("rgba"))
            return parseRGBA (s);

        // rgb(...)
        if (s.startsWith ("rgb"))
            return parseRGB (s);

        // #rrggbb / #rgb / #rrggbbaa
        if (s.startsWith ("#"))
            return parseHex (s);

        return juce::Colours::transparentBlack;
    }

    static juce::Colour parseHex (const juce::String& s)
    {
        auto hex { s.trimCharactersAtStart ("#") };

        if (hex.length() == 3)
        {
            const auto r { hex.substring (0, 1) };
            const auto g { hex.substring (1, 2) };
            const auto b { hex.substring (2, 3) };
            hex = r + r + g + g + b + b;
        }

        if (hex.length() == 6)
            hex = hex + "ff";

        return juce::Colour::fromString ("ff" + hex.substring (0, 6));
        // alpha is last two if 8 chars:
    }

    static juce::Colour parseRGB (const juce::String& s)
    {
        // rgb(r, g, b)
        const auto inner { s.fromFirstOccurrenceOf ("(", false, false)
                            .upToLastOccurrenceOf (")", false, false) };
        const auto tokens { juce::StringArray::fromTokens (inner, ",", "") };

        juce::Colour result { juce::Colours::transparentBlack };

        if (tokens.size() >= 3)
        {
            result = juce::Colour ((uint8_t) tokens[0].trim().getIntValue(),
                                   (uint8_t) tokens[1].trim().getIntValue(),
                                   (uint8_t) tokens[2].trim().getIntValue());
        }

        return result;
    }

    static juce::Colour parseRGBA (const juce::String& s)
    {
        const auto inner { s.fromFirstOccurrenceOf ("(", false, false)
                            .upToLastOccurrenceOf (")", false, false) };
        const auto tokens { juce::StringArray::fromTokens (inner, ",", "") };

        juce::Colour result { parseRGB (s) };

        if (tokens.size() >= 4)
        {
            const auto alpha { (uint8_t) juce::roundToInt (tokens[3].trim().getFloatValue() * 255.0f) };
            result = juce::Colour ((uint8_t) tokens[0].trim().getIntValue(),
                                   (uint8_t) tokens[1].trim().getIntValue(),
                                   (uint8_t) tokens[2].trim().getIntValue(),
                                   alpha);
        }

        return result;
    }

    static juce::Colour parseHSL (const juce::String& s)
    {
        // hsl(h, s%, l%) — mermaid emits decimal h
        const auto inner { s.fromFirstOccurrenceOf ("(", false, false)
                            .upToLastOccurrenceOf (")", false, false) };
        const auto tokens { juce::StringArray::fromTokens (inner, ",", "") };

        juce::Colour result { juce::Colours::transparentBlack };

        if (tokens.size() >= 3)
        {
            const float h   { tokens[0].trim().getFloatValue() / 360.0f };
            const float sat { tokens[1].trim().trimCharactersAtEnd ("%").getFloatValue() / 100.0f };
            const float lum { tokens[2].trim().trimCharactersAtEnd ("%").getFloatValue() / 100.0f };
            result = juce::Colour::fromHSL (h, sat, lum, 1.0f);
        }

        return result;
    }

    //==========================================================================
    // --- Stroke width normalisation ------------------------------------------

    static float parseStrokeWidth (const juce::String& raw)
    {
        const auto s { raw.trim().toLowerCase() };
        float result { s.getFloatValue() };  // bare float fallback

        if (s.endsWith ("px"))
            result = s.dropLastCharacters (2).getFloatValue();
        else if (s.endsWith ("pt"))
            result = s.dropLastCharacters (2).getFloatValue() * 1.333f;

        return result;
    }

    //==========================================================================
    // --- Transform parsing ---------------------------------------------------

    static juce::AffineTransform parseTransform (const juce::String& raw)
    {
        const auto s { raw.trim() };
        juce::AffineTransform result;

        // Iterate: multiple transforms can be chained, e.g. "translate(x,y) scale(s)"
        int pos { 0 };
        const int len { s.length() };

        auto extractArgs = [&](const juce::String& after) -> juce::StringArray
        {
            const auto inner { after.fromFirstOccurrenceOf ("(", false, false)
                                    .upToFirstOccurrenceOf (")", false, false) };
            return juce::StringArray::fromTokens (inner, ", ", "");
        };

        while (pos < len)
        {
            // Skip whitespace
            while (pos < len and s[pos] == ' ') ++pos;

            if (pos < len)
            {
                const auto remaining { s.substring (pos) };
                const auto lower     { remaining.toLowerCase() };

                if (lower.startsWith ("translate"))
                {
                    const auto args { extractArgs (remaining) };
                    const float tx { args.size() > 0 ? args[0].getFloatValue() : 0.0f };
                    const float ty { args.size() > 1 ? args[1].getFloatValue() : 0.0f };
                    result = result.followedBy (juce::AffineTransform::translation (tx, ty));
                }
                else if (lower.startsWith ("scale"))
                {
                    const auto args { extractArgs (remaining) };
                    const float sx { args.size() > 0 ? args[0].getFloatValue() : 1.0f };
                    const float sy { args.size() > 1 ? args[1].getFloatValue() : sx };
                    result = result.followedBy (juce::AffineTransform::scale (sx, sy));
                }
                else if (lower.startsWith ("rotate"))
                {
                    const auto args  { extractArgs (remaining) };
                    const float angle { args.size() > 0 ? args[0].getFloatValue() : 0.0f };
                    const float cx    { args.size() > 2 ? args[1].getFloatValue() : 0.0f };
                    const float cy    { args.size() > 2 ? args[2].getFloatValue() : 0.0f };
                    result = result.followedBy (
                        juce::AffineTransform::rotation (juce::degreesToRadians (angle), cx, cy));
                }
                else if (lower.startsWith ("matrix"))
                {
                    const auto args { extractArgs (remaining) };

                    if (args.size() >= 6)
                        result = result.followedBy (
                            juce::AffineTransform (args[0].getFloatValue(),
                                                   args[2].getFloatValue(),
                                                   args[4].getFloatValue(),
                                                   args[1].getFloatValue(),
                                                   args[3].getFloatValue(),
                                                   args[5].getFloatValue()));
                }

                // Advance past this transform call (find matching closing paren)
                int parenDepth { 0 };
                bool started { false };
                bool advanced { false };

                while (pos < len and not advanced)
                {
                    if (s[pos] == '(') { parenDepth++; started = true; }
                    if (s[pos] == ')') { parenDepth--; }
                    ++pos;
                    advanced = started and parenDepth == 0;
                }
            }
        }

        return result;
    }

    //==========================================================================
    // --- Style block parser --------------------------------------------------
    // Extracts .classname { fill: ...; stroke: ...; stroke-width: ... }
    // Only handles simple flat class rules — no descendant selectors

    static void parseStyleBlock (const juce::XmlElement& svgRoot, StyleMap& out)
    {
        // Find <style> child — can be at root or inside first <g>
        bool foundRoot { false };

        for (auto* child = svgRoot.getFirstChildElement(); child != nullptr and not foundRoot; child = child->getNextElement())
        {
            if (child->getTagName().toLowerCase() == "style")
            {
                parseCSS (child->getAllSubText(), out);
                foundRoot = true;
            }
        }

        // Also check one level deeper
        for (auto* child = svgRoot.getFirstChildElement(); child != nullptr; child = child->getNextElement())
        {
            bool foundDeep { false };

            for (auto* grandchild = child->getFirstChildElement(); grandchild != nullptr and not foundDeep; grandchild = grandchild->getNextElement())
            {
                if (grandchild->getTagName().toLowerCase() == "style")
                {
                    parseCSS (grandchild->getAllSubText(), out);
                    foundDeep = true;
                }
            }
        }
    }

    static void parseCSS (const juce::String& css, StyleMap& out)
    {
        // We only care about rules that set fill/stroke on classes.
        // Pattern: #id .classname { fill: X; stroke: Y; stroke-width: Z; }
        // We strip the #id prefix and key on classname only.

        int i { 0 };
        const int len { css.length() };
        bool hasMore { true };

        while (i < len and hasMore)
        {
            // Find a '{' — selector precedes it
            const int braceOpen { css.indexOfChar (i, '{') };

            if (braceOpen >= 0)
            {
                const auto selector { css.substring (i, braceOpen).trim() };
                const int braceClose { css.indexOfChar (braceOpen, '}') };

                if (braceClose >= 0)
                {
                    const auto body { css.substring (braceOpen + 1, braceClose) };

                    // Extract all class names from selector (tokens starting with '.')
                    // e.g. "#id .node rect, #id .node circle" -> extract "node" (ignore tag names)
                    const auto selectorTokens { juce::StringArray::fromTokens (selector, " ,\t\n", "") };

                    for (const auto& token : selectorTokens)
                    {
                        if (token.startsWith ("."))
                        {
                            const auto className { token.substring (1).toStdString() };
                            const auto paint     { parseCSSBody (body) };
                            // Merge: later rules with same class name win
                            auto existing { out.find (className) };

                            if (existing != out.end())
                                mergePaintState (existing->second, paint);
                            else
                                out[className] = paint;
                        }
                    }

                    i = braceClose + 1;
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
    }

    static PaintState parseCSSBody (const juce::String& body)
    {
        PaintState ps;
        ps.hasFill   = false;
        ps.hasStroke = false;

        const auto declarations { juce::StringArray::fromTokens (body, ";", "") };

        for (const auto& decl : declarations)
        {
            const int colonIdx { decl.indexOfChar (':') };

            if (colonIdx >= 0)
            {
                const auto prop { decl.substring (0, colonIdx).trim().toLowerCase() };
                auto val        { decl.substring (colonIdx + 1).trim() };
                // Strip !important
                val = val.replace ("!important", "").trim();

                if (prop == "fill")
                {
                    if (val != "none")
                    {
                        ps.fill    = parseColour (val, juce::Colours::black);
                        ps.hasFill = true;
                    }
                    else
                    {
                        ps.hasFill = false;
                    }
                }
                else if (prop == "stroke")
                {
                    if (val != "none")
                    {
                        ps.stroke    = parseColour (val, juce::Colours::black);
                        ps.hasStroke = true;
                    }
                    else
                    {
                        ps.hasStroke = false;
                    }
                }
                else if (prop == "stroke-width")
                {
                    ps.strokeWidth = parseStrokeWidth (val);
                }
                else if (prop == "font-size")
                {
                    ps.fontSize = parseStrokeWidth (val); // same unit handling
                }
            }
        }

        return ps;
    }

    static void mergePaintState (PaintState& base, const PaintState& override_)
    {
        if (override_.hasFill)   { base.fill    = override_.fill;   base.hasFill   = true; }
        if (override_.hasStroke) { base.stroke  = override_.stroke; base.hasStroke = true; }
        if (override_.strokeWidth > 0.0f) base.strokeWidth = override_.strokeWidth;
    }

    //==========================================================================
    // --- Marker / defs parsing -----------------------------------------------

    static void parseDefsMarkers (const juce::XmlElement& svgRoot, MarkerMap& out)
    {
        for (auto* child = svgRoot.getFirstChildElement(); child != nullptr; child = child->getNextElement())
        {
            if (child->getTagName().toLowerCase() == "defs")
                collectMarkers (*child, out);

            // markers can also appear directly under root <g>
            if (child->getTagName().toLowerCase() == "g")
                for (auto* gc = child->getFirstChildElement(); gc != nullptr; gc = gc->getNextElement())
                    if (gc->getTagName().toLowerCase() == "marker")
                        out[gc->getStringAttribute ("id").toStdString()] = gc;
        }
    }

    static void collectMarkers (const juce::XmlElement& defs, MarkerMap& out)
    {
        for (auto* child = defs.getFirstChildElement(); child != nullptr; child = child->getNextElement())
            if (child->getTagName().toLowerCase() == "marker")
                out[child->getStringAttribute ("id").toStdString()] = child;
    }

    //==========================================================================
    // --- Inline style attribute parser ---------------------------------------

    static void applyInlineStyle (const juce::String& styleAttr,
                                   PaintState& ps,
                                   const juce::Colour& currentColour)
    {
        const auto declarations { juce::StringArray::fromTokens (styleAttr, ";", "") };

        for (const auto& decl : declarations)
        {
            const int colonIdx { decl.indexOfChar (':') };

            if (colonIdx >= 0)
            {
                const auto prop { decl.substring (0, colonIdx).trim().toLowerCase() };
                const auto val  { decl.substring (colonIdx + 1).trim().replace ("!important", "").trim() };

                if (prop == "fill")
                {
                    if (val == "none") { ps.hasFill = false; }
                    else               { ps.fill = parseColour (val, currentColour); ps.hasFill = true; }
                }
                else if (prop == "stroke")
                {
                    if (val == "none") { ps.hasStroke = false; }
                    else               { ps.stroke = parseColour (val, currentColour); ps.hasStroke = true; }
                }
                else if (prop == "stroke-width")
                {
                    ps.strokeWidth = parseStrokeWidth (val);
                }
                else if (prop == "font-size")
                {
                    ps.fontSize = parseStrokeWidth (val);
                }
            }
        }
    }

    //==========================================================================
    // --- Paint resolution for an element ------------------------------------
    // Priority: inline attr > inline style attr > CSS class > inherited

    static PaintState resolveElementPaint (const juce::XmlElement& el,
                                            const PaintState& inherited,
                                            const StyleMap& classStyles)
    {
        PaintState ps { inherited };

        // 1. Apply CSS classes (lower priority than inline)
        const auto classAttr { el.getStringAttribute ("class") };

        if (classAttr.isNotEmpty())
        {
            const auto classes { juce::StringArray::fromTokens (classAttr, " ", "") };

            for (const auto& cls : classes)
            {
                const auto it { classStyles.find (cls.toStdString()) };

                if (it != classStyles.end())
                    mergePaintState (ps, it->second);
            }
        }

        // 2. Apply inline style= attribute
        const auto styleAttr { el.getStringAttribute ("style") };

        if (styleAttr.isNotEmpty())
            applyInlineStyle (styleAttr, ps, inherited.fill);

        // 3. Apply direct attributes (highest priority)
        if (el.hasAttribute ("fill"))
        {
            const auto val { el.getStringAttribute ("fill") };
            if (val == "none") { ps.hasFill = false; }
            else               { ps.fill = parseColour (val, inherited.fill); ps.hasFill = true; }
        }

        if (el.hasAttribute ("stroke"))
        {
            const auto val { el.getStringAttribute ("stroke") };
            if (val == "none") { ps.hasStroke = false; }
            else               { ps.stroke = parseColour (val, inherited.fill); ps.hasStroke = true; }
        }

        if (el.hasAttribute ("stroke-width"))
            ps.strokeWidth = parseStrokeWidth (el.getStringAttribute ("stroke-width"));

        if (el.hasAttribute ("font-size"))
            ps.fontSize = parseStrokeWidth (el.getStringAttribute ("font-size"));

        return ps;
    }

    //==========================================================================
    // --- Shape builders ------------------------------------------------------

    static juce::Path pathFromRect (const juce::XmlElement& el,
                                    const juce::AffineTransform& xf)
    {
        const float x  { (float) el.getDoubleAttribute ("x",      0.0) };
        const float y  { (float) el.getDoubleAttribute ("y",      0.0) };
        const float w  { (float) el.getDoubleAttribute ("width",  0.0) };
        const float h  { (float) el.getDoubleAttribute ("height", 0.0) };
        const float rx { (float) el.getDoubleAttribute ("rx",     0.0) };
        const float ry { (float) el.getDoubleAttribute ("ry",     rx) };  // ry defaults to rx if unset

        juce::Path p;

        if (rx > 0.0f or ry > 0.0f)
            p.addRoundedRectangle (x, y, w, h, rx, ry);
        else
            p.addRectangle (x, y, w, h);

        p.applyTransform (xf);
        return p;
    }

    static juce::Path pathFromCircle (const juce::XmlElement& el,
                                       const juce::AffineTransform& xf)
    {
        const float cx { (float) el.getDoubleAttribute ("cx", 0.0) };
        const float cy { (float) el.getDoubleAttribute ("cy", 0.0) };
        const float r  { (float) el.getDoubleAttribute ("r",  0.0) };

        juce::Path p;
        p.addEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        p.applyTransform (xf);
        return p;
    }

    static juce::Path pathFromEllipse (const juce::XmlElement& el,
                                        const juce::AffineTransform& xf)
    {
        const float cx { (float) el.getDoubleAttribute ("cx", 0.0) };
        const float cy { (float) el.getDoubleAttribute ("cy", 0.0) };
        const float rx { (float) el.getDoubleAttribute ("rx", 0.0) };
        const float ry { (float) el.getDoubleAttribute ("ry", 0.0) };

        juce::Path p;
        p.addEllipse (cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);
        p.applyTransform (xf);
        return p;
    }

    static juce::Path pathFromLine (const juce::XmlElement& el,
                                     const juce::AffineTransform& xf)
    {
        const float x1 { (float) el.getDoubleAttribute ("x1", 0.0) };
        const float y1 { (float) el.getDoubleAttribute ("y1", 0.0) };
        const float x2 { (float) el.getDoubleAttribute ("x2", 0.0) };
        const float y2 { (float) el.getDoubleAttribute ("y2", 0.0) };

        juce::Path p;
        p.startNewSubPath (x1, y1);
        p.lineTo (x2, y2);
        p.applyTransform (xf);
        return p;
    }

    static juce::Path pathFromPolygon (const juce::XmlElement& el,
                                        const juce::AffineTransform& xf)
    {
        const auto pointsStr { el.getStringAttribute ("points") };
        const auto tokens    { juce::StringArray::fromTokens (pointsStr, " ,\t\n", "") };

        juce::Path p;

        for (int i { 0 }; i + 1 < tokens.size(); i += 2)
        {
            const float x { tokens[i].getFloatValue() };
            const float y { tokens[i + 1].getFloatValue() };
            if (i == 0) p.startNewSubPath (x, y);
            else        p.lineTo (x, y);
        }

        p.closeSubPath();
        p.applyTransform (xf);
        return p;
    }

    static juce::Path pathFromPolyline (const juce::XmlElement& el,
                                         const juce::AffineTransform& xf)
    {
        const auto pointsStr { el.getStringAttribute ("points") };
        const auto tokens    { juce::StringArray::fromTokens (pointsStr, " ,\t\n", "") };

        juce::Path p;

        for (int i { 0 }; i + 1 < tokens.size(); i += 2)
        {
            const float x { tokens[i].getFloatValue() };
            const float y { tokens[i + 1].getFloatValue() };
            if (i == 0) p.startNewSubPath (x, y);
            else        p.lineTo (x, y);
        }

        p.applyTransform (xf);
        return p;
    }

    static juce::Path pathFromSVGPath (const juce::XmlElement& el,
                                        const juce::AffineTransform& xf)
    {
        const auto d { el.getStringAttribute ("d") };

        juce::Path p;

        if (d.isNotEmpty())
        {
            p = juce::Drawable::parseSVGPath (d);
            p.applyTransform (xf);
        }

        return p;
    }

    //==========================================================================
    // --- Marker stamping -----------------------------------------------------
    // Stamps a marker shape at a given endpoint with the correct orientation.
    // endpoint: world-space coordinate of the line/path endpoint
    // tangentAngle: angle (radians) of the path direction at that endpoint

    static juce::Path stampMarker (const juce::XmlElement& markerEl,
                                    juce::Point<float> endpoint,
                                    float tangentAngle)
    {
        juce::Path result;

        const float mw   { (float) markerEl.getDoubleAttribute ("markerWidth",  10.0) };
        const float mh   { (float) markerEl.getDoubleAttribute ("markerHeight", 10.0) };
        const float refX { (float) markerEl.getDoubleAttribute ("refX", 0.0) };
        const float refY { (float) markerEl.getDoubleAttribute ("refY", 0.0) };

        // Parse viewBox to get scale from marker space to markerWidth/Height
        float vbScaleX { 1.0f };
        float vbScaleY { 1.0f };
        const auto vbAttr { markerEl.getStringAttribute ("viewBox") };

        if (vbAttr.isNotEmpty())
        {
            const auto tokens { juce::StringArray::fromTokens (vbAttr, " ,", "") };

            if (tokens.size() >= 4)
            {
                const float vbW { tokens[2].getFloatValue() };
                const float vbH { tokens[3].getFloatValue() };
                if (vbW > 0.0f) vbScaleX = mw / vbW;
                if (vbH > 0.0f) vbScaleY = mh / vbH;
            }
        }

        // Build child shapes in marker local space
        const juce::AffineTransform localScale { juce::AffineTransform::scale (vbScaleX, vbScaleY) };

        for (auto* child = markerEl.getFirstChildElement(); child != nullptr; child = child->getNextElement())
        {
            const auto tag { child->getTagName().toLowerCase() };
            juce::Path childPath;

            if      (tag == "path")    childPath = pathFromSVGPath (*child, localScale);
            else if (tag == "rect")    childPath = pathFromRect    (*child, localScale);
            else if (tag == "circle")  childPath = pathFromCircle  (*child, localScale);
            else if (tag == "polygon") childPath = pathFromPolygon (*child, localScale);
            else if (tag == "line")    childPath = pathFromLine    (*child, localScale);

            if (not childPath.isEmpty())
                result.addPath (childPath);
        }

        // Orient and position:
        // 1. Translate so refPoint is at origin
        const auto orient { markerEl.getStringAttribute ("orient") };
        float angle { 0.0f };

        if (orient == "auto" or orient == "auto-start-reverse")
            angle = tangentAngle;
        else if (orient.isNotEmpty())
            angle = juce::degreesToRadians (orient.getFloatValue());

        const auto xf { juce::AffineTransform::translation (-(refX * vbScaleX), -(refY * vbScaleY))
                             .followedBy (juce::AffineTransform::rotation (angle, 0.0f, 0.0f))
                             .followedBy (juce::AffineTransform::translation (endpoint.x, endpoint.y)) };

        result.applyTransform (xf);
        return result;
    }

    //==========================================================================
    // --- Edge endpoint / tangent extraction ----------------------------------
    // Extracts the final endpoint and outgoing tangent angle from a juce::Path.
    // Works for paths produced by Path::fromSVGString — iterates segments.

    struct PathEndInfo
    {
        juce::Point<float> endpoint;
        float              tangentAngle { 0.0f };
        bool               valid        { false };
    };

    static PathEndInfo getPathEnd (const juce::Path& path)
    {
        juce::Path::Iterator it (path);
        PathEndInfo info;

        juce::Point<float> prev, cur;
        juce::Point<float> cp1, cp2;

        while (it.next())
        {
            switch (it.elementType)
            {
                case juce::Path::Iterator::startNewSubPath:
                    prev = cur = { it.x1, it.y1 };
                    break;

                case juce::Path::Iterator::lineTo:
                    prev = cur;
                    cur  = { it.x1, it.y1 };
                    info.tangentAngle = std::atan2 (cur.y - prev.y, cur.x - prev.x);
                    info.endpoint     = cur;
                    info.valid        = true;
                    break;

                case juce::Path::Iterator::cubicTo:
                    cp1  = { it.x1, it.y1 };
                    cp2  = { it.x2, it.y2 };
                    prev = cp2;
                    cur  = { it.x3, it.y3 };
                    // Tangent at end of cubic = direction from cp2 to endpoint
                    info.tangentAngle = std::atan2 (cur.y - prev.y, cur.x - prev.x);
                    info.endpoint     = cur;
                    info.valid        = true;
                    break;

                case juce::Path::Iterator::quadraticTo:
                    prev = { it.x1, it.y1 };
                    cur  = { it.x2, it.y2 };
                    info.tangentAngle = std::atan2 (cur.y - prev.y, cur.x - prev.x);
                    info.endpoint     = cur;
                    info.valid        = true;
                    break;

                case juce::Path::Iterator::closePath:
                    break;
            }
        }

        return info;
    }

    static PathEndInfo getPathStart (const juce::Path& path)
    {
        juce::Path::Iterator it (path);
        PathEndInfo info;

        if (it.next() and it.elementType == juce::Path::Iterator::startNewSubPath)
        {
            info.endpoint = { it.x1, it.y1 };
            info.valid    = true;

            // Tangent: look at first segment
            if (it.next())
            {
                juce::Point<float> second;

                switch (it.elementType)
                {
                    case juce::Path::Iterator::lineTo:
                        second = { it.x1, it.y1 }; break;
                    case juce::Path::Iterator::cubicTo:
                        second = { it.x1, it.y1 }; break;  // first control point
                    case juce::Path::Iterator::quadraticTo:
                        second = { it.x1, it.y1 }; break;
                    default:
                        second = info.endpoint; break;
                }

                // Start tangent points inward — reverse for marker-start orientation
                info.tangentAngle = std::atan2 (info.endpoint.y - second.y,
                                                info.endpoint.x - second.x);
            }
        }

        return info;
    }

    //==========================================================================
    // --- viewBox parsing -----------------------------------------------------

    static juce::Rectangle<float> parseViewBox (const juce::XmlElement& svg)
    {
        const auto vb { svg.getStringAttribute ("viewBox") };

        juce::Rectangle<float> result;

        if (vb.isNotEmpty())
        {
            const auto tokens { juce::StringArray::fromTokens (vb, " ,", "") };

            if (tokens.size() >= 4)
            {
                result = { tokens[0].getFloatValue(), tokens[1].getFloatValue(),
                           tokens[2].getFloatValue(), tokens[3].getFloatValue() };
            }
        }

        return result;
    }

    //==========================================================================
    // --- Text primitive builder ----------------------------------------------

    static void buildTextPrimitive (const juce::XmlElement& el,
                                     const juce::AffineTransform& xf,
                                     const PaintState& ps,
                                     const StyleMap& classStyles,
                                     std::vector<SVGTextPrimitive>& texts)
    {
        const auto paint { resolveElementPaint (el, ps, classStyles) };

        const float x  { (float) el.getDoubleAttribute ("x",  0.0) };
        const float y  { (float) el.getDoubleAttribute ("y",  0.0) };
        const float dy { (float) el.getDoubleAttribute ("dy", 0.0) };

        // Collect text from tspan children or direct text content
        bool hasTspans { false };
        float accumulatedDY { dy };

        for (auto* child = el.getFirstChildElement(); child != nullptr; child = child->getNextElement())
        {
            if (child->getTagName().toLowerCase() == "tspan")
            {
                hasTspans = true;
                const float tdy { (float) child->getDoubleAttribute ("dy", 0.0) };
                const float tx  { (float) child->getDoubleAttribute ("x", x) };
                accumulatedDY += tdy;

                const auto tContent { child->getAllSubText().trim() };

                if (tContent.isNotEmpty())
                {
                    SVGTextPrimitive tp;
                    tp.text     = tContent;
                    tp.colour   = paint.hasFill ? paint.fill : juce::Colours::black;
                    tp.fontSize = paint.fontSize;

                    // Text anchor -> Justification
                    auto anchor { el.getStringAttribute ("text-anchor") };

                    if (anchor.isEmpty())
                        anchor = child->getStringAttribute ("text-anchor");

                    if      (anchor == "middle") tp.justification = juce::Justification::centred;
                    else if (anchor == "end")    tp.justification = juce::Justification::right;
                    else                         tp.justification = juce::Justification::left;

                    // Position: centre the bounds on (tx, y + accumulatedDY)
                    const float estW { tContent.length() * paint.fontSize * kCharWidthEstimate };
                    const float posX { tx };
                    const float posY { y + accumulatedDY };

                    const juce::Point<float> topleft  { juce::Point<float> (posX - estW * 0.5f, posY - paint.fontSize).transformedBy (xf) };
                    const juce::Point<float> botright { juce::Point<float> (posX + estW * 0.5f, posY).transformedBy (xf) };
                    tp.bounds = juce::Rectangle<float> (topleft.x, topleft.y,
                                                        botright.x - topleft.x,
                                                        botright.y - topleft.y);

                    texts.push_back (std::move (tp));
                }
            }
        }

        if (not hasTspans)
        {
            const auto tContent { el.getAllSubText().trim() };

            if (tContent.isNotEmpty())
            {
                SVGTextPrimitive tp;
                tp.text     = tContent;
                tp.colour   = paint.hasFill ? paint.fill : juce::Colours::black;
                tp.fontSize = paint.fontSize;

                const auto anchor { el.getStringAttribute ("text-anchor") };

                if      (anchor == "middle") tp.justification = juce::Justification::centred;
                else if (anchor == "end")    tp.justification = juce::Justification::right;
                else                         tp.justification = juce::Justification::left;

                const float estW { tContent.length() * paint.fontSize * kCharWidthEstimate };
                const juce::Point<float> topleft  { juce::Point<float> (x - estW * 0.5f, y - paint.fontSize).transformedBy (xf) };
                const juce::Point<float> botright { juce::Point<float> (x + estW * 0.5f, y).transformedBy (xf) };
                tp.bounds = juce::Rectangle<float> (topleft.x, topleft.y,
                                                    botright.x - topleft.x,
                                                    botright.y - topleft.y);

                texts.push_back (std::move (tp));
            }
        }
    }

    //==========================================================================
    // --- Iterative tree walker -----------------------------------------------
    // Uses an explicit stack to avoid recursion.

    struct StackFrame
    {
        const juce::XmlElement* element;
        juce::AffineTransform   transform;
        PaintState              paint;
    };

    static void walkElement (const juce::XmlElement& root,
                              const juce::AffineTransform& rootTransform,
                              const PaintState& rootPaint,
                              const StyleMap& classStyles,
                              const MarkerMap& markers,
                              std::vector<SVGPrimitive>& primitives,
                              std::vector<SVGTextPrimitive>& texts)
    {
        std::vector<StackFrame> stack;
        stack.reserve (kStackReserveSize);

        // Seed with children of root svg element
        // Push in reverse order so first child is processed first
        auto pushChildren = [&](const juce::XmlElement& parent,
                                 const juce::AffineTransform& xf,
                                 const PaintState& pstate)
        {
            std::vector<const juce::XmlElement*> children;

            for (auto* child = parent.getFirstChildElement(); child != nullptr; child = child->getNextElement())
                children.push_back (child);

            for (int i { (int) children.size() - 1 }; i >= 0; --i)
                stack.push_back ({ children.at ((size_t) i), xf, pstate });
        };

        pushChildren (root, rootTransform, rootPaint);

        while (not stack.empty())
        {
            auto [el, xf, ps] = stack.back();
            stack.pop_back();

            const auto tag { el->getTagName().toLowerCase() };

            // Non-rendering elements: skip; groups/svg: push children; text: build primitive;
            // shape elements: build path + primitive; unknown: ignore
            const bool isNonRendering { tag == "defs" or tag == "style" or tag == "marker" or
                                        tag == "symbol" or tag == "foreignobject" or tag == "title" or
                                        tag == "desc" or tag == "clippath" or tag == "filter" };

            if (not isNonRendering)
            {
                // Accumulate transform for this element
                juce::AffineTransform elXF { xf };
                const auto transformAttr { el->getStringAttribute ("transform") };

                if (transformAttr.isNotEmpty())
                    elXF = xf.followedBy (parseTransform (transformAttr));

                if (tag == "g" or tag == "svg")
                {
                    // <g> — just inherit paint and push children
                    const auto groupPaint { resolveElementPaint (*el, ps, classStyles) };
                    pushChildren (*el, elXF, groupPaint);
                }
                else if (tag == "text")
                {
                    // <text> — build text primitive
                    buildTextPrimitive (*el, elXF, ps, classStyles, texts);
                }
                else
                {
                    // Shape elements — resolve paint and build path
                    const auto paint { resolveElementPaint (*el, ps, classStyles) };

                    juce::Path path;
                    if      (tag == "path")     path = pathFromSVGPath  (*el, elXF);
                    else if (tag == "rect")     path = pathFromRect     (*el, elXF);
                    else if (tag == "circle")   path = pathFromCircle   (*el, elXF);
                    else if (tag == "ellipse")  path = pathFromEllipse  (*el, elXF);
                    else if (tag == "line")     path = pathFromLine     (*el, elXF);
                    else if (tag == "polygon")  path = pathFromPolygon  (*el, elXF);
                    else if (tag == "polyline") path = pathFromPolyline (*el, elXF);

                    if (not path.isEmpty())
                    {
                        SVGPrimitive prim;
                        prim.path         = std::move (path);
                        prim.fillColour   = paint.fill;
                        prim.strokeColour = paint.stroke;
                        prim.strokeWidth  = paint.strokeWidth;
                        prim.hasFill      = paint.hasFill;
                        prim.hasStroke    = paint.hasStroke;
                        primitives.push_back (std::move (prim));

                        // --- Marker resolution ---
                        // marker-end
                        const auto markerEnd { el->getStringAttribute ("marker-end") };

                        if (markerEnd.isNotEmpty())
                        {
                            const auto markerId { extractMarkerRef (markerEnd) };
                            const auto it { markers.find (markerId.toStdString()) };

                            if (it != markers.end())
                            {
                                const auto endInfo { getPathEnd (primitives.back().path) };

                                if (endInfo.valid)
                                {
                                    const auto markerPaint { resolveElementPaint (*it->second, paint, classStyles) };
                                    const auto markerPath  { stampMarker (*it->second, endInfo.endpoint, endInfo.tangentAngle) };

                                    if (not markerPath.isEmpty())
                                    {
                                        SVGPrimitive mp;
                                        mp.path         = std::move (markerPath);
                                        mp.fillColour   = markerPaint.hasFill ? markerPaint.fill : paint.stroke;
                                        mp.strokeColour = markerPaint.stroke;
                                        mp.strokeWidth  = markerPaint.strokeWidth;
                                        mp.hasFill      = true;  // arrowheads are always filled
                                        mp.hasStroke    = markerPaint.hasStroke;
                                        primitives.push_back (std::move (mp));
                                    }
                                }
                            }
                        }

                        // marker-start
                        const auto markerStart { el->getStringAttribute ("marker-start") };

                        if (markerStart.isNotEmpty())
                        {
                            const auto markerId { extractMarkerRef (markerStart) };
                            const auto it { markers.find (markerId.toStdString()) };

                            if (it != markers.end())
                            {
                                const auto startInfo { getPathStart (primitives.back().path) };

                                if (startInfo.valid)
                                {
                                    const auto markerPath { stampMarker (*it->second, startInfo.endpoint, startInfo.tangentAngle) };

                                    if (not markerPath.isEmpty())
                                    {
                                        SVGPrimitive mp;
                                        mp.path       = std::move (markerPath);
                                        mp.fillColour = paint.stroke;
                                        mp.hasFill    = true;
                                        mp.hasStroke  = false;
                                        primitives.push_back (std::move (mp));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    //==========================================================================
    // --- Utility -------------------------------------------------------------

    // Extracts "markerId" from "url(#markerId)"
    static juce::String extractMarkerRef (const juce::String& urlRef)
    {
        return urlRef.fromFirstOccurrenceOf ("#", false, false)
                     .upToFirstOccurrenceOf (")", false, false)
                     .trim();
    }

    //==========================================================================
    // --- Constants -----------------------------------------------------------

    // Estimated character width ratio relative to font size (used for text bounds)
    static constexpr float kCharWidthEstimate { 0.6f };

    // Initial stack reservation for iterative tree walk
    static constexpr int kStackReserveSize { 64 };

    JUCE_DECLARE_NON_COPYABLE (MermaidSVGParser)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
