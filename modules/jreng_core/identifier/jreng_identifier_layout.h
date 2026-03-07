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

#define IDENTIFIER_LAYOUT(X) \
    X(windows,          "windows") \
    X(window,           "window") \
    X(openLastActive,   "openLastActive") \
    X(saveWindowsState, "saveWindowsState") \
    X(bounds,           "bounds") \
    X(orientation,      "orientation") \
    X(portrait,         "portrait") \
    X(landscape,        "landscape") \
    X(layout,           "layout") \
    X(panel,            "panel") \
    X(panelTop,         "panel_top") \
    X(panelBottom,      "panel_bottom") \
    X(panelHeight,      "panelHeight") \
    X(presetMenu,       "presetMenu") \
    X(recent,           "recent") \
    X(aboutBox,         "aboutBox") \
    X(UI_scale,         "UI_scale") \
    X(grid,             "grid") \
    X(metrics,          "metrics") \
    X(metric,           "metric") \
    X(row,              "row") \
    X(separator,        "separator") \
    X(column,           "column") \
    X(inset,            "inset")


/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
