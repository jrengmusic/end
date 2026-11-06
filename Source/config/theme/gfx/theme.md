---
version: 0.0.1
---

This file defines the visual appearance for the END default "gfx" theme. Each section maps 1:1 to a
component scope. A colour cell holds a key of the colours table (words), a jam colour name, or a
direct 0xAARRGGBB hex integer (Alpha|Red|Green|Blue). An empty dark cell means the same as light.
END reads the light column or the dark column according to the appearance row of display.md.

## colours

+----------------------+--------+------------+------------------------+
| key                  | type   | value      | description            |
+======================+========+============+========================+
| blue bikini          | colour | 0xff00c8d8 | Bright cyan.           |
+----------------------+--------+------------+------------------------+
| cavolo nero          | colour | 0xff6b9099 | Muted blue grey.       |
+----------------------+--------+------------+------------------------+
| clear black          | colour | 0x00000000 | Transparent black.     |
+----------------------+--------+------------+------------------------+
| crystal              | colour | 0xffa1d6e5 | Pale blue.             |
+----------------------+--------+------------+------------------------+
| glass corbeau        | colour | 0x800d141c | Blue black, 50% alpha. |
+----------------------+--------+------------+------------------------+
| glass little mermaid | colour | 0x802c4144 | Dark teal, 50% alpha.  |
+----------------------+--------+------------+------------------------+
| glass narwhal grey   | colour | 0xbf090d12 | Blue black, 75% alpha. |
+----------------------+--------+------------+------------------------+
| mediterranea         | colour | 0xff33535b | Dark teal.             |
+----------------------+--------+------------+------------------------+
| paradiso             | colour | 0xff4e8c93 | Muted teal.            |
+----------------------+--------+------------+------------------------+
| void                 | colour | 0xff001a20 | Very dark teal.        |
+----------------------+--------+------------+------------------------+
| vulcan               | colour | 0xff2d3b40 | Dark slate.            |
+----------------------+--------+------------+------------------------+

## window

+-------------+--------+--------------------+------+---------+-------------------------------------------------+
| key         | type   | light              | dark | choices | description                                     |
+=============+========+====================+======+=========+=================================================+
| background  | colour | glass narwhal grey |      |         | Window background tint. Alpha controls window   |
|             |        |                    |      |         | transparency (glass mode).                      |
+-------------+--------+--------------------+------+---------+-------------------------------------------------+
| blur radius | int    | 32                 |      |         | Background blur radius in pixels (0 = no blur). |
|             |        |                    |      |         | GPU only.                                       |
+-------------+--------+--------------------+------+---------+-------------------------------------------------+

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

+----------+------+-------+------+---------+-------------------------------------+
| key      | type | light | dark | choices | description                         |
+==========+======+=======+======+=========+=====================================+
| embolden | bool | false |      |         | Make text appear bolder. Useful for |
|          |      |       |      |         | thin fonts that are hard to read at |
|          |      |       |      |         | small sizes.                        |
+----------+------+-------+------+---------+-------------------------------------+

## scrollbar

+-------+--------+----------------------+------+---------+---------------------------------------------------+
| key   | type   | light                | dark | choices | description                                       |
+=======+========+======================+======+=========+===================================================+
| thumb | colour | glass little mermaid |      |         | Scrollbar thumb (juce::ScrollBar::thumbColourId). |
|       |        |                      |      |         | Semi-transparent recommended.                     |
+-------+--------+----------------------+------+---------+---------------------------------------------------+
| track | colour | clear black          |      |         | Scrollbar track (juce::ScrollBar::trackColourId). |
|       |        |                      |      |         | Transparent for no visible track.                 |
+-------+--------+----------------------+------+---------+---------------------------------------------------+

## tab

Tab bar.

