/*******************************************************************************
                        Codegen Annotated Source of Truth
————————————————————————————————————————————————————————————————————————————————

            ░░████████████░░████████████░░████████████░░████████████
            ░░████  ░░████░░████  ░░████░░████  ░░████    ░░████
            ░░████        ░░████  ░░████░░████            ░░████
            ░░████        ░░████████████░░████████████    ░░████
            ░░████        ░░████  ░░████        ░░████    ░░████
            ░░████  ░░████░░████  ░░████░░████  ░░████    ░░████
            ░░████████████░░████  ░░████░░████████████    ░░████

————————————————————————————————————————————————————————————————————————————————
                         FOR YOUR EYES ONLY, DO NOT EDIT
********************************************************************************/

/**
 * @file Generated.h
 * @brief Generated-header umbrella — includes every generated product header.
 */

#pragma once

#include "ProjectInfo.h"
#include "Identifiers.h"
#include "Bimaps.h"
#include "Files.h"
#include "jam_Generated.h"

struct Generated
{
    jam::SharedInstance<map::FileConfig> fileConfig { std::in_place };///< Config-file section registry.
    jam::SharedInstance<map::FileThemes> fileThemes { std::in_place };///< Theme-file registry.
    jam::SharedInstance<map::FileFlex>   fileFlex   { std::in_place };///< Theme SVG graphics-asset registry.
    jam::SharedInstance<map::Generated>  generated  { std::in_place };
};
