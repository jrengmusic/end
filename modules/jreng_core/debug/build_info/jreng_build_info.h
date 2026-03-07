/*  
  ==============================================================================

    jreng_build_info.h
    Created: 29 Oct 2025
    Author:  JRENG

    Unified build information widget that displays:
    - Build system used (Ninja/Xcode/MSVC)
    - Build number with timestamp
    - Plugin name and version

  ==============================================================================
*/

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

#if JUCE_DEBUG && JRENG_USING_BUILD_INFO

namespace jreng
{
namespace debug
{
/*____________________________________________________________________________*/

//==============================================================================
/**
    Displays build information at the top of the Editor component.

    Shows:
    - Plugin name, version, and format (VST3/AU/etc.)
    - Build timestamp
    - Build system used (Ninja/Xcode/MSVC)

    Only visible in JUCE_DEBUG builds when JRENG_USING_BUILD_INFO=1.

    Usage in PluginEditor.h:
        #if JRENG_USING_BUILD_INFO
            jreng::debug::BuildInfo buildInfo { this };
        #endif
*/
class BuildInfo : public juce::TextEditor
{
public:
    BuildInfo (juce::Component* editorToAdd)
    {
        if (editorToAdd == nullptr)
            return;

        setInterceptsMouseClicks (false, false);

        // Build the info text
        juce::StringArray lines;

        // Line 1: Plugin name + version + format
#if JUCE_MODULE_AVAILABLE_juce_audio_processors
        lines.add (projectName + " v" + versionString
            + " " + juce::AudioProcessor::getWrapperTypeDescription (juce::PluginHostType::getPluginLoadedAs()));
#else
        lines.add (projectName + " v" + versionString);
#endif

        // Line 2: Build timestamp + system
        juce::String buildSystem { "Unknown" };

        #if defined (__NINJA_BUILD__)
            buildSystem = "Ninja";
        #elif defined (__XCODE_BUILD__)
            buildSystem = "Xcode";
        #elif defined (__MSVC_BUILD__)
            buildSystem = "MSVC";
        #endif

        lines.add ("Build " + getFormattedTimestamp() + " (" + buildSystem + ")");
        
        // Configure
        setReadOnly(true);
        setMultiLine(true);
        setText (lines.joinIntoString ("\n"), juce::dontSendNotification);
        setJustification (juce::Justification::centred);
        setAlwaysOnTop (true);

        // Add to editor - bounds will be set by Widget's componentMovedOrResized
        editorToAdd->addAndMakeVisible (*this);
    }

private:

    /**
     * @brief Formats the current date and time into a timestamp.
     *
     * @return A juce::String containing the formatted timestamp.
     */
    static inline const juce::String getFormattedTimestamp()
    {
        juce::StringArray tokens;
        tokens.addTokens (__DATE__, false);
        tokens.move (0, 1);
        return tokens.joinIntoString (" ").toUpperCase() + " " + juce::String (__TIME__).upToLastOccurrenceOf (":", false, true);
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BuildInfo)
};

/**_____________________________END OF CLASS__________________________________*/
}// namespace debug
/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng

#endif// JUCE_DEBUG && JRENG_USING_BUILD_INFO
