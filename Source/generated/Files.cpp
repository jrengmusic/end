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

#include <JuceHeader.h>
#include "Files.h"

namespace files
{
/*_____________________________________________________________________________*/

/**
 * @brief Product asset file names — pane split/join corner-menu icons.
 *
 * Each constant is the literal file name of an embedded SVG resource,
 * resolved against the binary-data / asset search path at load time.
 */

    const juce::String splitVerticalNormal       { juce::String::fromUTF8 ("split_vertical_normal.svg") };
    const juce::String splitHorizontalNormal     { juce::String::fromUTF8 ("split_horizontal_normal.svg") };
    const juce::String joinCellsVerticalNormal   { juce::String::fromUTF8 ("join_cells_vertical_normal.svg") };
    const juce::String joinCellsHorizontalNormal { juce::String::fromUTF8 ("join_cells_horizontal_normal.svg") };

/**______________________________END OF NAMESPACE______________________________*/
}// namespace files
