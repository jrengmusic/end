This file is auto-generated with default values on first launch. Edit any
value below to customise your key bindings. Invalid or missing values fall
back to defaults and are reported in the message overlay with file:line.
Reload with Cmd+R (no restart needed).

Key binding format: "modifier+key" (e.g. "cmd+c", "ctrl+shift+t").
Modifiers: cmd, ctrl, alt, shift. Special keys: return, escape, space, tab,
backspace, delete, pageup, pagedown, home, end, f1-f12.

Direct bindings fire immediately (e.g. "cmd+c" for copy). Modal bindings
require the prefix key first, then the action key within the timeout
window (e.g. press \` then \\ to split).

## keys

+--------------------+--------+-----------+---------+----------------------------------------------+
| key                | type   | value     | choices | description                                  |
+====================+========+===========+=========+==============================================+
| prefix             | string | \`        |         | The prefix key activates modal mode. Press   |
|                    |        |           |         | it, then press a modal action key within the |
|                    |        |           |         | timeout. Set to "" to disable modal mode     |
|                    |        |           |         | entirely.                                    |
+--------------------+--------+-----------+---------+----------------------------------------------+
| prefixTimeout      | int    | 1000      |         | How long to wait (ms) for a modal key after  |
|                    |        |           |         | pressing the prefix key.                     |
+--------------------+--------+-----------+---------+----------------------------------------------+
| quit               | string | cmd+q     |         | Quit application.                            |
+--------------------+--------+-----------+---------+----------------------------------------------+
| closePane          | string | cmd+w     |         | Close active pane, then tab, then window.    |
+--------------------+--------+-----------+---------+----------------------------------------------+
| reload             | string | cmd+r     |         | Reload all configuration files.              |
+--------------------+--------+-----------+---------+----------------------------------------------+
| zoomIn             | string | cmd+=     |         | Increase font size.                          |
+--------------------+--------+-----------+---------+----------------------------------------------+
| zoomOut            | string | cmd+-     |         | Decrease font size.                          |
+--------------------+--------+-----------+---------+----------------------------------------------+
| zoomReset          | string | cmd+0     |         | Reset font size to configured default.       |
+--------------------+--------+-----------+---------+----------------------------------------------+
| newWindow          | string | cmd+n     |         | Open a new window.                           |
+--------------------+--------+-----------+---------+----------------------------------------------+
| reducePaneWidth    | string | cmd+alt+h |         | Reduce the focused pane's width by paneStep  |
|                    |        |           |         | (display.md).                                |
+--------------------+--------+-----------+---------+----------------------------------------------+
| reducePaneHeight   | string | cmd+alt+j |         | Reduce the focused pane's height by          |
|                    |        |           |         | paneStep (display.md).                       |
+--------------------+--------+-----------+---------+----------------------------------------------+
| expandPaneWidth    | string | cmd+alt+l |         | Expand the focused pane's width by paneStep  |
|                    |        |           |         | (display.md).                                |
+--------------------+--------+-----------+---------+----------------------------------------------+
| expandPaneHeight   | string | cmd+alt+k |         | Expand the focused pane's height by          |
|                    |        |           |         | paneStep (display.md).                       |
+--------------------+--------+-----------+---------+----------------------------------------------+
| newTab             | string | cmd+t     |         | Open a new tab.                              |
+--------------------+--------+-----------+---------+----------------------------------------------+
| prevTab            | string | cmd+[     |         | Switch to previous tab.                      |
+--------------------+--------+-----------+---------+----------------------------------------------+
| nextTab            | string | cmd+]     |         | Switch to next tab.                          |
+--------------------+--------+-----------+---------+----------------------------------------------+
| renameTab          | string | shift+t   |         | Rename the active tab. Press prefix first.   |
+--------------------+--------+-----------+---------+----------------------------------------------+
| splitVertical      | string | \\        |         | Split pane vertically (side-by-side          |
|                    |        |           |         | columns). Press prefix first.                |
+--------------------+--------+-----------+---------+----------------------------------------------+
| splitHorizontal    | string | \-        |         | Split pane horizontally (stacked rows).      |
|                    |        |           |         | Press prefix first.                          |
+--------------------+--------+-----------+---------+----------------------------------------------+
| paneLeft           | string | h         |         | Focus pane to the left. Press prefix first.  |
+--------------------+--------+-----------+---------+----------------------------------------------+
| paneDown           | string | j         |         | Focus pane below. Press prefix first.        |
+--------------------+--------+-----------+---------+----------------------------------------------+
| paneUp             | string | k         |         | Focus pane above. Press prefix first.        |
+--------------------+--------+-----------+---------+----------------------------------------------+
| paneRight          | string | l         |         | Focus pane to the right. Press prefix first. |
+--------------------+--------+-----------+---------+----------------------------------------------+
| joinLeft           | string | ctrl+h    |         | Absorb the pane to the left into the focused |
|                    |        |           |         | pane.                                        |
+--------------------+--------+-----------+---------+----------------------------------------------+
| joinDown           | string | ctrl+j    |         | Absorb the pane below into the focused pane. |
+--------------------+--------+-----------+---------+----------------------------------------------+
| joinUp             | string | ctrl+k    |         | Absorb the pane above into the focused pane. |
+--------------------+--------+-----------+---------+----------------------------------------------+
| joinRight          | string | ctrl+l    |         | Absorb the pane to the right into the        |
|                    |        |           |         | focused pane.                                |
+--------------------+--------+-----------+---------+----------------------------------------------+
| swapLeft           | string | shift+h   |         | Swap the focused pane with the pane to the   |
|                    |        |           |         | left. Press prefix first.                    |
+--------------------+--------+-----------+---------+----------------------------------------------+
| swapDown           | string | shift+j   |         | Swap the focused pane with the pane below.   |
|                    |        |           |         | Press prefix first.                          |
+--------------------+--------+-----------+---------+----------------------------------------------+
| swapUp             | string | shift+k   |         | Swap the focused pane with the pane above.   |
|                    |        |           |         | Press prefix first.                          |
+--------------------+--------+-----------+---------+----------------------------------------------+
| swapRight          | string | shift+l   |         | Swap the focused pane with the pane to the   |
|                    |        |           |         | right. Press prefix first.                   |
+--------------------+--------+-----------+---------+----------------------------------------------+
| actionList         | string | ?         |         | Open the action list (command palette).      |
|                    |        |           |         | Press prefix first.                          |
+--------------------+--------+-----------+---------+----------------------------------------------+
