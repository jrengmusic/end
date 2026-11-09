/*******************************************************************************
                        Codegen Annotated Source of Truth
————————————————————————————————————————————————————————————————————————————————

            ░░████████████░░████████████░░████████████░░████████████
            ░░████  ░░████░░████  ░░████░░████  ░░████    ░░████
            ░░████        ░░████  ░░████░░████            ░░████
            ░░████        ░░████████████░░████████████    ░░████
            ░░████        ░░████  ░░████        ░░████    ░░████
            ░░████  ░░████░░████  ░░████░░████  ░░████    ░░████
            ░░████████████░░████  ░░████░░████████████    ░░████

————————————————————————————————————————————————————————————————————————————————
                         FOR YOUR EYES ONLY, DO NOT EDIT
********************************************************************************/

#pragma once

namespace files
{
/*_____________________________________________________________________________*/

/**
 * @brief Product asset file names — pane split/join corner-menu icons, the plugin offload stamp, and the plugin file extensions.
 *
 * Each constant is the literal name of an embedded SVG resource, of the offload
 * stamp in the plugins folder, of the settings file, or of a file extension.
 */

    extern const juce::String splitVerticalNormal;      ///< Split-vertical corner-menu icon.
    extern const juce::String splitHorizontalNormal;    ///< Split-horizontal corner-menu icon.
    extern const juce::String joinCellsVerticalNormal;  ///< Join-cells-vertical corner-menu icon.
    extern const juce::String joinCellsHorizontalNormal;///< Join-cells-horizontal corner-menu icon.
    extern const juce::String pluginsStamp;             ///< Hash stamp of the plugin archives last offloaded.
    extern const juce::String archiveExtension;         ///< Extension of the plugin archives in the binary data.
    extern const juce::String audioUnitExtension;       ///< Extension of an Audio Unit bundle.
    extern const juce::String settingsFileName;         ///< Window state file of END in the config directory.
    extern const juce::String themeStyleSheet;          ///< Shipped theme stylesheet resource name.

/**______________________________END OF NAMESPACE______________________________*/
}// namespace files
