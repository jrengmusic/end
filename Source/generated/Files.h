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

    extern const juce::String splitVerticalNormal;      ///< Split-vertical corner-menu icon.
    extern const juce::String splitHorizontalNormal;    ///< Split-horizontal corner-menu icon.
    extern const juce::String joinCellsVerticalNormal;  ///< Join-cells-vertical corner-menu icon.
    extern const juce::String joinCellsHorizontalNormal;///< Join-cells-horizontal corner-menu icon.

/**______________________________END OF NAMESPACE______________________________*/
}// namespace files
