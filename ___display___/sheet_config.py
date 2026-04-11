#!/usr/bin/env python3
"""
Display Mono — single source of truth for grid, metrics, and font metadata.
All scripts import from here. No hardcoded derived values elsewhere.
"""

# ---------------------------------------------------------------------------
# Grid (primitives only — derived values computed below)
# ---------------------------------------------------------------------------
COLS = 10
MARGIN_TOP = 70
MARGIN_LEFT = 40
CELL_W = 120
CELL_H = 243
GUTTER = 10

# derived
COL_STEP = CELL_W + GUTTER
ROW_STEP = CELL_H + GUTTER

# ---------------------------------------------------------------------------
# Reference lines (offsets from cell top, in px)
# ---------------------------------------------------------------------------
REFLINES = [
    (31, "rgb(167,139,250)"),  # capline
    (71, "rgb(52,211,153)"),  # x-height
    (176, "rgb(250,204,21)"),  # baseline
    (217, "rgb(248,113,113)"),  # descender
]

# absolute positions for row 0 (derived from MARGIN_TOP + offset)
CAPLINE_Y = MARGIN_TOP + 31  # 101
BASELINE_Y = MARGIN_TOP + 176  # 246

# ---------------------------------------------------------------------------
# Font metrics (font units, derived from reference lines and scale)
# ---------------------------------------------------------------------------
CAP_H = 739
X_H = 552
ASC = 881
DESC = -122
LINE_GAP = 250
ADVANCE_W_FACTOR = 5.125  # advance_w = round(CELL_W * factor)

# ---------------------------------------------------------------------------
# Codepoints
# ---------------------------------------------------------------------------
CHARS = [
    "!",
    '"',
    "#",
    "$",
    "%",
    "&",
    "'",
    "(",
    ")",
    "*",
    "+",
    ",",
    "-",
    ".",
    "/",
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    ":",
    ";",
    "<",
    "=",
    ">",
    "?",
    "@",
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "[",
    "\\",
    "]",
    "^",
    "_",
    "`",
    "a",
    "b",
    "c",
    "d",
    "e",
    "f",
    "g",
    "h",
    "i",
    "j",
    "k",
    "l",
    "m",
    "n",
    "o",
    "p",
    "q",
    "r",
    "s",
    "t",
    "u",
    "v",
    "w",
    "x",
    "y",
    "z",
    "{",
    "|",
    "}",
    "~",
    "\u00a9",
    "\u00ae",
    "\u00b0",
    "\u2122",
]

# ---------------------------------------------------------------------------
# Ligatures (set to [] to disable)
# ---------------------------------------------------------------------------
LIGATURES_2 = [
    "->",
    "=>",
    "==",
    "!=",
    ">=",
    "<=",
    "&&",
    "||",
    "::",
    "<<",
    ">>",
]

LIGATURES_3 = [
    "<=>",
]

# ---------------------------------------------------------------------------
# Arrows (single-codepoint symbols placed after ligatures in the sheet)
# ---------------------------------------------------------------------------
ARROWS = [
    "\u2190",  # ← leftwards arrow
    "\u2191",  # ↑ upwards arrow
    "\u2192",  # → rightwards arrow
    "\u2193",  # ↓ downwards arrow
    "\u2194",  # ↔ left right arrow
]

# ---------------------------------------------------------------------------
# Font metadata
# ---------------------------------------------------------------------------
FAMILY = "Display Mono"
VERSION = "1.0"
COPYRIGHT = "MMXXVI \u00a9 JUBILANT RESEARCH OF ECLECTIC NOVELTY IN GENERATING MUSIC. ALL RIGHTS RESERVED."
TRADEMARK = "Display is a registered trademark of \u00a9 JRENG! ALL RIGHTS RESERVED."
VENDOR = "JRNG"
DESIGNER = "JRENG!"
DESCRIPTION = "Designed for Ephemeral Nexus Display."
URL = "https://jrengmusic.com"

# ---------------------------------------------------------------------------
# Weights
# ---------------------------------------------------------------------------
WEIGHTS = [
    {
        "svg": "GlyphSheet_Display-MONO-Book.svg",
        "style": "Book",
        "full": "Display Mono Book",
        "ps": "DisplayMono-Book",
        "weight_class": 350,
    },
    {
        "svg": "GlyphSheet_Display-MONO-Medium.svg",
        "style": "Medium",
        "full": "Display Mono Medium",
        "ps": "DisplayMono-Medium",
        "weight_class": 500,
    },
    {
        "svg": "GlyphSheet_Display-MONO-Bold.svg",
        "style": "Bold",
        "full": "Display Mono Bold",
        "ps": "DisplayMono-Bold",
        "weight_class": 700,
    },
]

# ---------------------------------------------------------------------------
# SVG appearance
# ---------------------------------------------------------------------------
BG_COLOR = "rgb(20,20,20)"
CELL_STROKE = "rgb(50,50,50)"
LABEL_COLOR = "rgb(100,100,100)"
LABEL_FONT_SIZE = 7
GLYPH_FILL = "rgb(224,224,224)"

# ---------------------------------------------------------------------------
# Refline / cell stroke colors (for filtering in transfer/build)
# ---------------------------------------------------------------------------
REFLINE_COLORS = {color for _, color in REFLINES}
