---
version: 0.0.1
---

This file holds the END theme settings that are not a font, a colour or a metric, then the tables built from the theme style sheet. An empty dark cell means the same as light.

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
| uppercase      | bool    | true       |      |         | Always convert tab label to upper-case.        |
+----------------+---------+------------+------+---------+------------------------------------------------+

## pane

+------------+--------+---------+------+---------+-------------------------------------------+
| key        | type   | light   | dark | choices | description                               |
+============+========+=========+======+=========+===========================================+
| split line | string | bracket |      | solid,  | Split preview overlay axis line style.    |
|            |        |         |      | dash,   |                                           |
|            |        |         |      | bracket |                                           |
+------------+--------+---------+------+---------+-------------------------------------------+
