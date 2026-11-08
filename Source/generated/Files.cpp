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
 * @brief Product asset file names — pane split/join corner-menu icons, the plugin bundle each new pane loads, and the plugin file extensions.
 *
 * Each constant is the literal name of an embedded SVG resource, of the plugin
 * bundle or its offload stamp in the plugins folder, or of a file extension.
 */

    const juce::String splitVerticalNormal       { juce::String::fromUTF8 ("split_vertical_normal.svg") };
    const juce::String splitHorizontalNormal     { juce::String::fromUTF8 ("split_horizontal_normal.svg") };
    const juce::String joinCellsVerticalNormal   { juce::String::fromUTF8 ("join_cells_vertical_normal.svg") };
    const juce::String joinCellsHorizontalNormal { juce::String::fromUTF8 ("join_cells_horizontal_normal.svg") };
    const juce::String pluginsStamp              { juce::String::fromUTF8 (".plugins-stamp") };
    const juce::String archiveExtension          { juce::String::fromUTF8 ("zip") };
    const juce::String audioUnitExtension        { juce::String::fromUTF8 ("component") };
    const juce::String settingsFileName          { juce::String::fromUTF8 ("END.settings") };

/**______________________________END OF NAMESPACE______________________________*/
}// namespace files

//==============================================================================

namespace files
{
/*_____________________________________________________________________________*/

    #if JUCE_MAC
    const juce::String evePlugin { juce::String::fromUTF8 ("EVE.vst") };
    #else
    const juce::String evePlugin { juce::String::fromUTF8 ("EVE.dll") };
    #endif

/**______________________________END OF NAMESPACE______________________________*/
}// namespace files
