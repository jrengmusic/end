/**
 * @file Gpu.mm
 * @brief macOS GPU probe using NSOpenGLContext (no NSView required).
 *
 * Requests NSOpenGLPFAAccelerated to fail fast when no hardware GL exists.
 *
 * @note MESSAGE THREAD — blocks up to ~20ms on cold probe.
 */

#include "Gpu.h"

#if JUCE_MAC

#define GL_SILENCE_DEPRECATION
#import <Cocoa/Cocoa.h>

using namespace juce::gl;

namespace
{

//==============================================================================

bool runPixelReadback() noexcept
{
    GLuint fbo { 0 };
    GLuint tex { 0 };
    bool result { false };

    glGenFramebuffers (1, &fbo);
    glGenTextures (1, &tex);

    glBindTexture (GL_TEXTURE_2D, tex);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, Gpu::probeSize, Gpu::probeSize, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindFramebuffer (GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D, tex, 0);

    const bool complete { glCheckFramebufferStatus (GL_FRAMEBUFFER)
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

    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers (1, &fbo);
    glDeleteTextures (1, &tex);

    return result;
}

} // anonymous namespace

//==============================================================================

Gpu::ProbeResult Gpu::probe() noexcept
{
    NSOpenGLPixelFormatAttribute attrs[]
    {
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        NSOpenGLPFAColorSize,     24,
        NSOpenGLPFAAlphaSize,     8,
        NSOpenGLPFAAccelerated,
        0
    };

    NSOpenGLPixelFormat* fmt {
        [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs] };

    ProbeResult result;

    if (fmt != nil)
    {
        NSOpenGLContext* ctx {
            [[NSOpenGLContext alloc] initWithFormat:fmt shareContext:nil] };

        if (ctx != nil)
        {
            [ctx makeCurrentContext];
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

            [NSOpenGLContext clearCurrentContext];
        }
    }

    return result;
}

#endif
