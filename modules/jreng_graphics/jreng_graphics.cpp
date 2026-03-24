#include "jreng_graphics.h"
#include "graphics/jreng_graphics_rotary.cpp"
#include "colours/jreng_colours_names.cpp"

// libunibreak — Unicode line breaking (UAX #14)
#include "fonts/linebreak/unibreakdef.c"
#include "fonts/linebreak/unibreakbase.c"
#include "fonts/linebreak/eastasianwidthdef.c"
#include "fonts/linebreak/linebreakdata.c"
#include "fonts/linebreak/linebreakdef.c"
#include "fonts/linebreak/linebreak.c"

#include "fonts/jreng_typeface.cpp"
#include "fonts/jreng_font.cpp"
#include "fonts/jreng_glyph_constraint_table.cpp"
#include "fonts/jreng_glyph_atlas.cpp"
#include "fonts/jreng_text_layout.cpp"
#include "rendering/jreng_graphics_text_renderer.cpp"
