/**
 * @file Gpu.cpp
 * @brief Windows GPU probe using WGL with a dummy HWND.
 *
 * Creates a 3.2 Core context via wglCreateContextAttribsARB.
 * Any failure (no ARB support, no FBO procs) results in isAvailable = false.
 *
 * @note MESSAGE THREAD — blocks up to ~20ms on cold probe.
 */

#include "Gpu.h"

#if JUCE_WINDOWS

#include <windows.h>

#pragma comment(lib, "opengl32.lib")

using namespace juce::gl;

//==============================================================================
// WGL extension constants

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
    #define WGL_CONTEXT_MAJOR_VERSION_ARB    0x2091
    #define WGL_CONTEXT_MINOR_VERSION_ARB    0x2092
    #define WGL_CONTEXT_PROFILE_MASK_ARB     0x9126
    #define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

//==============================================================================
// Extension function pointer types

using PFN_wglCreateContextAttribsARB = HGLRC (WINAPI*) (HDC, HGLRC, const int*);
using PFN_glGenFramebuffers          = void (APIENTRY*) (GLsizei, GLuint*);
using PFN_glDeleteFramebuffers       = void (APIENTRY*) (GLsizei, const GLuint*);
using PFN_glBindFramebuffer          = void (APIENTRY*) (GLenum, GLuint);
using PFN_glFramebufferTexture2D     = void (APIENTRY*) (GLenum, GLenum, GLenum, GLuint, GLint);
using PFN_glCheckFramebufferStatus   = GLenum (APIENTRY*) (GLenum);

namespace
{

//==============================================================================

static const wchar_t* kWindowClass { L"END_GpuProbe" };

//==============================================================================
// RAII context — destructor handles all cleanup regardless of creation stage.

struct ProbeContext
{
    HWND hwnd { nullptr };
    HDC hdc { nullptr };
    HGLRC context { nullptr };

