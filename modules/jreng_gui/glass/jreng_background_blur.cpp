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

const bool BackgroundBlur::enable (juce::Component* component, float blurRadius, juce::Colour tint, Type type)
{
    bool result { false };

    switch (type)
    {
        case Type::dwmGlass:
            result = applyDwmGlass (component, blurRadius, tint);
            break;
    }

    return result;
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
 * @see enableWindowTransparency()
 */
const bool BackgroundBlur::applyDwmGlass (juce::Component* component, float blurRadius, juce::Colour tint)
{
    bool result { false };

    if (auto* peer = component->getPeer())
    {
        HWND hwnd = (HWND) peer->getNativeHandle();

        if (hwnd != nullptr)
        {
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
                    COLORREF abgr = ((COLORREF) tint.getAlpha() << 24)
                                  | ((COLORREF) tint.getBlue()  << 16)
                                  | ((COLORREF) tint.getGreen() <<  8)
                                  | ((COLORREF) tint.getRed());

                    ACCENT_POLICY accent {};
                    accent.AccentState   = ACCENT_ENABLE_ACRYLICBLURBEHIND;
                    accent.AccentFlags   = 2;  // use GradientColor
                    accent.GradientColor = abgr;
                    accent.AnimationId   = 0;

                    WINDOWCOMPOSITIONATTRIBDATA data {};
                    data.Attrib = WCA_ACCENT_POLICY;
                    data.pvData = &accent;
                    data.cbData = sizeof (accent);

                    result = SetWindowCompositionAttribute (hwnd, &data) != FALSE;
                }
            }
            else
            {
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

                    result = SetWindowCompositionAttribute (hwnd, &data) != FALSE;
                }
                else
                {
                    // Fallback: legacy DwmEnableBlurBehindWindow
                    DWM_BLURBEHIND blurBehind {};
                    blurBehind.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
                    blurBehind.hRgnBlur = CreateRectRgn (0, 0, -1, -1);
                    blurBehind.fEnable  = TRUE;

                    result = SUCCEEDED (DwmEnableBlurBehindWindow (hwnd, &blurBehind));

                    if (blurBehind.hRgnBlur != nullptr)
                        DeleteObject (blurBehind.hRgnBlur);
                }
            }
        }
    }

    return result;
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
 * WGL context is current.  Performs two operations:
 *
 * 1. Strips @c WS_EX_LAYERED from the HWND.
 * 2. Calls DwmExtendFrameIntoClientArea({-1,-1,-1,-1}).
 *
 * On Windows 11, applyDwmGlass() already performs both operations.  This
 * function is idempotent — calling it again is harmless.  On Windows 10,
 * applyDwmGlass() does NOT strip WS_EX_LAYERED or extend the DWM frame,
 * so this function is required for GL windows on that OS.
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
const bool BackgroundBlur::enableWindowTransparency()
{
    bool result { true };

    // On Windows 11, applyDwmGlass() already strips WS_EX_LAYERED and extends
    // the DWM frame.  This function is only required on Windows 10, where
    // applyDwmGlass() does not perform those operations for GL windows.
    if (isWindows10())
    {
        // Obtain the HWND from the current WGL device context.
        // wglGetCurrentDC() returns the HDC that was passed to wglMakeCurrent()
        // by JUCE's OpenGL context setup.  On Windows, JUCE creates an internal
        // child window for the GL surface, so WindowFromDC() returns the child
        // HWND — not the top-level window.  We must walk up to the root window
        // because WS_EX_LAYERED and DwmExtendFrameIntoClientArea are top-level
        // window attributes.
        HDC hdc { wglGetCurrentDC() };

        if (hdc != nullptr)
        {
            HWND glHwnd { WindowFromDC (hdc) };

            if (glHwnd != nullptr)
            {
                // Walk up to the top-level (root) window that owns this GL child.
                HWND hwnd { GetAncestor (glHwnd, GA_ROOT) };

                if (hwnd == nullptr)
                    hwnd = glHwnd; // fallback: use the GL window itself

                // Strip WS_EX_LAYERED so DWM composites the GL framebuffer per-pixel.
                // GL does not use UpdateLayeredWindow, so removing this flag is safe.
                LONG_PTR exStyle { GetWindowLongPtrW (hwnd, GWL_EXSTYLE) };
                if (exStyle & WS_EX_LAYERED)
                    SetWindowLongPtrW (hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);

                // Extend the DWM frame across the entire client area so DWM respects
                // the alpha channel written by the GL renderer.
                MARGINS margins { -1, -1, -1, -1 };
                DwmExtendFrameIntoClientArea (hwnd, &margins);
            }
            else
            {
                result = false;
            }
        }
        else
        {
            result = false;
        }
    }

    return result;
}

/**
 * @brief Removes the DWM glass effect from the foreground window.
 *
 * Calls DwmExtendFrameIntoClientArea with zero margins to restore the
 * default non-extended frame.  Called from the message thread when
 * switching from the GPU renderer to the CPU renderer.
 *
 * @note MESSAGE THREAD.
 */
void BackgroundBlur::disable (juce::Component* component)
{
    if (auto* peer { component->getPeer() })
    {
        HWND hwnd = (HWND) peer->getNativeHandle();

        if (hwnd != nullptr)
        {
            HWND root { GetAncestor (hwnd, GA_ROOT) };

            if (root != nullptr)
            {
                // Reset DWM frame to default (no glass extension).
                MARGINS margins { 0, 0, 0, 0 };
                DwmExtendFrameIntoClientArea (root, &margins);

                // Reset accent policy — removes acrylic/blur compositing.
                auto SetWindowCompositionAttribute = (SetWindowCompositionAttribute_t)
                    GetProcAddress (GetModuleHandleW (L"user32.dll"),
                        "SetWindowCompositionAttribute");

                if (SetWindowCompositionAttribute != nullptr)
                {
                    ACCENT_POLICY accent {};
                    accent.AccentState = ACCENT_DISABLED;

                    WINDOWCOMPOSITIONATTRIBDATA data {};
                    data.Attrib = WCA_ACCENT_POLICY;
                    data.pvData = &accent;
                    data.cbData = sizeof (accent);

                    SetWindowCompositionAttribute (root, &data);
                }

                // Preserve rounded corners (Win11).
                if (not isWindows10())
                {
                    DWORD cornerPref { 2 }; // DWMWCP_ROUND
                    DwmSetWindowAttribute (root, 33, &cornerPref, sizeof (cornerPref));
                }
            }
        }
    }
}

/**_____________________________END_OF_NAMESPACE______________________________*/
}// namespace jreng

#endif
