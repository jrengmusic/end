## config

Root ValueTree type names and config-file section identifiers ConfigModel and Main use to compose and locate END's lua-backed configuration tree (config.lua/display.lua/keys.lua/popup.lua/theme.lua/whelmed.lua and their graphics/hint/menu/scrollbar runtime subtrees).

| word | string |
|---|---|
| config | config |
| display | display |
| hint | hint |
| keys | keys |
| menu | menu |
| popup | popup |
| scrollbar | scrollbar |
| successMessage | success_message |
| theme | theme |
| themes | themes |
| titleBarButtons | title_bar_buttons |
| whelmed | whelmed |

## fileExtension

File extension tokens Files::Config/Files::Themes/Files::Flex append when composing END's on-disk config, theme, and graphics-asset filenames.

| word | string |
|---|---|
| lua | lua |

## common

Generic layout/appearance property keys shared by more than one theme.lua config section (pane resize bar, sidebar, split-line, focus outline).

| word | string |
|---|---|
| focusedOutline | focused_outline |
| resizeBar | resize_bar |
| resizeBarHighlight | resize_bar_highlight |
| resizeBarThickness | resize_bar_thickness |
| sidebarSize | sidebar_size |
| splitLine | split_line |

## theme

theme.lua visual property keys — tab/pane/window chrome, font metrics, cursor/caret rendering, scrollbar and status-bar styling, and the button/text colour-state suffixes consumed by ENDLookAndFeel and EventRegistration.

| word | string |
|---|---|
| alwaysOnTop | always_on_top |
| alwaysVisible | always_visible |
| blink | blink |
| blinkInterval | blink_interval |
| blurRadius | blur_radius |
| buttonOn | button_on |
| caret | caret |
| cellWidth | cell_width |
| code | code |
| cursorChar | char |
| depth | depth |
| editorBackground | editor_background |
| editorOutline | editor_outline |
| flex | flex |
| fontFamily | font_family |
| force | force |
| kerningFactor | kerning_factor |
| labelBackground | label_background |
| labelText | label_text |
| ligatures | ligatures |
| lineHeight | line_height |
| overlay | overlay |
| pane | pane |
| resizerBar | resizer_bar |
| selectionCursor | selection_cursor |
| sessions | sessions |
| spinner | spinner |
| statusBar | status_bar |
| tab | tab |
| tabBar | tab_bar |
| tabButtonNormalOn | tab_button_normalOn |
| tabHighlight | tab_highlight |
| textFontSize | font_size |
| textOff | text_off |
| textOn | text_on |
| textPadding | text_padding |
| thumb | thumb |
| track | track |

## keys

keys.lua key-binding action names — tab/pane/session lifecycle, pane split/join/swap/resize navigation, and the action-list popup identifier.

| word | string |
|---|---|
| actionList | action_list |
| closePane | close_pane |
| closeTab | close_tab |
| expandPaneHeight | expand_pane_height |
| expandPaneWidth | expand_pane_width |
| joinDown | join_down |
| joinLeft | join_left |
| joinRight | join_right |
| joinUp | join_up |
| newPane | new_pane |
| newPlugin | new_plugin |
| newSession | new_session |
| newTab | new_tab |
| nextTab | next_tab |
| paneDown | pane_down |
| paneLeft | pane_left |
| paneRight | pane_right |
| paneUp | pane_up |
| prefix | prefix |
| prefixTimeout | prefix_timeout |
| prevTab | prev_tab |
| reducePaneHeight | reduce_pane_height |
| reducePaneWidth | reduce_pane_width |
| splitHorizontal | split_horizontal |
| splitVertical | split_vertical |
| swapDown | swap_down |
| swapLeft | swap_left |
| swapRight | swap_right |
| swapUp | swap_up |
| zoomIn | zoom_in |
| zoomOut | zoom_out |
| zoomReset | zoom_reset |

## app

config.lua application-runtime keys — GPU acceleration toggle, terminal integration, multi-file drop join mode, and pane/zoom step increments.

| word | string |
|---|---|
| dropMultifiles | drop_multifiles |
| paneStep | pane_step |
| terminal | terminal |
| useGpu | gpu |
| zoomStep | zoom_step |

