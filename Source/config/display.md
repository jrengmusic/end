This file is auto-generated with default values on first launch. Edit any
value below to customise your terminal. Invalid or missing values fall back
to defaults and are reported in the message overlay with file:line. Reload
with Cmd+R (no restart needed).

## display

Active theme directory name and window chrome and lifecycle toggles.

+----------------------+---------+----------+---------+--------------------------------------------+
| key                  | type    | value    | choices | description                                |
+======================+=========+==========+=========+============================================+
| theme                | string  | gfx      |         | Active theme directory name. Themes live   |
|                      |         |          |         | in ~/.config/end/themes/\<name\>/theme.md. |
|                      |         |          |         | Changing this value reloads all visual     |
|                      |         |          |         | properties (colours, fonts, metrics, SVGs) |
|                      |         |          |         | from the named theme directory.            |
+----------------------+---------+----------+---------+--------------------------------------------+
| size                 | numbers | 640, 480 |         | Initial window size in pixels {width,      |
|                      |         |          |         | height}.                                   |
+----------------------+---------+----------+---------+--------------------------------------------+
| zoom_step            | float   | 0.1      |         | Zoom step applied by the zoom_in/zoom_out  |
|                      |         |          |         | actions (keys.md). Added to (zoom_in) or   |
|                      |         |          |         | subtracted from (zoom_out) the focused     |
|                      |         |          |         | pane's current zoom factor, clamped to     |
|                      |         |          |         | [0.25, 4.0]. zoom_reset always sets zoom   |
|                      |         |          |         | to 1.0 directly, ignoring this step.       |
+----------------------+---------+----------+---------+--------------------------------------------+
| pane_step            | float   | 0.05     |         | Proportion step applied by the             |
|                      |         |          |         | reduce_pane_width/reduce_pane_height/      |
|                      |         |          |         | expand_pane_width/expand_pane_height       |
|                      |         |          |         | actions (keys.md) to the focused pane's    |
|                      |         |          |         | width or height.                           |
+----------------------+---------+----------+---------+--------------------------------------------+
| always_on_top        | bool    | true     |         | Keep window above all other windows.       |
+----------------------+---------+----------+---------+--------------------------------------------+
| title_bar_buttons    | bool    | false    |         | Show native title bar buttons (close /     |
|                      |         |          |         | minimise / maximise). macOS: hides/shows   |
|                      |         |          |         | traffic-light buttons and the title bar    |
|                      |         |          |         | together. Windows: toggles the native      |
|                      |         |          |         | title bar. Close/min/max buttons are fixed |
|                      |         |          |         | at construction and cannot be added/       |
|                      |         |          |         | removed at runtime.                        |
+----------------------+---------+----------+---------+--------------------------------------------+
| success_message      | string  | RELOAD   |         | Message shown briefly after a successful   |
|                      |         |          |         | config reload (Cmd+R).                     |
+----------------------+---------+----------+---------+--------------------------------------------+
| gpu                  | bool    | true     |         | Enable GPU-accelerated rendering. When     |
|                      |         |          |         | true, use GPU if available with CPU        |
|                      |         |          |         | fallback. When false, force CPU rendering  |
|                      |         |          |         | (no blur, no transparency).                |
+----------------------+---------+----------+---------+--------------------------------------------+

## graphics

Shader/graphics projects live in ~/.config/end/shaders/\<name\>/. Files:
Common, Image, BufferA, BufferB, BufferC, BufferD (Shadertoy convention).

