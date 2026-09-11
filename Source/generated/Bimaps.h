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
 * @brief Split preview overlay axis line style names.
 *
 * Consumed by theme.md pane.split_line; the default row is solid.
 */
struct OverlayAxisLine : public jam::Bimap<int>
{
    OverlayAxisLine() : jam::Bimap<int> { {
            { solid,   juce::String::fromUTF8 ("solid") },
            { dash,    juce::String::fromUTF8 ("dash") },
            { bracket, juce::String::fromUTF8 ("bracket") },
    } } {}

    enum value : int
    {
        solid   = 0,
        dash    = 1,
        bracket = 2,
    };

    static OverlayAxisLine* getInstance() noexcept
    {
        return jam::SharedInstance<OverlayAxisLine>::getInstance();
    }
};

//==============================================================================

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
            { tabBar,            juce::String::fromUTF8 ("tab_bar") },
            { tabHighlight,      juce::String::fromUTF8 ("tab_highlight") },
            { tabButtonNormalOn, juce::String::fromUTF8 ("tab_button_normalOn") },
            { resizerBar,        juce::String::fromUTF8 ("resizer_bar") },
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
