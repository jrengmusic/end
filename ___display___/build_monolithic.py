#!/usr/bin/env python3

import os
import sys
import math

from fontTools import ttLib
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.ttLib.tables._g_l_y_f import Glyph as TTGlyph

from sheet_config import (
    FAMILY,
    VERSION,
    COPYRIGHT,
    TRADEMARK,
    VENDOR,
    DESIGNER,
    DESCRIPTION,
    URL,
    WEIGHTS,
    ASC,
    DESC,
    LINE_GAP,
    ADVANCE_W_FACTOR,
    CELL_W,
)

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MONO_DIR = os.path.join(BASE_DIR, "fonts", "Display Mono")
OUT_DIR = os.path.join(BASE_DIR, "fonts", "Display Monolith")
DONORS_DIR = os.path.join(BASE_DIR, "donors")

DONOR_PATHS = [
    os.path.join(DONORS_DIR, "NotoSansSymbols2-Regular.ttf"),
    os.path.join(DONORS_DIR, "NotoEmoji-Regular.ttf"),
]

MONOLITHIC_FAMILY = "Display Mono"
MONOLITHIC_PS_PREFIX = "DisplayMono"

ADVANCE_W = round(CELL_W * ADVANCE_W_FACTOR)


def check_donors():
    for path in DONOR_PATHS:
        if not os.path.isfile(path):
            sys.exit(f"ERROR: donor font not found: {path}")


def get_cmap(font):
    cmap_table = font["cmap"]
    for subtable in cmap_table.tables:
        if subtable.format in (4, 12) and subtable.platformID in (0, 3):
            return subtable.cmap
    sys.exit(f"ERROR: no usable cmap subtable found in font")


def is_empty_glyph(glyph):
    return glyph is None or glyph.numberOfContours == 0


def decompose_glyph(glyph_name, glyf_table, hmtx_table, glyph_set=None):
    glyph = glyf_table[glyph_name]
    if glyph.isComposite():
        rec = DecomposingRecordingPen(glyph_set)
        glyph_set[glyph_name].draw(rec)
        pen = TTGlyphPen(None)
        rec.replay(pen)
        return pen.glyph()
    return glyph


def scale_glyph(glyph, scale):
    if scale == 1.0:
        return glyph
    if is_empty_glyph(glyph):
        return glyph
    if glyph.numberOfContours < 0:
        raise ValueError("composite glyph passed to scale_glyph — decompose first")

    class ScalingPen:
        def __init__(self, inner, s):
            self.inner = inner
            self.s = s

        def pt(self, pt):
            if pt is None:
                return None
            return (round(pt[0] * self.s), round(pt[1] * self.s))

        def moveTo(self, pt):
            self.inner.moveTo(self.pt(pt))

        def lineTo(self, pt):
            self.inner.lineTo(self.pt(pt))

        def qCurveTo(self, *pts):
            self.inner.qCurveTo(*[self.pt(p) for p in pts])

        def curveTo(self, *pts):
            self.inner.curveTo(*[self.pt(p) for p in pts])

        def closePath(self):
            self.inner.closePath()

        def endPath(self):
            self.inner.endPath()

    scaled_pen = TTGlyphPen(None)
    proxy = ScalingPen(scaled_pen, scale)
    glyph.draw(proxy, None)
    return scaled_pen.glyph()


def translate_glyph(glyph, dx, dy=0):
    if dx == 0 and dy == 0:
        return glyph
    if is_empty_glyph(glyph):
        return glyph
    if glyph.numberOfContours < 0:
        raise ValueError("composite glyph passed to translate_glyph — decompose first")

    class TranslatePen:
        def __init__(self, inner, dx, dy):
            self.inner = inner
            self.dx = dx
            self.dy = dy

        def pt(self, pt):
            if pt is None:
                return None
            return (pt[0] + self.dx, pt[1] + self.dy)

        def moveTo(self, pt):
            self.inner.moveTo(self.pt(pt))

        def lineTo(self, pt):
            self.inner.lineTo(self.pt(pt))

        def qCurveTo(self, *pts):
            self.inner.qCurveTo(*[self.pt(p) for p in pts])

        def curveTo(self, *pts):
            self.inner.curveTo(*[self.pt(p) for p in pts])

        def closePath(self):
            self.inner.closePath()

        def endPath(self):
            self.inner.endPath()

    out_pen = TTGlyphPen(None)
    proxy = TranslatePen(out_pen, dx, dy)
    glyph.draw(proxy, None)
    return out_pen.glyph()


class FakeGlyf:
    pass


def recalc_bounds(glyph):
    if glyph.numberOfContours != 0:
        glyph.recalcBounds(FakeGlyf())


