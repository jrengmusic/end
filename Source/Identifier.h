/**
 * @file Identifier.h
 * @brief END-scoped juce::Identifier constants.
 *
 * X-macro blocks expand into inline constants across four views
 * (ID, IDref, IDtag, IDtype). All categories are combined into a single
 * MAKE_VIEW invocation per view — adding a new category means adding
 * its X-macro block here, not redefining MAKE_VIEW.
 *
 * Keys already present in jam::ID are excluded — they are available globally.
 */
#pragma once
#include <JuceHeader.h>

// ============================================================================
// MACROS are evil. Yet, in the pursuit of one source of truth for convenience
// and consistency in the land of C++, this necessary horror spares us from
// repeating ourselves. May God forgive our sins.
//
// JRENG!
// ============================================================================

// Config + theme section and tree type names
#define IDENTIFIER_CONFIG(X)                 \
    X (config, "config")                     \
    X (init, "init")                         \
    X (end, "end")                           \
    X (keys, "keys")                         \
    X (popups, "popups")                     \
    X (whelmed, "whelmed")                   \
    X (graphics, "graphics")                 \
    X (loadMessage, "load_message")          \
    X (theme, "theme")                       \
    X (themes, "themes")                     \
    X (ansi, "ansi")                         \
    X (scrollbar, "scrollbar")               \
    X (hint, "hint")                         \
    X (titleBarButtons, "title_bar_buttons") \
    X (saveWindowState, "save_window_state")

// Keys shared across multiple config sections (not in jam::ID)
#define IDENTIFIER_COMMON(X)                       \
    X (position, "position")                       \
    X (orientation, "orientation")                 \
    X (inactive, "inactive")                       \
    X (active, "active")                           \
    X (scrollbarWidth, "scrollbar_width")          \
    X (resizeBar, "resize_bar")                    \
    X (resizeBarHighlight, "resize_bar_highlight") \
    X (resizeBarThickness, "resize_bar_thickness") \
    X (borderColour, "border_colour")              \
    X (borderWidth, "border_width")                \
    X (scrollbackLines, "scrollback_lines")        \
    X (scrollStep, "scroll_step")                  \
    X (cols, "cols")                               \
    X (rows, "rows")                               \
    X (scrollbarThumb, "scrollbar_thumb")          \
    X (scrollbarTrack, "scrollbar_track")

// Theme-specific keys (theme tree property and type names)
#define IDENTIFIER_THEME(X)                        \
    X (tab, "tab")                                 \
    X (tabBar, "tab_bar")                          \
    X (tabButton, "tab_button")                    \
    X (tabHighlight, "tab_highlight")              \
    X (resizerBar, "resizer_bar")                  \
    X (tabs, "tabs")                               \
    X (pane, "pane")                               \
    X (window, "window")                           \
    X (windowFx, "window_fx")                      \
    X (size, "size")                               \
    X (view, "view")                               \
    X (popup, "popup")                             \
    X (ligatures, "ligatures")                     \
    X (embolden, "embolden")                       \
    X (lineHeight, "line_height")                  \
    X (cellWidth, "cell_width")                    \
    X (desktopScale, "desktop_scale")              \
    X (blink, "blink")                             \
    X (blinkInterval, "blink_interval")            \
    X (force, "force")                             \
    X (editorBackground, "editor_background")      \
    X (editorOutline, "editor_outline")            \
    X (caret, "caret")                             \
    X (highlight, "highlight")                     \
    X (selectionCursor, "selection_cursor")        \
    X (black, "black")                             \
    X (red, "red")                                 \
    X (green, "green")                             \
    X (yellow, "yellow")                           \
    X (blue, "blue")                               \
    X (magenta, "magenta")                         \
    X (cyan, "cyan")                               \
    X (white, "white")                             \
    X (brightBlack, "bright_black")                \
    X (brightRed, "bright_red")                    \
    X (brightGreen, "bright_green")                \
    X (brightYellow, "bright_yellow")              \
    X (brightBlue, "bright_blue")                  \
    X (brightMagenta, "bright_magenta")            \
    X (brightCyan, "bright_cyan")                  \
    X (brightWhite, "bright_white")                \
    X (statusBar, "status_bar")                    \
    X (statusBarLabelBg, "status_bar_label_bg")    \
    X (statusBarLabelFg, "status_bar_label_fg")    \
    X (statusBarSpinner, "status_bar_spinner")     \
    X (hintLabelBg, "hint_label_bg")               \
    X (hintLabelFg, "hint_label_fg")               \
    X (thumb, "thumb")                             \
    X (track, "track")                             \
    X (labelBackground, "label_background")        \
    X (labelText, "label_text")                    \
    X (spinner, "spinner")                         \
    X (blurRadius, "blur_radius")                  \
    X (alwaysOnTop, "always_on_top")               \
    X (confirmationOnExit, "confirmation_on_exit") \
    X (forceDwm, "force_dwm")                      \
    X (closeOnRun, "close_on_run")                 \
    X (code, "code")                               \
    X (nameFontFamily, "name_font_family")         \
    X (nameFontStyle, "name_font_style")           \
    X (nameFontSize, "name_font_size")             \
    X (shortcutFontFamily, "shortcut_font_family") \
    X (shortcutFontStyle, "shortcut_font_style")   \
    X (shortcutFontSize, "shortcut_font_size")     \
    X (nameColour, "name_colour")                  \
    X (shortcutColour, "shortcut_colour")          \
    X (highlightColour, "highlight_colour")        \
    X (fontFamily, "font_family")                  \
    X (fontSize, "font_size")                      \
    X (fontStyle, "font_style")                    \
    X (textOn, "text_on")                          \
    X (textOff, "text_off")                        \
    X (buttonOn, "button_on")                      \
    X (uppercase, "uppercase")                     \
    X (depth, "depth")                             \
    X (kerningFactor, "kerning_factor")            \
    X (textPadding, "text_padding")

