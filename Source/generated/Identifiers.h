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
extern const juce::Identifier background;
extern const juce::Identifier backgroundOpacity;
extern const juce::Identifier backgroundResolution;
extern const juce::Identifier blurRadius;
extern const juce::Identifier bottom;
extern const juce::Identifier button;
extern const juce::Identifier buttonOn;
extern const juce::Identifier caret;
extern const juce::Identifier closePane;
extern const juce::Identifier closeTab;
extern const juce::Identifier depth;
extern const juce::Identifier editorBackground;
extern const juce::Identifier editorOutline;
extern const juce::Identifier enabled;
extern const juce::Identifier expandPaneHeight;
extern const juce::Identifier expandPaneWidth;
extern const juce::Identifier filter;
extern const juce::Identifier focusedOutline;
extern const juce::Identifier focusedSession;
extern const juce::Identifier fontContrast;
extern const juce::Identifier fontGamma;
extern const juce::Identifier fontRasterizer;
extern const juce::Identifier frameRate;
extern const juce::Identifier graphics;
extern const juce::Identifier highlight;
extern const juce::Identifier imouse;
extern const juce::Identifier joinDown;
extern const juce::Identifier joinLeft;
extern const juce::Identifier joinRight;
extern const juce::Identifier joinUp;
extern const juce::Identifier kerningFactor;
extern const juce::Identifier keys;
extern const juce::Identifier lineHeight;
extern const juce::Identifier menu;
extern const juce::Identifier mouse;
extern const juce::Identifier newPane;
extern const juce::Identifier newPlugin;
extern const juce::Identifier newSession;
extern const juce::Identifier newTab;
extern const juce::Identifier nextTab;
extern const juce::Identifier orbit;
extern const juce::Identifier outline;
extern const juce::Identifier overlay;                 ///< Transient message-overlay row.
extern const juce::Identifier pane;
extern const juce::Identifier paneDown;
extern const juce::Identifier paneLeft;
extern const juce::Identifier paneRight;
extern const juce::Identifier paneStep;
extern const juce::Identifier paneUp;
extern const juce::Identifier pluginId;
extern const juce::Identifier postProcessing;
extern const juce::Identifier postProcessingOpacity;
extern const juce::Identifier postProcessingResolution;
extern const juce::Identifier prefix;
extern const juce::Identifier prefixTimeout;
extern const juce::Identifier prevTab;
extern const juce::Identifier quit;
extern const juce::Identifier reducePaneHeight;
extern const juce::Identifier reducePaneWidth;
extern const juce::Identifier reload;
extern const juce::Identifier reset;
extern const juce::Identifier resizeBar;
extern const juce::Identifier resizeBarHighlight;
extern const juce::Identifier resizeBarThickness;
extern const juce::Identifier resizerBar;
extern const juce::Identifier scrollbar;
extern const juce::Identifier selectionCursor;
extern const juce::Identifier session;
extern const juce::Identifier sessions;
extern const juce::Identifier shaderFormat;
extern const juce::Identifier splitHorizontal;
extern const juce::Identifier splitVertical;
extern const juce::Identifier successMessage;
extern const juce::Identifier swapDown;
extern const juce::Identifier swapLeft;
extern const juce::Identifier swapRight;
extern const juce::Identifier swapUp;
extern const juce::Identifier tab;
extern const juce::Identifier tabBar;
extern const juce::Identifier tabHighlight;
extern const juce::Identifier textFontSize;
extern const juce::Identifier textOff;
extern const juce::Identifier textOn;
extern const juce::Identifier textPadding;
extern const juce::Identifier theme;
extern const juce::Identifier themes;
extern const juce::Identifier thumb;
extern const juce::Identifier titleBarButtons;
extern const juce::Identifier track;
extern const juce::Identifier useGpu;
extern const juce::Identifier visible;
extern const juce::Identifier zoom;
extern const juce::Identifier zoomIn;
extern const juce::Identifier zoomOut;
extern const juce::Identifier zoomReset;
extern const juce::Identifier zoomStep;

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Id
