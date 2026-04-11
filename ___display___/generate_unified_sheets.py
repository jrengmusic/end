#!/usr/bin/env python3

import math
import os
import xml.etree.ElementTree as ET

from sheet_config import (
    ARROWS,
    BG_COLOR,
    CELL_H,
    CELL_STROKE,
    CELL_W,
    CHARS,
    COLS,
    COL_STEP,
    GUTTER,
    LABEL_COLOR,
    LABEL_FONT_SIZE,
    LIGATURES_2,
    LIGATURES_3,
    MARGIN_LEFT,
    MARGIN_TOP,
    REFLINES,
    ROW_STEP,
    WEIGHTS,
)

ET.register_namespace("", "http://www.w3.org/2000/svg")


def char_id(ch):
    return "_" + "_".join(f"{ord(c):04X}" for c in ch)


def emit_guides(guides_g, x, y, w, label):
    text = ET.SubElement(guides_g, "text")
    text.set("x", str(x + 2))
    text.set("y", str(y + 8))
    text.set("style", f"font-size:{LABEL_FONT_SIZE}px;fill:{LABEL_COLOR}")
    text.text = label

    for dy, color in REFLINES:
        ry = y + dy
        path = ET.SubElement(guides_g, "path")
        path.set("d", f"M {x},{ry} L {x + w},{ry}")
        path.set("style", f"fill:none;stroke:{color};stroke-width:0.5")

    cx = x + w // 2
    center = ET.SubElement(guides_g, "path")
    center.set("d", f"M {cx},{y} L {cx},{y + CELL_H}")
    center.set(
        "style",
        "fill:none;stroke:rgb(68,68,68);stroke-width:0.5px;stroke-dasharray:2,3",
    )


def emit_cell_group(svg_root, glyph_id, x, y, w):
    g = ET.SubElement(svg_root, "g")
    g.set("id", glyph_id)
    rect = ET.SubElement(g, "rect")
    rect.set("x", str(x))
    rect.set("y", str(y))
    rect.set("width", str(w))
    rect.set("height", str(CELL_H))
    rect.set("style", f"fill:none;stroke:{CELL_STROKE};stroke-width:0.5")


def build_svg():
    char_rows = math.ceil(len(CHARS) / COLS)

    lig2_cols = COLS // 2
    lig2_rows = math.ceil(len(LIGATURES_2) / lig2_cols) if LIGATURES_2 else 0

    lig3_cols = COLS // 3
    lig3_rows = math.ceil(len(LIGATURES_3) / lig3_cols) if LIGATURES_3 else 0

    arrow_rows = math.ceil(len(ARROWS) / COLS) if ARROWS else 0

    total_rows = char_rows + lig2_rows + lig3_rows + arrow_rows

    svg_w = MARGIN_LEFT + COLS * COL_STEP - GUTTER + MARGIN_LEFT
    svg_h = MARGIN_TOP + total_rows * ROW_STEP - GUTTER + 20

    svg = ET.Element("svg")
    svg.set("xmlns", "http://www.w3.org/2000/svg")
    svg.set("width", str(svg_w))
    svg.set("height", str(svg_h))

    bg = ET.SubElement(svg, "rect")
    bg.set("width", str(svg_w))
    bg.set("height", str(svg_h))
    bg.set("fill", BG_COLOR)

    guides_g = ET.SubElement(svg, "g")
    guides_g.set("id", "guides")

    for idx, ch in enumerate(CHARS):
        row = idx // COLS
        col = idx % COLS
        x = MARGIN_LEFT + col * COL_STEP
        y = MARGIN_TOP + row * ROW_STEP
        emit_guides(guides_g, x, y, CELL_W, ch)

    lig2_w = CELL_W * 2 + GUTTER
    lig2_row_offset = char_rows
    for idx, lig in enumerate(LIGATURES_2):
        row = idx // lig2_cols
        col = idx % lig2_cols
        x = MARGIN_LEFT + col * (lig2_w + GUTTER)
        y = MARGIN_TOP + (lig2_row_offset + row) * ROW_STEP
        emit_guides(guides_g, x, y, lig2_w, lig)

    lig3_w = CELL_W * 3 + GUTTER * 2
    lig3_row_offset = char_rows + lig2_rows
    for idx, lig in enumerate(LIGATURES_3):
        row = idx // lig3_cols
        col = idx % lig3_cols
        x = MARGIN_LEFT + col * (lig3_w + GUTTER)
        y = MARGIN_TOP + (lig3_row_offset + row) * ROW_STEP
        emit_guides(guides_g, x, y, lig3_w, lig)

    arrow_row_offset = char_rows + lig2_rows + lig3_rows
    for idx, ch in enumerate(ARROWS):
        row = idx // COLS
        col = idx % COLS
        x = MARGIN_LEFT + col * COL_STEP
        y = MARGIN_TOP + (arrow_row_offset + row) * ROW_STEP
        emit_guides(guides_g, x, y, CELL_W, ch)

    for idx, ch in enumerate(CHARS):
        row = idx // COLS
        col = idx % COLS
        x = MARGIN_LEFT + col * COL_STEP
        y = MARGIN_TOP + row * ROW_STEP
        emit_cell_group(svg, char_id(ch), x, y, CELL_W)

    for idx, lig in enumerate(LIGATURES_2):
        row = idx // lig2_cols
        col = idx % lig2_cols
        x = MARGIN_LEFT + col * (lig2_w + GUTTER)
        y = MARGIN_TOP + (lig2_row_offset + row) * ROW_STEP
        emit_cell_group(svg, char_id(lig), x, y, lig2_w)

    for idx, lig in enumerate(LIGATURES_3):
        row = idx // lig3_cols
        col = idx % lig3_cols
        x = MARGIN_LEFT + col * (lig3_w + GUTTER)
        y = MARGIN_TOP + (lig3_row_offset + row) * ROW_STEP
        emit_cell_group(svg, char_id(lig), x, y, lig3_w)

    for idx, ch in enumerate(ARROWS):
        row = idx // COLS
        col = idx % COLS
        x = MARGIN_LEFT + col * COL_STEP
        y = MARGIN_TOP + (arrow_row_offset + row) * ROW_STEP
        emit_cell_group(svg, char_id(ch), x, y, CELL_W)

    ET.indent(svg, space="  ")
    return (
        ET.tostring(svg, encoding="unicode", xml_declaration=False),
        char_rows,
        lig2_rows,
        lig3_rows,
        arrow_rows,
        total_rows,
        svg_w,
        svg_h,
    )


out_dir = os.path.join(os.path.dirname(__file__), "svg-mono")
os.makedirs(out_dir, exist_ok=True)

svg_str, char_rows, lig2_rows, lig3_rows, arrow_rows, total_rows, svg_w, svg_h = build_svg()

print(f"grid: {total_rows} rows, {svg_w}x{svg_h}px")
print(
    f"  cell: {CELL_W}x{CELL_H}, gutter: {GUTTER}, col_step: {COL_STEP}, row_step: {ROW_STEP}"
)
print(f"  chars:  {len(CHARS)} ({char_rows} rows, 0-{char_rows - 1})")
print(f"  lig2:   {len(LIGATURES_2)} ({lig2_rows} rows, cols={COLS // 2})")
print(f"  lig3:   {len(LIGATURES_3)} ({lig3_rows} rows, cols={COLS // 3})")
print(f"  arrows: {len(ARROWS)} ({arrow_rows} rows, cols={COLS})")

for w in WEIGHTS:
    path = os.path.join(out_dir, w["svg"])
    with open(path, "w", encoding="utf-8") as f:
        f.write(svg_str)
    print(f"wrote {path}")
