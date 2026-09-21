This file defines the visual appearance for the END default "gfx" theme.
Each section maps 1:1 to a component scope. Colour format: 0xAARRGGBB hex
integer (Alpha|Red|Green|Blue). Alpha-blended colours below are pre-resolved
from their source palette entry and alpha fraction.

## window

+-------------+--------+------------+---------+----------------------------------------------------+
| key         | type   | value      | choices | description                                        |
+=============+========+============+=========+====================================================+
| background  | colour | 0xbf090d12 |         | Window background tint. Alpha controls window      |
|             |        |            |         | transparency (glass mode).                         |
+-------------+--------+------------+---------+----------------------------------------------------+
| blurRadius  | int    | 32         |         | Background blur radius in pixels (0 = no blur).    |
|             |        |            |         | GPU only.                                          |
+-------------+--------+------------+---------+----------------------------------------------------+

## style

Window visual effect style per platform — see the choices column for
accepted values.

+-----+--------+----------------+----------------------------------------------------+-------------+
| key | type   | value          | choices                                            | description |
+=====+========+================+====================================================+=============+
| mac | string | backgroundBlur | backgroundBlur, visualFXWindowBackground,          | macOS       |
|     |        |                | glassFXRegular, glassFXClear                       | window      |
|     |        |                |                                                    | visual      |
|     |        |                |                                                    | effect      |
|     |        |                |                                                    | style.      |
+-----+--------+----------------+----------------------------------------------------+-------------+
| win | string | blurBehind     | blurBehind, acrylic10, acrylic11, mica             | Windows     |
|     |        |                |                                                    | window      |
|     |        |                |                                                    | visual      |
|     |        |                |                                                    | effect      |
|     |        |                |                                                    | style.      |
+-----+--------+----------------+----------------------------------------------------+-------------+

## code

Terminal editing area.

+-------------------+---------+----------------+---------+-----------------------------------------+
| key               | type    | value          | choices | description                             |
+===================+=========+================+=========+=========================================+
| text              | colour  | 0xffa1d6e5     |         | Default text foreground.                |
+-------------------+---------+----------------+---------+-----------------------------------------+
| background        | colour  | 0x00000000     |         | Default background. Transparent lets    |
|                   |         |                |         | the window glass effect show through.   |
+-------------------+---------+----------------+---------+-----------------------------------------+
| caret             | colour  | 0xff4e8c93     |         | Caret (jam::CaretComponent::            |
|                   |         |                |         | caretColourId).                         |
+-------------------+---------+----------------+---------+-----------------------------------------+
| highlight         | colour  | 0x2000ddee     |         | Selection highlight (juce::TextEditor:: |
|                   |         |                |         | highlightColourId). Semi-transparent    |
|                   |         |                |         | recommended so text remains readable.   |
+-------------------+---------+----------------+---------+-----------------------------------------+
| selectionCursor   | colour  | 0xff00ddee     |         | Selection-mode cursor                   |
|                   |         |                |         | (selectionCursorColourId). Shown        |
|                   |         |                |         | instead of the normal cursor when       |
|                   |         |                |         | selection mode is active.               |
+-------------------+---------+----------------+---------+-----------------------------------------+
| editorBackground  | colour  | 0x00000000     |         | Editor widget background fill (juce::   |
|                   |         |                |         | TextEditor::backgroundColourId).        |
|                   |         |                |         | Transparent lets the window glass       |
|                   |         |                |         | effect show through.                    |
+-------------------+---------+----------------+---------+-----------------------------------------+
| editorOutline     | colour  | 0x00000000     |         | Editor widget outline (juce::           |
|                   |         |                |         | TextEditor::outlineColourId).           |
+-------------------+---------+----------------+---------+-----------------------------------------+
| font-family       | string  | Display Mono   |         | Font used for terminal text. Must be a  |
|                   |         |                |         | monospace font installed on the system. |
+-------------------+---------+----------------+---------+-----------------------------------------+
| fontSize          | int     | 12             |         | Font size in points before zoom is      |
|                   |         |                |         | applied (1 - 200).                      |
+-------------------+---------+----------------+---------+-----------------------------------------+
| embolden          | bool    | true           |         | Make text appear bolder. Useful for     |
|                   |         |                |         | thin fonts that are hard to read at     |
|                   |         |                |         | small sizes.                            |
+-------------------+---------+----------------+---------+-----------------------------------------+
| lineHeight        | float   | 1.0            |         | Line height multiplier applied to       |
|                   |         |                |         | terminal cell height (0.5 - 3.0). 1.0 = |
|                   |         |                |         | no adjustment. Values above 1.0         |
|                   |         |                |         | increase spacing, below decrease it.    |
+-------------------+---------+----------------+---------+-----------------------------------------+
| cellWidth         | float   | 1.0            |         | Cell width multiplier applied to        |
|                   |         |                |         | terminal cell width (0.5 - 3.0). 1.0 =  |
|                   |         |                |         | no adjustment. Values above 1.0 widen   |
|                   |         |                |         | cells, below narrow them.               |
+-------------------+---------+----------------+---------+-----------------------------------------+

