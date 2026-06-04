#!/usr/bin/env python3
"""
nf_constraint_gen.py — Nerd Font glyph constraint table generator.

Parses font-patcher via AST to extract per-glyph attributes and scale rules,
then emits gen/GlyphConstraintTable.cpp with a C++ switch table.

Usage: python3 gen/nf_constraint_gen.py
"""

from __future__ import annotations

import ast
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent
FONT_PATCHER = REPO_ROOT / "___display___" / "FontPatcher" / "font-patcher"
GLYPH_SRC_DIR = REPO_ROOT / "___display___" / "FontPatcher" / "src" / "glyphs"
SYMBOLS_NF = REPO_ROOT / "Source" / "fonts" / "SymbolsNerdFont-Regular.ttf"
OUTPUT_FILE = (
    REPO_ROOT / "Source" / "terminal" / "rendering" / "GlyphConstraintTable.cpp"
)

# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------


@dataclass
class GlyphConstraint:
    scaleMode: str = "none"
    alignH: str = "none"
    alignV: str = "none"
    heightRef: str = "cell"
    padTop: float = 0.0
    padLeft: float = 0.0
    padRight: float = 0.0
    padBottom: float = 0.0
    relativeWidth: float = 1.0
    relativeHeight: float = 1.0
    relativeX: float = 0.0
    relativeY: float = 0.0
    maxAspectRatio: float = 0.0
    maxCellSpan: int = 2

    def is_default(self) -> bool:
        return self == GlyphConstraint()

    def attr_hash(self) -> tuple:
        return (
            self.scaleMode,
            self.alignH,
            self.alignV,
            self.heightRef,
            round(self.padTop, 6),
            round(self.padLeft, 6),
            round(self.padRight, 6),
            round(self.padBottom, 6),
            round(self.relativeWidth, 6),
            round(self.relativeHeight, 6),
            round(self.relativeX, 6),
            round(self.relativeY, 6),
            round(self.maxAspectRatio, 6),
            self.maxCellSpan,
        )


# ---------------------------------------------------------------------------
# Phase 1: AST parsing of font-patcher
# ---------------------------------------------------------------------------


def literal_eval_node(node: ast.expr, symtable: dict[str, Any]) -> Any:
    """Evaluate an AST expression node to a Python value."""
    if isinstance(node, ast.Constant):
        return node.value
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
        return -literal_eval_node(node.operand, symtable)
    if isinstance(node, ast.Name):
        name = node.id
        if name == "True":
            return True
        if name == "False":
            return False
        if name == "None":
            return None
        return symtable.get(name)
    if isinstance(node, ast.Attribute):
        # self.args.* — treat all flags as True
        return True
    if isinstance(node, ast.Dict):
        result = {}
        for k, v in zip(node.keys, node.values):
            key = literal_eval_node(k, symtable) if k is not None else None
            val = literal_eval_node(v, symtable)
            result[key] = val
        return result
    if isinstance(node, ast.List):
        return [literal_eval_node(e, symtable) for e in node.elts]
    if isinstance(node, ast.Tuple):
        return tuple(literal_eval_node(e, symtable) for e in node.elts)
    if isinstance(node, ast.Set):
        return {literal_eval_node(e, symtable) for e in node.elts}
    if isinstance(node, ast.Call):
        func_name = None
        if isinstance(node.func, ast.Name):
            func_name = node.func.id
        elif isinstance(node.func, ast.Attribute):
            func_name = node.func.attr
        if func_name == "range" and len(node.args) >= 2:
            start = literal_eval_node(node.args[0], symtable)
            stop = literal_eval_node(node.args[1], symtable)
            step = (
                literal_eval_node(node.args[2], symtable) if len(node.args) > 2 else 1
            )
            if (
                isinstance(start, int)
                and isinstance(stop, int)
                and isinstance(step, int)
            ):
                return range(start, stop, step)
        return None
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.BitOr):
        # dict | dict merge (Python 3.9+)
        left = literal_eval_node(node.left, symtable)
        right = literal_eval_node(node.right, symtable)
        if isinstance(left, dict) and isinstance(right, dict):
            return {**left, **right}
    if isinstance(node, ast.JoinedStr):
        # f-string — skip
        return None
    if isinstance(node, ast.IfExp):
        # ternary — evaluate both branches, return body (assume condition True)
        return literal_eval_node(node.body, symtable)
    if isinstance(node, ast.Starred):
        return literal_eval_node(node.value, symtable)
    return None


