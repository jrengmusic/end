#if JUCE_WINDOWS

#include <windows.h>
#include <dwmapi.h>
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

// DWMWA_SYSTEMBACKDROP_TYPE = 38 (Windows 11 22H2+)
static constexpr DWORD DWMWA_SYSTEMBACKDROP_TYPE_ = 38;
// DWMWA_USE_IMMERSIVE_DARK_MODE = 20
static constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_ = 20;

/*____________________________________________________________________________*/

static std::function<void()> onWindowClosed;

/*____________________________________________________________________________*/

/** @brief Returns true if this is Windows 11 Build 22621 or later. */
static bool isWindows11_22H2OrLater()
{
    // RtlGetVersion gives the real OS version (not manifested)
    using RtlGetVersion_t = LONG (WINAPI*) (OSVERSIONINFOEXW*);
    auto RtlGetVersion = (RtlGetVersion_t) GetProcAddress (
        GetModuleHandleW (L"ntdll.dll"), "RtlGetVersion");

    if (RtlGetVersion == nullptr)
        return false;

    OSVERSIONINFOEXW osvi {};
    osvi.dwOSVersionInfoSize = sizeof (osvi);
    RtlGetVersion (&osvi);

    // Windows 11 = build 22000+, 22H2 = build 22621+
    return osvi.dwBuildNumber >= 22621;
}

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

const bool BackgroundBlur::applyDwmGlass (juce::Component* component, float blurRadius, juce::Colour tint)
{
    if (auto* peer = component->getPeer())
    {
        HWND hwnd = (HWND) peer->getNativeHandle();

        if (hwnd == nullptr)
            return false;

        // Strip WS_EX_LAYERED if JUCE added it (from setOpaque(false) + no native title bar).
        // Layered windows bypass DWM compositing, which prevents acrylic blur and
        // breaks OpenGL surface compositing.  Removing the flag returns the window
        // to normal DWM-composited mode where both GL and acrylic work correctly.
        LONG_PTR exStyle = GetWindowLongPtrW (hwnd, GWL_EXSTYLE);
        if (exStyle & WS_EX_LAYERED)
        {
            SetWindowLongPtrW (hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
        }

        // Extend DWM frame across the entire client area — required for both paths
        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea (hwnd, &margins);

        // --- Windows 11 22H2+: official Mica API ---
        if (isWindows11_22H2OrLater())
        {
            BOOL darkMode = TRUE;
            DwmSetWindowAttribute (hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_,
                &darkMode, sizeof (darkMode));

            // DWMSBT_MAINWINDOW = 2 (Mica)
            DWORD backdropType = 2;
            HRESULT hr = DwmSetWindowAttribute (hwnd, DWMWA_SYSTEMBACKDROP_TYPE_,
                &backdropType, sizeof (backdropType));

            return SUCCEEDED (hr);
        }

        // --- Windows 10: undocumented SetWindowCompositionAttribute ---
        auto SetWindowCompositionAttribute = (SetWindowCompositionAttribute_t)
            GetProcAddress (GetModuleHandleW (L"user32.dll"),
                "SetWindowCompositionAttribute");

        if (SetWindowCompositionAttribute != nullptr)
        {
            // Convert juce::Colour (ARGB) to ABGR for GradientColor
            COLORREF abgr = ((COLORREF) tint.getAlpha() << 24)
                          | ((COLORREF) tint.getBlue()  << 16)
                          | ((COLORREF) tint.getGreen() <<  8)
                          | ((COLORREF) tint.getRed());

            ACCENT_POLICY accent {};
            accent.AccentState  = ACCENT_ENABLE_ACRYLICBLURBEHIND;
            accent.AccentFlags  = 2;  // use GradientColor
            accent.GradientColor = abgr;
            accent.AnimationId  = 0;

            WINDOWCOMPOSITIONATTRIBDATA data {};
            data.Attrib = WCA_ACCENT_POLICY;
            data.pvData = &accent;
            data.cbData = sizeof (accent);

            return SetWindowCompositionAttribute (hwnd, &data) != FALSE;
        }

        // Fallback: legacy DwmEnableBlurBehindWindow (Vista/7 style, minimal effect on 10)
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

const bool BackgroundBlur::enableGLTransparency()
{
    // On Windows, DWM compositing with OpenGL requires the
    // DwmEnableBlurBehindWindow call (already done in applyDwmGlass)
    // to tell DWM to respect the alpha channel in the GL framebuffer.
    // The GL pixel format must have alpha bits (JUCE requests this).
    // No additional WGL call is needed — unlike macOS NSOpenGLContext.
    return true;
}

/**_____________________________END_OF_NAMESPACE______________________________*/
}// namespace jreng

#endif
