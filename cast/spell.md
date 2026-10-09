## index

+--------------------+-------------------------------------+
| alias              | symbol                              |
+====================+=====================================+
| @code              | ../../jam/cast/code.cast            |
+--------------------+-------------------------------------+
| @cmake             | cmake.cast                          |
+--------------------+-------------------------------------+
| @project-info      | ../project-info.md                  |
+--------------------+-------------------------------------+
| @signing           | signing.md                          |
+--------------------+-------------------------------------+
| @identifiers       | identifiers.md                      |
+--------------------+-------------------------------------+
| @bimaps            | bimaps.md                           |
+--------------------+-------------------------------------+
| @files             | files.md                            |
+--------------------+-------------------------------------+
| @source            | source.cast                         |
+--------------------+-------------------------------------+
| @bimap             | jam::Bimap<int>                     |
+--------------------+-------------------------------------+
| @semicolon         | ;                                   |
+--------------------+-------------------------------------+
| @ProjectInfo       | ../Source/generated/ProjectInfo.h   |
+--------------------+-------------------------------------+
| @Identifiers       | ../Source/generated/Identifiers.h   |
+--------------------+-------------------------------------+
| @IdentifiersSource | ../Source/generated/Identifiers.cpp |
+--------------------+-------------------------------------+
| @Bimaps            | ../Source/generated/Bimaps.h        |
+--------------------+-------------------------------------+
| @Files             | ../Source/generated/Files.h         |
+--------------------+-------------------------------------+
| @FilesSource       | ../Source/generated/Files.cpp       |
+--------------------+-------------------------------------+
| @Generated         | ../Source/generated/Generated.h     |
+--------------------+-------------------------------------+
| @CMakeLists        | ../CMakeLists.txt                   |
+--------------------+-------------------------------------+
| @Entitlements      | ../entitlements.plist               |
+--------------------+-------------------------------------+
| @installer         | installer.cast                      |
+--------------------+-------------------------------------+
| @Distribution      | installer/mac/distribution.xml      |
+--------------------+-------------------------------------+
| @Installer         | installer/win/installer.nsi         |
+--------------------+-------------------------------------+
| @Gh                | ../gh.cmake                         |
+--------------------+-------------------------------------+
| @Release           | ../RELEASE.md                       |
+--------------------+-------------------------------------+

## headers

+----------------+--------+------------------------------------------------------------------------------------------+----------------------------------------------------+
| file           | type   | brief                                                                                    | description                                        |
+================+========+==========================================================================================+====================================================+
| ProjectInfo.h  | header | ```                                                                                      |                                                    |
|                |        | @file ProjectInfo.h                                                                      |                                                    |
|                |        | @brief Project metadata — the generated ProjectInfo namespace.                           |                                                    |
|                |        | ```                                                                                      |                                                    |
+----------------+--------+------------------------------------------------------------------------------------------+----------------------------------------------------+
| Identifiers.h  | header | ```                                                                                      |                                                    |
|                |        | @file Identifiers.h                                                                      |                                                    |
|                |        | @brief Identifier and name-string constants for END's own vocabulary.                    |                                                    |
|                |        | ```                                                                                      |                                                    |
+----------------+--------+------------------------------------------------------------------------------------------+----------------------------------------------------+
| Bimaps.h       | header | ```                                                                                      |                                                    |
|                |        | @file Bimaps.h                                                                           |                                                    |
|                |        | @brief Bidirectional name-to-key registries for END's own vocabulary.                    |                                                    |
|                |        | ```                                                                                      |                                                    |
+----------------+--------+------------------------------------------------------------------------------------------+----------------------------------------------------+
| Files.h        | header | ```                                                                                      |                                                    |
|                |        | @file Files.h                                                                            |                                                    |
|                |        | @brief Product asset file names.                                                         |                                                    |
|                |        | ```                                                                                      |                                                    |
+----------------+--------+------------------------------------------------------------------------------------------+----------------------------------------------------+
| Generated.h    | header | ```                                                                                      | One construction point for every generated symbol. |
|                |        | @file Generated.h                                                                        |                                                    |
|                |        | @brief Generated-header umbrella — includes every generated product header.              |                                                    |
|                |        | ```                                                                                      |                                                    |
+----------------+--------+------------------------------------------------------------------------------------------+----------------------------------------------------+
| CMakeLists.txt |        | ```                                                                                      |                                                    |
|                |        | @file CMakeLists.txt                                                                     |                                                    |
|                |        | @brief END build manifest — a self-sufficient JUCE GUI application.                      |                                                    |
|                |        |                                                                                          |                                                    |
|                |        | Generated from project-info.md and cast/signing.md; every value traces to one table row. |                                                    |
|                |        | Edit the table, run cast, then configure.                                                |                                                    |
|                |        | ```                                                                                      |                                                    |
+----------------+--------+------------------------------------------------------------------------------------------+----------------------------------------------------+

