/**
 * @file jreng_background_blur.h
 * @brief Static utility for applying native macOS window background blur.
 *
 * @par Overview
 * BackgroundBlur provides two strategies for frosted-glass blur on macOS:
 *
 * | Strategy              | API                                   | Radius control |
 * |-----------------------|---------------------------------------|----------------|
 * | @c Type::coreGraphics | CGSSetWindowBackgroundBlurRadius (SPI) | Variable       |
 * | @c Type::nsVisualEffect | NSVisualEffectView                  | System-managed |
 *
 * The CoreGraphics path uses a private system API loaded at runtime via
 * @c dlsym().  @c isCoreGraphicsAvailable() must return @c true before
 * @c apply() is called with @c Type::coreGraphics.
 *
 * @par Platform
 * This entire header is compiled only when @c JUCE_MAC is defined.
 *
 * @par Usage
 * @code
 * if (BackgroundBlur::isCoreGraphicsAvailable())
 *     BackgroundBlur::apply (myComponent, 20.0f);
 * else
 *     BackgroundBlur::apply (myComponent, 0.0f, BackgroundBlur::Type::nsVisualEffect);
 * @endcode
 *
 * @see GlassWindow
 * @see GlassComponent
 */

#if JUCE_MAC

namespace jreng
{
/*____________________________________________________________________________*/

/**
 * @struct BackgroundBlur
 * @brief Static-only utility for macOS native window blur and button hiding.
 *
 * All methods are @c static; the struct is never instantiated.
 *
 * @note macOS only — the entire struct is conditionally compiled under
 *       @c JUCE_MAC.
 *
 * @see GlassWindow::handleAsyncUpdate()
 * @see GlassComponent::handleAsyncUpdate()
 */
struct BackgroundBlur
{
    /**
     * @enum Type
     * @brief Selects the blur implementation strategy.
     *
     * @var Type::coreGraphics
     *      Uses the private CoreGraphics SPI (CGSSetWindowBackgroundBlurRadius)
     *      for variable-radius blur.  Preferred when available.
     *
     * @var Type::nsVisualEffect
     *      Falls back to NSVisualEffectView with system-managed blur intensity.
     *      Always available on macOS 10.10+.
     */
    enum class Type
    {
        coreGraphics,   ///< Private CGS SPI — variable radius, preferred.
        nsVisualEffect  ///< NSVisualEffectView — system radius, always available.
    };

    /**
     * @brief Checks whether the CoreGraphics private blur API is available.
     *
     * Performs a one-time @c dlsym() lookup for @c CGSMainConnectionID and
     * @c CGSSetWindowBackgroundBlurRadius.  The result is cached in a
     * @c static const bool.
     *
     * @return @c true  if both symbols are found in the default dynamic linker
     *                  namespace (i.e. the private API is present).
     * @return @c false if either symbol is missing (older / future macOS).
     *
     * @note Thread-safe after first call due to static initialisation.
     */
    static const bool isCoreGraphicsAvailable();

    /**
     * @brief Applies a background blur to the native window of @p component.
     *
     * Dispatches to @c applyBackgroundBlur() or @c applyNSVisualEffect()
     * depending on @p type.  When @c Type::coreGraphics is requested but the
     * API is unavailable, falls back to @c applyNSVisualEffect() automatically.
     *
     * @param component   JUCE component whose native NSWindow receives the blur.
     *                    Must not be @c nullptr and must have a valid peer.
     * @param blurRadius  Blur radius in points.  Ignored for
     *                    @c Type::nsVisualEffect (system controls intensity).
     *                    Typical range for @c Type::coreGraphics: 10–40.
     * @param type        Blur strategy.  Defaults to @c Type::coreGraphics.
     *
     * @return @c true  if the blur was applied successfully.
     * @return @c false if the peer or NSWindow could not be obtained, or if
     *                  the requested API is unavailable.
     *
     * @note Must be called on the message thread after the native peer exists.
     *
     * @see isCoreGraphicsAvailable()
     * @see applyBackgroundBlur()
     * @see applyNSVisualEffect()
     */
    static const bool
    apply (juce::Component* component, float blurRadius, Type type = Type::coreGraphics);

    /**
     * @brief Registers a callback invoked when the native window is closed.
     *
     * Installs a @c GlassWindowDelegate as the NSWindow delegate and stores
     * @p callback in a file-scope global.  The callback is called from
     * @c -[GlassWindowDelegate windowShouldClose:].
     *
     * @param callback  Callable invoked on window close.  Captured by move;
     *                  must be safe to call from the main thread.
     *
     * @note Only one callback is supported at a time; subsequent calls
     *       overwrite the previous one.
     */
    static void setCloseCallback (std::function<void()> callback);

    /**
     * @brief Hides the native close, minimise, and zoom (traffic-light) buttons.
     *
     * Retrieves the NSWindow from @p component's peer and sets each standard
     * button's @c hidden property to @c YES.
     *
     * @param component  JUCE component whose native window buttons are hidden.
     *                   Must not be @c nullptr and must have a valid peer.
     *
     * @note Has no effect if the peer or NSWindow cannot be obtained.
     * @note macOS only.
     *
     * @see GlassWindow::handleAsyncUpdate()
     */
    static void hideWindowButtons (juce::Component* component);

    /**
     * @brief Makes the current NSOpenGLContext surface non-opaque.
     *
     * Sets @c NSOpenGLContextParameterSurfaceOpacity to 0 on the current
     * OpenGL context, allowing the blur layer beneath to show through OpenGL
     * content.
     *
     * @return @c true  if a current NSOpenGLContext was found and configured.
     * @return @c false if no current context exists (@c [NSOpenGLContext
     *                  currentContext] returned @c nil).
     *
     * @note Must be called from the OpenGL render thread while the context
     *       is current.
     */
    static const bool enableGLTransparency();

private:
    /**
     * @brief Applies blur via the CoreGraphics private SPI.
     *
     * Configures the NSWindow (opaque=NO, clear background, hidden title,
     * transparent titlebar, full-size content view) then calls
     * @c CGSSetWindowBackgroundBlurRadius() with the given @p blurRadius.
     *
     * @param component   Component whose native NSWindow is configured.
     * @param blurRadius  Blur radius in points forwarded to the CGS API.
     *
     * @return @c true on success; @c false if peer or window is unavailable.
     *
     * @note Uses private Apple SPI — behaviour may change across macOS versions.
     *
     * @see isCoreGraphicsAvailable()
     */
    static const bool applyBackgroundBlur (juce::Component* component, float blurRadius);

    /**
     * @brief Applies blur via NSVisualEffectView inserted behind content.
     *
     * Configures the NSWindow identically to @c applyBackgroundBlur(), then
     * allocates an @c NSVisualEffectView (HUDWindow material, active state,
     * behind-window blending) and inserts it below all existing subviews.
     *
     * @param component   Component whose native NSWindow receives the effect.
     * @param blurRadius  Accepted for API symmetry; ignored — NSVisualEffectView
     *                    does not expose a radius parameter.
     *
     * @return @c true on success; @c false if peer or window is unavailable.
     *
     * @note The NSVisualEffectView is autoreleased; the window retains it via
     *       the subview hierarchy.
     */
    static const bool applyNSVisualEffect (juce::Component* component, float blurRadius);
};

/**_____________________________END_OF_NAMESPACE______________________________*/
}// namespace jreng

#endif
