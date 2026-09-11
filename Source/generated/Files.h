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
 * @brief Product asset file names — pane split/join corner-menu icons.
 *
 * Each constant is the literal file name of an embedded SVG resource,
 * resolved against the binary-data / asset search path at load time.
 */

    inline const juce::String splitVerticalNormal       { juce::String::fromUTF8 ("split_vertical_normal.svg")        };///< Split-vertical corner-menu icon.
    inline const juce::String splitHorizontalNormal     { juce::String::fromUTF8 ("split_horizontal_normal.svg")      };///< Split-horizontal corner-menu icon.
    inline const juce::String joinCellsVerticalNormal   { juce::String::fromUTF8 ("join_cells_vertical_normal.svg")   };///< Join-cells-vertical corner-menu icon.
    inline const juce::String joinCellsHorizontalNormal { juce::String::fromUTF8 ("join_cells_horizontal_normal.svg") };///< Join-cells-horizontal corner-menu icon.

/**______________________________END OF NAMESPACE______________________________*/
}// namespace files
