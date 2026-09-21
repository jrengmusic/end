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

inline const juce::Identifier alwaysOnTop              { juce::String::fromUTF8 ("alwaysOnTop")              };
inline const juce::Identifier alwaysVisible            { juce::String::fromUTF8 ("alwaysVisible")            };
inline const juce::Identifier backgroundOpacity        { juce::String::fromUTF8 ("backgroundOpacity")        };
inline const juce::Identifier backgroundResolution     { juce::String::fromUTF8 ("backgroundResolution")     };
inline const juce::Identifier blurRadius               { juce::String::fromUTF8 ("blurRadius")               };
inline const juce::Identifier buttonOn                 { juce::String::fromUTF8 ("buttonOn")                 };
inline const juce::Identifier caret                    { juce::String::fromUTF8 ("caret")                    };
inline const juce::Identifier closePane                { juce::String::fromUTF8 ("closePane")                };
inline const juce::Identifier closeTab                 { juce::String::fromUTF8 ("closeTab")                 };
inline const juce::Identifier depth                    { juce::String::fromUTF8 ("depth")                    };
inline const juce::Identifier editorBackground         { juce::String::fromUTF8 ("editorBackground")         };
inline const juce::Identifier editorOutline            { juce::String::fromUTF8 ("editorOutline")            };
inline const juce::Identifier enabled                  { juce::String::fromUTF8 ("enabled")                  };
inline const juce::Identifier expandPaneHeight         { juce::String::fromUTF8 ("expandPaneHeight")         };
inline const juce::Identifier expandPaneWidth          { juce::String::fromUTF8 ("expandPaneWidth")          };
inline const juce::Identifier filter                   { juce::String::fromUTF8 ("filter")                   };
inline const juce::Identifier focusedOutline           { juce::String::fromUTF8 ("focusedOutline")           };
inline const juce::Identifier focusedSession           { juce::String::fromUTF8 ("focusedSession")           };
inline const juce::Identifier fontContrast             { juce::String::fromUTF8 ("fontContrast")             };
inline const juce::Identifier fontGamma                { juce::String::fromUTF8 ("fontGamma")                };
inline const juce::Identifier fontRasterizer           { juce::String::fromUTF8 ("fontRasterizer")           };
inline const juce::Identifier frameRate                { juce::String::fromUTF8 ("frameRate")                };
inline const juce::Identifier imouse                   { juce::String::fromUTF8 ("imouse")                   };
inline const juce::Identifier joinDown                 { juce::String::fromUTF8 ("joinDown")                 };
inline const juce::Identifier joinLeft                 { juce::String::fromUTF8 ("joinLeft")                 };
inline const juce::Identifier joinRight                { juce::String::fromUTF8 ("joinRight")                };
inline const juce::Identifier joinUp                   { juce::String::fromUTF8 ("joinUp")                   };
inline const juce::Identifier kerningFactor            { juce::String::fromUTF8 ("kerningFactor")            };
inline const juce::Identifier mouse                    { juce::String::fromUTF8 ("mouse")                    };
inline const juce::Identifier newPane                  { juce::String::fromUTF8 ("newPane")                  };
inline const juce::Identifier newPlugin                { juce::String::fromUTF8 ("newPlugin")                };
inline const juce::Identifier newSession               { juce::String::fromUTF8 ("newSession")               };
inline const juce::Identifier newTab                   { juce::String::fromUTF8 ("newTab")                   };
inline const juce::Identifier nextTab                  { juce::String::fromUTF8 ("nextTab")                  };
inline const juce::Identifier orbit                    { juce::String::fromUTF8 ("orbit")                    };
inline const juce::Identifier outline                  { juce::String::fromUTF8 ("outline")                  };
inline const juce::Identifier pane                     { juce::String::fromUTF8 ("pane")                     };
inline const juce::Identifier paneDown                 { juce::String::fromUTF8 ("paneDown")                 };
inline const juce::Identifier paneLeft                 { juce::String::fromUTF8 ("paneLeft")                 };
inline const juce::Identifier paneRight                { juce::String::fromUTF8 ("paneRight")                };
inline const juce::Identifier paneStep                 { juce::String::fromUTF8 ("paneStep")                 };
inline const juce::Identifier paneUp                   { juce::String::fromUTF8 ("paneUp")                   };
inline const juce::Identifier pluginId                 { juce::String::fromUTF8 ("pluginId")                 };
inline const juce::Identifier postProcessing           { juce::String::fromUTF8 ("postProcessing")           };
inline const juce::Identifier postProcessingOpacity    { juce::String::fromUTF8 ("postProcessingOpacity")    };
inline const juce::Identifier postProcessingResolution { juce::String::fromUTF8 ("postProcessingResolution") };
inline const juce::Identifier prefix                   { juce::String::fromUTF8 ("prefix")                   };
inline const juce::Identifier prefixTimeout            { juce::String::fromUTF8 ("prefixTimeout")            };
inline const juce::Identifier prevTab                  { juce::String::fromUTF8 ("prevTab")                  };
inline const juce::Identifier quit                     { juce::String::fromUTF8 ("quit")                     };
inline const juce::Identifier reducePaneHeight         { juce::String::fromUTF8 ("reducePaneHeight")         };
inline const juce::Identifier reducePaneWidth          { juce::String::fromUTF8 ("reducePaneWidth")          };
inline const juce::Identifier reload                   { juce::String::fromUTF8 ("reload")                   };
inline const juce::Identifier resizeBar                { juce::String::fromUTF8 ("resizeBar")                };
inline const juce::Identifier resizeBarHighlight       { juce::String::fromUTF8 ("resizeBarHighlight")       };
inline const juce::Identifier resizeBarThickness       { juce::String::fromUTF8 ("resizeBarThickness")       };
inline const juce::Identifier resizerBar               { juce::String::fromUTF8 ("resizerBar")               };
inline const juce::Identifier scrollbar                { juce::String::fromUTF8 ("scrollbar")                };
inline const juce::Identifier selectionCursor          { juce::String::fromUTF8 ("selectionCursor")          };
inline const juce::Identifier session                  { juce::String::fromUTF8 ("session")                  };
inline const juce::Identifier sessions                 { juce::String::fromUTF8 ("sessions")                 };
inline const juce::Identifier shaderFormat             { juce::String::fromUTF8 ("shaderFormat")             };
inline const juce::Identifier splitHorizontal          { juce::String::fromUTF8 ("splitHorizontal")          };
inline const juce::Identifier splitVertical            { juce::String::fromUTF8 ("splitVertical")            };
inline const juce::Identifier successMessage           { juce::String::fromUTF8 ("successMessage")           };
inline const juce::Identifier swapDown                 { juce::String::fromUTF8 ("swapDown")                 };
inline const juce::Identifier swapLeft                 { juce::String::fromUTF8 ("swapLeft")                 };
inline const juce::Identifier swapRight                { juce::String::fromUTF8 ("swapRight")                };
inline const juce::Identifier swapUp                   { juce::String::fromUTF8 ("swapUp")                   };
inline const juce::Identifier tab                      { juce::String::fromUTF8 ("tab")                      };
inline const juce::Identifier tabBar                   { juce::String::fromUTF8 ("tabBar")                   };
inline const juce::Identifier tabHighlight             { juce::String::fromUTF8 ("tabHighlight")             };
inline const juce::Identifier textFontSize             { juce::String::fromUTF8 ("fontSize")                 };
inline const juce::Identifier textOff                  { juce::String::fromUTF8 ("textOff")                  };
inline const juce::Identifier textOn                   { juce::String::fromUTF8 ("textOn")                   };
inline const juce::Identifier textPadding              { juce::String::fromUTF8 ("textPadding")              };
inline const juce::Identifier theme                    { juce::String::fromUTF8 ("theme")                    };
inline const juce::Identifier themes                   { juce::String::fromUTF8 ("themes")                   };
inline const juce::Identifier thumb                    { juce::String::fromUTF8 ("thumb")                    };
inline const juce::Identifier titleBarButtons          { juce::String::fromUTF8 ("titleBarButtons")          };
inline const juce::Identifier useGpu                   { juce::String::fromUTF8 ("gpu")                      };
inline const juce::Identifier visible                  { juce::String::fromUTF8 ("visible")                  };
inline const juce::Identifier zoom                     { juce::String::fromUTF8 ("zoom")                     };
inline const juce::Identifier zoomIn                   { juce::String::fromUTF8 ("zoomIn")                   };
inline const juce::Identifier zoomOut                  { juce::String::fromUTF8 ("zoomOut")                  };
inline const juce::Identifier zoomReset                { juce::String::fromUTF8 ("zoomReset")                };
inline const juce::Identifier zoomStep                 { juce::String::fromUTF8 ("zoomStep")                 };

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Id
