/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION
   ID:                           jreng_graphics
   vendor:                       JRENG!
   version:                      0.0.1
   name:                         JRENG! Graphics
   description:                  JRENG! auxiliary functions to read, write and draw Scalable Vector Graphics
   website:                      https://jrengmusic.com
   license:                      Proprietary
   dependencies:                 juce_gui_basics,
                                  juce_opengl,
                                  jreng_core,
    OSXFrameworks:               Accelerate,
  END_JUCE_MODULE_DECLARATION
 *******************************************************************************/
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
#include <jreng_core/jreng_core.h>

#if JUCE_MODULE_AVAILABLE_jreng_data_structures
#include <jreng_data_structures/jreng_data_structures.h>
#endif// jreng_data_structures


#include "graphics/jreng_graphics_utilities.h"
#include "graphics/jreng_graphics_fader.h"
#include "graphics/jreng_graphics_rotary.h"
#include "graphics/jreng_graphics_perimeter.h"
#include "graphics/jreng_graphics_segment.h"

#include "vignette/jreng_graphics_vignette.h"
#include "colours/jreng_colours_names.h"
#include "colours/jreng_colours_utilities.h"
