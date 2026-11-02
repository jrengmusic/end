---
version: 0.0.1
---

This file defines the visual appearance for the END default "gfx" theme. Each section maps 1:1 to a
component scope. A colour cell holds a key of the colours table (words), a jam colour name, or a direct
0xAARRGGBB hex integer (Alpha|Red|Green|Blue). An empty dark cell means the same as light. END reads the
light column or the dark column according to the appearance row of display.md.

## colours

+--------------------+--------+------------+-------------------------------------------------+
| key                | type   | value      | description                                     |
+====================+========+============+=================================================+
| aztec              | colour | 0xff273233 | jam colour name.                                |
+--------------------+--------+------------+-------------------------------------------------+
| blue bikini        | colour | 0xff00c8d8 | JFS palette colour.                             |
+--------------------+--------+------------+-------------------------------------------------+
| cavolo nero        | colour | 0xff6b9099 | jam colour name.                                |
+--------------------+--------+------------+-------------------------------------------------+
| continental waters | colour | 0xff8fc6d0 | jam colour name.                                |
+--------------------+--------+------------+-------------------------------------------------+
| crystal            | colour | 0xffa1d6e5 | jam colour name.                                |
+--------------------+--------+------------+-------------------------------------------------+
| little mermaid     | colour | 0xff2c4144 | JFS palette colour.                             |
+--------------------+--------+------------+-------------------------------------------------+
| mediterranea       | colour | 0xff33535b | JFS palette colour.                             |
+--------------------+--------+------------+-------------------------------------------------+
| paradiso           | colour | 0xff4e8c93 | JFS palette colour.                             |
+--------------------+--------+------------+-------------------------------------------------+
| void               | colour | 0xff001a20 | jam colour name.                                |
+--------------------+--------+------------+-------------------------------------------------+
| vulcan             | colour | 0xff2d3b40 | jam colour name.                                |
+--------------------+--------+------------+-------------------------------------------------+

## window

+-------------+--------+------------+------+---------+-------------------------------------------------+
| key         | type   | light      | dark | choices | description                                     |
+=============+========+============+======+=========+=================================================+
| background  | colour | 0xbf090d12 |      |         | Window background tint. Alpha controls window   |
|             |        |            |      |         | transparency (glass mode).                      |
+-------------+--------+------------+------+---------+-------------------------------------------------+
| blur radius | int    | 32         |      |         | Background blur radius in pixels (0 = no blur). |
|             |        |            |      |         | GPU only.                                       |
+-------------+--------+------------+------+---------+-------------------------------------------------+

## style

Window visual effect style per platform — see the choices column for accepted values.

+-----+--------+----------------+------+-------------------------------------------+-------------+
| key | type   | light          | dark | choices                                   | description |
+=====+========+================+======+===========================================+=============+
| mac | string | backgroundBlur |      | backgroundBlur, visualFXWindowBackground, | macOS       |
|     |        |                |      | glassFXRegular, glassFXClear              | window      |
|     |        |                |      |                                           | visual      |
|     |        |                |      |                                           | effect      |
|     |        |                |      |                                           | style.      |
+-----+--------+----------------+------+-------------------------------------------+-------------+
| win | string | blurBehind     |      | blurBehind, acrylic10, acrylic11, mica    | Windows     |
|     |        |                |      |                                           | window      |
|     |        |                |      |                                           | visual      |
|     |        |                |      |                                           | effect      |
|     |        |                |      |                                           | style.      |
+-----+--------+----------------+------+-------------------------------------------+-------------+

## font

+----------+------+-------+------+---------+-----------------------------------------+
| key      | type | light | dark | choices | description                             |
+==========+======+=======+======+=========+=========================================+
| embolden | bool | false |      |         | Make text appear bolder. Useful for     |
|          |      |       |      |         | thin fonts that are hard to read at     |
|          |      |       |      |         | small sizes.                            |
+----------+------+-------+------+---------+-----------------------------------------+