## scrollbar

+-------+--------+------------+---------+----------------------------------------------------------+
| key   | type   | value      | choices | description                                              |
+=======+========+============+=========+==========================================================+
| thumb | colour | 0x802c4144 |         | Scrollbar thumb (juce::ScrollBar::thumbColourId).        |
|       |        |            |         | Semi-transparent recommended.                            |
+-------+--------+------------+---------+----------------------------------------------------------+
| track | colour | 0x00000000 |         | Scrollbar track (juce::ScrollBar::trackColourId).        |
|       |        |            |         | Transparent for no visible track.                        |
+-------+--------+------------+---------+----------------------------------------------------------+

## tab

Tab bar.

+----------------+---------+------------+---------+------------------------------------------------+
| key            | type    | value      | choices | description                                    |
+================+=========+============+=========+================================================+
| position       | string  | top        | top,    | Tab bar position.                              |
|                |         |            | bottom, |                                                |
|                |         |            | left,   |                                                |
|                |         |            | right   |                                                |
+----------------+---------+------------+---------+------------------------------------------------+
| alwaysVisible  | bool    | true       |         | Tab bar visibility when only single tab        |
|                |         |            |         | opened.                                        |
+----------------+---------+------------+---------+------------------------------------------------+
| background     | colour  | 0xff8fc6d0 |         | Bar strip background (jam::button::Bar::       |
|                |         |            |         | backgroundColourId).                           |
+----------------+---------+------------+---------+------------------------------------------------+
| highlight      | colour  | 0xff6b9099 |         | Sliding selection highlight (jam::button::     |
|                |         |            |         | Bar::highlightColourId).                       |
+----------------+---------+------------+---------+------------------------------------------------+
| outline        | colour  | 0xff273233 |         | SVG group outline (jam::button::Bar::          |
|                |         |            |         | outlineColourId).                              |
+----------------+---------+------------+---------+------------------------------------------------+
| font-family    | string  | Display    |         | Tab bar font family.                           |
+----------------+---------+------------+---------+------------------------------------------------+
| fontSize       | int     | 12         |         | Tab bar font size in points.                   |
+----------------+---------+------------+---------+------------------------------------------------+
| kerningFactor  | float   | 0.075      |         | Extra spacing between tab label characters, as |
|                |         |            |         | a fraction of the font size (0.0 = font        |
|                |         |            |         | default).                                      |
+----------------+---------+------------+---------+------------------------------------------------+
| depth          | float   | 3.0        |         | Tab bar height as a multiple of the tab font   |
|                |         |            |         | height.                                        |
+----------------+---------+------------+---------+------------------------------------------------+
| textPadding    | int     | 8          |         | Horizontal space between tab text and tab      |
|                |         |            |         | edge, in pixels (per side).                    |
+----------------+---------+------------+---------+------------------------------------------------+
| padding        | numbers | 4, 8, 4, 8 |         | Component padding: space between bar edges and |
|                |         |            |         | tab content. CSS convention: { top, right,     |
|                |         |            |         | bottom, left }.                                |
+----------------+---------+------------+---------+------------------------------------------------+
| uppercase      | bool    | true       |         | Always convert tab label to upper-case.        |
+----------------+---------+------------+---------+------------------------------------------------+

