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
    const juce::Identifier background               { juce::String::fromUTF8 ("background") };
    const juce::Identifier backgroundOpacity        { juce::String::fromUTF8 ("backgroundOpacity") };
    const juce::Identifier backgroundResolution     { juce::String::fromUTF8 ("backgroundResolution") };
    const juce::Identifier blurRadius               { juce::String::fromUTF8 ("blurRadius") };
    const juce::Identifier bottom                   { juce::String::fromUTF8 ("bottom") };
    const juce::Identifier button                   { juce::String::fromUTF8 ("button") };
    const juce::Identifier buttonOn                 { juce::String::fromUTF8 ("buttonOn") };
    const juce::Identifier caret                    { juce::String::fromUTF8 ("caret") };
    const juce::Identifier closePane                { juce::String::fromUTF8 ("closePane") };
    const juce::Identifier closeTab                 { juce::String::fromUTF8 ("closeTab") };
    const juce::Identifier depth                    { juce::String::fromUTF8 ("depth") };
    const juce::Identifier editorBackground         { juce::String::fromUTF8 ("editorBackground") };
    const juce::Identifier editorOutline            { juce::String::fromUTF8 ("editorOutline") };
    const juce::Identifier enabled                  { juce::String::fromUTF8 ("enabled") };
    const juce::Identifier expandPaneHeight         { juce::String::fromUTF8 ("expandPaneHeight") };
    const juce::Identifier expandPaneWidth          { juce::String::fromUTF8 ("expandPaneWidth") };
    const juce::Identifier filter                   { juce::String::fromUTF8 ("filter") };
    const juce::Identifier focusedOutline           { juce::String::fromUTF8 ("focusedOutline") };
    const juce::Identifier focusedSession           { juce::String::fromUTF8 ("focusedSession") };
    const juce::Identifier fontContrast             { juce::String::fromUTF8 ("fontContrast") };
    const juce::Identifier fontGamma                { juce::String::fromUTF8 ("fontGamma") };
    const juce::Identifier fontRasterizer           { juce::String::fromUTF8 ("fontRasterizer") };
    const juce::Identifier frameRate                { juce::String::fromUTF8 ("frameRate") };
    const juce::Identifier graphics                 { juce::String::fromUTF8 ("graphics") };
    const juce::Identifier highlight                { juce::String::fromUTF8 ("highlight") };
    const juce::Identifier imouse                   { juce::String::fromUTF8 ("imouse") };
    const juce::Identifier joinDown                 { juce::String::fromUTF8 ("joinDown") };
    const juce::Identifier joinLeft                 { juce::String::fromUTF8 ("joinLeft") };
    const juce::Identifier joinRight                { juce::String::fromUTF8 ("joinRight") };
    const juce::Identifier joinUp                   { juce::String::fromUTF8 ("joinUp") };
    const juce::Identifier kerningFactor            { juce::String::fromUTF8 ("kerningFactor") };
    const juce::Identifier keys                     { juce::String::fromUTF8 ("keys") };
    const juce::Identifier lineHeight               { juce::String::fromUTF8 ("lineHeight") };
    const juce::Identifier menu                     { juce::String::fromUTF8 ("menu") };
    const juce::Identifier mouse                    { juce::String::fromUTF8 ("mouse") };
    const juce::Identifier newPane                  { juce::String::fromUTF8 ("newPane") };
    const juce::Identifier newPlugin                { juce::String::fromUTF8 ("newPlugin") };
    const juce::Identifier newSession               { juce::String::fromUTF8 ("newSession") };
    const juce::Identifier newTab                   { juce::String::fromUTF8 ("newTab") };
    const juce::Identifier nextTab                  { juce::String::fromUTF8 ("nextTab") };
    const juce::Identifier orbit                    { juce::String::fromUTF8 ("orbit") };
    const juce::Identifier outline                  { juce::String::fromUTF8 ("outline") };
    const juce::Identifier overlay                  { juce::String::fromUTF8 ("overlay") };
    const juce::Identifier pane                     { juce::String::fromUTF8 ("pane") };
    const juce::Identifier paneDown                 { juce::String::fromUTF8 ("paneDown") };
    const juce::Identifier paneLeft                 { juce::String::fromUTF8 ("paneLeft") };
    const juce::Identifier paneRight                { juce::String::fromUTF8 ("paneRight") };
    const juce::Identifier paneStep                 { juce::String::fromUTF8 ("paneStep") };
    const juce::Identifier paneUp                   { juce::String::fromUTF8 ("paneUp") };
    const juce::Identifier pluginId                 { juce::String::fromUTF8 ("pluginId") };
    const juce::Identifier postProcessing           { juce::String::fromUTF8 ("postProcessing") };
    const juce::Identifier postProcessingOpacity    { juce::String::fromUTF8 ("postProcessingOpacity") };
    const juce::Identifier postProcessingResolution { juce::String::fromUTF8 ("postProcessingResolution") };
    const juce::Identifier prefix                   { juce::String::fromUTF8 ("prefix") };
    const juce::Identifier prefixTimeout            { juce::String::fromUTF8 ("prefixTimeout") };
    const juce::Identifier prevTab                  { juce::String::fromUTF8 ("prevTab") };
    const juce::Identifier quit                     { juce::String::fromUTF8 ("quit") };
    const juce::Identifier reducePaneHeight         { juce::String::fromUTF8 ("reducePaneHeight") };
    const juce::Identifier reducePaneWidth          { juce::String::fromUTF8 ("reducePaneWidth") };
    const juce::Identifier reload                   { juce::String::fromUTF8 ("reload") };
    const juce::Identifier reset                    { juce::String::fromUTF8 ("reset") };
    const juce::Identifier resizeBar                { juce::String::fromUTF8 ("resizeBar") };
    const juce::Identifier resizeBarHighlight       { juce::String::fromUTF8 ("resizeBarHighlight") };
    const juce::Identifier resizeBarThickness       { juce::String::fromUTF8 ("resizeBarThickness") };
    const juce::Identifier resizerBar               { juce::String::fromUTF8 ("resizerBar") };
    const juce::Identifier scrollbar                { juce::String::fromUTF8 ("scrollbar") };
    const juce::Identifier selectionCursor          { juce::String::fromUTF8 ("selectionCursor") };
    const juce::Identifier session                  { juce::String::fromUTF8 ("session") };
    const juce::Identifier sessions                 { juce::String::fromUTF8 ("sessions") };
    const juce::Identifier shaderFormat             { juce::String::fromUTF8 ("shaderFormat") };
    const juce::Identifier splitHorizontal          { juce::String::fromUTF8 ("splitHorizontal") };
    const juce::Identifier splitVertical            { juce::String::fromUTF8 ("splitVertical") };
    const juce::Identifier successMessage           { juce::String::fromUTF8 ("successMessage") };
    const juce::Identifier swapDown                 { juce::String::fromUTF8 ("swapDown") };
    const juce::Identifier swapLeft                 { juce::String::fromUTF8 ("swapLeft") };
    const juce::Identifier swapRight                { juce::String::fromUTF8 ("swapRight") };
    const juce::Identifier swapUp                   { juce::String::fromUTF8 ("swapUp") };
    const juce::Identifier tab                      { juce::String::fromUTF8 ("tab") };
    const juce::Identifier tabBar                   { juce::String::fromUTF8 ("tabBar") };
    const juce::Identifier tabHighlight             { juce::String::fromUTF8 ("tabHighlight") };
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
    const juce::Identifier zoomIn                   { juce::String::fromUTF8 ("zoomIn") };
    const juce::Identifier zoomOut                  { juce::String::fromUTF8 ("zoomOut") };
    const juce::Identifier zoomReset                { juce::String::fromUTF8 ("zoomReset") };
    const juce::Identifier zoomStep                 { juce::String::fromUTF8 ("zoomStep") };

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Id
