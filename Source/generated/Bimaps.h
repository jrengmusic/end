/*******************************************************************************
                        Codegen Annotated Source of Truth
————————————————————————————————————————————————————————————————————————————————

            ░░████████████░░████████████░░████████████░░████████████
            ░░████  ░░████░░████  ░░████░░████  ░░████    ░░████
            ░░████        ░░████  ░░████░░████            ░░████
            ░░████        ░░████████████░░████████████    ░░████
            ░░████        ░░████  ░░████        ░░████    ░░████
            ░░████  ░░████░░████  ░░████░░████  ░░████    ░░████
            ░░████████████░░████  ░░████░░████████████    ░░████

————————————————————————————————————————————————————————————————————————————————
                         FOR YOUR EYES ONLY, DO NOT EDIT
********************************************************************************/

#pragma once

namespace map
{
/*_____________________________________________________________________________*/

/**
 * @brief Config-file section registry.
 *
 * Resolves each key to its own Identifier stem naming an on-disk config
 * section file; consumer-side helpers derive the full filename and directory
 * path. The default row is display.
 */
struct FileConfig : public jam::Bimap<int>
{
    FileConfig() : jam::Bimap<int> { {
            { display, juce::String::fromUTF8 ("display") },
            { keys,    juce::String::fromUTF8 ("keys") },
    } } {}

    enum value : int
    {
        display = 0,
        keys    = 1,
    };

    static FileConfig* getInstance() noexcept
    {
        return jam::SharedInstance<FileConfig>::getInstance();
    }
};

//==============================================================================

/**
 * @brief Theme-file registry.
 *
 * Resolves each key to its own Identifier stem naming an on-disk theme file;
 * consumer-side helpers derive the full filename and directory path. The
 * default row is theme.
 */
struct FileThemes : public jam::Bimap<int>
{
    FileThemes() : jam::Bimap<int> { {
            { theme, juce::String::fromUTF8 ("theme") },
    } } {}

    enum value : int
    {
        theme = 0,
    };

    static FileThemes* getInstance() noexcept
    {
        return jam::SharedInstance<FileThemes>::getInstance();
    }
};

//==============================================================================

/**
 * @brief Theme SVG graphics-asset registry.
 *
 * Resolves each key to its own Identifier stem naming an on-disk SVG file;
 * consumer-side helpers derive the full filename. The default row is tabBar.
 */
struct FileFlex : public jam::Bimap<int>
{
    FileFlex() : jam::Bimap<int> { {
            { tabBar,            juce::String::fromUTF8 ("tabBar") },
            { tabHighlight,      juce::String::fromUTF8 ("tabHighlight") },
            { tabButtonNormalOn, juce::String::fromUTF8 ("normalOn") },
            { resizerBar,        juce::String::fromUTF8 ("resizerBar") },
    } } {}

    enum value : int
    {
        tabBar            = 0,
        tabHighlight      = 1,
        tabButtonNormalOn = 2,
        resizerBar        = 3,
    };

    static FileFlex* getInstance() noexcept
    {
        return jam::SharedInstance<FileFlex>::getInstance();
    }
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace map

//==============================================================================

namespace map
{
/*_____________________________________________________________________________*/

/**
 * @brief END command-id to command-name map.
 *
 * Maps each command-id ordinal to the command name a config `keys` row uses.
 * The key is the juce::CommandID; the value is the row key.
 */
struct CommandId : public jam::Bimap<int, juce::Identifier>
{
    CommandId() : jam::Bimap<int, juce::Identifier> { {
            { quit,              juce::Identifier { "quit" } },
            { closePane,         juce::Identifier { "closePane" } },
            { closeTerminal,     juce::Identifier { "closeTerminal" } },
            { closeTerminalPane, juce::Identifier { "closeTerminalPane" } },
            { loadPlugin,        juce::Identifier { "loadPlugin" } },
            { reload,            juce::Identifier { "reload" } },
            { zoomIn,            juce::Identifier { "zoomIn" } },
            { zoomOut,           juce::Identifier { "zoomOut" } },
            { zoomReset,         juce::Identifier { "zoomReset" } },
            { reducePaneWidth,   juce::Identifier { "reducePaneWidth" } },
            { reducePaneHeight,  juce::Identifier { "reducePaneHeight" } },
            { expandPaneWidth,   juce::Identifier { "expandPaneWidth" } },
            { expandPaneHeight,  juce::Identifier { "expandPaneHeight" } },
            { newTab,            juce::Identifier { "newTab" } },
            { prevTab,           juce::Identifier { "prevTab" } },
            { nextTab,           juce::Identifier { "nextTab" } },
            { splitVertical,     juce::Identifier { "splitVertical" } },
            { splitHorizontal,   juce::Identifier { "splitHorizontal" } },
            { joinLeft,          juce::Identifier { "joinLeft" } },
            { joinDown,          juce::Identifier { "joinDown" } },
            { joinUp,            juce::Identifier { "joinUp" } },
            { joinRight,         juce::Identifier { "joinRight" } },
            { swapLeft,          juce::Identifier { "swapLeft" } },
            { swapDown,          juce::Identifier { "swapDown" } },
            { swapUp,            juce::Identifier { "swapUp" } },
            { swapRight,         juce::Identifier { "swapRight" } },
            { normal,            juce::Identifier { "normal" } },
            { prefix,            juce::Identifier { "prefix" } },
            { open,              juce::Identifier { "open" } },
            { list,              juce::Identifier { "list" } },
    } } {}

    enum value : int
    {
        quit              = 0x4700001,
        closePane         = 0x4700002,
        closeTerminal     = 0x4700023,
        closeTerminalPane = 0x4700024,
        loadPlugin        = 0x4700025,
        reload            = 0x4700003,
        zoomIn            = 0x4700004,
        zoomOut           = 0x4700005,
        zoomReset         = 0x4700006,
        reducePaneWidth   = 0x4700008,
        reducePaneHeight  = 0x4700009,
        expandPaneWidth   = 0x470000a,
        expandPaneHeight  = 0x470000b,
        newTab            = 0x470000c,
        prevTab           = 0x470000d,
        nextTab           = 0x470000e,
        splitVertical     = 0x4700010,
        splitHorizontal   = 0x4700011,
        joinLeft          = 0x4700016,
        joinDown          = 0x4700017,
        joinUp            = 0x4700018,
        joinRight         = 0x4700019,
        swapLeft          = 0x470001a,
        swapDown          = 0x470001b,
        swapUp            = 0x470001c,
        swapRight         = 0x470001d,
        normal            = 0x470001f,
        prefix            = 0x4700020,
        open              = 0x4700021,
        list              = 0x4700022,
    };

    static CommandId* getInstance() noexcept
    {
        return jam::SharedInstance<CommandId>::getInstance();
    }
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace map
