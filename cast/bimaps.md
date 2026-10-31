## FileConfig

```
@brief Config-file section registry.

Resolves each key to its own Identifier stem naming an on-disk config
section file; consumer-side helpers derive the full filename and directory
path. The default row is display.
```

+---------+-----+
| name    | key |
+=========+=====+
| display | 0   |
+---------+-----+
| keys    | 1   |
+---------+-----+

## FileThemes

```
@brief Theme-file registry.

Resolves each key to its own Identifier stem naming an on-disk theme file;
consumer-side helpers derive the full filename and directory path. The
default row is theme.
```

+-------+-----+
| name  | key |
+=======+=====+
| theme | 0   |
+-------+-----+

## FileFlex

```
@brief Theme SVG graphics-asset registry.

Resolves each key to its own Identifier stem naming an on-disk SVG file;
consumer-side helpers derive the full filename. The default row is tabBar.
```

+-------------------+-----+----------------+
| name              | key | value          |
+===================+=====+================+
| tabBar            | 0   | `tabBar`       |
+-------------------+-----+----------------+
| tabHighlight      | 1   | `tabHighlight` |
+-------------------+-----+----------------+
| tabButtonNormalOn | 2   | `normalOn`     |
+-------------------+-----+----------------+
| resizerBar        | 3   | `resizerBar`   |
+-------------------+-----+----------------+

## CommandId

```
@brief END command-id to command-name map.

Maps each command-id ordinal to the command name a config `keys` row uses.
The key is the juce::CommandID; the value is the row key.
```

+------------------+-----------+--------------------+
| name             | key       | value              |
+==================+===========+====================+
| quit             | 0x4700001 | `quit`             |
| closePane        | 0x4700002 | `closePane`        |
| reload           | 0x4700003 | `reload`           |
| zoomIn           | 0x4700004 | `zoomIn`           |
| zoomOut          | 0x4700005 | `zoomOut`          |
| zoomReset        | 0x4700006 | `zoomReset`        |
| newWindow        | 0x4700007 | `newWindow`        |
| reducePaneWidth  | 0x4700008 | `reducePaneWidth`  |
| reducePaneHeight | 0x4700009 | `reducePaneHeight` |
| expandPaneWidth  | 0x470000a | `expandPaneWidth`  |
| expandPaneHeight | 0x470000b | `expandPaneHeight` |
| newTab           | 0x470000c | `newTab`           |
| prevTab          | 0x470000d | `prevTab`          |
| nextTab          | 0x470000e | `nextTab`          |
| renameTab        | 0x470000f | `renameTab`        |
| splitVertical    | 0x4700010 | `splitVertical`    |
| splitHorizontal  | 0x4700011 | `splitHorizontal`  |
| paneLeft         | 0x4700012 | `paneLeft`         |
| paneDown         | 0x4700013 | `paneDown`         |
| paneUp           | 0x4700014 | `paneUp`           |
| paneRight        | 0x4700015 | `paneRight`        |
| joinLeft         | 0x4700016 | `joinLeft`         |
| joinDown         | 0x4700017 | `joinDown`         |
| joinUp           | 0x4700018 | `joinUp`           |
| joinRight        | 0x4700019 | `joinRight`        |
| swapLeft         | 0x470001a | `swapLeft`         |
| swapDown         | 0x470001b | `swapDown`         |
| swapUp           | 0x470001c | `swapUp`           |
| swapRight        | 0x470001d | `swapRight`        |
| actionList       | 0x470001e | `actionList`       |
+------------------+-----------+--------------------+
