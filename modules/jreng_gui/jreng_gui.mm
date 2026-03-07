/*
  ==============================================================================

   jreng_gui.mm
   Part of the jreng_gui module

   Platform-specific bridge (macOS/Cocoa)

  ==============================================================================
 */

#include "jreng_gui.cpp"
#if JUCE_MAC
#import <Cocoa/Cocoa.h>
#include "glass/jreng_background_blur.mm"
#endif
