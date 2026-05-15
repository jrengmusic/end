#include "Screen.h"

namespace Terminal
{
/*____________________________________________________________________________*/

Screen::Screen() noexcept
{
    setReadOnly (true);
    buffers.add (new jam::Buffer<jam::Cell>()); // index 1 = alternate
    buffers.add (new jam::Buffer<jam::Cell>()); // index 2 = live viewport
}

Screen::~Screen() = default;

void Screen::append (const jam::Cell* const* rows, int rowCount, int numCols) noexcept
{
    jassert (rows != nullptr);

    for (int i { 0 }; i < rowCount; ++i)
    {
        if (rows[i] != nullptr)
            appendRow (rows[i], numCols);
    }
}

void Screen::setLiveDimensions (int numRows, int numCols) noexcept
{
    jassert (numRows > 0 and numCols > 0);
    buffers[2]->setSize (1, numRows, numCols, false, true, true);
}

void Screen::updateLiveRows (const jam::Cell* const* rows, int numRows, int numCols) noexcept
{
    jassert (rows != nullptr);

    const int copyRows { juce::jmin (numRows, buffers[2]->getNumRows()) };
    const int copyCols { juce::jmin (numCols, buffers[2]->getNumCols()) };

    for (int r { 0 }; r < copyRows; ++r)
    {
        if (rows[r] != nullptr)
            buffers[2]->copyFrom (0, r, rows[r], copyCols);
    }

    calc();
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
