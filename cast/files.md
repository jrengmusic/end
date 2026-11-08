## index

+---------+------------------+
| alias   | symbol           |
+=========+==================+
| @id     | juce::Identifier |
+---------+------------------+
| @string | juce::String     |
+---------+------------------+

## files

```
@brief Product asset file names — pane split/join corner-menu icons, the plugin offload stamp, and the plugin file extensions.

Each constant is the literal name of an embedded SVG resource, of the offload
stamp in the plugins folder, of the settings file, or of a file extension.
```

+---------+------------------------------+---------+----------------------------------+-----------+------------------------------------------------------+
| type    | name                         | format  | value                            | format    | description                                          |
+=========+==============================+=========+==================================+===========+======================================================+
| @string | split vertical normal        | toCamel | split_vertical_normal.svg        | toLiteral | Split-vertical corner-menu icon.                     |
+---------+------------------------------+---------+----------------------------------+-----------+------------------------------------------------------+
| @string | split horizontal normal      | toCamel | split_horizontal_normal.svg      | toLiteral | Split-horizontal corner-menu icon.                   |
+---------+------------------------------+---------+----------------------------------+-----------+------------------------------------------------------+
| @string | join cells vertical normal   | toCamel | join_cells_vertical_normal.svg   | toLiteral | Join-cells-vertical corner-menu icon.                |
+---------+------------------------------+---------+----------------------------------+-----------+------------------------------------------------------+
| @string | join cells horizontal normal | toCamel | join_cells_horizontal_normal.svg | toLiteral | Join-cells-horizontal corner-menu icon.              |
+---------+------------------------------+---------+----------------------------------+-----------+------------------------------------------------------+
| @string | plugins stamp                | toCamel | .plugins-stamp                   | toLiteral | Hash stamp of the plugin archives last offloaded.    |
+---------+------------------------------+---------+----------------------------------+-----------+------------------------------------------------------+
| @string | archive extension            | toCamel | zip                              | toLiteral | Extension of the plugin archives in the binary data. |
+---------+------------------------------+---------+----------------------------------+-----------+------------------------------------------------------+
| @string | audio unit extension         | toCamel | component                        | toLiteral | Extension of an Audio Unit bundle.                   |
+---------+------------------------------+---------+----------------------------------+-----------+------------------------------------------------------+
| @string | settings file name           | toCamel | END.settings                     | toLiteral | Window state file of END in the config directory.    |
+---------+------------------------------+---------+----------------------------------+-----------+------------------------------------------------------+
