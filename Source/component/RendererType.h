/**
 * @file RendererType.h
 * @brief Rendering backend selection: GPU (OpenGL) or CPU (juce::Graphics).
 *
 * @see Config::Key::gpuAcceleration
 */

#pragma once

#include <JuceHeader.h>
#include "../AppState.h"
#include "../config/Config.h"

namespace Terminal
{

/**
 * @enum RendererType
 * @brief Active rendering backend for terminal components.
 */
enum class RendererType
{
    gpu,  ///< OpenGL accelerated — GLTextRenderer, glassmorphism enabled.
    cpu   ///< Software rendered — GraphicsTextRenderer, opaque background.
};

/**
 * @brief Returns the active renderer type from AppState (SSOT).
 *
 * AppState is populated at startup and on every config reload.
 *
 * @return The resolved RendererType.
 * @note MESSAGE THREAD.
 */
inline RendererType getRendererType() noexcept
{
    RendererType result { RendererType::gpu };

    if (AppState::getContext()->getRendererType() == "cpu")
    {
        result = RendererType::cpu;
    }

    return result;
}

} // namespace Terminal
