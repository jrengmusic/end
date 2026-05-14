#include "Screen.h"

namespace Terminal
{
/*____________________________________________________________________________*/

Screen::Screen() noexcept { setReadOnly (true); }

Screen::~Screen() = default;

void Screen::updateVisibleRow (int row, const jam::Cell* src, int numCols) noexcept
{
    jassert (src != nullptr);
    setVisibleRow (row, jam::Cells::fromArray (src, numCols));
}

void Screen::append (const jam::Cell* const* rows, int rowCount, int numCols) noexcept
{
    jassert (rows != nullptr);

    for (int i { 0 }; i < rowCount; ++i)
    {
        if (rows[i] != nullptr)
            appendRow (jam::Cells::fromArray (rows[i], numCols));
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
