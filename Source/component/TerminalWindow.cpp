/**
 * @file TerminalWindow.cpp
 * @brief Implementation of Terminal::Window — END-specific document window.
 *
 * Adds resize tracking (resizeStart/resizeEnd) on top of jam::Window.
 * All glass, renderer, and native-context logic delegates to the base.
 *
 * @see TerminalWindow.h
 * @see jam::Window
 */

#include "TerminalWindow.h"

namespace Terminal
{ /*____________________________________________________________________________*/

void Window::resizeStart()
{
    userResizing = true;
}

void Window::resizeEnd()
{
    userResizing = false;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
