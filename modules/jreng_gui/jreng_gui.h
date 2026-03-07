/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION
   ID:                          jreng_gui
   vendor:                      JRENG!
   version:                     0.0.1
   name:                        JRENG! GUI
   description:                 JRENG! GUI utilities
   website:                     https://jrengmusic.com
   license:                     Proprietary
   dependencies:                jreng_core,
                                juce_gui_basics,
    				juce_events,
    OSXFrameworks:              Cocoa,
    				CoreGraphics,
   END_JUCE_MODULE_DECLARATION
 *******************************************************************************/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_events/juce_events.h>
#include <jreng_core/jreng_core.h>

#include "glass/jreng_background_blur.h"
#include "glass/jreng_glass_component.h"
#include "glass/jreng_glass_window.h"
#include "animation/jreng_animator.h"
