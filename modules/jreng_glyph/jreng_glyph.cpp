#include "jreng_glyph.h"

// libunibreak — Unicode line breaking (UAX #14)
#include "linebreak/unibreakdef.c"
#include "linebreak/unibreakbase.c"
#include "linebreak/eastasianwidthdef.c"
#include "linebreak/linebreakdata.c"
#include "linebreak/linebreakdef.c"
#include "linebreak/linebreak.c"

#include "font/jreng_font.cpp"
#include "constraint/jreng_glyph_constraint_table.cpp"
#include "atlas_impl/jreng_glyph_atlas.cpp"
#include "render/jreng_gl_text_renderer.cpp"
#include "layout/jreng_text_layout.cpp"
