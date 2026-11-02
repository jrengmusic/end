This file is auto-generated with default values on first launch. Edit any
value below to customise your key bindings. Invalid or missing values fall
back to defaults and are reported in the message overlay with file:line.
Reload with Cmd+R (no restart needed).

Key binding format: "modifier+key" (e.g. "cmd+c", "ctrl+shift+t").
Modifiers: cmd, ctrl, alt, shift. Special keys: return, escape, space, tab,
backspace, delete, pageup, pagedown, home, end, f1-f12.

Direct bindings fire immediately (e.g. "cmd+t" for a new tab). A row with a
layer fires only inside that layer. The prefix key (\`) enters the prefix layer,
and the next key runs a row of that layer (\` then \\ splits). Press \` twice to
send it to the pane. Any other key in the layer is dropped. sequenceTimeout (ms)
ends the layer.

## keys

+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| key                | type   | value     | layer  | choices | description                                  |
+====================+========+===========+========+=========+==============================================+
| quit               | string | cmd+q     |        |         | Quit application.                            |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| closePane          | string | cmd+w     |        |         | Close active pane, then tab, then window.    |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| reload             | string | cmd+r     |        |         | Reload all configuration files.              |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| zoomIn             | string | cmd+=     |        |         | Zoom in the panes (exclusiveZoom,            |
|                    |        |           |        |         | display.md).                                 |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| zoomOut            | string | cmd+-     |        |         | Zoom out the panes (exclusiveZoom,           |
|                    |        |           |        |         | display.md).                                 |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| zoomReset          | string | cmd+0     |        |         | Reset the zoom of the panes (exclusiveZoom,  |
|                    |        |           |        |         | display.md).                                 |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| reducePaneWidth    | string | cmd+alt+h |        |         | Reduce the focused pane's width by paneStep  |
|                    |        |           |        |         | (display.md).                                |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| reducePaneHeight   | string | cmd+alt+j |        |         | Reduce the focused pane's height by          |
|                    |        |           |        |         | paneStep (display.md).                       |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| expandPaneWidth    | string | cmd+alt+l |        |         | Expand the focused pane's width by paneStep  |
|                    |        |           |        |         | (display.md).                                |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| expandPaneHeight   | string | cmd+alt+k |        |         | Expand the focused pane's height by          |
|                    |        |           |        |         | paneStep (display.md).                       |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| newTab             | string | cmd+t     |        |         | Open a new tab.                              |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| prevTab            | string | cmd+[     |        |         | Switch to previous tab.                      |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| nextTab            | string | cmd+]     |        |         | Switch to next tab.                          |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| splitVertical      | string | \\        | prefix |         | Split pane vertically (side-by-side          |
|                    |        |           |        |         | columns).                                    |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| splitHorizontal    | string | \-        | prefix |         | Split pane horizontally (stacked rows).      |
|                    |        |           |        |         |                                              |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| paneLeft           | string | h         | prefix |         | Focus pane to the left.                      |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| paneDown           | string | j         | prefix |         | Focus pane below.                            |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| paneUp             | string | k         | prefix |         | Focus pane above.                            |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| paneRight          | string | l         | prefix |         | Focus pane to the right.                     |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| joinLeft           | string | ctrl+h    |        |         | Absorb the pane to the left into the focused |
|                    |        |           |        |         | pane.                                        |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| joinDown           | string | ctrl+j    |        |         | Absorb the pane below into the focused pane. |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| joinUp             | string | ctrl+k    |        |         | Absorb the pane above into the focused pane. |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| joinRight          | string | ctrl+l    |        |         | Absorb the pane to the right into the        |
|                    |        |           |        |         | focused pane.                                |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| swapLeft           | string | H         | prefix |         | Swap the focused pane with the pane to the   |
|                    |        |           |        |         | left.                                        |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| swapDown           | string | J         | prefix |         | Swap the focused pane with the pane below.   |
|                    |        |           |        |         |                                              |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| swapUp             | string | K         | prefix |         | Swap the focused pane with the pane above.   |
|                    |        |           |        |         |                                              |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| swapRight          | string | L         | prefix |         | Swap the focused pane with the pane to the   |
|                    |        |           |        |         | right.                                       |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| normal             | string | ]         | prefix |         | Enter normal mode in the focused pane.       |
|                    |        |           |        |         |                                              |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| prefix             | string | \`        |        |         | Enter the prefix layer. Press it twice to    |
|                    |        |           |        |         | send it to the pane.                         |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| sequenceTimeout    | int    | 1000      |        |         | Time in ms to wait for the next key of a     |
|                    |        |           |        |         | sequence or a layer.                         |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