// Key binding names (keys.lua)
#define IDENTIFIER_KEYS(X)                             \
    X (prefix, "prefix")                               \
    X (prefixTimeout, "prefix_timeout")                \
    X (copy, "copy")                                   \
    X (paste, "paste")                                 \
    X (quit, "quit")                                   \
    X (closeTab, "close_tab")                          \
    X (closePane, "close_pane")                        \
    X (reload, "reload")                               \
    X (zoomIn, "zoom_in")                              \
    X (zoomOut, "zoom_out")                            \
    X (zoomReset, "zoom_reset")                        \
    X (newWindow, "new_window")                        \
    X (newTab, "new_tab")                              \
    X (prevTab, "prev_tab")                            \
    X (nextTab, "next_tab")                            \
    X (renameTab, "rename_tab")                        \
    X (splitHorizontal, "split_horizontal")            \
    X (splitVertical, "split_vertical")                \
    X (paneLeft, "pane_left")                          \
    X (paneDown, "pane_down")                          \
    X (paneUp, "pane_up")                              \
    X (paneRight, "pane_right")                        \
    X (newline, "newline")                             \
    X (actionList, "action_list")                      \
    X (enterSelection, "enter_selection")              \
    X (enterOpenFile, "enter_open_file")               \
    X (openFileNextPage, "open_file_next_page")        \
    X (selectionUp, "selection_up")                    \
    X (selectionDown, "selection_down")                \
    X (selectionLeft, "selection_left")                \
    X (selectionRight, "selection_right")              \
    X (selectionVisual, "selection_visual")            \
    X (selectionVisualLine, "selection_visual_line")   \
    X (selectionVisualBlock, "selection_visual_block") \
    X (selectionCopy, "selection_copy")                \
    X (selectionTop, "selection_top")                  \
    X (selectionBottom, "selection_bottom")            \
    X (selectionLineStart, "selection_line_start")     \
    X (selectionLineEnd, "selection_line_end")         \
    X (selectionExit, "selection_exit")                \
    X (scrollDown, "scroll_down")                      \
    X (scrollUp, "scroll_up")                          \
    X (scrollTop, "scroll_top")                        \
    X (scrollBottom, "scroll_bottom")

// Application runtime keys (init.lua sections) — excludes jam::ID: editor ("editor"),
// cwd ("cwd"), image ("image"), mode ("mode"), interval ("interval")
// jam::gpu has string "GPU" (uppercase) — "gpu" (lowercase) is a distinct lua key, declared here
#define IDENTIFIER_APP(X)                 \
    X (gpu, "gpu")                        \
    X (shaders, "shaders")                \
    X (daemon, "daemon")                  \
    X (autoReload, "auto_reload")         \
    X (shell, "shell")                    \
    X (program, "program")                \
    X (args, "args")                      \
    X (integration, "integration")        \
    X (terminal, "terminal")              \
    X (dropMultifiles, "drop_multifiles") \
    X (dropQuoted, "drop_quoted")         \
    X (hyperlinks, "hyperlinks")          \
    X (atlasDimension, "atlas_dimension") \
    X (border, "border")                  \
    X (tabOrientation, "tab_orientation")

// Popup entry keys (popups.lua) — cols, rows already in COMMON
#define IDENTIFIER_POPUPS(X) \
    X (command, "command")   \
    X (modal, "modal")

// Action entry keys (init.lua actions section)
#define IDENTIFIER_ACTIONS(X)      \
    X (description, "description") \
    X (execute, "execute")