+----------------------------+--------+----------+------------+------------------------------------+
| key                        | type   | value    | choices    | description                        |
+============================+========+==========+============+====================================+
| background                 | string |          |            | Background shader project          |
|                            |        |          |            | directory name. Empty string       |
|                            |        |          |            | disables.                          |
+----------------------------+--------+----------+------------+------------------------------------+
| background_opacity         | float  | 0.5      |            | Background shader opacity (0.0 =   |
|                            |        |          |            | transparent, 1.0 = opaque).        |
+----------------------------+--------+----------+------------+------------------------------------+
| frame_rate                 | int    | 30       |            | Shader frame rate (1-120).         |
|                            |        |          |            | Controls how many times per second |
|                            |        |          |            | shader passes execute. Lower       |
|                            |        |          |            | values reduce GPU load.            |
+----------------------------+--------+----------+------------+------------------------------------+
| background_resolution      | float  | 0.5      |            | Background shader resolution       |
|                            |        |          |            | (0.0-1.0). Fraction of screen      |
|                            |        |          |            | resolution at which the background |
|                            |        |          |            | shader's intermediate passes       |
|                            |        |          |            | (BufferA-D, Image) render before   |
|                            |        |          |            | upscale. Component/scene painting  |
|                            |        |          |            | is always full resolution — this   |
|                            |        |          |            | only scales the shader's own       |
|                            |        |          |            | offscreen buffers.                 |
+----------------------------+--------+----------+------------+------------------------------------+
| filter                     | string | linear   | linear,    | Texture filter mode for shader     |
|                            |        |          | nearest    | upscaling: "linear" (bilinear) or  |
|                            |        |          |            | "nearest" (pixel-sharp). Applies   |
|                            |        |          |            | to both the background and         |
|                            |        |          |            | post-processing upscale.           |
+----------------------------+--------+----------+------------+------------------------------------+
| post_processing            | string |          |            | Post-processing shader project     |
|                            |        |          |            | directory name. Empty string       |
|                            |        |          |            | disables.                          |
+----------------------------+--------+----------+------------+------------------------------------+
| post_processing_opacity    | float  | 1.0      |            | Post-processing effect intensity   |
|                            |        |          |            | (0.0 = original scene, 1.0 = fully |
|                            |        |          |            | processed).                        |
+----------------------------+--------+----------+------------+------------------------------------+
| post_processing_resolution | float  | 0.5      |            | Post-processing shader resolution  |
|                            |        |          |            | (0.0-1.0). Fraction of screen      |
|                            |        |          |            | resolution at which the            |
|                            |        |          |            | post-processing shader's           |
|                            |        |          |            | intermediate passes (BufferA-D,    |
|                            |        |          |            | Image) render before upscale.      |
|                            |        |          |            | Component/scene painting is always |
|                            |        |          |            | full resolution — this only scales |
|                            |        |          |            | the shader's own offscreen         |
|                            |        |          |            | buffers.                           |
+----------------------------+--------+----------+------------+------------------------------------+
| font_rasterizer            | string | freetype | edgeTable, | Glyph atlas mono rasterization     |
|                            |        |          | freetype,  | backend. "edgeTable" - unhinted    |
|                            |        |          | native     | juce::Typeface coverage            |
|                            |        |          |            | rasterization. "freetype" -        |
|                            |        |          |            | unhinted FreeType rasterization    |
|                            |        |          |            | (default), no autofitter, no stem  |
|                            |        |          |            | darkening. "native" - OS-native    |
|                            |        |          |            | font-smoothing (CoreText on macOS, |
|                            |        |          |            | DirectWrite on Windows).           |
+----------------------------+--------+----------+------------+------------------------------------+
| font_gamma                 | float  | 2.2      |            | Coverage LUT gamma exponent        |
|                            |        |          |            | applied to every rasterized mono   |
|                            |        |          |            | glyph byte. 2.2 is the sRGB        |
|                            |        |          |            | standard transfer-function         |
|                            |        |          |            | exponent - coverage boost pow(x,   |
|                            |        |          |            | 1/2.2) approximates linear-space   |
|                            |        |          |            | compositing for light-on-dark      |
|                            |        |          |            | text, correcting for sRGB-space    |
|                            |        |          |            | blending under-weighting partial   |
|                            |        |          |            | coverage.                          |
+----------------------------+--------+----------+------------+------------------------------------+
| font_contrast              | float  | 0.0      |            | Coverage LUT contrast applied      |
|                            |        |          |            | alongside font_gamma (0.0 = no     |
|                            |        |          |            | synthetic darkening). FreeType's   |
|                            |        |          |            | own autofitter and native          |
|                            |        |          |            | font-smoothing already bring their |
|                            |        |          |            | own boldening; this stays 0.0 by   |
|                            |        |          |            | default.                           |
+----------------------------+--------+----------+------------+------------------------------------+

## mouse

Mouse interaction: the Shadertoy iMouse uniform and, for shaders declaring
a mesh, camera orbit/reset/zoom. Button values: "left", "middle", "right",
or "none" (disables that one binding). The same button may be assigned to
more than one field.

+---------+--------+--------+---------+------------------------------------------------------------+
| key     | type   | value  | choices | description                                                |
+=========+========+========+=========+============================================================+
| enabled | bool   | true   |         | Master switch. false disables iMouse (always reads zero)   |
|         |        |        |         | and every camera interaction below (orbit/reset/zoom),     |
|         |        |        |         | regardless of the individual button choices.               |
+---------+--------+--------+---------+------------------------------------------------------------+
| imouse  | string | left   | left,   | Button that feeds the Shadertoy iMouse uniform.            |
|         |        |        | middle, |                                                            |
|         |        |        | right,  |                                                            |
|         |        |        | none    |                                                            |
+---------+--------+--------+---------+------------------------------------------------------------+
| orbit   | string | middle | left,   | Drag this button to orbit the camera (mesh-declaring       |
|         |        |        | middle, | shaders only).                                             |
|         |        |        | right,  |                                                            |
|         |        |        | none    |                                                            |
+---------+--------+--------+---------+------------------------------------------------------------+
| reset   | string | middle | left,   | Click (press+release, no drag) this button to reset the    |
|         |        |        | middle, | camera to its defaults.                                    |
|         |        |        | right,  |                                                            |
|         |        |        | none    |                                                            |
+---------+--------+--------+---------+------------------------------------------------------------+
