/**
 * @file jreng_child_window.cpp
 * @brief Windows implementation of ChildWindow — HWND owner attachment.
 */

#if JUCE_WINDOWS

#include <Windows.h>

namespace jreng
{
/*____________________________________________________________________________*/

bool ChildWindow::attach (juce::Component* child, juce::Component* parent)
{
    bool result { false };

    if (child != nullptr and parent != nullptr)
    {
        auto* childPeer  { child->getPeer() };
        auto* parentPeer { parent->getPeer() };

        if (childPeer != nullptr and parentPeer != nullptr)
        {
            HWND childHwnd  { static_cast<HWND> (childPeer->getNativeHandle()) };
            HWND parentHwnd { static_cast<HWND> (parentPeer->getNativeHandle()) };

            if (childHwnd != nullptr and parentHwnd != nullptr)
            {
                SetWindowLongPtr (childHwnd, GWLP_HWNDPARENT,
                                  reinterpret_cast<LONG_PTR> (parentHwnd));
                result = true;
            }
        }
    }

    return result;
}

bool ChildWindow::detach (juce::Component* child)
{
    bool result { false };

    if (child != nullptr)
    {
        auto* childPeer { child->getPeer() };

        if (childPeer != nullptr)
        {
            HWND childHwnd { static_cast<HWND> (childPeer->getNativeHandle()) };

            if (childHwnd != nullptr)
            {
                SetWindowLongPtr (childHwnd, GWLP_HWNDPARENT, 0);
                result = true;
            }
        }
    }

    return result;
}

/**_____________________________END_OF_NAMESPACE______________________________*/
}// namespace jreng

#endif