def patch_cmap_subtables(font, new_cmap_entries):
    cmap_table = font["cmap"]
    for subtable in cmap_table.tables:
        if subtable.format in (4, 12) and subtable.platformID in (0, 3):
            subtable.cmap.update(new_cmap_entries)


def get_ref_scale(donor_font, donor_cmap, donor_glyf, upm_scale, base_advance_w):
    """
    Compute a reference scale from U+25A0 BLACK SQUARE — a glyph intentionally
    designed to fill a full cell. All other donor glyphs are scaled by this same
    factor so their size relationships relative to ■ are preserved.

    If ■ is not in this donor, returns None (caller falls back to per-glyph fit_scale).
    """
    REF_CP = 0x25A0  # ■ BLACK SQUARE
    ref_gname = donor_cmap.get(REF_CP)
    if not ref_gname or ref_gname not in donor_glyf:
        return None

    try:
        ref_raw = donor_glyf[ref_gname]
        if ref_raw.isComposite() or is_empty_glyph(ref_raw):
            return None
        ref_raw.recalcBounds(donor_glyf)
    except Exception:
        return None

    ref_w = ref_raw.xMax - ref_raw.xMin
    ref_h = ref_raw.yMax - ref_raw.yMin
    if ref_w <= 0 or ref_h <= 0:
        return None

    # Apply UPM normalization to reference dimensions (same as applied to glyphs)
    ref_w_scaled = ref_w * upm_scale
    ref_h_scaled = ref_h * upm_scale

    max_h = (ASC + LINE_GAP) + (abs(DESC) + LINE_GAP)
    scale_by_w = base_advance_w / ref_w_scaled
    scale_by_h = max_h / ref_h_scaled
    return min(scale_by_w, scale_by_h)


def merge_donor(base_font, donor_font, donor_label, base_advance_w):
    base_glyf = base_font["glyf"]
    base_hmtx = base_font["hmtx"].metrics
    base_cmap = get_cmap(base_font)
    base_existing_cps = set(base_cmap.keys())
    base_glyph_names = set(base_font.getGlyphOrder())

    donor_glyf = donor_font["glyf"]
    donor_hmtx = donor_font["hmtx"].metrics
    donor_cmap = get_cmap(donor_font)

    base_upm = base_font["head"].unitsPerEm
    donor_upm = donor_font["head"].unitsPerEm
    upm_scale = base_upm / donor_upm if donor_upm != base_upm else 1.0

    # Compute reference scale from ■ U+25A0 so all donor glyphs preserve their
    # size relationships relative to a full-cell glyph. Falls back to None if
    # this donor does not contain ■ (e.g. NotoEmoji).
    ref_scale = get_ref_scale(donor_font, donor_cmap, donor_glyf, upm_scale, base_advance_w)
    if ref_scale is not None:
        print(f"  ref_scale from U+25A0: {ref_scale:.4f}")
    else:
        print(f"  ref_scale: not available, using per-glyph fit_scale")

    new_glyphs = {}
    new_metrics = {}
    new_cmap_entries = {}
    copied = 0

    for cp, donor_gname in sorted(donor_cmap.items()):
        if cp in base_existing_cps:
            continue

        if donor_gname not in donor_glyf:
            continue

        candidate = f"uni{cp:04X}"
        if candidate in base_glyph_names or candidate in new_glyphs:
            candidate = f"uni{cp:04X}.noto"

        try:
            raw = decompose_glyph(
                donor_gname, donor_glyf, donor_hmtx, donor_font.getGlyphSet()
            )
        except Exception as e:
            raise RuntimeError(
                f"failed to decompose {donor_gname} (U+{cp:04X}) from {donor_label}: {e}"
            ) from e

        if upm_scale != 1.0:
            raw = scale_glyph(raw, upm_scale)

        if is_empty_glyph(raw):
            continue

        max_h = (ASC + LINE_GAP) + (abs(DESC) + LINE_GAP)
        recalc_bounds(raw)
        glyph_w = raw.xMax - raw.xMin
        glyph_h = raw.yMax - raw.yMin
        if glyph_w > 0 and glyph_h > 0:
            scale_by_w = base_advance_w / glyph_w
            scale_by_h = max_h / glyph_h
            per_glyph_fit = min(scale_by_w, scale_by_h)
            # Use ref_scale to preserve size relationships between donor glyphs.
            # Still clamp with per_glyph_fit so oversized glyphs never overflow.
            fit_scale = min(ref_scale, per_glyph_fit) if ref_scale is not None else per_glyph_fit
            raw = scale_glyph(raw, fit_scale)
            recalc_bounds(raw)
            glyph_w = raw.xMax - raw.xMin
            glyph_h = raw.yMax - raw.yMin
        glyph_cx = raw.xMin + glyph_w // 2
        glyph_cy = raw.yMin + glyph_h // 2
        cell_cx = base_advance_w // 2
        cell_cy = (ASC + DESC) // 2
        dx = cell_cx - glyph_cx
        dy = cell_cy - glyph_cy
        raw = translate_glyph(raw, dx, dy)
        recalc_bounds(raw)
        lsb = raw.xMin

        new_glyphs[candidate] = raw
        new_metrics[candidate] = (base_advance_w, lsb)
        new_cmap_entries[cp] = candidate
        base_existing_cps.add(cp)
        base_glyph_names.add(candidate)
        copied += 1

    if new_glyphs:
        current_order = list(base_font.getGlyphOrder())
        base_font.setGlyphOrder(current_order + list(new_glyphs.keys()))

        for gname, glyph in new_glyphs.items():
            base_glyf[gname] = glyph

        base_hmtx.update(new_metrics)
        patch_cmap_subtables(base_font, new_cmap_entries)

    return copied


