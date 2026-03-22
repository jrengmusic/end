/**
 * @file jreng_constraint_transform.h
 * @brief Shared scale/align computation for Nerd Font icon constraints.
 *
 * `computeConstraintTransform` is used by both the FreeType and CoreText
 * rasterization backends to derive the scale factors and position offsets
 * that place a glyph within a terminal cell according to a `Constraint`.
 */

#pragma once

#include <cmath>
#include <algorithm>

#include "jreng_glyph_constraint.h"

namespace jreng::Glyph
{

/**
 * @struct ConstraintTransform
 * @brief Scale and position result from `computeConstraintTransform`.
 */
struct ConstraintTransform
{
    float scaleX { 1.0f };
    float scaleY { 1.0f };
    float posX   { 0.0f };
    float posY   { 0.0f };
};

/**
 * @brief Derive scale and position for a constrained glyph within a cell.
 *
 * Computes `ConstraintTransform` from the constraint's scale mode, alignment,
 * and padding applied to the glyph's natural dimensions and the effective
 * cell dimensions.
 *
 * @param constraint     Nerd Font layout descriptor.
 * @param glyphNaturalW  Natural glyph width in the same pixel space as the cell.
 * @param glyphNaturalH  Natural glyph height in the same pixel space as the cell.
 * @param effCellW       Effective cell width (cellWidth × spanCells).
 * @param effCellH       Effective cell height (cellHeight).
 * @return Populated `ConstraintTransform`.
 */
inline ConstraintTransform computeConstraintTransform (
    const Constraint& constraint,
    float glyphNaturalW, float glyphNaturalH,
    float effCellW, float effCellH) noexcept
{
    ConstraintTransform result;

    const float inset { Constraint::iconInset };
    const float targetW { effCellW * (1.0f - constraint.padLeft - constraint.padRight - inset * 2.0f) };
    const float targetH { effCellH * (1.0f - constraint.padTop - constraint.padBottom - inset * 2.0f) };

    if (glyphNaturalW > 0.0f and glyphNaturalH > 0.0f)
    {
        result.scaleX = targetW / glyphNaturalW;
        result.scaleY = targetH / glyphNaturalH;

        if (constraint.scaleMode == Constraint::ScaleMode::fit
            or constraint.scaleMode == Constraint::ScaleMode::adaptiveScale)
        {
            const float uniform { std::min (result.scaleX, result.scaleY) };
            result.scaleX = std::min (1.0f, uniform);
            result.scaleY = result.scaleX;
        }
        else if (constraint.scaleMode == Constraint::ScaleMode::cover)
        {
            const float uniform { std::min (result.scaleX, result.scaleY) };
            result.scaleX = uniform;
            result.scaleY = uniform;
        }
    }

    if (constraint.maxAspectRatio > 0.0f)
    {
        const float scaledW { glyphNaturalW * result.scaleX };
        const float scaledH { glyphNaturalH * result.scaleY };

        if (scaledH > 0.0f and scaledW / scaledH > constraint.maxAspectRatio)
        {
            result.scaleX = scaledH * constraint.maxAspectRatio / glyphNaturalW;
        }
    }

    const float scaledW { glyphNaturalW * result.scaleX };
    const float scaledH { glyphNaturalH * result.scaleY };

    result.posX = constraint.padLeft * effCellW;
    result.posY = constraint.padBottom * effCellH;

    if (constraint.alignH == Constraint::Align::center)
    {
        result.posX = (effCellW - scaledW) * 0.5f;
    }
    else if (constraint.alignH == Constraint::Align::end)
    {
        result.posX = effCellW - scaledW - constraint.padRight * effCellW;
    }

    if (constraint.alignV == Constraint::Align::center)
    {
        result.posY = (effCellH - scaledH) * 0.5f;
    }
    else if (constraint.alignV == Constraint::Align::end)
    {
        result.posY = effCellH - scaledH - constraint.padTop * effCellH;
    }

    return result;
}

} // namespace jreng::Glyph
