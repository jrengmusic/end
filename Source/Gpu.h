/**
 * @file Gpu.h
 * @brief GPU capability probe and renderer type resolution.
 *
 * Stateless struct. Probes native GL context at startup, checks against
 * known software renderers, and provides renderer type resolution from
 * AppState (SSOT).
 *
 * Platform probe implementations:
 * - Gpu.cpp (Windows — WGL + dummy HWND)
 * - Gpu.mm (macOS — NSOpenGLContext)
 *
 * @note MESSAGE THREAD.
 */

#pragma once

#include <JuceHeader.h>

struct Gpu
{
    /** @brief Result of the GPU capability probe. */
    struct ProbeResult
    {
        juce::String rendererName; ///< Raw GL_RENDERER string.
        bool isAvailable { false }; ///< True if hardware-accelerated GL pipeline verified.
    };

    /**
     * @brief Probe GPU capability.
     *
     * Creates a native GL context (no JUCE component required), queries
     * GL_RENDERER, checks against known software renderers, and runs a
     * pixel-readback probe to verify the pipeline is functional.
     *
     * Stateless — holds nothing, caches nothing. Caller stores result.
     */
    static ProbeResult probe() noexcept;

    /**
     * @brief Checks a GL_RENDERER string against known software renderers.
     * @param rendererName  The GL_RENDERER string to check.
     * @return True if the name contains a known software renderer substring.
     */
    static bool isSoftwareRenderer (const juce::String& rendererName) noexcept
    {
        return std::any_of (softwareRenderers.begin(), softwareRenderers.end(),
            [&rendererName] (const juce::String& entry) { return rendererName.contains (entry); });
    }

    //==========================================================================
    // Probe constants (used by platform implementations)
    //==========================================================================

    static constexpr int probeSize { 16 };
    static constexpr int pixelTolerance { 3 };

    // Seed: glClearColor (0.5, 0.25, 0.75, 1.0) -> expected pixel (127, 63, 191)
    static constexpr int expectedR { 127 };
    static constexpr int expectedG { 63 };
    static constexpr int expectedB { 191 };

    /** @brief GL_RENDERER substrings identifying known software renderers. */
    static inline const juce::StringArray softwareRenderers
    {
        "llvmpipe",
        "softpipe",
        "Microsoft Basic Render",
        "SwiftShader",
        "VirtIO",
        "QEMU",
        "VMware SVGA",
        "Parallels",
        "virgl",
        "GDI Generic",
        "Apple Software Renderer"
    };
};
