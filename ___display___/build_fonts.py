#!/usr/bin/env python3
"""
Build Display Mono TTF fonts from GlyphSheet SVGs.
All config imported from sheet_config.py.
"""

import xml.etree.ElementTree as ET
import re
import math
import os
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.transformPen import TransformPen
from fontTools import ttLib
from sheet_config import (
    COLS,
    MARGIN_TOP,
    MARGIN_LEFT,
    CELL_W,
    CELL_H,
    GUTTER,
    COL_STEP,
    ROW_STEP,
    BASELINE_Y,
    CAPLINE_Y,
    CAP_H,
    X_H,
    ASC,
    DESC,
    LINE_GAP,
    ADVANCE_W_FACTOR,
    FAMILY,
    VERSION,
    COPYRIGHT,
    TRADEMARK,
    VENDOR,
    DESIGNER,
    DESCRIPTION,
    URL,
    WEIGHTS,
)

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
SVG_DIR = os.path.join(BASE_DIR, "svg-mono")
OUT_DIR = os.path.join(BASE_DIR, "fonts", "Display Mono")
os.makedirs(OUT_DIR, exist_ok=True)

NS = {"svg": "http://www.w3.org/2000/svg"}

# ---------------------------------------------------------------------------
# SVG path tokenizer / transformer
# ---------------------------------------------------------------------------


def tokenize_path(d):
    """Yield (cmd, [args]) tuples from an SVG path d string."""
    token_re = re.compile(
        r"([MmZzLlHhVvCcSsQqTtAa])|([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)"
    )
    tokens = token_re.findall(d)
    cmd = None
    args = []
    for cmd_tok, num_tok in tokens:
        if cmd_tok:
            if cmd is not None:
                yield cmd, args
            cmd = cmd_tok
            args = []
        elif num_tok:
            args.append(float(num_tok))
    if cmd is not None:
        yield cmd, args


def chunks(lst, n):
    for i in range(0, len(lst), n):
        yield lst[i : i + n]