// Whelmed-specific keys (whelmed.lua)
#define IDENTIFIER_WHELMED(X)                            \
    X (bodyColour, "body_colour")                        \
    X (linkColour, "link_colour")                        \
    X (h1Size, "h1_size")                                \
    X (h2Size, "h2_size")                                \
    X (h3Size, "h3_size")                                \
    X (h4Size, "h4_size")                                \
    X (h5Size, "h5_size")                                \
    X (h6Size, "h6_size")                                \
    X (h1Colour, "h1_colour")                            \
    X (h2Colour, "h2_colour")                            \
    X (h3Colour, "h3_colour")                            \
    X (h4Colour, "h4_colour")                            \
    X (h5Colour, "h5_colour")                            \
    X (h6Colour, "h6_colour")                            \
    X (codeFenceBackground, "code_fence_background")     \
    X (codeColour, "code_colour")                        \
    X (tokenError, "token_error")                        \
    X (tokenComment, "token_comment")                    \
    X (tokenKeyword, "token_keyword")                    \
    X (tokenOperator, "token_operator")                  \
    X (tokenIdentifier, "token_identifier")              \
    X (tokenInteger, "token_integer")                    \
    X (tokenFloat, "token_float")                        \
    X (tokenString, "token_string")                      \
    X (tokenBracket, "token_bracket")                    \
    X (tokenPunctuation, "token_punctuation")            \
    X (tokenPreprocessor, "token_preprocessor")          \
    X (tableBackground, "table_background")              \
    X (tableHeaderBackground, "table_header_background") \
    X (tableRowAlt, "table_row_alt")                     \
    X (tableBorderColour, "table_border_colour")         \
    X (tableHeaderText, "table_header_text")             \
    X (tableCellText, "table_cell_text")                 \
    X (progressBackground, "progress_background")        \
    X (progressForeground, "progress_foreground")        \
    X (progressTextColour, "progress_text_colour")       \
    X (progressSpinnerColour, "progress_spinner_colour") \
    X (scrollbarBackground, "scrollbar_background")      \
    X (selectionColour, "selection_colour")

// Shader pass name identifiers + embedded shader uniform names.
// common ("Common") and image ("Image") shadow jam::ID lowercase versions intentionally —
// these match the Shadertoy on-disk filenames (uppercase first letter).
// background shadows jam::ID::background intentionally — END's ID struct is separate from jam::ID.
// iChannel0..3 mirror the GLSL uniform keyword names declared in Source/shader/wrapper.frag —
// the C++ side uses them as iChannel slot identifiers in config::Shaders.
#define IDENTIFIER_SHADER(X)                    \
    X (iResolution, "iResolution")              \
    X (iTime, "iTime")                          \
    X (iTimeDelta, "iTimeDelta")                \
    X (iFrame, "iFrame")                        \
    X (iChannel0, "iChannel0")                  \
    X (iChannel1, "iChannel1")                  \
    X (iChannel2, "iChannel2")                  \
    X (iChannel3, "iChannel3")                  \
    X (common, "Common")                        \
    X (image, "Image")                          \
    X (bufferA, "BufferA")                      \
    X (bufferB, "BufferB")                      \
    X (bufferC, "BufferC")                      \
    X (bufferD, "BufferD")                      \
    X (background, "background")                \
    X (backgroundOpacity, "background_opacity") \
    X (postProcessing, "post_processing")

// Platform + BackgroundBlur WindowFX keys (theme.lua / window.window_fx)
// "mac"/"win" identify the platform in the window_fx table;
// the remaining values map to jam::BackgroundBlur::WindowFX per platform.
#define IDENTIFIER_BACKEND(X)                                \
    X (mac, "mac")                                           \
    X (win, "win")                                           \
    X (backgroundBlur, "backgroundBlur")                     \
    X (visualFXWindowBackground, "visualFXWindowBackground") \
    X (glassFXRegular, "glassFXRegular")                     \
    X (glassFXClear, "glassFXClear")                         \
    X (blurBehind, "blurBehind")                             \
    X (acrylic10, "acrylic10")                               \
    X (acrylic11, "acrylic11")                               \
    X (mica, "mica")

// Runtime state keys (end::Model)
#define IDENTIFIER_MODEL(X)         \
    X (focusedPane, "focused_pane") \
    X (focus, "focus")              \
    X (zoom, "zoom")                \
    X (renderer, "renderer")

// ============================================================================
#define END_MAKE_VIEW(ViewName, EXPANDER) \
    struct ViewName                       \
    {                                     \
        IDENTIFIER_CONFIG (EXPANDER)      \
        IDENTIFIER_COMMON (EXPANDER)      \
        IDENTIFIER_THEME (EXPANDER)       \
        IDENTIFIER_KEYS (EXPANDER)        \
        IDENTIFIER_APP (EXPANDER)         \
        IDENTIFIER_POPUPS (EXPANDER)      \
        IDENTIFIER_ACTIONS (EXPANDER)     \
        IDENTIFIER_WHELMED (EXPANDER)     \
        IDENTIFIER_BACKEND (EXPANDER)     \
        IDENTIFIER_SHADER (EXPANDER)      \
        IDENTIFIER_MODEL (EXPANDER)       \
    };

END_MAKE_VIEW (ID, AS_IDENTIFIER)
END_MAKE_VIEW (IDref, AS_STRINGREF)
END_MAKE_VIEW (IDtag, AS_UPPER)
END_MAKE_VIEW (IDtype, AS_TYPE)
