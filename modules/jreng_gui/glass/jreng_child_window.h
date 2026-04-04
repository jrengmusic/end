/**
 * @file jreng_child_window.h
 * @brief OS-level child/owned window attachment utility.
 *
 * Attaches a JUCE window as a child/owned window of another at the native
 * level.  The child window moves with and stays on top of its parent.
 *
 * - macOS: uses [NSWindow addChildWindow:ordered:].
 * - Windows: uses SetWindowLongPtr with GWLP_HWNDPARENT to set the owner.
 *
 * @see BackgroundBlur
 */

#if JUCE_MAC || JUCE_WINDOWS

namespace jreng
{
/*____________________________________________________________________________*/

struct ChildWindow
{
    /**
     * @brief Attaches a child component's window to a parent component's window.
     *
     * Both components must have a valid native peer (i.e. must be visible).
     * Call after setVisible(true) on both windows.
     *
     * @param child   The child/owned window component.
     * @param parent  The parent/owner window component.
     * @return @c true on success; @c false if either peer is unavailable.
     */
    static bool attach (juce::Component* child, juce::Component* parent);

    /**
     * @brief Detaches a child component's window from its parent.
     *
     * @param child  The child/owned window component to detach.
     * @return @c true on success; @c false if the peer is unavailable.
     */
    static bool detach (juce::Component* child);
};

/**_____________________________END_OF_NAMESPACE______________________________*/
}// namespace jreng

#endif