def recalc_os2_avg_char_width(font):
    hmtx = font["hmtx"].metrics
    widths = [aw for aw, _ in hmtx.values() if aw > 0]
    if widths:
        font["OS/2"].xAvgCharWidth = round(sum(widths) / len(widths))


def recalc_hhea_advance_width_max(font):
    hmtx = font["hmtx"].metrics
    font["hhea"].advanceWidthMax = max(aw for aw, _ in hmtx.values())


def update_name_table(font, ps_name, style):
    name_table = font["name"]

    def set_name(name_id, value):
        for platform_id, enc_id, lang_id in ((3, 1, 0x0409), (1, 0, 0)):
            name_table.setName(value, name_id, platform_id, enc_id, lang_id)

    is_ribbi = style in ("Regular", "Bold", "Italic", "Bold Italic")
    win_family = MONOLITHIC_FAMILY if is_ribbi else f"{MONOLITHIC_FAMILY} {style}"
    win_style = style if is_ribbi else "Regular"

    set_name(1, win_family)
    set_name(2, win_style)
    set_name(4, f"{MONOLITHIC_FAMILY} {style}")
    set_name(6, ps_name)
    set_name(16, MONOLITHIC_FAMILY)
    set_name(17, style)


def build_monolithic(cfg):
    mono_ps = cfg["ps"]
    style = cfg["style"]
    mono_path = os.path.join(MONO_DIR, f"{mono_ps}.ttf")

    if not os.path.isfile(mono_path):
        sys.exit(f"ERROR: base font not found: {mono_path}\nRun build_fonts.py first.")

    print(f"\n=== {MONOLITHIC_FAMILY} {style} ===")
    print(f"  Base: {mono_path}")

    font = ttLib.TTFont(mono_path)

    donor_counts = []
    for donor_path in DONOR_PATHS:
        donor_label = os.path.basename(donor_path)
        donor_font = ttLib.TTFont(donor_path)
        count = merge_donor(font, donor_font, donor_label, ADVANCE_W)
        donor_font.close()
        donor_counts.append((donor_label, count))
        print(f"  Merged {donor_label}: {count} glyphs copied")

    font["post"].isFixedPitch = 1
    recalc_os2_avg_char_width(font)
    recalc_hhea_advance_width_max(font)

    mono_suffix = mono_ps.replace("DisplayMono-", "")
    ps_name = f"{MONOLITHIC_PS_PREFIX}-{mono_suffix}"
    update_name_table(font, ps_name, style)

    os.makedirs(OUT_DIR, exist_ok=True)
    out_path = os.path.join(OUT_DIR, f"{ps_name}.ttf")
    font.save(out_path)
    font.close()

    total = len(ttLib.TTFont(out_path).getGlyphOrder())
    print(f"  Saved: {out_path}")
    print(f"  Total glyphs: {total}")

    return out_path, donor_counts


if __name__ == "__main__":
    check_donors()
    os.makedirs(OUT_DIR, exist_ok=True)

    all_counts = []
    final_paths = []

    for cfg in WEIGHTS:
        path, counts = build_monolithic(cfg)
        final_paths.append(path)
        all_counts.append((cfg["style"], counts))

    print("\n=== Summary ===")
    for style, counts in all_counts:
        print(f"  {style}:")
        for label, n in counts:
            print(f"    {label}: {n} glyphs")
    print(f"\n  Final output: {OUT_DIR}")
    for p in final_paths:
        print(f"    {os.path.basename(p)}")
