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

+-------------------+-----+-----------------------+
| name              | key | value                 |
+===================+=====+=======================+
| tabBar            | 0   | `tab_bar`             |
+-------------------+-----+-----------------------+
| tabHighlight      | 1   | `tab_highlight`       |
+-------------------+-----+-----------------------+
| tabButtonNormalOn | 2   | `tab_button_normalOn` |
+-------------------+-----+-----------------------+
| resizerBar        | 3   | `resizer_bar`         |
+-------------------+-----+-----------------------+