+----------------+---------+------------+------+---------+------------------------------------------------+
| key            | type    | light      | dark | choices | description                                    |
+================+=========+============+======+=========+================================================+
| position       | string  | top        |      | top,    | Tab bar position.                              |
|                |         |            |      | bottom, |                                                |
|                |         |            |      | left,   |                                                |
|                |         |            |      | right   |                                                |
+----------------+---------+------------+------+---------+------------------------------------------------+
| always visible | bool    | true       |      |         | Tab bar visibility when only single tab        |
|                |         |            |      |         | opened.                                        |
+----------------+---------+------------+------+---------+------------------------------------------------+
| font family    | string  | Display    |      |         | Tab bar font family.                           |
+----------------+---------+------------+------+---------+------------------------------------------------+
| font size      | int     | 12         |      |         | Tab bar font size in points.                   |
+----------------+---------+------------+------+---------+------------------------------------------------+
| kerning factor | float   | 0.075      |      |         | Extra spacing between tab label characters, as |
|                |         |            |      |         | a fraction of the font size (0.0 = font        |
|                |         |            |      |         | default).                                      |
+----------------+---------+------------+------+---------+------------------------------------------------+
| depth          | float   | 3.0        |      |         | Tab bar height as a multiple of the tab font   |
|                |         |            |      |         | height.                                        |
+----------------+---------+------------+------+---------+------------------------------------------------+
| text padding   | int     | 8          |      |         | Horizontal space between tab text and tab      |
|                |         |            |      |         | edge, in pixels (per side).                    |
+----------------+---------+------------+------+---------+------------------------------------------------+
| padding        | numbers | 4, 8, 4, 8 |      |         | Component padding: space between bar edges and |
|                |         |            |      |         | tab content. CSS convention: { top, right,     |
|                |         |            |      |         | bottom, left }.                                |
+----------------+---------+------------+------+---------+------------------------------------------------+
| uppercase      | bool    | true       |      |         | Always convert tab label to upper-case.        |
+----------------+---------+------------+------+---------+------------------------------------------------+

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

+-------------+--------+--------------------+------+---------+-------------------------------------------------+
| key         | type   | light              | dark | choices | description                                     |
+=============+========+====================+======+=========+=================================================+
| background  | colour | glass narwhal grey |      |         | Overlay background (juce::Label::               |
|             |        |                    |      |         | backgroundColourId).                            |
+-------------+--------+--------------------+------+---------+-------------------------------------------------+
| text        | colour | paradiso           |      |         | Overlay text (juce::Label::textColourId).       |
+-------------+--------+--------------------+------+---------+-------------------------------------------------+
| font family | string | Display Mono       |      |         | Overlay font family (used for status messages). |
+-------------+--------+--------------------+------+---------+-------------------------------------------------+
| font size   | int    | 14                 |      |         | Overlay font size in points.                    |
+-------------+--------+--------------------+------+---------+-------------------------------------------------+

## pane

+----------------------+--------+---------------+------+---------+-------------------------------------------+
| key                  | type   | light         | dark | choices | description                               |
+======================+========+===============+======+=========+===========================================+
| resize bar thickness | int    | 8             |      |         | Pane divider bar thickness in pixels.     |
+----------------------+--------+---------------+------+---------+-------------------------------------------+
| padding              | int    | 8             |      |         | Space in pixels between the pane bounds   |
|                      |        |               |      |         | and its outline. The hosted editor sits   |
|                      |        |               |      |         | inside the outline.                       |
+----------------------+--------+---------------+------+---------+-------------------------------------------+
| outline              | colour | clear black   |      |         | Pane outline stroke (jam::PaneComponent:: |
|                      |        |               |      |         | outlineColourId).                         |
+----------------------+--------+---------------+------+---------+-------------------------------------------+
| focused outline      | colour | clear black   |      |         | Focused pane outline stroke (jam::        |
|                      |        |               |      |         | PaneComponent::focusedOutlineColourId).   |
+----------------------+--------+---------------+------+---------+-------------------------------------------+
| background           | colour | clear black   |      |         | Pane background fill (jam::               |
|                      |        |               |      |         | PaneComponent::backgroundColourId).       |
+----------------------+--------+---------------+------+---------+-------------------------------------------+
| focused background   | colour | glass corbeau |      |         | Focused pane background fill (jam::       |
|                      |        |               |      |         | PaneComponent::                           |
|                      |        |               |      |         | focusedBackgroundColourId).               |
+----------------------+--------+---------------+------+---------+-------------------------------------------+
| split line           | string | bracket       |      | solid,  | Split preview overlay axis line style.    |
|                      |        |               |      | dash,   |                                           |
|                      |        |               |      | bracket |                                           |
+----------------------+--------+---------------+------+---------+-------------------------------------------+

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
