/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION
   ID:                          jreng_data_structures
   vendor:                      JRENG!
   version:                     0.0.1
   name:                        JRENG! Data Structures
   description:                 ValueTree management and data model utilities
   website:                     https://jrengmusic.com
   license:                     Proprietary
   dependencies:                jreng_core,
                                juce_data_structures,
   OSXFrameworks:
   iOSFrameworks:
 END_JUCE_MODULE_DECLARATION
 *******************************************************************************/

#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <jreng_core/jreng_core.h>

#if JUCE_MODULE_AVAILABLE_juce_gui_basics
#include <juce_gui_basics/juce_gui_basics.h>
#endif

#include "value_tree/jreng_value_tree.h"
