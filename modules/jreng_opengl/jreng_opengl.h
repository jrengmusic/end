/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION
 ID             :   jreng_opengl
 vendor         :   JRENG
 version        :   0.0.1
 name           :   JRENG OpenGL
 description    :   OpenGL-accelerated path rendering with juce::Graphics-like API
 website        :   https://jrengmusic.com
 license        :   Proprietary
 dependencies   :   juce_opengl, jreng_core, jreng_graphics
 OSXFrameworks  :
 iOSFrameworks  :
 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#include <juce_opengl/juce_opengl.h>
#include <jreng_core/jreng_core.h>
#include <jreng_graphics/jreng_graphics.h>

#include "renderers/jreng_gl_path.h"
#include "context/jreng_gl_graphics.h"
#include "context/jreng_gl_mailbox.h"
#include "context/jreng_gl_snapshot_buffer.h"
#include "context/jreng_gl_component.h"
#include "renderers/jreng_gl_vignette.h"
#include "context/jreng_gl_shader_compiler.h"
#include "context/jreng_gl_vertex_layout.h"
#include "context/jreng_gl_renderer.h"
#include "context/jreng_gl_overlay.h"
#include "renderers/jreng_glyph_render.h"
#include "renderers/jreng_glyph_shaders.h"
#include "renderers/jreng_gl_text_renderer.h"
