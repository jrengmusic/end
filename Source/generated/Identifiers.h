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

/**
 * @file Identifiers.h
 * @brief Identifier and name-string constants for END's own vocabulary.
 */

#pragma once

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

extern const juce::Identifier alwaysOnTop;
extern const juce::Identifier alwaysVisible;
extern const juce::Identifier backgroundOpacity;
extern const juce::Identifier backgroundResolution;
extern const juce::Identifier button;
extern const juce::Identifier buttonOn;
extern const juce::Identifier enabled;
extern const juce::Identifier exclusiveZoom;
extern const juce::Identifier filter;
extern const juce::Identifier focusedBackground;
extern const juce::Identifier focusedOutline;
extern const juce::Identifier focusedSession;
extern const juce::Identifier frameRate;
extern const juce::Identifier graphNode;
extern const juce::Identifier graphics;
extern const juce::Identifier imouse;
extern const juce::Identifier mouse;
extern const juce::Identifier newPane;
extern const juce::Identifier orbit;
extern const juce::Identifier outline;
extern const juce::Identifier pane;
extern const juce::Identifier paneComponentPadding;
extern const juce::Identifier paneComponentResizeBarThickness;
extern const juce::Identifier paneEditor;
extern const juce::Identifier panePrompt;
extern const juce::Identifier panes;
extern const juce::Identifier paneStep;
extern const juce::Identifier plugin;
extern const juce::Identifier pluginState;
extern const juce::Identifier plugins;
extern const juce::Identifier postProcessing;
extern const juce::Identifier postProcessingOpacity;
extern const juce::Identifier postProcessingResolution;
extern const juce::Identifier profiler;
extern const juce::Identifier reset;
extern const juce::Identifier session;
extern const juce::Identifier sessionPlugin;
extern const juce::Identifier sessions;
extern const juce::Identifier shaderFormat;
extern const juce::Identifier splitLine;
extern const juce::Identifier successMessage;
extern const juce::Identifier tab;
extern const juce::Identifier tabBar;
extern const juce::Identifier tabDepth;
extern const juce::Identifier tabHighlight;
extern const juce::Identifier tabPaddingBottom;
extern const juce::Identifier tabPaddingLeft;
extern const juce::Identifier tabPaddingRight;
extern const juce::Identifier tabPaddingTop;
extern const juce::Identifier tabTextPadding;
extern const juce::Identifier textOff;
extern const juce::Identifier textOn;
extern const juce::Identifier theme;
extern const juce::Identifier themes;
extern const juce::Identifier titleBarButtons;
extern const juce::Identifier useGpu;
extern const juce::Identifier zoom;
extern const juce::Identifier zoomStep;

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Id
