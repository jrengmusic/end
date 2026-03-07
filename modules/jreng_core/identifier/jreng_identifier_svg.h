#pragma once

namespace jreng
{
/*____________________________________________________________________________*/
// ============================================================================
// MACROS are evil. Yet, in the pursuit of one source of truth for convenience
// and consistency in the land of C++, this necessary horror spares us from
// repeating ourselves. May God forgive our sins.
//
// JRENG!
// ============================================================================

#define IDENTIFIER_SVG(X) \
    X(serifID,     "serif:id") \
    X(id,          "id") \
    X(svg,         "svg") \
    X(viewBox,     "viewBox") \
    X(g,           "g") \
    X(d,           "d") \
    X(r,           "r") \
    X(rect,        "rect") \
    X(ellipse,     "ellipse") \
    X(circle,      "circle") \
    X(cx,          "cx") \
    X(cy,          "cy") \
    X(rx,          "rx") \
    X(ry,          "ry") \
    X(x,           "x") \
    X(y,           "y") \
    X(width,       "width") \
    X(height,      "height") \
    X(line,        "line") \
    X(text,        "text") \
    X(font,        "font") \
    X(font_size,   "font-size") \
    X(font_family, "font-family") \
    X(fill,        "fill") \
    X(decoration,  "decoration") \
    X(style,       "style")

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace jreng
