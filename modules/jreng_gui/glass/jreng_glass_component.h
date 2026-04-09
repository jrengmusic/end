/**
 * @file jreng_glass_component.h
 * @brief Lightweight JUCE Component that applies a native background blur.
 *
 * @par Overview
 * GlassComponent is a thin wrapper around juce::Component that triggers a
 * fixed-radius (20 pt) background blur via BackgroundBlur::enable() the first
 * time the component becomes visible.  Blur application is deferred through
 * juce::AsyncUpdater to ensure the native peer exists before any platform
 * calls are made.
 *
 * @par Typical Use
 * Use GlassComponent as a base class (or direct instance) for any UI panel
 * that should appear frosted-glass over the desktop.
 *
 * @code
 * class MyPanel : public jreng::GlassComponent
 * {
 *     // paint(), resized(), etc.
 * };
 * @endcode
 *
 * @note The blur radius is fixed at 20 pt.  For a configurable radius use
 *       Window instead.
 *
 * @see Window
 * @see BackgroundBlur
 */

namespace jreng
{
/*____________________________________________________________________________*/

/**
 * @class GlassComponent
 * @brief juce::Component that auto-applies a 20 pt background blur on show.
 *
 * Inherits juce::Component publicly and juce::AsyncUpdater privately.
 * The async update is triggered at most once (guarded by @c blurApplied).
 *
 * @par Platform Notes
 * The inner @c OS struct exposes @c makeGlass() only on macOS (@c JUCE_MAC).
 * On other platforms @c handleAsyncUpdate() calls BackgroundBlur::enable()
 * which is itself a no-op.
 *
 * @see BackgroundBlur::enable()
 */
class GlassComponent : public juce::Component,
                       private juce::AsyncUpdater
{
public:
    /**
     * @brief Default constructor.  No blur is applied until the component
     *        becomes visible for the first time.
     */
    GlassComponent();

    /**
     * @brief Triggers deferred blur on first visibility change.
     *
     * Sets @c blurApplied to @c true immediately (preventing re-entry) and
     * calls @c triggerAsyncUpdate() so that @c handleAsyncUpdate() runs on
     * the message thread once the native peer is ready.
     *
     * @note Subsequent visibility changes are ignored.
     *
     * @see handleAsyncUpdate()
     */
    void visibilityChanged() override;

    /**
     * @brief Applies the background blur on the message thread.
     *
     * Calls BackgroundBlur::enable() with a fixed radius of 20 pt.
     *
     * @note Called automatically by JUCE's AsyncUpdater; do not invoke
     *       directly.
     *
     * @see BackgroundBlur::enable()
     */
    void handleAsyncUpdate() override;

private:
    /** @brief Guards against applying the blur more than once. */
    bool blurApplied { false };

    /**
     * @struct OS
     * @brief Platform-specific glass helpers (macOS only).
     *
     * Provides a static @c makeGlass() method compiled only when
     * @c JUCE_MAC is defined.  Separating platform code into a nested
     * struct keeps the class declaration clean on non-Apple targets.
     */
    struct OS
    {
#if JUCE_MAC
        /**
         * @brief Applies a native macOS blur to @p component.
         *
         * Delegates to BackgroundBlur::enable() with the given @p blurRadius.
         *
         * @param component   The JUCE component whose native window receives
         *                    the blur.  Must not be @c nullptr.
         * @param blurRadius  Blur radius in points.  Typical range: 10–40.
         *
         * @note macOS only — not compiled on other platforms.
         */
        static void makeGlass (juce::Component* component, float blurRadius);
#endif
    };
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlassComponent)
};

/**_____________________________END OF_NAMESPACE______________________________*/
} /** namespace jreng */
