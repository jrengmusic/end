/**
 * @file jreng_background_blur.mm
 * @brief Objective-C++ implementation of BackgroundBlur — macOS native blur.
 *
 * @par Implementation Notes
 * This file is compiled as Objective-C++ (.mm) because it interacts directly
 * with AppKit (NSWindow, NSVisualEffectView) and the CoreGraphics private SPI.
 *
 * @par CoreGraphics Private SPI
 * @c CGSSetWindowBackgroundBlurRadius is a private CoreGraphics function not
 * declared in any public header.  It is loaded at runtime via @c dlsym() to
 * avoid link-time dependency on a private framework symbol.  The function
 * signature used here matches the observed ABI on macOS 10.15–14.x:
 * @code
 * int32_t CGSSetWindowBackgroundBlurRadius(CGSConnectionID, int32_t windowNumber, int64_t radius);
 * @endcode
 *
 * @par NSVisualEffectView Fallback
 * When the private SPI is unavailable, an @c NSVisualEffectView is inserted
 * behind the window's content view.  The system controls blur intensity;
 * the @p blurRadius parameter is ignored in this path.
 *
 * @par Window Delegate
 * @c GlassWindowDelegate is an Objective-C class defined at file scope
 * (required by the Objective-C runtime).  A single instance is stored in
 * @c g_windowDelegate and a single close callback in @c g_windowCloseCallback.
 *
 * @note macOS only — the entire file is conditionally compiled under
 *       @c JUCE_MAC.
 *
 * @see jreng_background_blur.h
 * @see GlassWindow
 * @see GlassComponent
 */

#if JUCE_MAC
#include <dlfcn.h>

/*____________________________________________________________________________*/

/**
 * @brief File-scope close callback invoked by GlassWindowDelegate.
 *
 * Stored and replaced by BackgroundBlur::setCloseCallback().  Declared at
 * global scope because Objective-C @c @implementation blocks cannot capture
 * C++ lambdas directly.
 */
static std::function<void()> g_windowCloseCallback;

/**
 * @interface GlassWindowDelegate
 * @brief NSWindowDelegate that forwards close events to a C++ callback.
 *
 * Installed as the NSWindow delegate by BackgroundBlur::setCloseCallback().
 * Only @c windowShouldClose: is implemented; all other delegate methods use
 * the default NSObject behaviour.
 */
@interface GlassWindowDelegate : NSObject<NSWindowDelegate>
@end

@implementation GlassWindowDelegate

/**
 * @brief Called by AppKit when the user requests window close.
 *
 * Invokes @c g_windowCloseCallback if one has been registered, then returns
 * @c YES to allow the window to close normally.
 *
 * @param sender  The NSWindow that is about to close.
 * @return @c YES — always permits the close; the callback handles any
 *         application-level veto logic.
 */
- (BOOL)windowShouldClose:(NSWindow*)sender
{
    if (g_windowCloseCallback)
        g_windowCloseCallback();
    return YES;
}

@end

/**
 * @brief Retained instance of GlassWindowDelegate used as NSWindow delegate.
 *
 * Initialised to @c nil; set by BackgroundBlur::setCloseCallback().
 */
static GlassWindowDelegate* g_windowDelegate = nil;

/*____________________________________________________________________________*/

