/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION
   ID:                          jreng_core
   vendor:                      Jubilant Research of Eclectic of Novelty in Generating music
   version:                     0.0.1
   name:                        JRENG Core
   description:                 JRENG Core
   website:                     https://jrengmusic.com
   license:                     Proprietary
   dependencies:                juce_core,
                                juce_data_structures,
                                juce_graphics,
   OSXFrameworks:
   iOSFrameworks:
  END_JUCE_MODULE_DECLARATION
 *******************************************************************************/

#pragma once

//==============================================================================
/** Config: JRENG_USING_AQUATIC_PRIME
    Enables JRENG Aquatic Prime.
*/
#ifndef JRENG_USING_AQUATIC_PRIME
 #define JRENG_USING_AQUATIC_PRIME 1
#endif

/** Config: JRENG_USING_MULTI_ORIENTATION
    Enables Multi-orientation for Portrait and Landscape.
*/
#ifndef JRENG_USING_MULTI_ORIENTATION
 #define JRENG_USING_MULTI_ORIENTATION 0
#endif

/** Config: JRENG_USING_OVERSAMPLING
    Enables Oversampling.
*/
#ifndef JRENG_USING_OVERSAMPLING
 #define JRENG_USING_OVERSAMPLING 1
#endif

//==============================================================================
#include <ciso646>
#include <assert.h>
#include <any>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>

#if JUCE_MODULE_AVAILABLE_juce_gui_basics
#include <juce_gui_basics/juce_gui_basics.h>
#endif

#if JUCE_MODULE_AVAILABLE_juce_audio_processors
#include <juce_audio_processors/juce_audio_processors.h>
#endif


#include "project_info/jreng_project_info.h"
#include "text/jreng_text.h"

#include "utilities/jreng_zip.h"
#include "utilities/jreng_range.h"
#include "utilities/jreng_toInt.h"
#include "utilities/jreng_math.h"
#include "utilities/jreng_audio_processor_utils.h"
#include "utilities/jreng_owner.h"
#include "utilities/jreng_any_owner.h"
#include "identifier/jreng_identifier.h"

#if JUCE_DEBUG
#include "debug/jreng_debug.h"
#endif

#include "function_map/jreng_function_map.h"
#include "string/jreng_string.h"
#include "context/jreng_context.h"
#include "map/jreng_map.h"
#include "binary_data/jreng_binary_data.h"
#include "file/jreng_file.h"

#include "image/jreng_image.h"
#include "value/jreng_value.h"
#include "utilities/jreng_decibels.h"
#include "utilities/jreng_frequency.h"
#include "utilities/jreng_taper.h"
#include "xml/jreng_xml.h"
#include "xml/jreng_svg.h"
#include "fuzzy_search/jreng_fuzzy_search.h"

#include "concurrency/jreng_mailbox.h"
#include "concurrency/jreng_snapshot_buffer.h"