def expand_list_with_starred(elts: list[ast.expr], symtable: dict[str, Any]) -> list:
    """Expand a list literal that may contain starred expressions."""
    result = []
    for elt in elts:
        if isinstance(elt, ast.Starred):
            val = literal_eval_node(elt.value, symtable)
            if val is not None:
                result.extend(val)
        else:
            val = literal_eval_node(elt, symtable)
            if val is not None:
                result.append(val)
    return result


def eval_list_node(node: ast.expr, symtable: dict[str, Any]) -> list | range | None:
    """Evaluate a list/range node, handling starred expansions."""
    if isinstance(node, ast.List):
        return expand_list_with_starred(node.elts, symtable)
    if isinstance(node, ast.Call):
        return literal_eval_node(node, symtable)
    if isinstance(node, ast.Name):
        return symtable.get(node.id)
    return None


def parse_font_patcher() -> list[dict]:
    """
    Parse font-patcher via AST.
    Returns the resolved patch_set list.
    """
    print(f"Parsing {FONT_PATCHER} ...", file=sys.stderr)
    source = FONT_PATCHER.read_text(encoding="utf-8")
    tree = ast.parse(source, filename=str(FONT_PATCHER))

    # Find class font_patcher -> method setup_patch_set
    setup_node: ast.FunctionDef | None = None
    for node in ast.walk(tree):
        if isinstance(node, ast.ClassDef) and node.name == "font_patcher":
            for item in node.body:
                if isinstance(item, ast.FunctionDef) and item.name == "setup_patch_set":
                    setup_node = item
                    break
        if setup_node:
            break

    if setup_node is None:
        print("ERROR: Could not find font_patcher.setup_patch_set", file=sys.stderr)
        sys.exit(1)

    # Walk statements and build a symbol table
    symtable: dict[str, Any] = {}

    patch_set: list[dict] | None = None

    for stmt in ast.walk(setup_node):
        if not isinstance(stmt, ast.Assign):
            continue
        for target in stmt.targets:
            # Simple name assignment: FOO = ...
            if isinstance(target, ast.Name):
                name = target.id
                val = literal_eval_node(stmt.value, symtable)
                symtable[name] = val

            # self.patch_set = [...]
            elif (
                isinstance(target, ast.Attribute)
                and isinstance(target.value, ast.Name)
                and target.value.id == "self"
                and target.attr == "patch_set"
            ):
                if isinstance(stmt.value, ast.List):
                    patch_set = resolve_patch_set(stmt.value, symtable)

    if patch_set is None:
        print("ERROR: Could not find self.patch_set assignment", file=sys.stderr)
        sys.exit(1)

    return patch_set


def resolve_patch_set(list_node: ast.List, symtable: dict[str, Any]) -> list[dict]:
    """Resolve each dict in the patch_set list."""
    result = []
    for elt in list_node.elts:
        if not isinstance(elt, ast.Dict):
            continue
        entry: dict = {}
        for k, v in zip(elt.keys, elt.values):
            key = literal_eval_node(k, symtable)
            if key is None:
                continue
            val = literal_eval_node(v, symtable)
            entry[key] = val
        result.append(entry)
    return result


# ---------------------------------------------------------------------------
# Phase 2: Build codepoint mapping
# ---------------------------------------------------------------------------


def get_existing_codepoints_in_font(font_path: Path) -> set[int]:
    """Return set of codepoints present in the font file."""
    try:
        from fontTools.ttLib import TTFont

        font = TTFont(str(font_path), lazy=True)
        cmap = font.getBestCmap()
        font.close()
        if cmap:
            return set(cmap.keys())
        return set()
    except Exception as exc:
        print(f"  WARNING: Could not open {font_path}: {exc}", file=sys.stderr)
        return set()