def draw_svg_path_to_pen(d, pen, transform=None):
    """
    Parse SVG path d and draw to a fontTools pen.
    transform: (sx, 0, 0, sy, dx, dy) affine tuple applied to every point.
    """

    def tx(x, y):
        if transform is None:
            return x, y
        sx, shy, shx, sy, dx, dy = transform
        return sx * x + shx * y + dx, shy * x + sy * y + dy

    cx, cy = 0.0, 0.0  # current point
    sx, sy_ = 0.0, 0.0  # subpath start
    last_ctrl = None  # for S/s, T/t reflection

    in_contour = False

    def ensure_open():
        nonlocal in_contour
        # nothing needed; moveTo opens it

    def close_if_open():
        nonlocal in_contour
        if in_contour:
            pen.endPath()
            in_contour = False

    for cmd, args in tokenize_path(d):
        if cmd == "M":
            close_if_open()
            pts = list(chunks(args, 2))
            first = pts[0]
            cx, cy = first
            sx, sy_ = cx, cy
            pen.moveTo(tx(cx, cy))
            in_contour = True
            last_ctrl = None
            for p in pts[1:]:
                cx, cy = p
                pen.lineTo(tx(cx, cy))
                last_ctrl = None

        elif cmd == "m":
            close_if_open()
            pts = list(chunks(args, 2))
            first = pts[0]
            cx += first[0]
            cy += first[1]
            sx, sy_ = cx, cy
            pen.moveTo(tx(cx, cy))
            in_contour = True
            last_ctrl = None
            for p in pts[1:]:
                cx += p[0]
                cy += p[1]
                pen.lineTo(tx(cx, cy))
                last_ctrl = None

        elif cmd == "Z" or cmd == "z":
            if in_contour:
                pen.closePath()
                in_contour = False
            cx, cy = sx, sy_
            last_ctrl = None

        elif cmd == "L":
            for p in chunks(args, 2):
                cx, cy = p
                pen.lineTo(tx(cx, cy))
                last_ctrl = None

        elif cmd == "l":
            for p in chunks(args, 2):
                cx += p[0]
                cy += p[1]
                pen.lineTo(tx(cx, cy))
                last_ctrl = None

        elif cmd == "H":
            for x in args:
                cx = x
                pen.lineTo(tx(cx, cy))
                last_ctrl = None

        elif cmd == "h":
            for x in args:
                cx += x
                pen.lineTo(tx(cx, cy))
                last_ctrl = None

        elif cmd == "V":
            for y in args:
                cy = y
                pen.lineTo(tx(cx, cy))
                last_ctrl = None

        elif cmd == "v":
            for y in args:
                cy += y
                pen.lineTo(tx(cx, cy))
                last_ctrl = None

        elif cmd == "C":
            for chunk in chunks(args, 6):
                x1, y1, x2, y2, x, y = chunk
                pen.curveTo(tx(x1, y1), tx(x2, y2), tx(x, y))
                last_ctrl = (x2, y2)
                cx, cy = x, y

        elif cmd == "c":
            for chunk in chunks(args, 6):
                x1, y1, x2, y2, x, y = chunk
                ax1, ay1 = cx + x1, cy + y1
                ax2, ay2 = cx + x2, cy + y2
                ax, ay = cx + x, cy + y
                pen.curveTo(tx(ax1, ay1), tx(ax2, ay2), tx(ax, ay))
                last_ctrl = (ax2, ay2)
                cx, cy = ax, ay

        elif cmd == "S":
            for chunk in chunks(args, 4):
                x2, y2, x, y = chunk
                if last_ctrl is not None:
                    x1 = 2 * cx - last_ctrl[0]
                    y1 = 2 * cy - last_ctrl[1]
                else:
                    x1, y1 = cx, cy
                pen.curveTo(tx(x1, y1), tx(x2, y2), tx(x, y))
                last_ctrl = (x2, y2)
                cx, cy = x, y

        elif cmd == "s":
            for chunk in chunks(args, 4):
                x2, y2, x, y = chunk
                ax2, ay2 = cx + x2, cy + y2
                ax, ay = cx + x, cy + y
                if last_ctrl is not None:
                    ax1 = 2 * cx - last_ctrl[0]
                    ay1 = 2 * cy - last_ctrl[1]
                else:
                    ax1, ay1 = cx, cy
                pen.curveTo(tx(ax1, ay1), tx(ax2, ay2), tx(ax, ay))
                last_ctrl = (ax2, ay2)
                cx, cy = ax, ay

        elif cmd == "Q":
            for chunk in chunks(args, 4):
                x1, y1, x, y = chunk
                pen.qCurveTo(tx(x1, y1), tx(x, y))
                last_ctrl = (x1, y1)
                cx, cy = x, y

        elif cmd == "q":
            for chunk in chunks(args, 4):
                x1, y1, x, y = chunk
                ax1, ay1 = cx + x1, cy + y1
                ax, ay = cx + x, cy + y
                pen.qCurveTo(tx(ax1, ay1), tx(ax, ay))
                last_ctrl = (ax1, ay1)
                cx, cy = ax, ay

        elif cmd == "T":
            for p in chunks(args, 2):
                x, y = p
                if last_ctrl is not None:
                    x1 = 2 * cx - last_ctrl[0]
                    y1 = 2 * cy - last_ctrl[1]
                else:
                    x1, y1 = cx, cy
                pen.qCurveTo(tx(x1, y1), tx(x, y))
                last_ctrl = (x1, y1)
                cx, cy = x, y

        elif cmd == "t":
            for p in chunks(args, 2):
                x, y = p
                ax, ay = cx + p[0], cy + p[1]
                if last_ctrl is not None:
                    ax1 = 2 * cx - last_ctrl[0]
                    ay1 = 2 * cy - last_ctrl[1]
                else:
                    ax1, ay1 = cx, cy
                pen.qCurveTo(tx(ax1, ay1), tx(ax, ay))
                last_ctrl = (ax1, ay1)
                cx, cy = ax, ay

        elif cmd == "A" or cmd == "a":
            # Arc: approximate with cubic beziers
            relative = cmd == "a"
            for chunk in chunks(args, 7):
                rx, ry, x_rot, large, sweep, x, y = chunk
                if relative:
                    x, y = cx + x, cy + y
                arc_to_bezier(
                    pen, cx, cy, rx, ry, x_rot, int(large), int(sweep), x, y, tx
                )
                last_ctrl = None
                cx, cy = x, y

    if in_contour:
        pen.endPath()


