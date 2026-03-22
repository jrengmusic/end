/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION
 ID             :   jreng_glyph
 vendor         :   JRENG
 version        :   1.0.0
 name           :   Glyph Rendering Pipeline
 description    :   Universal glyph atlas, shaping, and instanced glyph rendering
 website        :   https://jrengmusic.com
 license        :   Proprietary
 dependencies   :   jreng_opengl, jreng_core, jreng_freetype, juce_graphics, juce_opengl
 OSXFrameworks  :   CoreText, CoreFoundation, CoreGraphics, ApplicationServices
 iOSFrameworks  :
 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once

#include <jreng_core/jreng_core.h>
#include <jreng_opengl/jreng_opengl.h>
#include <jreng_freetype/jreng_freetype.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_opengl/juce_opengl.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include "font/jreng_font.h"

#include "atlas/jreng_atlas_packer.h"
#include "atlas/jreng_glyph_key.h"
#include "atlas/jreng_atlas_glyph.h"
#include "atlas/jreng_staged_bitmap.h"
#include "atlas/jreng_lru_glyph_cache.h"

#include "constraint/jreng_glyph_constraint.h"
#include "constraint/jreng_constraint_transform.h"
#include "drawing/jreng_box_drawing.h"
#include "atlas_impl/jreng_glyph_atlas.h"

#include "render/jreng_glyph_render.h"
#include "render/jreng_glyph_shaders.h"
#include "render/jreng_gl_text_renderer.h"

#include "layout/jreng_text_layout.h"
