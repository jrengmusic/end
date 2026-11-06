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
#include "Identifiers.h"

namespace Id
{
/*_____________________________________________________________________________*/

/**
 * @brief Identifier vocabulary END consumes beyond jam's own vocabulary.
 *
 * Each entry is a provenance/stamp key or vocabulary word this project's config
 * tree and views read; the value is the on-tree string this project has always
 * used, byte-identical.
 */

    const juce::Identifier alwaysOnTop              { juce::String::fromUTF8 ("alwaysOnTop") };
    const juce::Identifier alwaysVisible            { juce::String::fromUTF8 ("alwaysVisible") };
    const juce::Identifier backgroundOpacity        { juce::String::fromUTF8 ("backgroundOpacity") };
    const juce::Identifier backgroundResolution     { juce::String::fromUTF8 ("backgroundResolution") };
    const juce::Identifier blurRadius               { juce::String::fromUTF8 ("blurRadius") };
    const juce::Identifier button                   { juce::String::fromUTF8 ("button") };
    const juce::Identifier buttonOn                 { juce::String::fromUTF8 ("buttonOn") };
    const juce::Identifier depth                    { juce::String::fromUTF8 ("depth") };
    const juce::Identifier enabled                  { juce::String::fromUTF8 ("enabled") };
    const juce::Identifier exclusiveZoom            { juce::String::fromUTF8 ("exclusiveZoom") };
    const juce::Identifier filter                   { juce::String::fromUTF8 ("filter") };
    const juce::Identifier focusedBackground        { juce::String::fromUTF8 ("focusedBackground") };
    const juce::Identifier focusedOutline           { juce::String::fromUTF8 ("focusedOutline") };
    const juce::Identifier focusedSession           { juce::String::fromUTF8 ("focusedSession") };
    const juce::Identifier fontContrast             { juce::String::fromUTF8 ("fontContrast") };
    const juce::Identifier fontGamma                { juce::String::fromUTF8 ("fontGamma") };
    const juce::Identifier fontRasterizer           { juce::String::fromUTF8 ("fontRasterizer") };
    const juce::Identifier frameRate                { juce::String::fromUTF8 ("frameRate") };
    const juce::Identifier graphNode                { juce::String::fromUTF8 ("graphNode") };
    const juce::Identifier graphics                 { juce::String::fromUTF8 ("graphics") };
    const juce::Identifier highlight                { juce::String::fromUTF8 ("highlight") };
    const juce::Identifier imouse                   { juce::String::fromUTF8 ("imouse") };
    const juce::Identifier kerningFactor            { juce::String::fromUTF8 ("kerningFactor") };
    const juce::Identifier menu                     { juce::String::fromUTF8 ("menu") };
    const juce::Identifier mouse                    { juce::String::fromUTF8 ("mouse") };
    const juce::Identifier newPane                  { juce::String::fromUTF8 ("newPane") };
    const juce::Identifier orbit                    { juce::String::fromUTF8 ("orbit") };
    const juce::Identifier outline                  { juce::String::fromUTF8 ("outline") };
    const juce::Identifier overlay                  { juce::String::fromUTF8 ("overlay") };
    const juce::Identifier pane                     { juce::String::fromUTF8 ("pane") };
    const juce::Identifier paneEditor               { juce::String::fromUTF8 ("paneEditor") };
    const juce::Identifier panePrompt               { juce::String::fromUTF8 ("panePrompt") };
    const juce::Identifier panes                    { juce::String::fromUTF8 ("panes") };
    const juce::Identifier paneStep                 { juce::String::fromUTF8 ("paneStep") };
    const juce::Identifier plugin                   { juce::String::fromUTF8 ("plugin") };
    const juce::Identifier pluginState              { juce::String::fromUTF8 ("pluginState") };
    const juce::Identifier plugins                  { juce::String::fromUTF8 ("plugins") };
    const juce::Identifier postProcessing           { juce::String::fromUTF8 ("postProcessing") };
    const juce::Identifier postProcessingOpacity    { juce::String::fromUTF8 ("postProcessingOpacity") };
    const juce::Identifier postProcessingResolution { juce::String::fromUTF8 ("postProcessingResolution") };
    const juce::Identifier profiler                 { juce::String::fromUTF8 ("profiler") };
    const juce::Identifier reset                    { juce::String::fromUTF8 ("reset") };
    const juce::Identifier resizeBarThickness       { juce::String::fromUTF8 ("resizeBarThickness") };
    const juce::Identifier resizerBar               { juce::String::fromUTF8 ("resizerBar") };
    const juce::Identifier scrollbar                { juce::String::fromUTF8 ("scrollbar") };
    const juce::Identifier session                  { juce::String::fromUTF8 ("session") };
    const juce::Identifier sessionPlugin            { juce::String::fromUTF8 ("sessionPlugin") };
    const juce::Identifier sessions                 { juce::String::fromUTF8 ("sessions") };
    const juce::Identifier shaderFormat             { juce::String::fromUTF8 ("shaderFormat") };
    const juce::Identifier splitLine                { juce::String::fromUTF8 ("splitLine") };
    const juce::Identifier successMessage           { juce::String::fromUTF8 ("successMessage") };
    const juce::Identifier tab                      { juce::String::fromUTF8 ("tab") };
    const juce::Identifier tabBar                   { juce::String::fromUTF8 ("tabBar") };
    const juce::Identifier tabHighlight             { juce::String::fromUTF8 ("tabHighlight") };
    const juce::Identifier textFontFamily           { juce::String::fromUTF8 ("fontFamily") };
    const juce::Identifier textFontSize             { juce::String::fromUTF8 ("fontSize") };
    const juce::Identifier textOff                  { juce::String::fromUTF8 ("textOff") };
    const juce::Identifier textOn                   { juce::String::fromUTF8 ("textOn") };
    const juce::Identifier textPadding              { juce::String::fromUTF8 ("textPadding") };
    const juce::Identifier theme                    { juce::String::fromUTF8 ("theme") };
    const juce::Identifier themes                   { juce::String::fromUTF8 ("themes") };
    const juce::Identifier thumb                    { juce::String::fromUTF8 ("thumb") };
    const juce::Identifier titleBarButtons          { juce::String::fromUTF8 ("titleBarButtons") };
    const juce::Identifier track                    { juce::String::fromUTF8 ("track") };
    const juce::Identifier useGpu                   { juce::String::fromUTF8 ("gpu") };
    const juce::Identifier visible                  { juce::String::fromUTF8 ("visible") };
    const juce::Identifier zoom                     { juce::String::fromUTF8 ("zoom") };
    const juce::Identifier zoomStep                 { juce::String::fromUTF8 ("zoomStep") };

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Id
