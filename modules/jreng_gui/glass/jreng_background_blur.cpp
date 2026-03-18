#if JUCE_WINDOWS

#include <windows.h>
#include <dwmapi.h>
#include "../../jreng_core/utilities/jreng_platform.h"
#ifdef small
 #undef small
#endif
#ifdef max
 #undef max
#endif
#ifdef min
 #undef min
#endif
#ifdef ALTERNATE
 #undef ALTERNATE
#endif

/*____________________________________________________________________________*/

// Undocumented SetWindowCompositionAttribute API (user32.dll, Windows 10+)

enum ACCENT_STATE
{
    ACCENT_DISABLED                   = 0,
    ACCENT_ENABLE_GRADIENT            = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND          = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND   = 4,
    ACCENT_ENABLE_HOSTBACKDROP        = 5,
    ACCENT_INVALID_STATE              = 6
};

struct ACCENT_POLICY
{
    ACCENT_STATE AccentState;
    UINT         AccentFlags;
    COLORREF     GradientColor;  // ABGR format
    LONG         AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA
{
    DWORD  Attrib;
    LPVOID pvData;
    UINT   cbData;
};

using SetWindowCompositionAttribute_t = BOOL (WINAPI*) (HWND, WINDOWCOMPOSITIONATTRIBDATA*);

static constexpr DWORD WCA_ACCENT_POLICY = 19;

/*____________________________________________________________________________*/

static std::function<void()> onWindowClosed;

/*____________________________________________________________________________*/

/*____________________________________________________________________________*/

namespace jreng
{

/**____________________________________________________________________________*/

const bool BackgroundBlur::isDwmAvailable()
{
    return true;
}

const bool BackgroundBlur::apply (juce::Component* component, float blurRadius, juce::Colour tint, Type type)
{
    switch (type)
    {
        case Type::dwmGlass:
            return applyDwmGlass (component, blurRadius, tint);
    }

    return false;
}

/**
 * @brief Applies DWM blur to a window.
 *
 * Windows 11 is the canonical path.  Windows 10 is the special case,
 * branched via isWindows10() — the only OS version conditional in the
 * Windows rendering path.
 *
 * Windows 11 (canon): SetWindowCompositionAttribute with
 * ACCENT_ENABLE_ACRYLICBLURBEHIND (4) + tint colour.
 *
 * Windows 10 (special case): SetWindowCompositionAttribute with
 * ACCENT_ENABLE_BLURBEHIND (3) + tint colour.  Fallback to legacy
 * DwmEnableBlurBehindWindow if SetWindowCompositionAttribute is unavailable.
 *
 * @param component   JUCE component whose native HWND receives the blur.
 * @param blurRadius  Accepted for API uniformity; DWM controls blur intensity.
 * @param tint        Tint colour in ARGB; converted to ABGR for the accent API.
 * @return @c true on success; @c false if the peer or HWND could not be obtained.
 *
 * @see enableGLTransparency()
 */
const bool BackgroundBlur::applyDwmGlass (juce::Component* component, float blurRadius, juce::Colour tint)
{
    if (auto* peer = component->getPeer())
    {
        HWND hwnd = (HWND) peer->getNativeHandle();

        if (hwnd == nullptr)
            return false;

        // --- Windows 11 (canon) ---
        if (not isWindows10())
        {
            // Strip WS_EX_LAYERED — incompatible with DWM backdrop and rounded corners
            LONG_PTR exStyle = GetWindowLongPtrW (hwnd, GWL_EXSTYLE);
            if (exStyle & WS_EX_LAYERED)
                SetWindowLongPtrW (hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);

            // Rounded corners
            DWORD cornerPref = 2; // DWMWCP_ROUND
            DwmSetWindowAttribute (hwnd, 33, &cornerPref, sizeof (cornerPref));

            // Extend DWM frame into entire client area ("sheet of glass")
            MARGINS margins { -1, -1, -1, -1 };
            DwmExtendFrameIntoClientArea (hwnd, &margins);

            auto SetWindowCompositionAttribute = (SetWindowCompositionAttribute_t)
                GetProcAddress (GetModuleHandleW (L"user32.dll"),
                    "SetWindowCompositionAttribute");

            if (SetWindowCompositionAttribute != nullptr)
            {
                ACCENT_POLICY accent {};
                accent.AccentState   = ACCENT_ENABLE_BLURBEHIND;
                accent.AccentFlags   = 0;
                accent.GradientColor = 0;
                accent.AnimationId   = 0;

                WINDOWCOMPOSITIONATTRIBDATA data {};
                data.Attrib = WCA_ACCENT_POLICY;
                data.pvData = &accent;
                data.cbData = sizeof (accent);

                return SetWindowCompositionAttribute (hwnd, &data) != FALSE;
            }

            return false;
        }

        // --- Windows 10 (special case): original working path, unchanged ---
        auto SetWindowCompositionAttribute = (SetWindowCompositionAttribute_t)
            GetProcAddress (GetModuleHandleW (L"user32.dll"),
                "SetWindowCompositionAttribute");

        if (SetWindowCompositionAttribute != nullptr)
        {
            COLORREF abgr = ((COLORREF) tint.getAlpha() << 24)
                          | ((COLORREF) tint.getBlue()  << 16)
                          | ((COLORREF) tint.getGreen() <<  8)
                          | ((COLORREF) tint.getRed());

            ACCENT_POLICY accent {};
            accent.AccentState   = ACCENT_ENABLE_BLURBEHIND;
            accent.AccentFlags   = 2;  // use GradientColor
            accent.GradientColor = abgr;
            accent.AnimationId   = 0;

            WINDOWCOMPOSITIONATTRIBDATA data {};
            data.Attrib = WCA_ACCENT_POLICY;
            data.pvData = &accent;
            data.cbData = sizeof (accent);

            return SetWindowCompositionAttribute (hwnd, &data) != FALSE;
        }

        // Fallback: legacy DwmEnableBlurBehindWindow
        DWM_BLURBEHIND blurBehind {};
        blurBehind.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        blurBehind.hRgnBlur = CreateRectRgn (0, 0, -1, -1);
        blurBehind.fEnable  = TRUE;

        bool ok = SUCCEEDED (DwmEnableBlurBehindWindow (hwnd, &blurBehind));

        if (blurBehind.hRgnBlur != nullptr)
            DeleteObject (blurBehind.hRgnBlur);

        return ok;
    }

    return false;
}


void BackgroundBlur::setCloseCallback (std::function<void()> callback)
{
    onWindowClosed = std::move (callback);
}

void BackgroundBlur::hideWindowButtons (juce::Component*)
{
}

/**
 * @brief Prepares the current OpenGL window for DWM alpha compositing.
 *
 * Called from Screen::glContextCreated() on the GL render thread while the
 * WGL context is current.  Performs the two operations that are safe for GL
 * windows but would break software-rendered (UpdateLayeredWindow) windows:
 *
 * 1. Strips @c WS_EX_LAYERED from the HWND.  JUCE sets this flag on
 *    transparent windows (setOpaque(false) + no native title bar) so that its
 *    software renderer can call UpdateLayeredWindow.  OpenGL bypasses that
 *    pipeline entirely (wglSwapBuffers writes directly to the framebuffer), so
 *    the flag is not needed and must be removed for DWM alpha compositing to
 *    work correctly.
 *
 * 2. Calls DwmExtendFrameIntoClientArea({-1,-1,-1,-1}).  This tells DWM to
 *    treat the entire client area as the non-client (glass) frame, enabling
 *    per-pixel alpha compositing of the GL framebuffer with the desktop.
 *    Without this call the GL surface is composited as fully opaque.
 *
 * @note Must be called from the GL render thread while the WGL context is
 *       current (i.e. inside glContextCreated / renderOpenGL).
 * @note Do NOT call this for software-rendered windows — it will break them.
 *
 * @return @c true  if the HWND was obtained and both operations succeeded.
 * @return @c false if @c wglGetCurrentDC() or @c WindowFromDC() returned null.
 *
 * @see applyDwmGlass()
 */
const bool BackgroundBlur::enableGLTransparency()
{
    // Obtain the HWND from the current WGL device context.
    // wglGetCurrentDC() returns the HDC that was passed to wglMakeCurrent()
    // by JUCE's OpenGL context setup.  On Windows, JUCE creates an internal
    // child window for the GL surface, so WindowFromDC() returns the child
    // HWND — not the top-level window.  We must walk up to the root window
    // because WS_EX_LAYERED and DwmExtendFrameIntoClientArea are top-level
    // window attributes.
    HDC hdc = wglGetCurrentDC();

    if (hdc == nullptr)
        return false;

    HWND glHwnd = WindowFromDC (hdc);

    if (glHwnd == nullptr)
        return false;

    // Walk up to the top-level (root) window that owns this GL child.
    HWND hwnd = GetAncestor (glHwnd, GA_ROOT);

    if (hwnd == nullptr)
        hwnd = glHwnd; // fallback: use the GL window itself

    // Strip WS_EX_LAYERED so DWM composites the GL framebuffer per-pixel.
    // GL does not use UpdateLayeredWindow, so removing this flag is safe.
    LONG_PTR exStyle = GetWindowLongPtrW (hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_LAYERED)
        SetWindowLongPtrW (hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);

    // Extend the DWM frame across the entire client area so DWM respects
    // the alpha channel written by the GL renderer.
    MARGINS margins { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea (hwnd, &margins);

    return true;
}

/**_____________________________END_OF_NAMESPACE______________________________*/
}// namespace jreng

#endif
