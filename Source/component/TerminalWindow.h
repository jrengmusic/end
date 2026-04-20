/**
 * @file TerminalWindow.h
 * @brief END-specific document window subclass of jam::Window.
 *
 * Terminal::Window extends jam::Window with resize-tracking — a lightweight
 * overlay that exposes whether the user is actively dragging a resize handle.
 * This is used by MainComponent::resized() to suppress ruler overlays during
 * live resize.
 *
 * All glass, renderer, and native-context management remains in jam::Window.
 * Only END-specific window behaviour lives here.
 *
 * @see jam::Window
 * @see MainComponent::resized
 */

#pragma once
#include <JuceHeader.h>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class Window
 * @brief END document window — jam::Window + resize tracking.
 *
 * Inherits the full glassmorphism, GL renderer, and native-context API from
 * jam::Window.  Adds resizeStart/resizeEnd overrides that set the
 * `userResizing` flag, queryable via isUserResizing().
 *
 * @note MESSAGE THREAD — all methods called on the JUCE message thread.
 * @see jam::Window
 */
class Window : public jam::Window
{
public:
    using jam::Window::Window;

    /** @brief Returns true while the user is actively dragging a resize handle. */
    bool isUserResizing() const noexcept { return userResizing; }

    void resizeStart() override;
    void resizeEnd() override;

private:
    /** @brief True while the user is actively dragging a resize handle. */
    bool userResizing { false };

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Window)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
