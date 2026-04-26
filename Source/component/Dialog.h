/**
 * @file Dialog.h
 * @brief Generic two-choice modal confirmation dialog.
 *
 * Terminal::Dialog is a juce::Component intended to be hosted inside
 * Terminal::Popup.  It renders a message label and two TextButton controls
 * ("Yes" / "No") and exposes callbacks for each choice plus Esc dismissal.
 *
 * ### Font tagging
 * All labels and buttons are tagged with `jam::ID::name` so
 * Terminal::LookAndFeel::getLabelFont / getTextButtonFont dispatches the
 * action-list name font without hardcoding a family here.
 *
 * ### Keyboard
 * Y / y fires `onYes`.  N / n fires `onNo`.  Escape is NOT handled here —
 * it propagates to ModalWindow which calls exitModalState(0), which in turn
 * fires Terminal::Popup's `onDismiss`.
 *
 * ### Sizing
 * Call getPreferredWidth() / getPreferredHeight() before Popup::show().
 * Dimensions are derived from content: message text width measured with the
 * action-list name font, button widths sized to fit "Yes" / "No" plus padding,
 * and height computed from line height plus vertical padding.
 *
 * @note All methods are called on the MESSAGE THREAD.
 *
 * @see Terminal::Popup
 * @see Terminal::LookAndFeel
 */

#pragma once
#include <JuceHeader.h>
#include "../lua/Engine.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class Dialog
 * @brief Generic two-choice modal confirmation body (message + Yes / No buttons).
 *
 * Construct with a message string, wire `onYes` and `onNo` callbacks, then
 * pass to Terminal::Popup::show().  Escape propagates upward — do not handle
 * it here.
 *
 * @par Thread context
 * MESSAGE THREAD — all public methods.
 *
 * @see Terminal::Popup
 */
class Dialog : public jam::NativeContextResource
{
public:
    /**
     * @brief Constructs the dialog with the given confirmation message.
     *
     * Tags all child components with the `jam::ID::name` font role so
     * LookAndFeel dispatches the action-list name font.  Keyboard focus is
     * requested so Y / N key events arrive without a mouse click first.
     *
     * @param message  The question to display to the user.
     * @note MESSAGE THREAD.
     */
    explicit Dialog (const juce::String& message);

    ~Dialog() override;

    /** @brief Callback invoked when the user confirms (Y key or Yes button click). */
    std::function<void()> onYes;

    /** @brief Callback invoked when the user declines (N key or No button click). */
    std::function<void()> onNo;

    /**
     * @brief Callback invoked when the dialog is dismissed without a choice (Esc).
     *
     * Wire this at the call site if dismissal requires additional cleanup.
     * Popup's own `onDismiss` fires on the same Esc path — this is provided
     * for call-site convenience only.
     */
    std::function<void()> onDismiss;

    /**
     * @brief Transparent background — Popup / ModalWindow provides the glass blur.
     * @note MESSAGE THREAD.
     */
    void paint (juce::Graphics& g) override;

    /**
     * @brief Lays out the message label above the two side-by-side buttons.
     * @note MESSAGE THREAD.
     */
    void resized() override;

    /**
     * @brief Handles Y / y → onYes and N / n → onNo key presses.
     *
     * Returns false for all other keys so Escape propagates to ModalWindow.
     *
     * @param key  The key press event.
     * @return true if consumed (Y or N), false otherwise.
     * @note MESSAGE THREAD.
     */
    bool keyPressed (const juce::KeyPress& key) override;

    /**
     * @brief Returns the preferred dialog width in logical pixels.
     *
     * Derived from the rendered message text width (action-list name font) and
     * the sum of button widths with horizontal padding.  Whichever is wider
     * determines the result, padded by `horizontalPadding` on each side.
     *
     * @return Preferred width in pixels.
     * @note MESSAGE THREAD.
     */
    int getPreferredWidth() const noexcept;

    /**
     * @brief Returns the preferred dialog height in logical pixels.
     *
     * Derived from message line height plus button height plus vertical padding.
     *
     * @return Preferred height in pixels.
     * @note MESSAGE THREAD.
     */
    int getPreferredHeight() const noexcept;

    /**
     * @brief Defers a keyboard focus grab via callAsync when the component becomes visible.
     *
     * Handles the case where the modal window is made visible after the component is
     * already in the hierarchy.  Uses `juce::MessageManager::callAsync` with a
     * `SafePointer` to defer one message-loop tick, ensuring the native peer has
     * fully realised focus before `grabKeyboardFocus` is called.
     *
     * @note MESSAGE THREAD.
     */
    void visibilityChanged() override;

    /**
     * @brief Defers a keyboard focus grab via callAsync when the hierarchy is established.
     *
     * Guards on `isShowing()` so the async dispatch fires only when the component is
     * already on-screen.  The `SafePointer` guard in the async lambda prevents a
     * use-after-free if the dialog is dismissed before the tick fires.
     *
     * @note MESSAGE THREAD.
     */
    void parentHierarchyChanged() override;

    /**
     * @brief Propagates the resolved LookAndFeel to all child components.
     *
     * Called by JUCE when this component's effective LookAndFeel changes (e.g.
     * after Terminal::ModalWindow is mounted by jam::ModalWindow and the
     * resolved LookAndFeel propagates down the component hierarchy).
     * Explicitly setting LookAndFeel on each button ensures getTextButtonFont
     * dispatch uses Terminal::LookAndFeel regardless of hierarchy-walk order.
     *
     * @note MESSAGE THREAD.
     */
    void lookAndFeelChanged() override;

private:
    //==========================================================================
    /** @brief Horizontal padding added to each side of the widest content line (pixels). */
    static constexpr int horizontalPadding { 24 };

    /** @brief Vertical padding above and below each content row (pixels). */
    static constexpr int verticalPadding { 12 };

    /** @brief Horizontal padding added to each side of button text (pixels). */
    static constexpr int buttonTextPadding { 20 };

    /** @brief Horizontal gap between the two buttons (pixels). */
    static constexpr int buttonGap { 12 };

    //==========================================================================
    // Access config via lua::Engine::getContext() at call sites.

    juce::Label     messageLabel;
    juce::TextButton yesButton { "Yes" };
    juce::TextButton noButton  { "No" };

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Dialog)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