def build_codepoint_map(patch_set: list[dict]) -> dict[int, dict]:
    """
    For each enabled patch set, map NF destination codepoints to their
    attribute dicts.

    Returns: {nf_codepoint: attr_dict}
    """
    codepoint_map: dict[int, dict] = {}

    for entry in patch_set:
        if not entry.get("Enabled", False):
            continue

        name = entry.get("Name", "?")
        filename = entry.get("Filename", "")
        exact = entry.get("Exact", True)
        sym_start = entry.get("SymStart", 0)
        sym_end = entry.get("SymEnd", 0)
        src_start = entry.get("SrcStart")
        attributes = entry.get("Attributes") or {}

        if not isinstance(attributes, dict):
            attributes = {}

        if exact:
            # NF codepoint == source codepoint
            for cp in range(sym_start, sym_end + 1):
                attr = attributes.get(cp) or attributes.get("default") or {}
                codepoint_map[cp] = attr
        else:
            # Non-exact: glyphs packed from SrcStart, skipping missing
            if src_start is None:
                print(
                    f"  WARNING: {name}: Exact=False but SrcStart is None, skipping",
                    file=sys.stderr,
                )
                continue

            font_path = GLYPH_SRC_DIR / filename
            if not font_path.exists():
                print(
                    f"  WARNING: {name}: font not found at {font_path}, skipping",
                    file=sys.stderr,
                )
                continue

            print(f"  Loading {font_path.name} for {name} ...", file=sys.stderr)
            existing = get_existing_codepoints_in_font(font_path)
            if not existing:
                print(
                    f"  WARNING: {name}: no codepoints found in font, skipping",
                    file=sys.stderr,
                )
                continue

            dst = src_start
            for src_cp in range(sym_start, sym_end + 1):
                if src_cp in existing:
                    attr = attributes.get(src_cp) or attributes.get("default") or {}
                    codepoint_map[dst] = attr
                    dst += 1

    return codepoint_map


# ---------------------------------------------------------------------------
# Phase 3: Scale group bounding box measurement
# ---------------------------------------------------------------------------


@dataclass
class BBox:
    xmin: float
    ymin: float
    xmax: float
    ymax: float

    @property
    def width(self) -> float:
        return self.xmax - self.xmin

    @property
    def height(self) -> float:
        return self.ymax - self.ymin

    def union(self, other: "BBox") -> "BBox":
        return BBox(
            min(self.xmin, other.xmin),
            min(self.ymin, other.ymin),
            max(self.xmax, other.xmax),
            max(self.ymax, other.ymax),
        )


def measure_glyph_bbox(glyph_set: Any, glyph_name: str) -> BBox | None:
    """Measure bounding box of a single glyph using BoundsPen."""
    try:
        from fontTools.pens.boundsPen import BoundsPen

        pen = BoundsPen(glyph_set)
        glyph_set[glyph_name].draw(pen)
        if pen.bounds is None:
            return None
        xmin, ymin, xmax, ymax = pen.bounds
        return BBox(xmin, ymin, xmax, ymax)
    except Exception:
        return None