## scrollbar

+-------+--------+------------+------+---------+---------------------------------------------------+
| key   | type   | light      | dark | choices | description                                       |
+=======+========+============+======+=========+===================================================+
| thumb | colour | 0x802c4144 |      |         | Scrollbar thumb (juce::ScrollBar::thumbColourId). |
|       |        |            |      |         | Semi-transparent recommended.                     |
+-------+--------+------------+------+---------+---------------------------------------------------+
| track | colour | 0x00000000 |      |         | Scrollbar track (juce::ScrollBar::trackColourId). |
|       |        |            |      |         | Transparent for no visible track.                 |
+-------+--------+------------+------+---------+---------------------------------------------------+

## tab

Tab bar.

+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| key               | type    | light              | dark | choices | description                                    |
+===================+=========+====================+======+=========+================================================+
| position          | string  | top                |      | top,    | Tab bar position.                              |
|                   |         |                    |      | bottom, |                                                |
|                   |         |                    |      | left,   |                                                |
|                   |         |                    |      | right   |                                                |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| always visible    | bool    | true               |      |         | Tab bar visibility when only single tab        |
|                   |         |                    |      |         | opened.                                        |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| background        | colour  | continental waters |      |         | Bar strip background (jam::button::Bar::       |
|                   |         |                    |      |         | backgroundColourId).                           |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| highlight         | colour  | cavolo nero        |      |         | Sliding selection highlight (jam::button::     |
|                   |         |                    |      |         | Bar::highlightColourId).                       |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| outline           | colour  | aztec              |      |         | SVG group outline (jam::button::Bar::          |
|                   |         |                    |      |         | outlineColourId).                              |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| caret             | colour  | paradiso           |      |         | Caret (jam::CaretComponent::caretColourId).    |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| editor background | colour  | 0x00000000         |      |         | Editor widget background fill (juce::          |
|                   |         |                    |      |         | TextEditor::backgroundColourId). Transparent   |
|                   |         |                    |      |         | lets the window glass effect show through.     |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| editor outline    | colour  | 0x00000000         |      |         | Editor widget outline (juce::TextEditor::      |
|                   |         |                    |      |         | outlineColourId).                              |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| font family       | string  | Display            |      |         | Tab bar font family.                           |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| font size         | int     | 12                 |      |         | Tab bar font size in points.                   |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| kerning factor    | float   | 0.075              |      |         | Extra spacing between tab label characters, as |
|                   |         |                    |      |         | a fraction of the font size (0.0 = font        |
|                   |         |                    |      |         | default).                                      |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| depth             | float   | 3.0                |      |         | Tab bar height as a multiple of the tab font   |
|                   |         |                    |      |         | height.                                        |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| text padding      | int     | 8                  |      |         | Horizontal space between tab text and tab      |
|                   |         |                    |      |         | edge, in pixels (per side).                    |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| padding           | numbers | 4, 8, 4, 8         |      |         | Component padding: space between bar edges and |
|                   |         |                    |      |         | tab content. CSS convention: { top, right,     |
|                   |         |                    |      |         | bottom, left }.                                |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+
| uppercase         | bool    | true               |      |         | Always convert tab label to upper-case.        |
+-------------------+---------+--------------------+------+---------+------------------------------------------------+

## button

Tab button.

+-----------+--------+--------------+------+---------+-----------------------------------------------------+
| key       | type   | light        | dark | choices | description                                         |
+===========+========+==============+======+=========+=====================================================+
| button    | colour | void         |      |         | Inactive tab fill (juce::TextButton::               |
|           |        |              |      |         | buttonColourId).                                    |
+-----------+--------+--------------+------+---------+-----------------------------------------------------+
| button on | colour | vulcan       |      |         | Active tab fill (juce::TextButton::                 |
|           |        |              |      |         | buttonOnColourId).                                  |
+-----------+--------+--------------+------+---------+-----------------------------------------------------+
| text off  | colour | mediterranea |      |         | Inactive tab text (juce::TextButton::               |
|           |        |              |      |         | textColourOffId).                                   |
+-----------+--------+--------------+------+---------+-----------------------------------------------------+
| text on   | colour | blue bikini  |      |         | Active tab text (juce::TextButton::textColourOnId). |
+-----------+--------+--------------+------+---------+-----------------------------------------------------+