## output

+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| list                                          | separator                 | structure                                           | file               |
+===============================================+===========================+=====================================================+====================+
| > - [list]: @project-info:project info        |                           | @code:namespace                                     | @ProjectInfo       |
|                                               |                           | - macro: #pragma once                               |                    |
|                                               |                           | - name: ProjectInfo                                 |                    |
|                                               |                           | - [description]: @headers:brief                     |                    |
|                                               |                           | > - [list]: @code:constant                          |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| - [list]: @identifiers                        |                           | @code:namespace                                     | @Identifiers       |
|                                               |                           | - macro: #pragma once                               |                    |
|                                               |                           | - name: Id                                          |                    |
|                                               |                           | - [description]: @headers:brief                     |                    |
|                                               |                           | - [list]: @code:identifier                          |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| > - [list]: @identifiers                      |                           | @source:source                                      | @IdentifiersSource |
|                                               |                           | - file: Identifiers.h                               |                    |
|                                               |                           |                                                     |                    |
|                                               |                           | @code:namespace                                     |                    |
|                                               |                           | - name: Id                                          |                    |
|                                               |                           | > - [list]: @code:identifier-definition             |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| > - [list]: @files:files                      | - [list]: @code:linebreak | @code:line                                          | @Files             |
|                                               |                           | - macro: #pragma once                               |                    |
|                                               |                           |                                                     |                    |
|                                               |                           | @code:namespace                                     |                    |
|                                               |                           | - name: files                                       |                    |
|                                               |                           | > - [list]: @code:identifier                        |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| > - [list]: @files:files                      | - [list]: @code:linebreak | @source:source                                      | @FilesSource       |
|                                               |                           | - file: Files.h                                     |                    |
|                                               |                           |                                                     |                    |
|                                               |                           | @code:namespace                                     |                    |
|                                               |                           | - name: files                                       |                    |
|                                               |                           | > - [list]: @code:identifier-definition             |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| > > > - [list]: @bimaps:FileConfig            | - [list]: @code:linebreak | @code:namespace                                     | @Bimaps            |
|                                               |                           | - macro: #pragma once                               |                    |
| > > - [list]: @bimaps:FileConfig              |                           | - name: map                                         |                    |
|                                               |                           |                                                     |                    |
|                                               |                           | @code:bimap                                         |                    |
|                                               |                           | - name: FileConfig                                  |                    |
|                                               |                           | - type: map::FileConfig                             |                    |
|                                               |                           | - instance: fileConfig                              |                    |
|                                               |                           | - [description]: Config-file section registry.      |                    |
|                                               |                           | - base: @bimap                                      |                    |
|                                               |                           | - keyType: int                                      |                    |
|                                               |                           | - valueType: juce::String                           |                    |
|                                               |                           | > > > - [list]: @code:name-entry                    |                    |
|                                               |                           | > > - [list]: @code:enum-entry                      |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| > > > - [list]: @bimaps:FileThemes            | - [list]: @code:linebreak | @code:namespace                                     | @Bimaps            |
|                                               |                           | - macro: #pragma once                               |                    |
| > > - [list]: @bimaps:FileThemes              |                           | - name: map                                         |                    |
|                                               |                           |                                                     |                    |
|                                               |                           | @code:bimap                                         |                    |
|                                               |                           | - name: FileThemes                                  |                    |
|                                               |                           | - type: map::FileThemes                             |                    |
|                                               |                           | - instance: fileThemes                              |                    |
|                                               |                           | - [description]: Theme-file registry.               |                    |
|                                               |                           | - base: @bimap                                      |                    |
|                                               |                           | - keyType: int                                      |                    |
|                                               |                           | - valueType: juce::String                           |                    |
|                                               |                           | > > > - [list]: @code:name-entry                    |                    |
|                                               |                           | > > - [list]: @code:enum-entry                      |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| > > > - [list]: @bimaps:FileFlex              | - [list]: @code:linebreak | @code:namespace                                     | @Bimaps            |
|                                               |                           | - macro: #pragma once                               |                    |
| > > - [list]: @bimaps:FileFlex                |                           | - name: map                                         |                    |
|                                               |                           |                                                     |                    |
|                                               |                           | @code:bimap                                         |                    |
|                                               |                           | - name: FileFlex                                    |                    |
|                                               |                           | - type: map::FileFlex                               |                    |
|                                               |                           | - instance: fileFlex                                |                    |
|                                               |                           | - [description]: Theme SVG graphics-asset registry. |                    |
|                                               |                           | - base: @bimap                                      |                    |
|                                               |                           | - keyType: int                                      |                    |
|                                               |                           | - valueType: juce::String                           |                    |
|                                               |                           | > > > - [list]: @code:bimap-entry                   |                    |
|                                               |                           | > > - [list]: @code:enum-entry                      |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| - [list]: @headers:type=header                |                           | @code:struct                                        | @Generated         |
|                                               |                           | - macro: #pragma once                               |                    |
|                                               |                           | - name: Generated                                   |                    |
|                                               |                           | - [description]: @headers:brief                     |                    |
|                                               |                           | - [list]: @code:include                             |                    |
| > - [list]: instance                          |                           | > - [list]: @code:shared-instance                   |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| - [list]: @project-info:project info          | - [list]:                 | @cmake:cmake                                        | @CMakeLists        |
|                                               |                           | - [description]: @headers:brief                     |                    |
| - [list]: @project-info:cmake                 |                           |                                                     |                    |
| - [list]: @signing:signing                    |                           |                                                     |                    |
| - [list]: @project-info:architecture          | - [list]: @semicolon      | - [list]: @cmake:value                              |                    |
| - [list]: @project-info:release:stage=        | - [list]: @semicolon      | - [list]: @cmake:mac                                |                    |
| - [list]: @project-info:release:stage=linker  | - [list]: @semicolon      | - [list]: @cmake:mac                                |                    |
| - [list]: @project-info:debug:stage=          | - [list]: @semicolon      | - [list]: @cmake:mac                                |                    |
| - [list]: @project-info:release:stage=        | - [list]: @semicolon      | - [list]: @cmake:win                                |                    |
| - [list]: @project-info:release:stage=linker  | - [list]: @semicolon      | - [list]: @cmake:win                                |                    |
| - [list]: @project-info:debug:stage=          | - [list]: @semicolon      | - [list]: @cmake:win                                |                    |
| - [list]: @project-info:patch                 |                           | - [list]: @cmake:patch                              |                    |
| - [list]: @project-info:user module           |                           | - [list]: @cmake:module                             |                    |
| - [list]: @project-info:source glob           |                           | - [list]: @cmake:glob-pattern                       |                    |
| > - [list]: @project-info:define              |                           | > - [list]: @cmake:entry                            |                    |
| > - [list]: @project-info:include             |                           | > - [list]: @cmake:entry                            |                    |
| > > - [list]: @project-info:juce module       |                           | > > - [list]: @cmake:value                          |                    |
| > > - [list]: @project-info:user module       |                           | > > - [list]: @cmake:link                           |                    |
| > - [list]: @project-info:layout glob         |                           | > - [list]: @cmake:entry                            |                    |
| - [list]: @project-info:pack                  |                           | - [list]: @cmake:pack-value                         |                    |
|                                               |                           | - xattr: @cmake:xattr                               |                    |
|                                               |                           | - codesign: @cmake:codesign                         |                    |
|                                               |                           | - verify: @cmake:verify                             |                    |
|                                               |                           | - pkg-staging: @cmake:pkg-staging                   |                    |
|                                               |                           | - pkgbuild: @cmake:pkgbuild                         |                    |
|                                               |                           | - productbuild: @cmake:productbuild                 |                    |
|                                               |                           | - productsign: @cmake:productsign                   |                    |
|                                               |                           | - notarize: @cmake:notarize                         |                    |
|                                               |                           | - staple: @cmake:staple                             |                    |
|                                               |                           | - makensis: @cmake:makensis                         |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| > - [list]: @signing:signing:type=entitlement |                           | @code:[xml]entitlements                             | @Entitlements      |
|                                               |                           | > - [list]: @code:entitlement                       |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| - [list]: @project-info:cmake                 |                           | @installer:distribution                             | @Distribution      |
| - [list]: @project-info:project info          |                           |                                                     |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| - [list]: @project-info:project info          |                           | @installer:installer                                | @Installer         |
| - [list]: @project-info:cmake                 |                           |                                                     |                    |
| - [list]: @signing:signing                    |                           |                                                     |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| - [list]: @project-info:cmake                 |                           | @cmake:gh                                           | @Gh                |
| - [list]: @project-info:project info          |                           | - [list]: @cmake:pack-value                         |                    |
| - [list]: @project-info:pack                  |                           |                                                     |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
| - [list]: @project-info:cmake                 |                           | @installer:[no-banner]release                       | @Release           |
| - [list]: @project-info:release notes         |                           | - [list]: @installer:note                           |                    |
+-----------------------------------------------+---------------------------+-----------------------------------------------------+--------------------+
