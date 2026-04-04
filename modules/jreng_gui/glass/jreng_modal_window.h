/**
 * @file jreng_modal_window.h
 * @brief GlassWindow subclass with modal dialog semantics.
 *
 * ModalWindow combines GlassWindow's glassmorphism (blur, DWM corners,
 * deferred first-show) with modal input blocking and Escape dismiss.
 * Replaces the need to subclass juce::DialogWindow for glass modals.
 *
 * @par Usage
 * Subclass ModalWindow, call setGlass() and setVisible(true), then
 * enterModalState(true).  Escape and close button call closeButtonPressed()
 * which exits modal state and invokes the onModalDismissed callback.
 *
 * @see GlassWindow
 */

namespace jreng
{
/*____________________________________________________________________________*/

/**
 * @class ModalWindow
 * @brief GlassWindow with modal semantics: Escape dismiss, input blocking.
 *
 * Provides the common pattern shared by all modal glass windows:
 * - Escape key exits modal state
 * - Close button exits modal state
 * - Clicks outside bring window to front (does not dismiss)
 * - Optional dismiss callback
 *
 * @see GlassWindow
 */
class ModalWindow : public GlassWindow
{
public:
    /**
     * @brief Constructs a modal glass window.
     *
     * @param mainComponent      Content component; ownership transferred.
     * @param name               Window title (empty for borderless).
     * @param alwaysOnTop        Whether the window floats above others.
     * @param showWindowButtons  Whether native close/min/max buttons are shown.
     */
    ModalWindow (juce::Component* mainComponent,
                 const juce::String& name,
                 bool alwaysOnTop,
                 bool showWindowButtons = false);

    ~ModalWindow() override = default;

    void closeButtonPressed() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void inputAttemptWhenModal() override;

    /** @brief Callback invoked when the modal window is dismissed. */
    std::function<void()> onModalDismissed;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModalWindow)
};

/**_____________________________END_OF_NAMESPACE______________________________*/
} /** namespace jreng */

