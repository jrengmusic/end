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

#define IDENTIFIER_FILES(X) \
    X(file,                 "file") \
    X(path,                 "path") \
    X(extension,            "extension") \
    X(recentPath,           "recentPath") \
    X(desktop,              "desktop") \
    X(applicationSupport,   "Application Support") \
    X(userManuals,          "User Manuals") \
    X(userPresets,          "User Presets") \
    X(defaultPresets,       "Default Presets") \
    X(presets,              "Presets") \
    X(preset,               "preset") \
    X(downloads,            "Downloads") \
    X(licenses,             "Licenses") \
    X(all,                  "All") \
    X(source,               "Source") \
    X(pluginWrapper,        "pluginWrapper") \
    X(css,                  "StyleSheet.xml") \
    X(pluginMetadata,       "Parameters.xml") \
    X(editorLayout,      "EditorLayout.svg") \
    X(tame,                 ".tame")

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
