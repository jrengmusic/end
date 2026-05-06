#pragma once
#include <JuceHeader.h>
#include "../data/Cell.h"
#include "../logic/Grid.h"

namespace Terminal
{ /*____________________________________________________________________________*/

class Screen : public jam::TextEditor
{
public:
    struct CursorInfo
    {
        juce::Point<int> position;
        int shape  { 0 };
        bool visible  { false };
        bool focused  { false };
        bool blinkOn  { true };
    };

    enum ColourIds
    {
        defaultForegroundColourId  = 0x3000001,
        defaultBackgroundColourId  = 0x3000002,
        cursorColourId             = 0x3000003,
        selectionColourId          = 0x3000004,
        selectionCursorColourId    = 0x3000005,
        hintLabelFgColourId        = 0x3000006,
        hintLabelBgColourId        = 0x3000007,
        ansi0ColourId              = 0x3000010,
        ansi1ColourId              = 0x3000011,
        ansi2ColourId              = 0x3000012,
        ansi3ColourId              = 0x3000013,
        ansi4ColourId              = 0x3000014,
        ansi5ColourId              = 0x3000015,
        ansi6ColourId              = 0x3000016,
        ansi7ColourId              = 0x3000017,
        ansi8ColourId              = 0x3000018,
        ansi9ColourId              = 0x3000019,
        ansi10ColourId             = 0x300001A,
        ansi11ColourId             = 0x300001B,
        ansi12ColourId             = 0x300001C,
        ansi13ColourId             = 0x300001D,
        ansi14ColourId             = 0x300001E,
        ansi15ColourId             = 0x300001F,
    };

    Screen() noexcept;

    ~Screen() override;

    void render (const Grid& grid, const CursorInfo& cursor) noexcept;

    void setScrollBarWidth (int width) noexcept;

    void lookAndFeelChanged() override;
    void resized() override;

    int getCellWidth()      const noexcept { return cellWidth; }
    int getCellHeight()     const noexcept { return cellHeight; }
    int getPhysCellWidth()  const noexcept { return physCellWidth; }
    int getPhysCellHeight() const noexcept { return physCellHeight; }

private:
    juce::Colour resolveColour (const Color& c, bool isFg) const noexcept;
    void rebuildContent (const Grid& grid);
    void updateCellMetrics();

    juce::Colour defaultFg;
    juce::Colour defaultBg;
    std::array<juce::Colour, 16> ansiColours {};

    CursorInfo lastCursor;

    int cellWidth      { 0 };
    int cellHeight     { 0 };
    int physCellWidth  { 0 };
    int physCellHeight { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