def build_scale_group_data(
    patch_set: list[dict],
) -> dict[int, tuple[float, float, float, float]]:
    """
    For each patch set with ScaleGroups, measure group bounding boxes
    from the Symbols NF font and compute per-glyph relative metrics.

    Returns: {nf_codepoint: (relativeWidth, relativeHeight, relativeX, relativeY)}
    Only populated when Symbols NF font is available.
    """
    if not SYMBOLS_NF.exists():
        print(
            f"WARNING: {SYMBOLS_NF} not found — scale group relative metrics will be skipped",
            file=sys.stderr,
        )
        return {}

    print(f"Loading Symbols NF font from {SYMBOLS_NF} ...", file=sys.stderr)
    try:
        from fontTools.ttLib import TTFont

        symbols_font = TTFont(str(SYMBOLS_NF), lazy=True)
        glyph_set = symbols_font.getGlyphSet()
        cmap = symbols_font.getBestCmap() or {}
        cmap_rev = {name: cp for cp, name in cmap.items()}
    except Exception as exc:
        print(f"WARNING: Could not load Symbols NF font: {exc}", file=sys.stderr)
        return {}

    relative_data: dict[int, tuple[float, float, float, float]] = {}

    for entry in patch_set:
        if not entry.get("Enabled", False):
            continue
        scale_rules = entry.get("ScaleRules")
        if not scale_rules:
            continue
        scale_groups = scale_rules.get("ScaleGroups")
        if not scale_groups:
            continue

        for group in scale_groups:
            # Collect codepoints in this group
            group_cps: list[int] = []
            if isinstance(group, range):
                group_cps = list(group)
            elif isinstance(group, (list, tuple)):
                for item in group:
                    if isinstance(item, int):
                        group_cps.append(item)
                    elif isinstance(item, range):
                        group_cps.extend(item)

            if not group_cps:
                continue

            # Measure each glyph and compute group union bbox
            glyph_bboxes: dict[int, BBox] = {}
            group_bbox: BBox | None = None

            for cp in group_cps:
                glyph_name = cmap.get(cp)
                if glyph_name is None or glyph_name not in glyph_set:
                    continue
                bbox = measure_glyph_bbox(glyph_set, glyph_name)
                if bbox is None:
                    continue
                glyph_bboxes[cp] = bbox
                group_bbox = bbox if group_bbox is None else group_bbox.union(bbox)

            if group_bbox is None or group_bbox.height == 0:
                continue

            group_w = group_bbox.width
            group_h = group_bbox.height

            for cp, bbox in glyph_bboxes.items():
                rel_h = bbox.height / group_h if group_h > 0 else 1.0
                rel_y = (bbox.ymin - group_bbox.ymin) / group_h if group_h > 0 else 0.0
                rel_w = bbox.width / group_w if group_w > 0 else 1.0
                rel_x = (bbox.xmin - group_bbox.xmin) / group_w if group_w > 0 else 0.0
                relative_data[cp] = (rel_w, rel_h, rel_x, rel_y)

    try:
        symbols_font.close()
    except Exception:
        pass

    return relative_data


# ---------------------------------------------------------------------------
# Phase 4: Translate attributes to GlyphConstraint
# ---------------------------------------------------------------------------


def translate_attr(
    attr: dict, relative: tuple[float, float, float, float] | None
) -> GlyphConstraint:
    """Convert a font-patcher attribute dict to a GlyphConstraint."""
    c = GlyphConstraint()

    stretch: str = attr.get("stretch", "pa") or "pa"
    params: dict = attr.get("params") or {}
    align: str = attr.get("align", "c") or "c"
    valign: str = attr.get("valign", "c") or "c"

    overlap: float = float(params.get("overlap", 0.0) or 0.0)
    ypadding: float = float(params.get("ypadding", 0.0) or 0.0)
    xy_ratio: float = float(params.get("xy-ratio", 0.0) or 0.0)

    # HeightRef: '^' means use full cell height
    if "^" in stretch:
        c.heightRef = "cell"
    else:
        c.heightRef = "icon"

    # ScaleMode
    base = stretch.replace("^", "").replace("!", "")
    force = "!" in stretch

    if base == "pa":
        if force or overlap > 0:
            c.scaleMode = "cover"
        else:
            c.scaleMode = "adaptiveScale"
        c.maxCellSpan = 2
    elif base == "pa1":
        if force or overlap > 0:
            c.scaleMode = "cover"
        else:
            c.scaleMode = "adaptiveScale"
        c.maxCellSpan = 1
    elif base == "pa1!":
        c.scaleMode = "cover"
        c.maxCellSpan = 1
    elif base == "xy":
        c.scaleMode = "stretch"
        c.maxCellSpan = 1
    elif base == "xy1":
        c.scaleMode = "stretch"
        c.maxCellSpan = 1
    elif base == "xy2":
        c.scaleMode = "stretch"
        c.maxCellSpan = 2
    elif base == "":
        c.scaleMode = "none"
    else:
        # Fallback: treat unknown as adaptiveScale
        c.scaleMode = "adaptiveScale"
        c.maxCellSpan = 2

    # AlignH
    if align == "l":
        c.alignH = "start"
    elif align == "r":
        c.alignH = "end"
    else:
        c.alignH = "center"

    # AlignV
    if valign == "c":
        c.alignV = "center"
    else:
        c.alignV = "none"

    # Padding from overlap
    if overlap != 0.0:
        horiz = overlap / 2.0
        vert = min(overlap / 2.0, 0.01)
        c.padLeft = -horiz
        c.padRight = -horiz
        c.padTop = -vert
        c.padBottom = -vert

    # Padding from ypadding
    if ypadding != 0.0:
        c.padTop = ypadding / 2.0
        c.padBottom = ypadding / 2.0

    # maxAspectRatio
    if xy_ratio > 0.0:
        c.maxAspectRatio = xy_ratio

    # Relative metrics from scale group measurement
    if relative is not None:
        rel_w, rel_h, rel_x, rel_y = relative
        c.relativeWidth = rel_w
        c.relativeHeight = rel_h
        c.relativeX = rel_x
        c.relativeY = rel_y

    return c