## shader

graphics.lua shader-pipeline keys — background/post-processing opacity and resolution, frame rate, font rasterization tuning, and the active shader-manifest-format tag.

| word | string |
|---|---|
| backgroundOpacity | background_opacity |
| backgroundResolution | background_resolution |
| filter | filter |
| fontContrast | font_contrast |
| fontGamma | font_gamma |
| fontRasterizer | font_rasterizer |
| frameRate | frame_rate |
| postProcessing | post_processing |
| postProcessingOpacity | post_processing_opacity |
| postProcessingResolution | post_processing_resolution |
| shaderFormat | shader_format |

## mouse

graphics.mouse nested config-table keys — interaction toggle and the imouse/orbit/reset button-role slots validated against Id::MouseButton.

| word | string |
|---|---|
| imouse | imouse |
| mouse | mouse |
| orbit | orbit |
| reset | reset |

## model

ENDModel runtime state keys — focused session, viewport zoom, status message, and the hosted plugin identifier stamped onto a pane row.

| word | string |
|---|---|
| focusedSession | focused_session |
| message | message |
| pluginId | plugin_id |
| zoom | zoom |

## shared

Generic ValueTree property keys and a terminal-session type-name token that jam:: modules previously declared globally; END is now their sole consumer across LookAndFeel colour/geometry lookups, mouse-enable state, visibility toggling, and terminal Session state.

| word | string |
|---|---|
| enabled | enabled |
| outline | outline |
| session | session |
| visible | visible |

## dropMode

Multi-file drop separator token backing the DropMode bimap's newline key (its space key reuses Id::space, already declared by jam_mermaid).

| word | string |
|---|---|
| newline | newline |

## cursorShape

Terminal caret geometry names backing the CursorShape bimap (its bar key reuses Id::bar, already declared by jam_mermaid).

| word | string |
|---|---|
| block | block |
| underline | underline |

## axisLine

Split preview overlay axis line style names backing the OverlayAxisLine bimap (its solid key reuses Id::solid, already declared by jam_mermaid).

| word | string |
|---|---|
| bracket | bracket |
| dash | dash |

## bimap DropMode

Multi-file drop separator mode bimap consumed by init.terminal.drop_multifiles.

| key | value | views | default |
|---|---|---|---|
| space | 0 |  | yes |
| newline | 1 |  |  |

## bimap CursorShape

Terminal caret geometry bimap consumed by theme.lua cursor.style; integer keys mirror jam::CaretShape's enumerator order.

| key | value | views | default |
|---|---|---|---|
| block | 0 |  | yes |
| underline | 1 |  |  |
| bar | 2 |  |  |

## bimap OverlayAxisLine

Split preview overlay axis line style bimap consumed by theme.lua pane.split_line.

| key | value | views | default |
|---|---|---|---|
| solid | 0 |  | yes |
| dash | 1 |  |  |
| bracket | 2 |  |  |

## bimap FileConfig

Lua config-file section registry — resolves each key to its own Identifier stem naming the on-disk config file (display.lua/popup.lua/keys.lua); consumer-side helpers derive the full filename and directory path.

| key | value | views | default |
|---|---|---|---|
| display | 0 |  | yes |
| popup | 1 |  |  |
| keys | 2 |  |  |

## bimap FileThemes

Lua theme-directory file registry — resolves each key to its own Identifier stem naming the on-disk theme file (theme.lua/whelmed.lua); consumer-side helpers derive the full filename and directory path.

| key | value | views | default |
|---|---|---|---|
| theme | 0 |  | yes |
| whelmed | 1 |  |  |

## bimap FileFlex

Theme SVG graphics-asset registry — resolves each key to its own Identifier stem naming the on-disk SVG file (tab_bar.svg/tab_highlight.svg/tab_button_normalOn.svg/resizer_bar.svg); consumer-side helpers derive the full filename.

| key | value | views | default |
|---|---|---|---|
| tabBar | 0 |  | yes |
| tabHighlight | 1 |  |  |
| tabButtonNormalOn | 2 |  |  |
| resizerBar | 3 |  |  |
