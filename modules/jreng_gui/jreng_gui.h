/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION
   ID:                          jreng_gui
   vendor:                      JRENG!
   version:                     0.0.1
   name:                        JRENG! GUI
   description:                 JRENG! GUI utilities (window, layout, OpenGL rendering)
   website:                     https://jrengmusic.com
   license:                     Proprietary
   dependencies:                jreng_core,
                                jreng_graphics,
                                juce_gui_basics,
                                juce_events,
                                juce_opengl,
   OSXFrameworks:               Cocoa,
                                CoreGraphics,
 END_JUCE_MODULE_DECLARATION
 *******************************************************************************/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_events/juce_events.h>
#include <juce_opengl/juce_opengl.h>
#include <jreng_core/jreng_core.h>
#include <jreng_graphics/jreng_graphics.h>

#include "window/jreng_background_blur.h"
#include "window/jreng_glass_component.h"
#include "window/jreng_window.h"
#include "window/jreng_modal_window.h"
#include "animation/jreng_animator.h"
#include "layout/jreng_pane_manager.h"
#include "layout/jreng_pane_resizer_bar.h"

#include "opengl/renderers/jreng_gl_path.h"
#include "opengl/context/jreng_gl_graphics.h"
#include "opengl/context/jreng_gl_component.h"
#include "opengl/renderers/jreng_gl_vignette.h"
#include "opengl/context/jreng_gl_shader_compiler.h"
#include "opengl/context/jreng_gl_vertex_layout.h"
#include "opengl/renderers/jreng_glyph_shaders.h"
#include "opengl/context/jreng_glyph_atlas.h"
#include "opengl/renderers/jreng_gl_context.h"
#include "opengl/context/jreng_gl_renderer.h"
#include "opengl/context/jreng_gl_atlas_renderer.h"
#include "opengl/context/jreng_gl_overlay.h"