    ~ProbeContext()
    {
        if (context != nullptr)
        {
            wglMakeCurrent (nullptr, nullptr);
            wglDeleteContext (context);
        }

        if (hdc != nullptr)
            ReleaseDC (hwnd, hdc);

        if (hwnd != nullptr)
            DestroyWindow (hwnd);
    }
};

//==============================================================================

bool createProbeWindow (ProbeContext& ctx) noexcept
{
    bool created { false };

    WNDCLASSEXW wc {};
    wc.cbSize = sizeof (wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW (nullptr);
    wc.lpszClassName = kWindowClass;
    RegisterClassExW (&wc);

    ctx.hwnd = CreateWindowExW (0, kWindowClass, L"",
                                WS_OVERLAPPEDWINDOW,
                                0, 0, 1, 1,
                                nullptr, nullptr,
                                GetModuleHandleW (nullptr), nullptr);

    if (ctx.hwnd != nullptr)
    {
        ctx.hdc = GetDC (ctx.hwnd);

        if (ctx.hdc != nullptr)
        {
            PIXELFORMATDESCRIPTOR pfd {};
            pfd.nSize = sizeof (pfd);
            pfd.nVersion = 1;
            pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
            pfd.iPixelType = PFD_TYPE_RGBA;
            pfd.cColorBits = 32;
            pfd.iLayerType = PFD_MAIN_PLANE;

            const int pf { ChoosePixelFormat (ctx.hdc, &pfd) };

            if (pf != 0)
            {
                if (SetPixelFormat (ctx.hdc, pf, &pfd))
                {
                    created = true;
                }
            }
        }
    }

    return created;
}

//==============================================================================

bool createCoreContext (ProbeContext& ctx) noexcept
{
    bool created { false };

    HGLRC legacy { wglCreateContext (ctx.hdc) };

    if (legacy != nullptr)
    {
        wglMakeCurrent (ctx.hdc, legacy);

        const auto wglCreateContextAttribs {
            reinterpret_cast<PFN_wglCreateContextAttribsARB> (
                wglGetProcAddress ("wglCreateContextAttribsARB")) };

        if (wglCreateContextAttribs != nullptr)
        {
            const int attribs[]
            {
                WGL_CONTEXT_MAJOR_VERSION_ARB,   3,
                WGL_CONTEXT_MINOR_VERSION_ARB,   2,
                WGL_CONTEXT_PROFILE_MASK_ARB,    WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                0
            };

            HGLRC core { wglCreateContextAttribs (ctx.hdc, nullptr, attribs) };

            if (core != nullptr)
            {
                wglMakeCurrent (nullptr, nullptr);
                wglDeleteContext (legacy);
                wglMakeCurrent (ctx.hdc, core);
                ctx.context = core;
                created = true;
            }
        }

        if (not created)
        {
            wglMakeCurrent (nullptr, nullptr);
            wglDeleteContext (legacy);
        }
    }

    return created;
}

//==============================================================================

bool runPixelReadback() noexcept
{
    const auto genFramebuffers { reinterpret_cast<PFN_glGenFramebuffers> (
        wglGetProcAddress ("glGenFramebuffers")) };
    const auto deleteFramebuffers { reinterpret_cast<PFN_glDeleteFramebuffers> (
        wglGetProcAddress ("glDeleteFramebuffers")) };
    const auto bindFramebuffer { reinterpret_cast<PFN_glBindFramebuffer> (
        wglGetProcAddress ("glBindFramebuffer")) };
    const auto framebufferTexture2D { reinterpret_cast<PFN_glFramebufferTexture2D> (
        wglGetProcAddress ("glFramebufferTexture2D")) };
    const auto checkFramebufferStatus { reinterpret_cast<PFN_glCheckFramebufferStatus> (
        wglGetProcAddress ("glCheckFramebufferStatus")) };

    const bool procsLoaded { genFramebuffers != nullptr
                             and deleteFramebuffers != nullptr
                             and bindFramebuffer != nullptr
                             and framebufferTexture2D != nullptr
                             and checkFramebufferStatus != nullptr };

    bool result { false };

    if (procsLoaded)
    {
        GLuint fbo { 0 };
        GLuint tex { 0 };
        genFramebuffers (1, &fbo);
        glGenTextures (1, &tex);

        glBindTexture (GL_TEXTURE_2D, tex);
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, Gpu::probeSize, Gpu::probeSize, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        bindFramebuffer (GL_FRAMEBUFFER, fbo);
        framebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, tex, 0);

        const bool complete { checkFramebufferStatus (GL_FRAMEBUFFER)
                              == GL_FRAMEBUFFER_COMPLETE };

        if (complete)
        {
            glViewport (0, 0, Gpu::probeSize, Gpu::probeSize);
            glClearColor (0.5f, 0.25f, 0.75f, 1.0f);
            glClear (GL_COLOR_BUFFER_BIT);
            glFinish();

            std::array<uint8_t, 4> pixel { 0, 0, 0, 0 };
            glReadPixels (0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());

            result = (std::abs (static_cast<int> (pixel.at (0)) - Gpu::expectedR) <= Gpu::pixelTolerance
                      and std::abs (static_cast<int> (pixel.at (1)) - Gpu::expectedG) <= Gpu::pixelTolerance
                      and std::abs (static_cast<int> (pixel.at (2)) - Gpu::expectedB) <= Gpu::pixelTolerance);
        }

        bindFramebuffer (GL_FRAMEBUFFER, 0);
        deleteFramebuffers (1, &fbo);
        glDeleteTextures (1, &tex);
    }

    return result;
}

} // anonymous namespace

//==============================================================================

Gpu::ProbeResult Gpu::probe() noexcept
{
    ProbeContext ctx;
    ProbeResult result;

    if (createProbeWindow (ctx))
    {
        if (createCoreContext (ctx))
        {
            juce::gl::loadFunctions();

            const char* raw { reinterpret_cast<const char*> (
                glGetString (GL_RENDERER)) };

            if (raw != nullptr)
            {
                result.rendererName = juce::String (raw);

                if (not isSoftwareRenderer (result.rendererName))
                {
                    result.isAvailable = runPixelReadback();
                }
            }
        }
    }

    return result;
}

#endif
