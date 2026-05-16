#include "Screen.h"

namespace Terminal
{
/*____________________________________________________________________________*/

Screen::Screen() noexcept
    : jam::TextEditor ({}, 2)
{
    setWantsKeyboardFocus (false);
}

Screen::~Screen() = default;

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
