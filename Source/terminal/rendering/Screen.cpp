#include "Screen.h"

namespace Terminal
{ /*____________________________________________________________________________*/

Screen::Screen() noexcept
    : jam::TextEditor ("Screen")
{
    setMultiLine (true);
    setReadOnly (true);
    setScrollbarsShown (true);
    setCaretVisible (false);
    setOpaque (false);
    setFont (font);
}

void Screen::setText() noexcept
{
    // Cell: { codepoint, style, layout, width, reserved, fg, bg }
    // Every field explicit. Comprehensive rendering test.

    // Colours — fg
    const auto w { juce::Colours::white };
    const auto red { juce::Colour (0xFFFF0000u) };
    const auto grn { juce::Colour (0xFF00FF00u) };
    const auto ylw { juce::Colour (0xFFFFFF00u) };
    const auto blu { juce::Colour (0xFF5555FFu) };
    const auto mag { juce::Colour (0xFFFF55FFu) };
    const auto cyn { juce::Colour (0xFF55FFFFu) };
    const auto ora { juce::Colour (0xFFFF6432u) };// truecolor sample
    const auto dim { juce::Colour (0xFF888888u) };

    // Colours — bg
    const auto nb { juce::Colours::transparentBlack };// no bg
    const auto dbg { juce::Colour (0xFF333333u) };// dark bg
    const auto mbg { juce::Colour (0xFF555555u) };// mid bg
    const auto rbg { juce::Colour (0xFF440000u) };// red bg
    const auto bbg { juce::Colour (0xFF000044u) };// blue bg

    // Style shortcuts
    constexpr uint8_t B { jam::Cell::BOLD };
    constexpr uint8_t I { jam::Cell::ITALIC };
    constexpr uint8_t U { jam::Cell::UNDERLINE };
    constexpr uint8_t S { jam::Cell::STRIKE };
    constexpr uint8_t R { jam::Cell::INVERSE };
    constexpr uint8_t D { jam::Cell::DIM };
    constexpr uint8_t BI { static_cast<uint8_t> (jam::Cell::BOLD | jam::Cell::ITALIC) };

    const jam::Cell pens[] = {
        // === SGR styles (white on transparent) ===
        // Cell: { codepoint, style, layout, width, reserved, fg, bg }
        // "Bold"
        { 'B',     B,  0,                      1, 0, w,                          nb  },
        { 'o',     B,  0,                      1, 0, w,                          nb  },
        { 'l',     B,  0,                      1, 0, w,                          nb  },
        { 'd',     B,  0,                      1, 0, w,                          nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        // "Italic"
        { 'I',     I,  0,                      1, 0, w,                          nb  },
        { 't',     I,  0,                      1, 0, w,                          nb  },
        { 'a',     I,  0,                      1, 0, w,                          nb  },
        { 'l',     I,  0,                      1, 0, w,                          nb  },
        { 'i',     I,  0,                      1, 0, w,                          nb  },
        { 'c',     I,  0,                      1, 0, w,                          nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        // "Under"
        { 'U',     U,  0,                      1, 0, w,                          nb  },
        { 'n',     U,  0,                      1, 0, w,                          nb  },
        { 'd',     U,  0,                      1, 0, w,                          nb  },
        { 'e',     U,  0,                      1, 0, w,                          nb  },
        { 'r',     U,  0,                      1, 0, w,                          nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        // "Strike"
        { 'S',     S,  0,                      1, 0, w,                          nb  },
        { 't',     S,  0,                      1, 0, w,                          nb  },
        { 'r',     S,  0,                      1, 0, w,                          nb  },
        { 'i',     S,  0,                      1, 0, w,                          nb  },
        { 'k',     S,  0,                      1, 0, w,                          nb  },
        { 'e',     S,  0,                      1, 0, w,                          nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        // "Reverse"
        { 'R',     R,  0,                      1, 0, w,                          nb  },
        { 'e',     R,  0,                      1, 0, w,                          nb  },
        { 'v',     R,  0,                      1, 0, w,                          nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        // "Dim"
        { 'D',     D,  0,                      1, 0, dim,                        nb  },
        { 'i',     D,  0,                      1, 0, dim,                        nb  },
        { 'm',     D,  0,                      1, 0, dim,                        nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        // "BoldItalic"
        { 'B',     BI, 0,                      1, 0, w,                          nb  },
        { 'I',     BI, 0,                      1, 0, w,                          nb  },
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === ANSI foreground colours ===
        // "Red Green Yellow Blue Magenta Cyan"
        { 'R',     0,  0,                      1, 0, red,                        nb  },
        { 'e',     0,  0,                      1, 0, red,                        nb  },
        { 'd',     0,  0,                      1, 0, red,                        nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 'G',     0,  0,                      1, 0, grn,                        nb  },
        { 'r',     0,  0,                      1, 0, grn,                        nb  },
        { 'n',     0,  0,                      1, 0, grn,                        nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 'Y',     0,  0,                      1, 0, ylw,                        nb  },
        { 'l',     0,  0,                      1, 0, ylw,                        nb  },
        { 'w',     0,  0,                      1, 0, ylw,                        nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 'B',     0,  0,                      1, 0, blu,                        nb  },
        { 'l',     0,  0,                      1, 0, blu,                        nb  },
        { 'u',     0,  0,                      1, 0, blu,                        nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 'M',     0,  0,                      1, 0, mag,                        nb  },
        { 'a',     0,  0,                      1, 0, mag,                        nb  },
        { 'g',     0,  0,                      1, 0, mag,                        nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 'C',     0,  0,                      1, 0, cyn,                        nb  },
        { 'y',     0,  0,                      1, 0, cyn,                        nb  },
        { 'n',     0,  0,                      1, 0, cyn,                        nb  },
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Truecolor gradient (8 steps) ===
        { 0x2588,  0,  0,                      1, 0, juce::Colour (0xFF0000FFu), nb  }, // █ blue→
        { 0x2588,  0,  0,                      1, 0, juce::Colour (0xFF2200DDu), nb  },
        { 0x2588,  0,  0,                      1, 0, juce::Colour (0xFF4400BBu), nb  },
        { 0x2588,  0,  0,                      1, 0, juce::Colour (0xFF660099u), nb  },
        { 0x2588,  0,  0,                      1, 0, juce::Colour (0xFF880077u), nb  },
        { 0x2588,  0,  0,                      1, 0, juce::Colour (0xFFAA0055u), nb  },
        { 0x2588,  0,  0,                      1, 0, juce::Colour (0xFFCC0033u), nb  },
        { 0x2588,  0,  0,                      1, 0, juce::Colour (0xFFFF0000u), nb  }, // →red
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Background colours ===
        { ' ',     0,  0,                      1, 0, w,                          dbg },
        { 'D',     0,  0,                      1, 0, w,                          dbg },
        { 'a',     0,  0,                      1, 0, w,                          dbg },
        { 'r',     0,  0,                      1, 0, w,                          dbg },
        { 'k',     0,  0,                      1, 0, w,                          dbg },
        { ' ',     0,  0,                      1, 0, w,                          dbg },
        { ' ',     0,  0,                      1, 0, w,                          mbg },
        { 'M',     0,  0,                      1, 0, w,                          mbg },
        { 'i',     0,  0,                      1, 0, w,                          mbg },
        { 'd',     0,  0,                      1, 0, w,                          mbg },
        { ' ',     0,  0,                      1, 0, w,                          mbg },
        { ' ',     0,  0,                      1, 0, w,                          rbg },
        { 'R',     0,  0,                      1, 0, w,                          rbg },
        { 'e',     0,  0,                      1, 0, w,                          rbg },
        { 'd',     0,  0,                      1, 0, w,                          rbg },
        { ' ',     0,  0,                      1, 0, w,                          rbg },
        { ' ',     0,  0,                      1, 0, w,                          bbg },
        { 'B',     0,  0,                      1, 0, w,                          bbg },
        { 'l',     0,  0,                      1, 0, w,                          bbg },
        { 'u',     0,  0,                      1, 0, w,                          bbg },
        { ' ',     0,  0,                      1, 0, w,                          bbg },
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === ASCII printable (full range) ===
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { '!',     0,  0,                      1, 0, w,                          nb  },
        { '"',     0,  0,                      1, 0, w,                          nb  },
        { '#',     0,  0,                      1, 0, w,                          nb  },
        { '$',     0,  0,                      1, 0, w,                          nb  },
        { '%',     0,  0,                      1, 0, w,                          nb  },
        { '&',     0,  0,                      1, 0, w,                          nb  },
        { '\'',    0,  0,                      1, 0, w,                          nb  },
        { '(',     0,  0,                      1, 0, w,                          nb  },
        { ')',     0,  0,                      1, 0, w,                          nb  },
        { '*',     0,  0,                      1, 0, w,                          nb  },
        { '+',     0,  0,                      1, 0, w,                          nb  },
        { ',',     0,  0,                      1, 0, w,                          nb  },
        { '-',     0,  0,                      1, 0, w,                          nb  },
        { '.',     0,  0,                      1, 0, w,                          nb  },
        { '/',     0,  0,                      1, 0, w,                          nb  },
        { '0',     0,  0,                      1, 0, w,                          nb  },
        { '1',     0,  0,                      1, 0, w,                          nb  },
        { '2',     0,  0,                      1, 0, w,                          nb  },
        { '3',     0,  0,                      1, 0, w,                          nb  },
        { '4',     0,  0,                      1, 0, w,                          nb  },
        { '5',     0,  0,                      1, 0, w,                          nb  },
        { '6',     0,  0,                      1, 0, w,                          nb  },
        { '7',     0,  0,                      1, 0, w,                          nb  },
        { '8',     0,  0,                      1, 0, w,                          nb  },
        { '9',     0,  0,                      1, 0, w,                          nb  },
        { ':',     0,  0,                      1, 0, w,                          nb  },
        { ';',     0,  0,                      1, 0, w,                          nb  },
        { '<',     0,  0,                      1, 0, w,                          nb  },
        { '=',     0,  0,                      1, 0, w,                          nb  },
        { '>',     0,  0,                      1, 0, w,                          nb  },
        { '?',     0,  0,                      1, 0, w,                          nb  },
        { '\n',    0,  0,                      1, 0, w,                          nb  },
        { '@',     0,  0,                      1, 0, w,                          nb  },
        { 'A',     0,  0,                      1, 0, w,                          nb  },
        { 'B',     0,  0,                      1, 0, w,                          nb  },
        { 'C',     0,  0,                      1, 0, w,                          nb  },
        { 'D',     0,  0,                      1, 0, w,                          nb  },
        { 'E',     0,  0,                      1, 0, w,                          nb  },
        { 'F',     0,  0,                      1, 0, w,                          nb  },
        { 'G',     0,  0,                      1, 0, w,                          nb  },
        { 'H',     0,  0,                      1, 0, w,                          nb  },
        { 'I',     0,  0,                      1, 0, w,                          nb  },
        { 'J',     0,  0,                      1, 0, w,                          nb  },
        { 'K',     0,  0,                      1, 0, w,                          nb  },
        { 'L',     0,  0,                      1, 0, w,                          nb  },
        { 'M',     0,  0,                      1, 0, w,                          nb  },
        { 'N',     0,  0,                      1, 0, w,                          nb  },
        { 'O',     0,  0,                      1, 0, w,                          nb  },
        { 'P',     0,  0,                      1, 0, w,                          nb  },
        { 'Q',     0,  0,                      1, 0, w,                          nb  },
        { 'R',     0,  0,                      1, 0, w,                          nb  },
        { 'S',     0,  0,                      1, 0, w,                          nb  },
        { 'T',     0,  0,                      1, 0, w,                          nb  },
        { 'U',     0,  0,                      1, 0, w,                          nb  },
        { 'V',     0,  0,                      1, 0, w,                          nb  },
        { 'W',     0,  0,                      1, 0, w,                          nb  },
        { 'X',     0,  0,                      1, 0, w,                          nb  },
        { 'Y',     0,  0,                      1, 0, w,                          nb  },
        { 'Z',     0,  0,                      1, 0, w,                          nb  },
        { '\n',    0,  0,                      1, 0, w,                          nb  },
        { '[',     0,  0,                      1, 0, w,                          nb  },
        { '\\',    0,  0,                      1, 0, w,                          nb  },
        { ']',     0,  0,                      1, 0, w,                          nb  },
        { '^',     0,  0,                      1, 0, w,                          nb  },
        { '_',     0,  0,                      1, 0, w,                          nb  },
        { '`',     0,  0,                      1, 0, w,                          nb  },
        { 'a',     0,  0,                      1, 0, w,                          nb  },
        { 'b',     0,  0,                      1, 0, w,                          nb  },
        { 'c',     0,  0,                      1, 0, w,                          nb  },
        { 'd',     0,  0,                      1, 0, w,                          nb  },
        { 'e',     0,  0,                      1, 0, w,                          nb  },
        { 'f',     0,  0,                      1, 0, w,                          nb  },
        { 'g',     0,  0,                      1, 0, w,                          nb  },
        { 'h',     0,  0,                      1, 0, w,                          nb  },
        { 'i',     0,  0,                      1, 0, w,                          nb  },
        { 'j',     0,  0,                      1, 0, w,                          nb  },
        { 'k',     0,  0,                      1, 0, w,                          nb  },
        { 'l',     0,  0,                      1, 0, w,                          nb  },
        { 'm',     0,  0,                      1, 0, w,                          nb  },
        { 'n',     0,  0,                      1, 0, w,                          nb  },
        { 'o',     0,  0,                      1, 0, w,                          nb  },
        { 'p',     0,  0,                      1, 0, w,                          nb  },
        { 'q',     0,  0,                      1, 0, w,                          nb  },
        { 'r',     0,  0,                      1, 0, w,                          nb  },
        { 's',     0,  0,                      1, 0, w,                          nb  },
        { 't',     0,  0,                      1, 0, w,                          nb  },
        { 'u',     0,  0,                      1, 0, w,                          nb  },
        { 'v',     0,  0,                      1, 0, w,                          nb  },
        { 'w',     0,  0,                      1, 0, w,                          nb  },
        { 'x',     0,  0,                      1, 0, w,                          nb  },
        { 'y',     0,  0,                      1, 0, w,                          nb  },
        { 'z',     0,  0,                      1, 0, w,                          nb  },
        { '{',     0,  0,                      1, 0, w,                          nb  },
        { '|',     0,  0,                      1, 0, w,                          nb  },
        { '}',     0,  0,                      1, 0, w,                          nb  },
        { '~',     0,  0,                      1, 0, w,                          nb  },
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Box drawing light ===
        { 0x250C,  0,  0,                      1, 0, w,                          nb  }, // ┌
        { 0x2500,  0,  0,                      1, 0, w,                          nb  }, // ─
        { 0x2500,  0,  0,                      1, 0, w,                          nb  }, // ─
        { 0x252C,  0,  0,                      1, 0, w,                          nb  }, // ┬
        { 0x2500,  0,  0,                      1, 0, w,                          nb  }, // ─
        { 0x2500,  0,  0,                      1, 0, w,                          nb  }, // ─
        { 0x2510,  0,  0,                      1, 0, w,                          nb  }, // ┐
        { '\n',    0,  0,                      1, 0, w,                          nb  },
        { 0x2502,  0,  0,                      1, 0, w,                          nb  }, // │
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0x2502,  0,  0,                      1, 0, w,                          nb  }, // │
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0x2502,  0,  0,                      1, 0, w,                          nb  }, // │
        { '\n',    0,  0,                      1, 0, w,                          nb  },
        { 0x251C,  0,  0,                      1, 0, w,                          nb  }, // ├
        { 0x2500,  0,  0,                      1, 0, w,                          nb  }, // ─
        { 0x2500,  0,  0,                      1, 0, w,                          nb  }, // ─
        { 0x253C,  0,  0,                      1, 0, w,                          nb  }, // ┼
        { 0x2500,  0,  0,                      1, 0, w,                          nb  }, // ─
        { 0x2500,  0,  0,                      1, 0, w,                          nb  }, // ─
        { 0x2524,  0,  0,                      1, 0, w,                          nb  }, // ┤
        { '\n',    0,  0,                      1, 0, w,                          nb  },
        { 0x2514,  0,  0,                      1, 0, w,                          nb  }, // └
        { 0x2500,  0,  0,                      1, 0, w,                          nb  }, // ─
        { 0x2500,  0,  0,                      1, 0, w,                          nb  }, // ─
        { 0x2534,  0,  0,                      1, 0, w,                          nb  }, // ┴
        { 0x2500,  0,  0,                      1, 0, w,                          nb  }, // ─
        { 0x2500,  0,  0,                      1, 0, w,                          nb  }, // ─
        { 0x2518,  0,  0,                      1, 0, w,                          nb  }, // ┘
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Box drawing heavy ===
        { 0x250F,  0,  0,                      1, 0, w,                          nb  }, // ┏
        { 0x2501,  0,  0,                      1, 0, w,                          nb  }, // ━
        { 0x2501,  0,  0,                      1, 0, w,                          nb  }, // ━
        { 0x2533,  0,  0,                      1, 0, w,                          nb  }, // ┳
        { 0x2501,  0,  0,                      1, 0, w,                          nb  }, // ━
        { 0x2501,  0,  0,                      1, 0, w,                          nb  }, // ━
        { 0x2513,  0,  0,                      1, 0, w,                          nb  }, // ┓
        { '\n',    0,  0,                      1, 0, w,                          nb  },
        { 0x2503,  0,  0,                      1, 0, w,                          nb  }, // ┃
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0x2503,  0,  0,                      1, 0, w,                          nb  }, // ┃
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0x2503,  0,  0,                      1, 0, w,                          nb  }, // ┃
        { '\n',    0,  0,                      1, 0, w,                          nb  },
        { 0x2517,  0,  0,                      1, 0, w,                          nb  }, // ┗
        { 0x2501,  0,  0,                      1, 0, w,                          nb  }, // ━
        { 0x2501,  0,  0,                      1, 0, w,                          nb  }, // ━
        { 0x253B,  0,  0,                      1, 0, w,                          nb  }, // ┻
        { 0x2501,  0,  0,                      1, 0, w,                          nb  }, // ━
        { 0x2501,  0,  0,                      1, 0, w,                          nb  }, // ━
        { 0x251B,  0,  0,                      1, 0, w,                          nb  }, // ┛
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Box drawing double ===
        { 0x2554,  0,  0,                      1, 0, w,                          nb  }, // ╔
        { 0x2550,  0,  0,                      1, 0, w,                          nb  }, // ═
        { 0x2550,  0,  0,                      1, 0, w,                          nb  }, // ═
        { 0x2557,  0,  0,                      1, 0, w,                          nb  }, // ╗
        { '\n',    0,  0,                      1, 0, w,                          nb  },
        { 0x2551,  0,  0,                      1, 0, w,                          nb  }, // ║
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0x2551,  0,  0,                      1, 0, w,                          nb  }, // ║
        { '\n',    0,  0,                      1, 0, w,                          nb  },
        { 0x255A,  0,  0,                      1, 0, w,                          nb  }, // ╚
        { 0x2550,  0,  0,                      1, 0, w,                          nb  }, // ═
        { 0x2550,  0,  0,                      1, 0, w,                          nb  }, // ═
        { 0x255D,  0,  0,                      1, 0, w,                          nb  }, // ╝
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Box drawing rounded ===
        { 0x256D,  0,  0,                      1, 0, w,                          nb  }, // ╭
        { 0x2500,  0,  0,                      1, 0, w,                          nb  }, // ─
        { 0x256E,  0,  0,                      1, 0, w,                          nb  }, // ╮
        { '\n',    0,  0,                      1, 0, w,                          nb  },
        { 0x2502,  0,  0,                      1, 0, w,                          nb  }, // │
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0x2502,  0,  0,                      1, 0, w,                          nb  }, // │
        { '\n',    0,  0,                      1, 0, w,                          nb  },
        { 0x2570,  0,  0,                      1, 0, w,                          nb  }, // ╰
        { 0x2500,  0,  0,                      1, 0, w,                          nb  }, // ─
        { 0x256F,  0,  0,                      1, 0, w,                          nb  }, // ╯
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Block elements ===
        { 0x2580,  0,  0,                      1, 0, w,                          nb  }, // ▀
        { 0x2581,  0,  0,                      1, 0, w,                          nb  }, // ▁
        { 0x2582,  0,  0,                      1, 0, w,                          nb  }, // ▂
        { 0x2583,  0,  0,                      1, 0, w,                          nb  }, // ▃
        { 0x2584,  0,  0,                      1, 0, w,                          nb  }, // ▄
        { 0x2585,  0,  0,                      1, 0, w,                          nb  }, // ▅
        { 0x2586,  0,  0,                      1, 0, w,                          nb  }, // ▆
        { 0x2587,  0,  0,                      1, 0, w,                          nb  }, // ▇
        { 0x2588,  0,  0,                      1, 0, w,                          nb  }, // █
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0x258C,  0,  0,                      1, 0, w,                          nb  }, // ▌
        { 0x2590,  0,  0,                      1, 0, w,                          nb  }, // ▐
        { 0x2591,  0,  0,                      1, 0, w,                          nb  }, // ░
        { 0x2592,  0,  0,                      1, 0, w,                          nb  }, // ▒
        { 0x2593,  0,  0,                      1, 0, w,                          nb  }, // ▓
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Braille ===
        { 0x2800,  0,  0,                      1, 0, w,                          nb  }, // ⠀
        { 0x2801,  0,  0,                      1, 0, w,                          nb  }, // ⠁
        { 0x2803,  0,  0,                      1, 0, w,                          nb  }, // ⠃
        { 0x2807,  0,  0,                      1, 0, w,                          nb  }, // ⠇
        { 0x280F,  0,  0,                      1, 0, w,                          nb  }, // ⠏
        { 0x283F,  0,  0,                      1, 0, w,                          nb  }, // ⠿
        { 0x28FF,  0,  0,                      1, 0, w,                          nb  }, // ⣿
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Arrows ===
        { 0x2190,  0,  0,                      1, 0, w,                          nb  }, // ←
        { 0x2191,  0,  0,                      1, 0, w,                          nb  }, // ↑
        { 0x2192,  0,  0,                      1, 0, w,                          nb  }, // →
        { 0x2193,  0,  0,                      1, 0, w,                          nb  }, // ↓
        { 0x2194,  0,  0,                      1, 0, w,                          nb  }, // ↔
        { 0x2195,  0,  0,                      1, 0, w,                          nb  }, // ↕
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Math symbols ===
        { 0x2200,  0,  0,                      1, 0, w,                          nb  }, // ∀
        { 0x2203,  0,  0,                      1, 0, w,                          nb  }, // ∃
        { 0x2205,  0,  0,                      1, 0, w,                          nb  }, // ∅
        { 0x221A,  0,  0,                      1, 0, w,                          nb  }, // √
        { 0x221E,  0,  0,                      1, 0, w,                          nb  }, // ∞
        { 0x2248,  0,  0,                      1, 0, w,                          nb  }, // ≈
        { 0x2260,  0,  0,                      1, 0, w,                          nb  }, // ≠
        { 0x2264,  0,  0,                      1, 0, w,                          nb  }, // ≤
        { 0x2265,  0,  0,                      1, 0, w,                          nb  }, // ≥
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Currency ===
        { 0x00A2,  0,  0,                      1, 0, w,                          nb  }, // ¢
        { 0x00A3,  0,  0,                      1, 0, w,                          nb  }, // £
        { 0x00A5,  0,  0,                      1, 0, w,                          nb  }, // ¥
        { 0x20AC,  0,  0,                      1, 0, w,                          nb  }, // €
        { 0x20BF,  0,  0,                      1, 0, w,                          nb  }, // ₿
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Latin extended ===
        { 0x00E0,  0,  0,                      1, 0, w,                          nb  }, // à
        { 0x00E1,  0,  0,                      1, 0, w,                          nb  }, // á
        { 0x00E9,  0,  0,                      1, 0, w,                          nb  }, // é
        { 0x00F1,  0,  0,                      1, 0, w,                          nb  }, // ñ
        { 0x00FC,  0,  0,                      1, 0, w,                          nb  }, // ü
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Greek ===
        { 0x03B1,  0,  0,                      1, 0, w,                          nb  }, // α
        { 0x03B2,  0,  0,                      1, 0, w,                          nb  }, // β
        { 0x03B3,  0,  0,                      1, 0, w,                          nb  }, // γ
        { 0x03B4,  0,  0,                      1, 0, w,                          nb  }, // δ
        { 0x03BB,  0,  0,                      1, 0, w,                          nb  }, // λ
        { 0x03C0,  0,  0,                      1, 0, w,                          nb  }, // π
        { 0x03C9,  0,  0,                      1, 0, w,                          nb  }, // ω
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Cyrillic ===
        { 0x0430,  0,  0,                      1, 0, w,                          nb  }, // а
        { 0x0431,  0,  0,                      1, 0, w,                          nb  }, // б
        { 0x0432,  0,  0,                      1, 0, w,                          nb  }, // в
        { 0x0433,  0,  0,                      1, 0, w,                          nb  }, // г
        { 0x0434,  0,  0,                      1, 0, w,                          nb  }, // д
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === CJK — width 2 ===
        { 0x4F60,  0,  0,                      2, 0, w,                          nb  }, // 你
        { 0x597D,  0,  0,                      2, 0, w,                          nb  }, // 好
        { 0x4E16,  0,  0,                      2, 0, w,                          nb  }, // 世
        { 0x754C,  0,  0,                      2, 0, w,                          nb  }, // 界
        { 0x7AEF,  0,  0,                      2, 0, w,                          nb  }, // 端
        { 0x672B,  0,  0,                      2, 0, w,                          nb  }, // 末
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Hiragana ===
        { 0x3042,  0,  0,                      2, 0, w,                          nb  }, // あ
        { 0x3044,  0,  0,                      2, 0, w,                          nb  }, // い
        { 0x3046,  0,  0,                      2, 0, w,                          nb  }, // う
        { 0x3048,  0,  0,                      2, 0, w,                          nb  }, // え
        { 0x304A,  0,  0,                      2, 0, w,                          nb  }, // お
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Katakana ===
        { 0x30A2,  0,  0,                      2, 0, w,                          nb  }, // ア
        { 0x30A4,  0,  0,                      2, 0, w,                          nb  }, // イ
        { 0x30A6,  0,  0,                      2, 0, w,                          nb  }, // ウ
        { 0x30A8,  0,  0,                      2, 0, w,                          nb  }, // エ
        { 0x30AA,  0,  0,                      2, 0, w,                          nb  }, // オ
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Korean Hangul — width 2 ===
        { 0xAC00,  0,  0,                      2, 0, w,                          nb  }, // 가
        { 0xB098,  0,  0,                      2, 0, w,                          nb  }, // 나
        { 0xB2E4,  0,  0,                      2, 0, w,                          nb  }, // 다
        { 0xB77C,  0,  0,                      2, 0, w,                          nb  }, // 라
        { 0xB9C8,  0,  0,                      2, 0, w,                          nb  }, // 마
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Fullwidth — width 2 ===
        { 0xFF21,  0,  0,                      2, 0, w,                          nb  }, // Ａ
        { 0xFF22,  0,  0,                      2, 0, w,                          nb  }, // Ｂ
        { 0xFF23,  0,  0,                      2, 0, w,                          nb  }, // Ｃ
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Emoji — width 2, LAYOUT_EMOJI ===
        { 0x1F525, 0,  jam::Cell::LAYOUT_EMOJI, 2, 0, w,                          nb  }, // 🔥
        { 0x1F680, 0,  jam::Cell::LAYOUT_EMOJI, 2, 0, w,                          nb  }, // 🚀
        { 0x1F4BB, 0,  jam::Cell::LAYOUT_EMOJI, 2, 0, w,                          nb  }, // 💻
        { 0x1F427, 0,  jam::Cell::LAYOUT_EMOJI, 2, 0, w,                          nb  }, // 🐧
        { 0x1F3B5, 0,  jam::Cell::LAYOUT_EMOJI, 2, 0, w,                          nb  }, // 🎵
        { 0x1F30D, 0,  jam::Cell::LAYOUT_EMOJI, 2, 0, w,                          nb  }, // 🌍
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Powerline — width 1 ===
        { 0xE0A0,  0,  0,                      1, 0, w,                          nb  }, //  (branch)
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0xE0A1,  0,  0,                      1, 0, w,                          nb  }, //  (LN)
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0xE0A2,  0,  0,                      1, 0, w,                          nb  }, //  (lock)
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0xE0B0,  0,  0,                      1, 0, w,                          nb  }, //  (right triangle)
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0xE0B2,  0,  0,                      1, 0, w,                          nb  }, //  (left triangle)
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Nerd Font icons — width 1 ===
        { 0xF121,  0,  0,                      1, 0, w,                          nb  }, //  (code)
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0xF07B,  0,  0,                      1, 0, w,                          nb  }, //  (folder)
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0xF15C,  0,  0,                      1, 0, w,                          nb  }, //  (file)
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0xF1D3,  0,  0,                      1, 0, w,                          nb  }, //  (git)
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0xF013,  0,  0,                      1, 0, w,                          nb  }, //  (gear)
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0xF015,  0,  0,                      1, 0, w,                          nb  }, //  (home)
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Geometric shapes ===
        { 0x25A0,  0,  0,                      1, 0, w,                          nb  }, // ■
        { 0x25A1,  0,  0,                      1, 0, w,                          nb  }, // □
        { 0x25B2,  0,  0,                      1, 0, w,                          nb  }, // ▲
        { 0x25BC,  0,  0,                      1, 0, w,                          nb  }, // ▼
        { 0x25C0,  0,  0,                      1, 0, w,                          nb  }, // ◀
        { 0x25B6,  0,  0,                      1, 0, w,                          nb  }, // ▶
        { 0x25CB,  0,  0,                      1, 0, w,                          nb  }, // ○
        { 0x25CF,  0,  0,                      1, 0, w,                          nb  }, // ●
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Misc symbols ===
        { 0x2605,  0,  0,                      1, 0, w,                          nb  }, // ★
        { 0x2606,  0,  0,                      1, 0, w,                          nb  }, // ☆
        { 0x2660,  0,  0,                      1, 0, w,                          nb  }, // ♠
        { 0x2663,  0,  0,                      1, 0, w,                          nb  }, // ♣
        { 0x2665,  0,  0,                      1, 0, w,                          nb  }, // ♥
        { 0x2666,  0,  0,                      1, 0, w,                          nb  }, // ♦
        { 0x266A,  0,  0,                      1, 0, w,                          nb  }, // ♪
        { 0x266B,  0,  0,                      1, 0, w,                          nb  }, // ♫
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Dingbats ===
        { 0x2713,  0,  0,                      1, 0, grn,                        nb  }, // ✓ green
        { 0x2717,  0,  0,                      1, 0, red,                        nb  }, // ✗ red
        { 0x2718,  0,  0,                      1, 0, red,                        nb  }, // ✘ red
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === General punctuation ===
        { 0x2013,  0,  0,                      1, 0, w,                          nb  }, // –
        { 0x2014,  0,  0,                      1, 0, w,                          nb  }, // —
        { 0x2018,  0,  0,                      1, 0, w,                          nb  }, // '
        { 0x2019,  0,  0,                      1, 0, w,                          nb  }, // '
        { 0x201C,  0,  0,                      1, 0, w,                          nb  }, // "
        { 0x201D,  0,  0,                      1, 0, w,                          nb  }, // "
        { 0x2022,  0,  0,                      1, 0, w,                          nb  }, // •
        { 0x2026,  0,  0,                      1, 0, w,                          nb  }, // …
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Keyboard symbols ===
        { 0x2318,  0,  0,                      1, 0, w,                          nb  }, // ⌘
        { 0x2325,  0,  0,                      1, 0, w,                          nb  }, // ⌥
        { 0x21E7,  0,  0,                      1, 0, w,                          nb  }, // ⇧
        { 0x2303,  0,  0,                      1, 0, w,                          nb  }, // ⌃
        { 0x238B,  0,  0,                      1, 0, w,                          nb  }, // ⎋
        { 0x23CE,  0,  0,                      1, 0, w,                          nb  }, // ⏎
        { 0x232B,  0,  0,                      1, 0, w,                          nb  }, // ⌫
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === END branding (Display Mono PUA) — 3-cell span ===
        { 0xE000,  0,  0,                      1, 0, w,                          nb  }, // END logo glyph 1
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { 0xE001,  0,  0,                      1, 0, w,                          nb  }, // END logo glyph 2
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { ' ',     0,  0,                      1, 0, w,                          nb  },
        { '\n',    0,  0,                      1, 0, w,                          nb  },

        // === Cell alignment: A(1) 🔥(2) B(1) 你(2) C(1) 好(2) D(1) ===
        { 'A',     0,  0,                      1, 0, w,                          nb  },
        { 0x1F525, 0,  jam::Cell::LAYOUT_EMOJI, 2, 0, w,                          nb  },
        { 'B',     0,  0,                      1, 0, w,                          nb  },
        { 0x4F60,  0,  0,                      2, 0, w,                          nb  },
        { 'C',     0,  0,                      1, 0, w,                          nb  },
        { 0x597D,  0,  0,                      2, 0, w,                          nb  },
        { 'D',     0,  0,                      1, 0, w,                          nb  },
        { '\n',    0,  0,                      1, 0, w,                          nb  },
    };

    const int penCount { static_cast<int> (sizeof (pens) / sizeof (pens[0])) };
    jam::TextEditor::setText (jam::Cells::fromArray (pens, penCount));
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
