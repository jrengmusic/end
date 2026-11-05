---
version: 0.0.1
---

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
send it to the pane. Any other key in the layer is dropped. sequence timeout (ms)
ends the layer.

## keys

+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| key                | type   | value     | layer  | choices | description                                  |
+====================+========+===========+========+=========+==============================================+
| quit               | string | cmd+q     |        |         | Quit application.                            |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| close pane         | string | cmd+w     |        |         | Close active pane, then tab, then window.    |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| reload             | string | cmd+r     |        |         | Reload all configuration files.              |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| zoom in            | string | cmd+=     |        |         | Zoom in the panes (exclusive zoom,           |
|                    |        |           |        |         | display.md).                                 |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| zoom out           | string | cmd+-     |        |         | Zoom out the panes (exclusive zoom,          |
|                    |        |           |        |         | display.md).                                 |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| zoom reset         | string | cmd+0     |        |         | Reset the zoom of the panes (exclusive zoom, |
|                    |        |           |        |         | display.md).                                 |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| reduce pane width  | string | cmd+alt+h |        |         | Reduce the focused pane's width by pane step |
|                    |        |           |        |         | (display.md).                                |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| reduce pane height | string | cmd+alt+j |        |         | Reduce the focused pane's height by pane     |
|                    |        |           |        |         | step (display.md).                           |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| expand pane width  | string | cmd+alt+l |        |         | Expand the focused pane's width by pane step |
|                    |        |           |        |         | (display.md).                                |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| expand pane height | string | cmd+alt+k |        |         | Expand the focused pane's height by pane     |
|                    |        |           |        |         | step (display.md).                           |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| new tab            | string | cmd+t     |        |         | Open a new tab.                              |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| prev tab           | string | cmd+[     |        |         | Switch to previous tab.                      |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| next tab           | string | cmd+]     |        |         | Switch to next tab.                          |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| split vertical     | string | \\        | prefix |         | Split pane vertically (side-by-side          |
|                    |        |           |        |         | columns).                                    |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| split horizontal   | string | \-        | prefix |         | Split pane horizontally (stacked rows).      |
|                    |        |           |        |         |                                              |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| pane left          | string | h         | prefix |         | Focus pane to the left.                      |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| pane down          | string | j         | prefix |         | Focus pane below.                            |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| pane up            | string | k         | prefix |         | Focus pane above.                            |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| pane right         | string | l         | prefix |         | Focus pane to the right.                     |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| join left          | string | ctrl+h    |        |         | Absorb the pane to the left into the focused |
|                    |        |           |        |         | pane.                                        |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| join down          | string | ctrl+j    |        |         | Absorb the pane below into the focused pane. |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| join up            | string | ctrl+k    |        |         | Absorb the pane above into the focused pane. |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| join right         | string | ctrl+l    |        |         | Absorb the pane to the right into the        |
|                    |        |           |        |         | focused pane.                                |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| swap left          | string | H         | prefix |         | Swap the focused pane with the pane to the   |
|                    |        |           |        |         | left.                                        |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| swap down          | string | J         | prefix |         | Swap the focused pane with the pane below.   |
|                    |        |           |        |         |                                              |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| swap up            | string | K         | prefix |         | Swap the focused pane with the pane above.   |
|                    |        |           |        |         |                                              |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| swap right         | string | L         | prefix |         | Swap the focused pane with the pane to the   |
|                    |        |           |        |         | right.                                       |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| normal             | string | ]         | prefix |         | Enter normal mode in the focused pane.       |
|                    |        |           |        |         |                                              |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| open               | string | o         | prefix |         | Enter open mode in the focused pane.         |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| list               | string | f         | prefix |         | Enter list mode in the focused pane.         |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| prefix             | string | \`        |        |         | Enter the prefix layer. Press it twice to    |
|                    |        |           |        |         | send it to the pane.                         |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
| sequence timeout   | int    | 1000      |        |         | Time in ms to wait for the next key of a     |
|                    |        |           |        |         | sequence or a layer.                         |
+--------------------+--------+-----------+--------+---------+----------------------------------------------+
