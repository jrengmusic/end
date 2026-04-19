/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION
   ID:                           jreng_graphics
   vendor:                       JRENG!
   version:                      0.0.1
   name:                         JRENG! Graphics
   description:                  Graphics utilities, font management, glyph atlas, and text layout
   website:                      https://jrengmusic.com
   license:                      Proprietary
   dependencies:                 juce_gui_basics,
                                  juce_graphics,
                                  jreng_core,
                                  jreng_freetype,
    OSXFrameworks:               Accelerate,
                                  CoreText,
                                  CoreFoundation,
                                  CoreGraphics,
                                  ApplicationServices,
  END_JUCE_MODULE_DECLARATION
 *******************************************************************************/
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <jreng_core/jreng_core.h>
#include <jreng_freetype/jreng_freetype.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#if JUCE_MODULE_AVAILABLE_jreng_data_structures
#include <jreng_data_structures/jreng_data_structures.h>
#endif// jreng_data_structures

// =========================================================================
// Graphics utilities
// =========================================================================

#include "graphics/jreng_graphics_utilities.h"
#include "graphics/jreng_graphics_fader.h"
#include "graphics/jreng_graphics_rotary.h"
#include "graphics/jreng_graphics_perimeter.h"
#include "graphics/jreng_graphics_segment.h"

#include "vignette/jreng_graphics_vignette.h"
#include "colours/jreng_colours_names.h"
#include "colours/jreng_colours_utilities.h"

// =========================================================================
// Glyph atlas, fonts, and text layout
// =========================================================================

#include "fonts/jreng_atlas_packer.h"
#include "fonts/jreng_glyph_key.h"
#include "fonts/jreng_atlas_glyph.h"
#include "fonts/jreng_staged_bitmap.h"
#include "fonts/jreng_lru_glyph_cache.h"

#include "fonts/jreng_glyph_constraint.h"
#include "fonts/jreng_constraint_transform.h"
#include "fonts/jreng_box_drawing.h"
#include "fonts/jreng_glyph_packer.h"

#include "fonts/jreng_typeface.h"
#include "fonts/jreng_font.h"

#include "rendering/jreng_glyph_render.h"
#include "rendering/jreng_glyph_graphics_context.h"

#include "fonts/jreng_text_layout.h"