namespace jreng
{

/**
 * @typedef CGSConnectionID
 * @brief Opaque integer handle for a CoreGraphics server connection.
 *
 * Matches the private typedef used internally by CoreGraphics on macOS.
 */
typedef intptr_t CGSConnectionID;

/**
 * @typedef CGSMainConnectionID_Func
 * @brief Function pointer type for the private @c CGSMainConnectionID symbol.
 *
 * Signature: @code CGSConnectionID CGSMainConnectionID(); @endcode
 */
using CGSMainConnectionID_Func = CGSConnectionID (*)();

/**
 * @typedef CGSSetWindowBackgroundBlurRadius_Func
 * @brief Function pointer type for the private @c CGSSetWindowBackgroundBlurRadius symbol.
 *
 * Signature:
 * @code
 * int32_t CGSSetWindowBackgroundBlurRadius(CGSConnectionID connection,
 *                                          int32_t         windowNumber,
 *                                          int64_t         blurRadius);
 * @endcode
 */
using CGSSetWindowBackgroundBlurRadius_Func = int32_t (*) (CGSConnectionID,
                                                           int32_t,
                                                           int64_t);

/**____________________________________________________________________________*/

/**
 * @brief Checks runtime availability of the CoreGraphics blur SPI.
 *
 * Uses @c dlsym(RTLD_DEFAULT, ...) to probe for both required symbols without
 * linking against them.  The result is cached in a @c static const bool so
 * the lookup happens only once per process lifetime.
 *
 * @return @c true  if @c CGSMainConnectionID and
 *                  @c CGSSetWindowBackgroundBlurRadius are both present.
 * @return @c false otherwise.
 */
const bool BackgroundBlur::isCoreGraphicsAvailable()
{
    static const bool result =
        (dlsym (RTLD_DEFAULT, "CGSMainConnectionID") != nullptr
         && dlsym (RTLD_DEFAULT, "CGSSetWindowBackgroundBlurRadius")
                != nullptr);
    return result;
}

/**
 * @brief Dispatches to the appropriate blur implementation.
 *
 * When @c Type::coreGraphics is requested:
 * - Asserts (debug) that the SPI is available.
 * - Calls @c applyBackgroundBlur() if available, else falls back to
 *   @c applyNSVisualEffect().
 *
 * When @c Type::nsVisualEffect is requested:
 * - Calls @c applyNSVisualEffect() directly.
 *
 * @param component   JUCE component whose native NSWindow receives the blur.
 * @param blurRadius  Radius in points (CoreGraphics path only).
 * @param type        Blur strategy; defaults to @c Type::coreGraphics.
 *
 * @return @c true on success; @c false if the window could not be obtained
 *         or the API is unavailable.
 */
const bool
BackgroundBlur::apply (juce::Component* component, float blurRadius, juce::Colour tint, Type type)
{
    switch (type)
    {
        case Type::coreGraphics:
        {
            jassert (isCoreGraphicsAvailable());

            if (isCoreGraphicsAvailable())
                return applyBackgroundBlur (component, blurRadius, tint);
            else
                return applyNSVisualEffect (component, blurRadius, tint);
        }

        case Type::nsVisualEffect:
            return applyNSVisualEffect (component, blurRadius, tint);
    }

    return false;
}

/**
 * @brief Applies variable-radius blur via the CoreGraphics private SPI.
 *
 * Steps:
 * 1. Obtains the NSView and NSWindow from the JUCE peer.
 * 2. Configures the window: non-opaque, clear background, hidden title,
 *    transparent titlebar, full-size content view mask.
 * 3. Loads @c CGSMainConnectionID and @c CGSSetWindowBackgroundBlurRadius
 *    via @c dlsym().
 * 4. Calls @c CGSSetWindowBackgroundBlurRadius() with the window number and
 *    the requested @p blurRadius.
 *
 * @param component   Component whose native NSWindow is configured.
 * @param blurRadius  Blur radius in points forwarded to the CGS function.
 *
 * @return @c true on success; @c false if peer or NSWindow is @c nil.
 *
 * @note The window is styled in-place — no window swap or view relocation
 *       is performed, preserving JUCE's peer/view ownership model.
 */
const bool BackgroundBlur::applyBackgroundBlur (juce::Component* component,
                                                float blurRadius,
                                                juce::Colour tint)
{
    if (auto* peer { component->getPeer() })
    {
        NSView* view = (NSView*) peer->getNativeHandle();
        NSWindow* window = [view window];

        if (window != nil)
        {
            // Style existing JUCE window — NO window swap, NO view move
            [window setOpaque:NO];
            [window setBackgroundColor:[NSColor colorWithRed:tint.getFloatRed()
                                                      green:tint.getFloatGreen()
                                                       blue:tint.getFloatBlue()
                                                      alpha:tint.getFloatAlpha()]];
            [window setTitleVisibility:NSWindowTitleHidden];
            [window setTitlebarAppearsTransparent:YES];
            [window setStyleMask:window.styleMask |
                                 NSWindowStyleMaskFullSizeContentView];

            auto CGSMainConnectionID = (CGSMainConnectionID_Func) dlsym (
                RTLD_DEFAULT, "CGSMainConnectionID");
            auto CGSSetWindowBackgroundBlurRadius =
                (CGSSetWindowBackgroundBlurRadius_Func) dlsym (
                    RTLD_DEFAULT, "CGSSetWindowBackgroundBlurRadius");

            auto connection = CGSMainConnectionID();
            CGSSetWindowBackgroundBlurRadius (
                connection, [window windowNumber], (int64_t) blurRadius);

            return true;
        }
    }

    return false;
}

/**
 * @brief Applies system-managed blur via NSVisualEffectView.
 *
 * Steps:
 * 1. Obtains the NSView and NSWindow from the JUCE peer.
 * 2. Configures the window identically to @c applyBackgroundBlur().
 * 3. Allocates an @c NSVisualEffectView sized to the content view frame.
 * 4. Configures: HUDWindow material, active state, behind-window blending,
 *    width+height autoresizing mask.
 * 5. Inserts the view below all existing content subviews.
 *
 * @param component   Component whose native NSWindow receives the effect.
 * @param blurRadius  Accepted for API symmetry; not forwarded to AppKit
 *                    (NSVisualEffectView does not expose a radius parameter).
 *
 * @return @c true on success; @c false if peer or NSWindow is @c nil.
 *
 * @note The NSVisualEffectView is autoreleased; the window retains it through
 *       the subview hierarchy.
 * @note The window is styled in-place — no window swap or view relocation.
 */
const bool BackgroundBlur::applyNSVisualEffect (juce::Component* component,
                                                float blurRadius,
                                                juce::Colour tint)
{
    if (auto* peer { component->getPeer() })
    {
        NSView* view = (NSView*) peer->getNativeHandle();
        NSWindow* window = [view window];

        if (window != nil)
        {
            // Style existing JUCE window — NO window swap, NO view move
            [window setOpaque:NO];
            [window setBackgroundColor:[NSColor colorWithRed:tint.getFloatRed()
                                                      green:tint.getFloatGreen()
                                                       blue:tint.getFloatBlue()
                                                      alpha:tint.getFloatAlpha()]];
            [window setTitleVisibility:NSWindowTitleHidden];
            [window setTitlebarAppearsTransparent:YES];
            [window setStyleMask:window.styleMask |
                                 NSWindowStyleMaskFullSizeContentView];

            NSRect frame = [[window contentView] frame];
            NSVisualEffectView* visualEffect =
                [[NSVisualEffectView alloc] initWithFrame:frame];
            [visualEffect setMaterial:NSVisualEffectMaterialHUDWindow];
            [visualEffect setState:NSVisualEffectStateActive];
            [visualEffect
                setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
            [visualEffect
                setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

            // Insert visual effect behind existing content
            [[window contentView] addSubview:visualEffect
                                  positioned:NSWindowBelow
                                  relativeTo:nil];

            return true;
        }
    }

    return false;
}

/**
 * @brief Makes the current NSOpenGLContext surface non-opaque.
 *
 * Retrieves the current NSOpenGLContext and sets
 * @c NSOpenGLContextParameterSurfaceOpacity to 0, allowing the blur layer
 * beneath to show through OpenGL-rendered content.
 *
 * @return @c true  if a current context was found and configured.
 * @return @c false if @c [NSOpenGLContext currentContext] returned @c nil.
 *
 * @note Must be called from the OpenGL render thread while the context is
 *       current (e.g. inside @c OpenGLRenderer::renderOpenGL()).
 */
const bool BackgroundBlur::enableGLTransparency()
{
    NSOpenGLContext* ctx = [NSOpenGLContext currentContext];

    if (ctx == nil)
        return false;

    GLint opaque = 0;
    [ctx setValues:&opaque forParameter:NSOpenGLContextParameterSurfaceOpacity];

    return true;
}

/**
 * @brief Registers a C++ callback for native window close events.
 *
 * Moves @p callback into the file-scope @c g_windowCloseCallback.  The
 * callback is invoked by @c GlassWindowDelegate when AppKit calls
 * @c -windowShouldClose:.
 *
 * @param callback  Callable (e.g. lambda) invoked on window close.  Ownership
 *                  is transferred via @c std::move.  Pass an empty
 *                  @c std::function to clear the callback.
 *
 * @note Only one callback is active at a time; subsequent calls replace the
 *       previous one.
 * @note The callback is invoked on the main thread.
 */
void BackgroundBlur::setCloseCallback (std::function<void()> callback)
{
    g_windowCloseCallback = std::move (callback);
}

/**
 * @brief Hides the native close, minimise, and zoom (traffic-light) buttons.
 *
 * Retrieves the NSWindow from @p component's peer and sets @c hidden = YES
 * on each of the three standard window buttons.
 *
 * @param component  JUCE component whose native window buttons are hidden.
 *                   Must not be @c nullptr.  Has no effect if the peer or
 *                   NSWindow cannot be obtained.
 *
 * @note Typically called from GlassWindow::handleAsyncUpdate() after blur is
 *       applied, so the window is fully configured before buttons are hidden.
 *
 * @see GlassWindow::handleAsyncUpdate()
 */
void BackgroundBlur::hideWindowButtons (juce::Component* component)
{
    if (auto* peer { component->getPeer() })
    {
        NSView* view = (NSView*) peer->getNativeHandle();
        NSWindow* window = [view window];

        if (window != nil)
        {
            [[window standardWindowButton:NSWindowCloseButton] setHidden:YES];
            [[window standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
            [[window standardWindowButton:NSWindowZoomButton] setHidden:YES];
        }
    }
}

/**_____________________________END_OF_NAMESPACE______________________________*/
}// namespace jreng

#endif
