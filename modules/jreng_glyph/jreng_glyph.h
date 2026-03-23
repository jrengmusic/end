/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION
 ID             :   jreng_glyph
 vendor         :   JRENG
 version        :   1.0.0
 name           :   GL Glyph Rendering
 description    :   GL-specific instanced glyph and background rendering (temporary — will fold into jreng_opengl)
 website        :   https://jrengmusic.com
 license        :   Proprietary
 dependencies   :   jreng_opengl, jreng_core, jreng_graphics, jreng_freetype, juce_graphics, juce_opengl
 OSXFrameworks  :
 iOSFrameworks  :
 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once

#include <jreng_core/jreng_core.h>
#include <jreng_opengl/jreng_opengl.h>
#include <jreng_graphics/jreng_graphics.h>
#include <jreng_freetype/jreng_freetype.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_opengl/juce_opengl.h>

#include "render/jreng_glyph_render.h"
#include "render/jreng_glyph_shaders.h"
#include "render/jreng_gl_text_renderer.h"
