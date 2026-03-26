/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION
   ID:                           jreng_markdown
   vendor:                       JRENG!
   version:                      0.0.1
   name:                         JRENG! Markdown
   description:                  Markdown and Mermaid parsing — block extraction, inline tokenization, GFM tables, SVG parser
   website:                      https://jrengmusic.com
   license:                      Proprietary
   dependencies:                 juce_core,
                                 juce_graphics,
                                 jreng_core,
  END_JUCE_MODULE_DECLARATION
 *******************************************************************************/
#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <jreng_core/jreng_core.h>

// =========================================================================
// Mermaid extraction (order matters — Markdown::Parser uses Mermaid::extractBlocks)
// =========================================================================

#include "mermaid/jreng_mermaid_extract.h"
#include "mermaid/jreng_mermaid_svg_parser.h"

// =========================================================================
// Markdown parsing
// =========================================================================

#include "markdown/jreng_markdown_types.h"
#include "markdown/jreng_markdown_parser.h"
#include "markdown/jreng_markdown_table.h"