def arc_to_bezier(pen, x1, y1, rx, ry, phi, large_arc, sweep, x2, y2, tx):
    """Convert SVG arc to cubic bezier curves and draw to pen."""
    if x1 == x2 and y1 == y2:
        return
    if rx == 0 or ry == 0:
        pen.lineTo(tx(x2, y2))
        return

    phi_rad = math.radians(phi)
    cos_phi = math.cos(phi_rad)
    sin_phi = math.sin(phi_rad)

    dx = (x1 - x2) / 2
    dy = (y1 - y2) / 2
    x1p = cos_phi * dx + sin_phi * dy
    y1p = -sin_phi * dx + cos_phi * dy

    rx = abs(rx)
    ry = abs(ry)
    x1p_sq = x1p * x1p
    y1p_sq = y1p * y1p
    rx_sq = rx * rx
    ry_sq = ry * ry

    lam = x1p_sq / rx_sq + y1p_sq / ry_sq
    if lam > 1:
        s = math.sqrt(lam)
        rx *= s
        ry *= s
        rx_sq = rx * rx
        ry_sq = ry * ry

    num = max(0, rx_sq * ry_sq - rx_sq * y1p_sq - ry_sq * x1p_sq)
    den = rx_sq * y1p_sq + ry_sq * x1p_sq
    sq = math.sqrt(num / den) if den != 0 else 0
    if large_arc == sweep:
        sq = -sq

    cxp = sq * rx * y1p / ry
    cyp = -sq * ry * x1p / rx

    cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) / 2
    cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) / 2

    def angle(ux, uy, vx, vy):
        n = math.sqrt(ux * ux + uy * uy) * math.sqrt(vx * vx + vy * vy)
        if n == 0:
            return 0
        c = max(-1, min(1, (ux * vx + uy * vy) / n))
        a = math.acos(c)
        if ux * vy - uy * vx < 0:
            a = -a
        return a

    theta1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    dtheta = angle(
        (x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry
    )

    if not sweep and dtheta > 0:
        dtheta -= 2 * math.pi
    elif sweep and dtheta < 0:
        dtheta += 2 * math.pi

    n_segs = max(1, math.ceil(abs(dtheta) / (math.pi / 2)))
    d_theta_per = dtheta / n_segs
    alpha = 4 / 3 * math.tan(d_theta_per / 4)

    theta = theta1
    for _ in range(n_segs):
        cos_t = math.cos(theta)
        sin_t = math.sin(theta)
        cos_t2 = math.cos(theta + d_theta_per)
        sin_t2 = math.sin(theta + d_theta_per)

        p1x = cx + cos_phi * (rx * cos_t) - sin_phi * (ry * sin_t)
        p1y = cy + sin_phi * (rx * cos_t) + cos_phi * (ry * sin_t)
        d1x = cos_phi * (-rx * sin_t) - sin_phi * (ry * cos_t)
        d1y = sin_phi * (-rx * sin_t) + cos_phi * (ry * cos_t)
        p2x = cx + cos_phi * (rx * cos_t2) - sin_phi * (ry * sin_t2)
        p2y = cy + sin_phi * (rx * cos_t2) + cos_phi * (ry * sin_t2)
        d2x = cos_phi * (-rx * sin_t2) - sin_phi * (ry * cos_t2)
        d2y = sin_phi * (-rx * sin_t2) + cos_phi * (ry * cos_t2)

        cp1x = p1x + alpha * d1x
        cp1y = p1y + alpha * d1y
        cp2x = p2x - alpha * d2x
        cp2y = p2y - alpha * d2y

        pen.curveTo(tx(cp1x, cp1y), tx(cp2x, cp2y), tx(p2x, p2y))
        theta += d_theta_per


# ---------------------------------------------------------------------------
# SVG rect helper
# ---------------------------------------------------------------------------


def draw_rect_to_pen(x, y, w, h, pen, transform=None):
    """Draw a rectangle as a closed contour."""

    def tx(px, py):
        if transform is None:
            return px, py
        sx, shy, shx, sy, dx, dy = transform
        return sx * px + shx * py + dx, shy * px + sy * py + dy

    pen.moveTo(tx(x, y))
    pen.lineTo(tx(x + w, y))
    pen.lineTo(tx(x + w, y + h))
    pen.lineTo(tx(x, y + h))
    pen.closePath()


# ---------------------------------------------------------------------------
# Parse one SVG sheet into glyph data
# ---------------------------------------------------------------------------


def parse_svg(svg_filename):
    """
    Returns (glyphs_dict, scale).
    glyphs_dict: char -> {"rect": (x, y, w, h), "artwork": [("path", d), ...]}
    """
    svg_path = os.path.join(SVG_DIR, svg_filename)
    tree = ET.parse(svg_path)
    root = tree.getroot()

    scale = CAP_H / (BASELINE_Y - CAPLINE_Y)

    glyphs = {}

    for child in root:
        tag = child.tag.split("}")[-1]
        if tag != "g":
            continue

        gid = child.get("id", "")
        if gid == "guides":
            continue

        parts = [p for p in gid.split("_") if p]
        valid = True
        for part in parts:
            try:
                chr(int(part, 16))
            except (ValueError, OverflowError):
                print(
                    f"  WARNING: skipping <g id={gid!r}> -- invalid hex codepoint {part!r}"
                )
                valid = False
                break
        if not valid or not parts:
            continue

        char = "".join(chr(int(p, 16)) for p in parts)

        cell_rect = None
        for sub in child:
            if sub.tag.split("}")[-1] == "rect":
                cell_rect = sub
                break

        if cell_rect is None:
            print(f"  WARNING: <g id={gid!r}> has no <rect> child -- skipping")
            continue

        rx = float(cell_rect.get("x", "0"))
        ry = float(cell_rect.get("y", "0"))
        rw = float(cell_rect.get("width", "0"))
        rh = float(cell_rect.get("height", "0"))

        artwork = []
        for sub in child:
            if sub is cell_rect:
                continue
            subtag = sub.tag.split("}")[-1]
            if subtag == "path":
                d = sub.get("d", "")
                if d:
                    artwork.append(("path", d))
            elif subtag == "rect":
                ax = float(sub.get("x", "0"))
                ay = float(sub.get("y", "0"))
                aw = float(sub.get("width", "0"))
                ah = float(sub.get("height", "0"))
                artwork.append(("rect", (ax, ay, aw, ah)))

        if not artwork:
            print(f"  INFO: <g id={gid!r}> has no artwork (empty cell) -- skipping")
            continue

        glyphs[char] = {"rect": (rx, ry, rw, rh), "artwork": artwork}

    return glyphs, scale


# ---------------------------------------------------------------------------
# Build transform: SVG coords -> font units
# ---------------------------------------------------------------------------


def make_transform(cell_x, cell_y, scale):
    """
    Returns affine transform (sx, shy, shx, sy, dx, dy) that maps
    SVG pixel coords to font units with Y-flip.
    """
    baseline_offset = BASELINE_Y - MARGIN_TOP
    baseline_svg = cell_y + baseline_offset

    sx = scale
    shy = 0.0
    shx = 0.0
    sy = -scale
    dx = -cell_x * scale
    dy = baseline_svg * scale
    return (sx, shy, shx, sy, dx, dy)


# ---------------------------------------------------------------------------
# Build one TTF
# ---------------------------------------------------------------------------


def build_font(cfg):
    glyphs_data, scale = parse_svg(cfg["svg"])

    upm = ASC - DESC
    advance_w = round(CELL_W * ADVANCE_W_FACTOR)

    print(f"\n=== {cfg['full']} ===")
    print(
        f"  UPM={upm}, asc={ASC}, desc={DESC}, capH={CAP_H}, xH={X_H}, advance_w={advance_w}"
    )
    print(f"  svg_scale={scale:.4f}")
    print(f"  Glyphs parsed: {len(glyphs_data)}")

    all_chars = sorted(
        (c for c in glyphs_data if len(c) == 1),
        key=lambda c: ord(c),
    )
    all_ligs = sorted(
        (c for c in glyphs_data if len(c) > 1),
        key=lambda c: c,
    )

    def lig_name(chars):
        return "lig_" + "_".join(f"{ord(ch):04X}" for ch in chars)

    glyph_order = (
        [".notdef", "space"]
        + [f"uni{ord(c):04X}" for c in all_chars]
        + [lig_name(c) for c in all_ligs]
    )

    cmap = {0x0020: "space"}
    for c in all_chars:
        cmap[ord(c)] = f"uni{ord(c):04X}"

    metrics = {}

    # ---- draw all glyphs ----
    tt_glyphs = {}

    pen = TTGlyphPen(None)
    m = round(advance_w * 0.06)
    im = round(advance_w * 0.18)
    pen.moveTo((m, DESC))
    pen.lineTo((advance_w - m, DESC))
    pen.lineTo((advance_w - m, ASC))
    pen.lineTo((m, ASC))
    pen.closePath()
    pen.moveTo((im, DESC + im))
    pen.lineTo((im, ASC - im))
    pen.lineTo((advance_w - im, ASC - im))
    pen.lineTo((advance_w - im, DESC + im))
    pen.closePath()
    tt_glyphs[".notdef"] = pen.glyph()
    metrics[".notdef"] = (advance_w, m)

    tt_glyphs["space"] = TTGlyphPen(None).glyph()
    metrics["space"] = (advance_w, 0)

    from fontTools.pens.cu2quPen import Cu2QuPen

    class FakeGlyf:
        pass

    errors = []
    for c in all_chars:
        gname = f"uni{ord(c):04X}"
        glyph_entry = glyphs_data[c]
        cell_x, cell_y, cw, ch = glyph_entry["rect"]
        xform = make_transform(cell_x, cell_y, scale)
        segs = glyph_entry["artwork"]

        tt_pen = TTGlyphPen(None)
        pen = Cu2QuPen(tt_pen, max_err=1.0, reverse_direction=True)
        try:
            for kind, data in segs:
                if kind == "path":
                    draw_svg_path_to_pen(data, pen, transform=xform)
                elif kind == "rect":
                    draw_rect_to_pen(*data, pen, transform=xform)
            g = tt_pen.glyph()
            if g.numberOfContours != 0:
                g.recalcBounds(FakeGlyf())
                lsb = g.xMin
            else:
                lsb = 0
            tt_glyphs[gname] = g
            metrics[gname] = (advance_w, lsb)
        except Exception as e:
            errors.append((c, str(e)))
            tt_glyphs[gname] = TTGlyphPen(None).glyph()
            metrics[gname] = (advance_w, 0)

    if errors:
        print(f"  Errors on {len(errors)} glyphs: {[e[0] for e in errors]}")

    for chars in all_ligs:
        gname = lig_name(chars)
        glyph_entry = glyphs_data[chars]
        cell_x, cell_y, cw, ch = glyph_entry["rect"]
        xform = make_transform(cell_x, cell_y, scale)
        segs = glyph_entry["artwork"]
        lig_advance = advance_w * len(chars)

        tt_pen = TTGlyphPen(None)
        pen = Cu2QuPen(tt_pen, max_err=1.0, reverse_direction=True)
        try:
            for kind, data in segs:
                if kind == "path":
                    draw_svg_path_to_pen(data, pen, transform=xform)
                elif kind == "rect":
                    draw_rect_to_pen(*data, pen, transform=xform)
            g = tt_pen.glyph()
            if g.numberOfContours != 0:
                g.recalcBounds(FakeGlyf())
                lsb = g.xMin
            else:
                lsb = 0
            tt_glyphs[gname] = g
            metrics[gname] = (lig_advance, lsb)
        except Exception as e:
            errors.append((chars, str(e)))
            tt_glyphs[gname] = TTGlyphPen(None).glyph()
            metrics[gname] = (lig_advance, 0)

    # ---- assemble font via FontBuilder ----
    fb = FontBuilder(upm, isTTF=True)
    fb.setupGlyphOrder(glyph_order)
    fb.setupCharacterMap(cmap)

    fb.setupGlyf(tt_glyphs)
    fb.setupHorizontalMetrics(metrics)

    fb.setupHorizontalHeader(ascent=ASC, descent=DESC, lineGap=LINE_GAP)

    style = cfg["style"]
    is_ribbi = style in ("Regular", "Bold", "Italic", "Bold Italic")
    win_family = FAMILY if is_ribbi else f"{FAMILY} {style}"
    win_style = style if is_ribbi else "Regular"

    fb.setupNameTable(
        {
            "copyright": COPYRIGHT,
            "familyName": FAMILY,
            "styleName": style,
            "uniqueFontIdentifier": f"{VERSION};{VENDOR};{cfg['ps']}",
            "fullName": cfg["full"],
            "version": f"Version {VERSION}",
            "psName": cfg["ps"],
            "trademark": TRADEMARK,
            "manufacturer": VENDOR,
            "designer": DESIGNER,
            "description": DESCRIPTION,
            "vendorURL": URL,
            "designerURL": URL,
            "licenseDescription": COPYRIGHT,
            "licenseInfoURL": URL,
            "typographicFamily": FAMILY,
            "typographicSubfamily": style,
        }
    )

    name_table = fb.font["name"]
    for r in list(name_table.names):
        if r.nameID in (1, 2) and r.platformID == 3:
            name_table.removeNames(
                nameID=r.nameID, platformID=3, platEncID=r.platEncID, langID=r.langID
            )
    name_table.setName(win_family, 1, 3, 1, 0x0409)
    name_table.setName(win_style, 2, 3, 1, 0x0409)

    fs_sel = 0x20 if style == "Bold" else 0x40  # BOLD=0x20, REGULAR=0x40 (no USE_TYPO_METRICS)

    from fontTools.ttLib.tables.O_S_2f_2 import Panose

    panose = Panose()
    panose.bFamilyType = 2
    panose.bSerifStyle = 0
    panose.bWeight = max(2, min(9, cfg["weight_class"] // 100))
    panose.bProportion = 9
    panose.bContrast = 0
    panose.bStrokeVariation = 0
    panose.bArmStyle = 0
    panose.bLetterForm = 0
    panose.bMidline = 0
    panose.bXHeight = 0

    mac_style = 0x01 if style == "Bold" else 0x00

    fb.setupOS2(
        version=4,
        sTypoAscender=ASC,
        sTypoDescender=DESC,
        sTypoLineGap=LINE_GAP,
        usWinAscent=ASC,
        usWinDescent=abs(DESC),
        sxHeight=X_H,
        sCapHeight=CAP_H,
        fsType=0,
        fsSelection=fs_sel,
        achVendID=VENDOR[:4].ljust(4),
        usWeightClass=cfg["weight_class"],
        panose=panose,
    )

    fb.setupHead(unitsPerEm=upm, macStyle=mac_style)
    fb.setupPost(isFixedPitch=1)

    if all_ligs:
        from fontTools.ttLib.tables import otTables
        from fontTools.ttLib.tables.otTables import (
            GSUB as GSUBTable,
            ScriptList,
            ScriptRecord,
            Script,
            DefaultLangSys,
            FeatureList,
            FeatureRecord,
            Feature,
            LookupList,
            Lookup,
            LigatureSubst,
            LigatureSet,
            Ligature,
        )

        lig_by_first = {}
        for chars in all_ligs:
            first_glyph = f"uni{ord(chars[0]):04X}"
            components = [f"uni{ord(ch):04X}" for ch in chars[1:]]
            lig_glyph = lig_name(chars)
            lig_by_first.setdefault(first_glyph, []).append((components, lig_glyph))

        lig_subst = LigatureSubst()
        lig_subst.Format = 1
        lig_subst.ligatures = {}
        for first_glyph, entries in lig_by_first.items():
            lig_set = []
            for components, lig_glyph in entries:
                lig = Ligature()
                lig.Component = components
                lig.LigGlyph = lig_glyph
                lig_set.append(lig)
            lig_subst.ligatures[first_glyph] = lig_set

        lookup = Lookup()
        lookup.LookupType = 4
        lookup.LookupFlag = 0
        lookup.SubTable = [lig_subst]
        lookup.SubTableCount = 1

        lookup_list = LookupList()
        lookup_list.Lookup = [lookup]
        lookup_list.LookupCount = 1

        feature = Feature()
        feature.FeatureParams = None
        feature.LookupListIndex = [0]
        feature.LookupCount = 1

        feature_record = FeatureRecord()
        feature_record.FeatureTag = "calt"
        feature_record.Feature = feature

        feature_list = FeatureList()
        feature_list.FeatureRecord = [feature_record]
        feature_list.FeatureCount = 1

        default_lang = DefaultLangSys()
        default_lang.ReqFeatureIndex = 0xFFFF
        default_lang.FeatureIndex = [0]
        default_lang.FeatureCount = 1
        default_lang.LookupOrderIndex = None

        script = Script()
        script.DefaultLangSys = default_lang
        script.LangSysRecord = []
        script.LangSysCount = 0

        script_record = ScriptRecord()
        script_record.ScriptTag = "DFLT"
        script_record.Script = script

        script_list = ScriptList()
        script_list.ScriptRecord = [script_record]
        script_list.ScriptCount = 1

        gsub_raw = GSUBTable()
        gsub_raw.Version = 0x00010000
        gsub_raw.ScriptList = script_list
        gsub_raw.FeatureList = feature_list
        gsub_raw.LookupList = lookup_list

        gsub = ttLib.newTable("GSUB")
        gsub.table = gsub_raw
        fb.font["GSUB"] = gsub

    out_path = os.path.join(OUT_DIR, f"{cfg['ps']}.ttf")
    fb.font.save(out_path)
    print(f"  Saved: {out_path}")
    return out_path


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    for cfg in WEIGHTS:
        try:
            build_font(cfg)
        except Exception as e:
            import traceback

            print(f"FAILED {cfg['full']}: {e}")
            traceback.print_exc()

    print("\nDone.")
