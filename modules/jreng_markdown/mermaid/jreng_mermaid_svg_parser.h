#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

namespace jreng::Mermaid
{
/*____________________________________________________________________________*/

struct Graphic
{
    /**
     * Represents a single drawable element from an SVG.
     *
     * Each Diagram contains the type, geometry, styling, and rendering
     * information needed to draw one element of a mermaid diagram.
     */
    struct Diagram
    {
        enum Type
        {
            StrokePath,
            FillPath,
            DrawText,
            DrawMarker
        };
        Type type;
        juce::Path path;
        juce::GlyphArrangement glyphs;
        juce::Colour colour;
        float strokeWidth { 1.0f };
        juce::AffineTransform transform;
        int zOrder { 0 };
    };

    /** Type alias for a collection of Diagram elements */
    using Diagrams = std::vector<Diagram>;

    /**
     * CSS rule structure for styling extraction.
     */
    struct CSSRule
    {
        juce::String selector;
        juce::StringPairArray properties;
    };

    /**
     * Extract CSS rules from <style> element text content.
     */
    static juce::Array<CSSRule> extractCSSRules (juce::XmlElement* styleElement);

    /**
     * Parse inline style attribute into key-value map.
     */
    static juce::StringPairArray parseInlineStyle (const juce::String& styleAttr);

    /**
     * Convert CSS colour string to juce::Colour.
     */
    static juce::Colour resolveColour (const juce::String& colourStr);

    /**
     * Parse numeric CSS value (strip units).
     */
    static float resolveFloat (const juce::String& numericStr);

    /**
     * Find CSS rules matching element's class/id.
     */
    static juce::Array<CSSRule> matchCSSSelectors (
        const juce::String& classAttr,
        const juce::String& idAttr,
        const juce::Array<CSSRule>& allCSSRules);

    /**
     * Merge CSS properties with inline style.
     */
    static juce::StringPairArray mergeStyles (
        const juce::Array<CSSRule>& cssRules,
        const juce::StringPairArray& inlineStyle);

    /**
     * Extract <path> element to Diagram.
     */
    static Diagram extractPath (
        juce::XmlElement* pathElement,
        const juce::StringPairArray& resolvedStyles);

    /**
     * Extract <rect> element to Diagram.
     */
    static Diagram extractRect (
        juce::XmlElement* rectElement,
        const juce::StringPairArray& resolvedStyles);

    /**
     * Extract <circle> element to Diagram.
     */
    static Diagram extractCircle (
        juce::XmlElement* circleElement,
        const juce::StringPairArray& resolvedStyles);

    /**
     * Extract <ellipse> element to Diagram.
     */
    static Diagram extractEllipse (
        juce::XmlElement* ellipseElement,
        const juce::StringPairArray& resolvedStyles);

    /**
     * Extract <text> element to Diagram.
     */
    static Diagram extractText (
        juce::XmlElement* textElement,
        const juce::StringPairArray& resolvedStyles);

    /**
     * Parse SVG transform attribute into juce::AffineTransform.
     */
    static juce::AffineTransform parseTransform (const juce::String& transformAttr);

    /**
     * Process single element: extract styling and geometry.
     */
    static void processElement (
        juce::XmlElement* element,
        const juce::Array<CSSRule>& allCSSRules,
        Diagrams& outDiagrams);

    /**
     * Main API: Convert SVG string to Graphic::Diagrams.
     */
    static Diagrams extractFromSVG (const juce::String& svgString);
};

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng::Mermaid */
