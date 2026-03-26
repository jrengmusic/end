/**
 * @file RendererType.h
 * @brief Renderer type resolution from AppState (SSOT).
 *
 * @see PaneComponent::RendererType
 * @see Config::Key::gpuAcceleration
 */

#pragma once

#include <JuceHeader.h>
#include "../AppState.h"
#include "PaneComponent.h"

/**
 * @brief Returns the active renderer type from AppState (SSOT).
 *
 * AppState is populated at startup and on every config reload.
 *
 * @return The resolved RendererType.
 * @note MESSAGE THREAD.
 */
inline PaneComponent::RendererType getRendererType() noexcept
{
    PaneComponent::RendererType result { PaneComponent::RendererType::gpu };

    if (AppState::getContext()->getRendererType() == "cpu")
    {
        result = PaneComponent::RendererType::cpu;
    }

    return result;
}