# ---------------------------------------------------------------------------
# Phase 5: Emit C++ switch table
# ---------------------------------------------------------------------------


def fmt_float(v: float) -> str:
    """Format a float for C++ source: 4 decimal places, trailing f."""
    return f"{v:.4f}f"


def emit_constraint_block(
    c: GlyphConstraint, indent: str = "            "
) -> list[str]:
    """Emit the body of a switch arm (without case/return)."""
    lines = []
    lines.append(f"{indent}GlyphConstraint c;")

    default = GlyphConstraint()

    if c.scaleMode != default.scaleMode:
        lines.append(
            f"{indent}c.scaleMode = GlyphConstraint::ScaleMode::{c.scaleMode};"
        )
    if c.alignH != default.alignH:
        lines.append(f"{indent}c.alignH = GlyphConstraint::Align::{c.alignH};")
    if c.alignV != default.alignV:
        lines.append(f"{indent}c.alignV = GlyphConstraint::Align::{c.alignV};")
    if c.heightRef != default.heightRef:
        lines.append(
            f"{indent}c.heightRef = GlyphConstraint::HeightRef::{c.heightRef};"
        )
    if c.padTop != default.padTop:
        lines.append(f"{indent}c.padTop = {fmt_float(c.padTop)};")
    if c.padLeft != default.padLeft:
        lines.append(f"{indent}c.padLeft = {fmt_float(c.padLeft)};")
    if c.padRight != default.padRight:
        lines.append(f"{indent}c.padRight = {fmt_float(c.padRight)};")
    if c.padBottom != default.padBottom:
        lines.append(f"{indent}c.padBottom = {fmt_float(c.padBottom)};")
    if c.relativeWidth != default.relativeWidth:
        lines.append(f"{indent}c.relativeWidth = {fmt_float(c.relativeWidth)};")
    if c.relativeHeight != default.relativeHeight:
        lines.append(f"{indent}c.relativeHeight = {fmt_float(c.relativeHeight)};")
    if c.relativeX != default.relativeX:
        lines.append(f"{indent}c.relativeX = {fmt_float(c.relativeX)};")
    if c.relativeY != default.relativeY:
        lines.append(f"{indent}c.relativeY = {fmt_float(c.relativeY)};")
    if c.maxAspectRatio != default.maxAspectRatio:
        lines.append(f"{indent}c.maxAspectRatio = {fmt_float(c.maxAspectRatio)};")
    if c.maxCellSpan != default.maxCellSpan:
        lines.append(f"{indent}c.maxCellSpan = {c.maxCellSpan};")

    lines.append(f"{indent}return c;")
    return lines


def coalesce_ranges(codepoints: list[int]) -> list[tuple[int, int]]:
    """Coalesce a sorted list of codepoints into (start, end) inclusive ranges."""
    if not codepoints:
        return []
    codepoints = sorted(codepoints)
    ranges: list[tuple[int, int]] = []
    start = codepoints[0]
    end = codepoints[0]
    for cp in codepoints[1:]:
        if cp == end + 1:
            end = cp
        else:
            ranges.append((start, end))
            start = cp
            end = cp
    ranges.append((start, end))
    return ranges


def emit_case_label(start: int, end: int) -> str:
    if start == end:
        return f"        case 0x{start:x}:"
    lines = []
    for cp in range(start, end + 1):
        lines.append(f"        case 0x{cp:x}:")
    return "\n".join(lines)


