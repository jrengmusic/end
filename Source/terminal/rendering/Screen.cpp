#include "Screen.h"

namespace Terminal
{
/*____________________________________________________________________________*/

Screen::Screen() noexcept { setReadOnly (true); }

Screen::~Screen() = default;

void Screen::setLiveDimensions (int numRows, int numCols) noexcept
{
    jassert (numRows > 0 and numCols > 0);
    live.setSize (numRows, numCols, false, true, true);
    setLiveBuffer (&live);
    setLiveGraphemeBuffer (&liveGrapheme);
}

void Screen::updateVisibleRow (int row, const jam::Cell* src, int numCols) noexcept
{
    jassert (src != nullptr);
    jassert (row >= 0 and row < live.getNumRows());

    const int copyCount { juce::jmin (numCols, live.getNumCols()) };
    std::memcpy (live.getWritePointer (row), src,
                 static_cast<size_t> (copyCount) * sizeof (jam::Cell));
}

void Screen::append (const jam::Cell* const* rows, int rowCount, int numCols) noexcept
{
    jassert (rows != nullptr);

    for (int i { 0 }; i < rowCount; ++i)
    {
        if (rows[i] != nullptr)
            appendRow (rows[i], numCols);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
