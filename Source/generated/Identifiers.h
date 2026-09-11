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

inline const juce::Identifier actionList               { juce::String::fromUTF8 ("action_list")                };
inline const juce::Identifier alwaysOnTop              { juce::String::fromUTF8 ("always_on_top")              };
inline const juce::Identifier alwaysVisible            { juce::String::fromUTF8 ("always_visible")             };
inline const juce::Identifier backgroundOpacity        { juce::String::fromUTF8 ("background_opacity")         };
inline const juce::Identifier backgroundResolution     { juce::String::fromUTF8 ("background_resolution")      };
inline const juce::Identifier blurRadius               { juce::String::fromUTF8 ("blur_radius")                };
inline const juce::Identifier buttonOn                 { juce::String::fromUTF8 ("button_on")                  };
inline const juce::Identifier caret                    { juce::String::fromUTF8 ("caret")                      };
inline const juce::Identifier cellWidth                { juce::String::fromUTF8 ("cell_width")                 };
inline const juce::Identifier closePane                { juce::String::fromUTF8 ("close_pane")                 };
inline const juce::Identifier closeTab                 { juce::String::fromUTF8 ("close_tab")                  };
inline const juce::Identifier config                   { juce::String::fromUTF8 ("config")                     };
inline const juce::Identifier depth                    { juce::String::fromUTF8 ("depth")                      };
inline const juce::Identifier editorBackground         { juce::String::fromUTF8 ("editor_background")          };
inline const juce::Identifier editorOutline            { juce::String::fromUTF8 ("editor_outline")             };
inline const juce::Identifier enabled                  { juce::String::fromUTF8 ("enabled")                    };
inline const juce::Identifier expandPaneHeight         { juce::String::fromUTF8 ("expand_pane_height")         };
inline const juce::Identifier expandPaneWidth          { juce::String::fromUTF8 ("expand_pane_width")          };
inline const juce::Identifier filter                   { juce::String::fromUTF8 ("filter")                     };
inline const juce::Identifier focusedOutline           { juce::String::fromUTF8 ("focused_outline")            };
inline const juce::Identifier focusedSession           { juce::String::fromUTF8 ("focused_session")            };
inline const juce::Identifier fontContrast             { juce::String::fromUTF8 ("font_contrast")              };
inline const juce::Identifier fontGamma                { juce::String::fromUTF8 ("font_gamma")                 };
inline const juce::Identifier fontRasterizer           { juce::String::fromUTF8 ("font_rasterizer")            };
inline const juce::Identifier frameRate                { juce::String::fromUTF8 ("frame_rate")                 };
inline const juce::Identifier hint                     { juce::String::fromUTF8 ("hint")                       };
inline const juce::Identifier imouse                   { juce::String::fromUTF8 ("imouse")                     };
inline const juce::Identifier joinDown                 { juce::String::fromUTF8 ("join_down")                  };
inline const juce::Identifier joinLeft                 { juce::String::fromUTF8 ("join_left")                  };
inline const juce::Identifier joinRight                { juce::String::fromUTF8 ("join_right")                 };
inline const juce::Identifier joinUp                   { juce::String::fromUTF8 ("join_up")                    };
inline const juce::Identifier kerningFactor            { juce::String::fromUTF8 ("kerning_factor")             };
inline const juce::Identifier keys                     { juce::String::fromUTF8 ("keys")                       };
inline const juce::Identifier labelBackground          { juce::String::fromUTF8 ("label_background")           };
inline const juce::Identifier labelText                { juce::String::fromUTF8 ("label_text")                 };
inline const juce::Identifier ligatures                { juce::String::fromUTF8 ("ligatures")                  };
inline const juce::Identifier message                  { juce::String::fromUTF8 ("message")                    };
inline const juce::Identifier mouse                    { juce::String::fromUTF8 ("mouse")                      };
inline const juce::Identifier newPane                  { juce::String::fromUTF8 ("new_pane")                   };
inline const juce::Identifier newPlugin                { juce::String::fromUTF8 ("new_plugin")                 };
inline const juce::Identifier newSession               { juce::String::fromUTF8 ("new_session")                };
inline const juce::Identifier newTab                   { juce::String::fromUTF8 ("new_tab")                    };
inline const juce::Identifier nextTab                  { juce::String::fromUTF8 ("next_tab")                   };
inline const juce::Identifier orbit                    { juce::String::fromUTF8 ("orbit")                      };
inline const juce::Identifier outline                  { juce::String::fromUTF8 ("outline")                    };
inline const juce::Identifier overlay                  { juce::String::fromUTF8 ("overlay")                    };
inline const juce::Identifier pane                     { juce::String::fromUTF8 ("pane")                       };
inline const juce::Identifier paneDown                 { juce::String::fromUTF8 ("pane_down")                  };
inline const juce::Identifier paneLeft                 { juce::String::fromUTF8 ("pane_left")                  };
inline const juce::Identifier paneRight                { juce::String::fromUTF8 ("pane_right")                 };
inline const juce::Identifier paneStep                 { juce::String::fromUTF8 ("pane_step")                  };
inline const juce::Identifier paneUp                   { juce::String::fromUTF8 ("pane_up")                    };
inline const juce::Identifier pluginId                 { juce::String::fromUTF8 ("plugin_id")                  };
inline const juce::Identifier postProcessing           { juce::String::fromUTF8 ("post_processing")            };
inline const juce::Identifier postProcessingOpacity    { juce::String::fromUTF8 ("post_processing_opacity")    };
inline const juce::Identifier postProcessingResolution { juce::String::fromUTF8 ("post_processing_resolution") };
inline const juce::Identifier prefix                   { juce::String::fromUTF8 ("prefix")                     };
inline const juce::Identifier prefixTimeout            { juce::String::fromUTF8 ("prefix_timeout")             };
inline const juce::Identifier prevTab                  { juce::String::fromUTF8 ("prev_tab")                   };
inline const juce::Identifier reducePaneHeight         { juce::String::fromUTF8 ("reduce_pane_height")         };
inline const juce::Identifier reducePaneWidth          { juce::String::fromUTF8 ("reduce_pane_width")          };
inline const juce::Identifier resizeBar                { juce::String::fromUTF8 ("resize_bar")                 };
inline const juce::Identifier resizeBarHighlight       { juce::String::fromUTF8 ("resize_bar_highlight")       };
inline const juce::Identifier resizeBarThickness       { juce::String::fromUTF8 ("resize_bar_thickness")       };
inline const juce::Identifier resizerBar               { juce::String::fromUTF8 ("resizer_bar")                };
inline const juce::Identifier scrollbar                { juce::String::fromUTF8 ("scrollbar")                  };
inline const juce::Identifier selectionCursor          { juce::String::fromUTF8 ("selection_cursor")           };
inline const juce::Identifier session                  { juce::String::fromUTF8 ("session")                    };
inline const juce::Identifier sessions                 { juce::String::fromUTF8 ("sessions")                   };
inline const juce::Identifier shaderFormat             { juce::String::fromUTF8 ("shader_format")              };
inline const juce::Identifier sidebarSize              { juce::String::fromUTF8 ("sidebar_size")               };
inline const juce::Identifier spinner                  { juce::String::fromUTF8 ("spinner")                    };
inline const juce::Identifier splitHorizontal          { juce::String::fromUTF8 ("split_horizontal")           };
inline const juce::Identifier splitLine                { juce::String::fromUTF8 ("split_line")                 };
inline const juce::Identifier splitVertical            { juce::String::fromUTF8 ("split_vertical")             };
inline const juce::Identifier statusBar                { juce::String::fromUTF8 ("status_bar")                 };
inline const juce::Identifier successMessage           { juce::String::fromUTF8 ("success_message")            };
inline const juce::Identifier swapDown                 { juce::String::fromUTF8 ("swap_down")                  };
inline const juce::Identifier swapLeft                 { juce::String::fromUTF8 ("swap_left")                  };
inline const juce::Identifier swapRight                { juce::String::fromUTF8 ("swap_right")                 };
inline const juce::Identifier swapUp                   { juce::String::fromUTF8 ("swap_up")                    };
inline const juce::Identifier tab                      { juce::String::fromUTF8 ("tab")                        };
inline const juce::Identifier tabBar                   { juce::String::fromUTF8 ("tab_bar")                    };
inline const juce::Identifier tabHighlight             { juce::String::fromUTF8 ("tab_highlight")              };
inline const juce::Identifier textFontSize             { juce::String::fromUTF8 ("font_size")                  };
inline const juce::Identifier textOff                  { juce::String::fromUTF8 ("text_off")                   };
inline const juce::Identifier textOn                   { juce::String::fromUTF8 ("text_on")                    };
inline const juce::Identifier textPadding              { juce::String::fromUTF8 ("text_padding")               };
inline const juce::Identifier theme                    { juce::String::fromUTF8 ("theme")                      };
inline const juce::Identifier themes                   { juce::String::fromUTF8 ("themes")                     };
inline const juce::Identifier thumb                    { juce::String::fromUTF8 ("thumb")                      };
inline const juce::Identifier titleBarButtons          { juce::String::fromUTF8 ("title_bar_buttons")          };
inline const juce::Identifier useGpu                   { juce::String::fromUTF8 ("gpu")                        };
inline const juce::Identifier visible                  { juce::String::fromUTF8 ("visible")                    };
inline const juce::Identifier zoom                     { juce::String::fromUTF8 ("zoom")                       };
inline const juce::Identifier zoomIn                   { juce::String::fromUTF8 ("zoom_in")                    };
inline const juce::Identifier zoomOut                  { juce::String::fromUTF8 ("zoom_out")                   };
inline const juce::Identifier zoomReset                { juce::String::fromUTF8 ("zoom_reset")                 };
inline const juce::Identifier zoomStep                 { juce::String::fromUTF8 ("zoom_step")                  };

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Id