## overlay

+-------------+--------+--------------+------+---------+-------------------------------------------------+
| key         | type   | light        | dark | choices | description                                     |
+=============+========+==============+======+=========+=================================================+
| background  | colour | 0xbf090d12   |      |         | Overlay background (juce::Label::               |
|             |        |              |      |         | backgroundColourId).                            |
+-------------+--------+--------------+------+---------+-------------------------------------------------+
| text        | colour | paradiso     |      |         | Overlay text (juce::Label::textColourId).       |
+-------------+--------+--------------+------+---------+-------------------------------------------------+
| font family | string | Display Mono |      |         | Overlay font family (used for status messages). |
+-------------+--------+--------------+------+---------+-------------------------------------------------+
| font size   | int    | 14           |      |         | Overlay font size in points.                    |
+-------------+--------+--------------+------+---------+-------------------------------------------------+

## pane

+----------------------+--------+----------------+------+---------+-------------------------------------------+
| key                  | type   | light          | dark | choices | description                               |
+======================+========+================+======+=========+===========================================+
| resize bar           | colour | little mermaid |      |         | Pane divider bar (paneBarColourId).       |
+----------------------+--------+----------------+------+---------+-------------------------------------------+
| resize bar highlight | colour | cavolo nero    |      |         | Pane divider bar when dragging or         |
|                      |        |                |      |         | hovering (paneBarHighlightColourId).      |
+----------------------+--------+----------------+------+---------+-------------------------------------------+
| resize bar thickness | int    | 8              |      |         | Pane divider bar thickness in pixels.     |
+----------------------+--------+----------------+------+---------+-------------------------------------------+
| padding              | int    | 8              |      |         | Space in pixels between the pane bounds   |
|                      |        |                |      |         | and its outline. The hosted editor sits   |
|                      |        |                |      |         | inside the outline.                       |
+----------------------+--------+----------------+------+---------+-------------------------------------------+
| outline              | colour | 0x00000000     |      |         | Pane outline stroke (jam::PaneComponent:: |
|                      |        |                |      |         | outlineColourId).                         |
+----------------------+--------+----------------+------+---------+-------------------------------------------+
| focused outline      | colour | mediterranea   |      |         | Focused pane outline stroke (jam::        |
|                      |        |                |      |         | PaneComponent::focusedOutlineColourId).   |
+----------------------+--------+----------------+------+---------+-------------------------------------------+
| split line           | string | bracket        |      | solid,  | Split preview overlay axis line style.    |
|                      |        |                |      | dash,   |                                           |
|                      |        |                |      | bracket |                                           |
+----------------------+--------+----------------+------+---------+-------------------------------------------+

## menu

+-----------+--------+-------------+------+---------+-------------------------------------------------+
| key       | type   | light       | dark | choices | description                                     |
+===========+========+=============+======+=========+=================================================+
| opacity   | float  | 0.65        |      |         | Popup menu background opacity (0.0 - 1.0).      |
+-----------+--------+-------------+------+---------+-------------------------------------------------+
| text      | colour | crystal     |      |         | Popup menu text (juce::PopupMenu::textColourId, |
|           |        |             |      |         | highlightedTextColourId).                       |
+-----------+--------+-------------+------+---------+-------------------------------------------------+
| highlight | colour | cavolo nero |      |         | Popup menu highlighted row background (juce::   |
|           |        |             |      |         | PopupMenu::highlightedBackgroundColourId).      |
+-----------+--------+-------------+------+---------+-------------------------------------------------+