def generate_cpp(
    codepoint_map: dict[int, dict],
    relative_data: dict[int, tuple[float, float, float, float]],
    patch_set: list[dict],
) -> str:
    """Generate the full C++ source string."""

    # Build per-codepoint constraints
    cp_constraints: dict[int, GlyphConstraint] = {}
    for cp, attr in codepoint_map.items():
        rel = relative_data.get(cp)
        c = translate_attr(attr, rel)
        cp_constraints[cp] = c

    # Filter out default (inactive) constraints — they fall through to default:
    active: dict[int, GlyphConstraint] = {
        cp: c for cp, c in cp_constraints.items() if not c.is_default()
    }

    # Group codepoints by attribute hash
    hash_to_cps: dict[tuple, list[int]] = {}
    hash_to_constraint: dict[tuple, GlyphConstraint] = {}
    for cp, c in active.items():
        h = c.attr_hash()
        hash_to_cps.setdefault(h, []).append(cp)
        hash_to_constraint[h] = c

    # Build patch set name lookup for comments
    # Map each codepoint to its patch set name (last writer wins, same as patcher)
    cp_to_name: dict[int, str] = {}
    for cp in active:
        cp_to_name[cp] = "Nerd Fonts"
    for entry in patch_set:
        if not entry.get("Enabled", False):
            continue
        name = entry.get("Name", "")
        sym_start = entry.get("SymStart", 0)
        sym_end = entry.get("SymEnd", 0)
        for cp in range(sym_start, sym_end + 1):
            if cp in active:
                cp_to_name[cp] = name

    # Sort arms by first codepoint for readability
    arms: list[tuple[tuple, GlyphConstraint, list[int]]] = []
    for h, cps in hash_to_cps.items():
        arms.append((h, hash_to_constraint[h], sorted(cps)))
    arms.sort(key=lambda x: x[2][0])

    total_cps = len(active)
    total_arms = len(arms)

    lines: list[str] = []
    lines.append("// GENERATED FILE -- do not edit")
    lines.append("// Generated by gen/nf_constraint_gen.py from NF patcher v3.4.0")
    lines.append(f"// Codepoints: {total_cps} across {total_arms} switch arms")
    lines.append("")
    lines.append('#include "GlyphConstraint.h"')
    lines.append("")
    lines.append("GlyphConstraint getGlyphConstraint (uint32_t codepoint) noexcept")
    lines.append("{")
    lines.append("    switch (codepoint)")
    lines.append("    {")

    prev_name: str | None = None
    for h, c, cps in arms:
        # Section comment when patch set changes
        arm_name = cp_to_name.get(cps[0], "")
        if arm_name != prev_name:
            lines.append(f"        // {arm_name}")
            prev_name = arm_name

        ranges = coalesce_ranges(cps)
        for start, end in ranges:
            lines.append(emit_case_label(start, end))
        lines.append("        {")
        lines.extend(emit_constraint_block(c))
        lines.append("        }")
        lines.append("")

    lines.append("        default:")
    lines.append("            return {};")
    lines.append("    }")
    lines.append("}")
    lines.append("")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main() -> None:
    if not FONT_PATCHER.exists():
        print(f"ERROR: font-patcher not found at {FONT_PATCHER}", file=sys.stderr)
        sys.exit(1)

    # Phase 1
    patch_set = parse_font_patcher()
    print(f"  Found {len(patch_set)} patch set entries", file=sys.stderr)

    # Phase 2
    print("Building codepoint map ...", file=sys.stderr)
    codepoint_map = build_codepoint_map(patch_set)
    print(f"  Mapped {len(codepoint_map)} NF codepoints", file=sys.stderr)

    # Phase 3
    print("Measuring scale group bounding boxes ...", file=sys.stderr)
    relative_data = build_scale_group_data(patch_set)
    print(f"  Relative metrics for {len(relative_data)} codepoints", file=sys.stderr)

    # Phase 4 + 5
    print("Generating C++ ...", file=sys.stderr)
    cpp = generate_cpp(codepoint_map, relative_data, patch_set)

    OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_FILE.write_text(cpp, encoding="utf-8")
    print(f"Wrote {OUTPUT_FILE}", file=sys.stderr)

    # Count active arms for summary
    lines = cpp.splitlines()
    arm_count = sum(1 for l in lines if l.strip().startswith("case "))
    print(f"Done: {arm_count} case labels in {OUTPUT_FILE.name}", file=sys.stderr)


if __name__ == "__main__":
    main()