## button

Tab button.

+-----------+--------+------------+---------+------------------------------------------------------+
| key       | type   | value      | choices | description                                          |
+===========+========+============+=========+======================================================+
| button    | colour | 0xff001a20 |         | Inactive tab fill (juce::TextButton::                |
|           |        |            |         | buttonColourId).                                     |
+-----------+--------+------------+---------+------------------------------------------------------+
| buttonOn  | colour | 0xff2d3b40 |         | Active tab fill (juce::TextButton::                  |
|           |        |            |         | buttonOnColourId).                                   |
+-----------+--------+------------+---------+------------------------------------------------------+
| textOff   | colour | 0xff33535b |         | Inactive tab text (juce::TextButton::                |
|           |        |            |         | textColourOffId).                                    |
+-----------+--------+------------+---------+------------------------------------------------------+
| textOn    | colour | 0xff00c8d8 |         | Active tab text (juce::TextButton::textColourOnId).  |
+-----------+--------+------------+---------+------------------------------------------------------+

## overlay

+-------------+--------+--------------+---------+--------------------------------------------------+
| key         | type   | value        | choices | description                                      |
+=============+========+==============+=========+==================================================+
| background  | colour | 0xbf090d12   |         | Overlay background (juce::Label::                |
|             |        |              |         | backgroundColourId).                             |
+-------------+--------+--------------+---------+--------------------------------------------------+
| text        | colour | 0xff4e8c93   |         | Overlay text (juce::Label::textColourId).        |
+-------------+--------+--------------+---------+--------------------------------------------------+
| font-family | string | Display Mono |         | Overlay font family (used for status messages).  |
+-------------+--------+--------------+---------+--------------------------------------------------+
| fontSize    | int    | 14           |         | Overlay font size in points.                     |
+-------------+--------+--------------+---------+--------------------------------------------------+

## pane

+----------------------+--------+------------+---------+-------------------------------------------+
| key                  | type   | value      | choices | description                               |
+======================+========+============+=========+===========================================+
| resizeBar            | colour | 0xff2c4144 |         | Pane divider bar (paneBarColourId).       |
+----------------------+--------+------------+---------+-------------------------------------------+
| resizeBarHighlight   | colour | 0xff6b9099 |         | Pane divider bar when dragging or         |
|                      |        |            |         | hovering (paneBarHighlightColourId).      |
+----------------------+--------+------------+---------+-------------------------------------------+
| resizeBarThickness   | int    | 8          |         | Pane divider bar thickness in pixels.     |
+----------------------+--------+------------+---------+-------------------------------------------+
| outline              | colour | 0x00000000 |         | Pane outline stroke (jam::PaneComponent:: |
|                      |        |            |         | outlineColourId).                         |
+----------------------+--------+------------+---------+-------------------------------------------+
| focusedOutline       | colour | 0xff33535b |         | Focused pane outline stroke (jam::        |
|                      |        |            |         | PaneComponent::focusedOutlineColourId).   |
+----------------------+--------+------------+---------+-------------------------------------------+
| splitLine            | string | bracket    | solid,  | Split preview overlay axis line style.    |
|                      |        |            | dash,   |                                           |
|                      |        |            | bracket |                                           |
+----------------------+--------+------------+---------+-------------------------------------------+

## menu

+-----------+--------+------------+---------+------------------------------------------------------+
| key       | type   | value      | choices | description                                          |
+===========+========+============+=========+======================================================+
| opacity   | float  | 0.65       |         | Popup menu background opacity (0.0 - 1.0).           |
+-----------+--------+------------+---------+------------------------------------------------------+
| text      | colour | 0xffa1d6e5 |         | Popup menu text (juce::PopupMenu::textColourId,      |
|           |        |            |         | highlightedTextColourId).                            |
+-----------+--------+------------+---------+------------------------------------------------------+
| highlight | colour | 0xff6b9099 |         | Popup menu highlighted row background (juce::        |
|           |        |            |         | PopupMenu::highlightedBackgroundColourId).           |
+-----------+--------+------------+---------+------------------------------------------------------+
