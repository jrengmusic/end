#!/usr/bin/env python3
"""
gen_char_props.py — Unicode character property table generator for C++17 terminal emulator.
Port of Kitty's approach. Outputs Source/terminal/data/CharPropsData.h.

Usage: python3 gen/gen_char_props.py [--output path/to/CharPropsData.h]
"""

from __future__ import annotations

import argparse
import re
import sys
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterator

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

UNICODE_BASE = "https://www.unicode.org/Public/UCD/latest/ucd"
EMOJI_BASE = "https://www.unicode.org/Public/emoji/latest"
CACHE_DIR = Path("/tmp/unicode_cache")

CHAR_PROPS_SHIFT = 8
CHAR_PROPS_MASK = 0xFF
MAX_UNICODE = 0x10FFFF
WIDTH_SHIFT = 4

# GraphemeBreakProperty enum order (must match C++ enum)
GBP_NAMES: list[str] = [
    "AtStart",
    "None",
    "CR",
    "LF",
    "Control",
    "Extend",
    "ZWJ",
    "Regional_Indicator",
    "Prepend",
    "SpacingMark",
    "L",
    "V",
    "T",
    "LV",
    "LVT",
    "Private_Expecting_RI",
]
GBP_INDEX: dict[str, int] = {n: i for i, n in enumerate(GBP_NAMES)}

# InCB enum order
INCB_NAMES: list[str] = ["None", "Linker", "Consonant", "Extend"]
INCB_INDEX: dict[str, int] = {n: i for i, n in enumerate(INCB_NAMES)}

# Unicode general categories (Cn first = 0 = unassigned)
CATEGORY_NAMES: list[str] = [
    "Cn",
    "Lu",
    "Ll",
    "Lt",
    "Lm",
    "Lo",
    "Mn",
    "Mc",
    "Me",
    "Nd",
    "Nl",
    "No",
    "Pc",
    "Pd",
    "Ps",
    "Pe",
    "Pi",
    "Pf",
    "Po",
    "Sm",
    "Sc",
    "Sk",
    "So",
    "Zs",
    "Zl",
    "Zp",
    "Cc",
    "Cf",
    "Cs",
    "Co",
]
CATEGORY_INDEX: dict[str, int] = {n: i for i, n in enumerate(CATEGORY_NAMES)}

# ---------------------------------------------------------------------------
# Download / cache helpers
# ---------------------------------------------------------------------------


def _download(url: str) -> str:
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    fname = CACHE_DIR / url.split("/")[-1]
    if not fname.exists():
        print(f"  downloading {url}", file=sys.stderr)
        with urllib.request.urlopen(url) as resp:
            data = resp.read()
        fname.write_bytes(data)
    else:
        print(f"  cached      {fname.name}", file=sys.stderr)
    return fname.read_text(encoding="utf-8", errors="replace")


def _iter_ranges(
    text: str, prop_name: str | None = None
) -> Iterator[tuple[int, int, str]]:
    """Yield (lo, hi, value) from a Unicode data file.
    If prop_name is given, only yield lines whose property field matches."""
    for line in text.splitlines():
        line = line.split("#")[0].strip()
        if not line:
            continue
        parts = [p.strip() for p in line.split(";")]
        if len(parts) < 2:
            continue
        value = parts[1].strip()
        if prop_name is not None and value != prop_name:
            continue
        rng = parts[0].strip()
        if ".." in rng:
            lo_s, hi_s = rng.split("..")
            lo, hi = int(lo_s, 16), int(hi_s, 16)
        else:
            lo = hi = int(rng, 16)
        yield lo, hi, value


# ---------------------------------------------------------------------------
# Unicode data loading
# ---------------------------------------------------------------------------


def load_unicode_version() -> str:
    text = _download(f"{UNICODE_BASE}/ReadMe.txt")
    m = re.search(r"Version\s+(\d+\.\d+\.\d+)", text)
    return m.group(1) if m else "unknown"


def load_categories() -> dict[int, str]:
    """Returns cp → category string (2-letter)."""
    text = _download(f"{UNICODE_BASE}/UnicodeData.txt")
    cats: dict[int, str] = {}
    for line in text.splitlines():
        if not line.strip():
            continue
        parts = line.split(";")
        if len(parts) < 3:
            continue
        cp = int(parts[0], 16)
        cat = parts[2].strip()
        cats[cp] = cat
    # Expand ranges (lines like "4E00;<CJK Ideograph, First>;Lo;...")
    # UnicodeData.txt uses First/Last pairs
    result: dict[int, str] = {}
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        if not line.strip():
            i += 1
            continue
        parts = line.split(";")
        if len(parts) < 3:
            i += 1
            continue
        cp = int(parts[0], 16)
        name = parts[1]
        cat = parts[2].strip()
        if ", First>" in name:
            # next line is Last
            next_parts = lines[i + 1].split(";")
            cp_end = int(next_parts[0], 16)
            for c in range(cp, cp_end + 1):
                result[c] = cat
            i += 2
        else:
            result[cp] = cat
            i += 1
    return result


def load_east_asian_width() -> dict[int, str]:
    """Returns cp → 'W', 'F', 'A', 'N', 'Na', 'H'."""
    text = _download(f"{UNICODE_BASE}/EastAsianWidth.txt")
    result: dict[int, str] = {}
    for lo, hi, val in _iter_ranges(text):
        for cp in range(lo, hi + 1):
            result[cp] = val
    return result


def load_prop_list(prop: str) -> set[int]:
    text = _download(f"{UNICODE_BASE}/PropList.txt")
    result: set[int] = set()
    for lo, hi, _ in _iter_ranges(text, prop):
        for cp in range(lo, hi + 1):
            result.add(cp)
    return result


def load_grapheme_break() -> dict[int, str]:
    text = _download(f"{UNICODE_BASE}/auxiliary/GraphemeBreakProperty.txt")
    result: dict[int, str] = {}
    for lo, hi, val in _iter_ranges(text):
        for cp in range(lo, hi + 1):
            result[cp] = val
    return result


def load_incb() -> dict[int, str]:
    """Load InCB (Indic_Conjunct_Break) from DerivedCoreProperties.txt.
    Format: codepoint ; InCB; Value  (3 semicolon-separated fields)
    """
    text = _download(f"{UNICODE_BASE}/DerivedCoreProperties.txt")
    result: dict[int, str] = {}
    for line in text.splitlines():
        line = line.split("#")[0].strip()
        if not line:
            continue
        parts = [p.strip() for p in line.split(";")]
        if len(parts) < 3:
            continue
        if parts[1] != "InCB":
            continue
        incb_val = parts[2].strip()
        if incb_val not in ("Linker", "Consonant", "Extend"):
            continue
        rng = parts[0].strip()
        if ".." in rng:
            lo_s, hi_s = rng.split("..")
            lo, hi = int(lo_s, 16), int(hi_s, 16)
        else:
            lo = hi = int(rng, 16)
        for cp in range(lo, hi + 1):
            result[cp] = incb_val
    return result


def load_extended_pictographic() -> set[int]:
    text = _download(f"{UNICODE_BASE}/emoji/emoji-data.txt")
    result: set[int] = set()
    for lo, hi, val in _iter_ranges(text, "Extended_Pictographic"):
        for cp in range(lo, hi + 1):
            result.add(cp)
    return result


def load_emoji_data() -> tuple[set[int], set[int]]:
    """Returns (emoji_set, emoji_presentation_base_set).
    emoji_presentation_base = codepoints that are default emoji presentation."""
    text = _download(f"{UNICODE_BASE}/emoji/emoji-data.txt")
    emoji: set[int] = set()
    presentation_base: set[int] = set()
    for lo, hi, val in _iter_ranges(text):
        if val == "Emoji":
            for cp in range(lo, hi + 1):
                emoji.add(cp)
        if val == "Emoji_Presentation":
            for cp in range(lo, hi + 1):
                presentation_base.add(cp)
    return emoji, presentation_base


def load_emoji_variation_bases() -> set[int]:
    """Returns set of codepoints that accept VS15/VS16 variation selectors.
    Parsed from emoji-variation-sequences.txt (UTS #51)."""
    text = _download(f"{UNICODE_BASE}/emoji/emoji-variation-sequences.txt")
    bases: set[int] = set()
    for line in text.splitlines():
        line = line.split("#")[0].strip()
        if not line:
            continue
        parts = [p.strip() for p in line.split(";")]
        if len(parts) < 2:
            continue
        codepoints = parts[0].strip().split()
        if len(codepoints) >= 1:
            bases.add(int(codepoints[0], 16))
    return bases


def load_grapheme_break_test() -> list[tuple[list[list[int]], str]]:
    """Load GraphemeBreakTest.txt. Returns list of (clusters, comment)."""
    url = f"{UNICODE_BASE}/auxiliary/GraphemeBreakTest.txt"
    text = _download(url)
    tests: list[tuple[list[list[int]], str]] = []
    for line in text.splitlines():
        comment_part = ""
        if "#" in line:
            line, comment_part = line.split("#", 1)
            comment_part = comment_part.strip()
        line = line.strip()
        if not line:
            continue
        # Format: ÷ cp (× cp)* ÷  where ÷=break, ×=extend
        # Unicode chars ÷ = U+00F7, × = U+00D7
        tokens = re.split(r"[\u00F7\u00D7]", line)
        tokens = [t.strip() for t in tokens if t.strip()]
        clusters: list[list[int]] = []
        current: list[int] = []
        raw = line
        # Re-parse with separators
        parts = re.split(r"(\u00F7|\u00D7)", raw)
        current_cluster: list[int] = []
        for part in parts:
            part = part.strip()
            if part == "\u00f7":
                if current_cluster:
                    clusters.append(current_cluster)
                    current_cluster = []
            elif part == "\u00d7":
                pass  # continue same cluster
            elif part:
                try:
                    cp = int(part, 16)
                    current_cluster.append(cp)
                except ValueError:
                    pass
        if current_cluster:
            clusters.append(current_cluster)
        if clusters:
            tests.append((clusters, comment_part))
    return tests


# ---------------------------------------------------------------------------
# CharProps computation
# ---------------------------------------------------------------------------


@dataclass
class CharProps:
    is_emoji_presentation: int = 0  # 1 bit — Emoji_Presentation=yes (default emoji)
    shifted_width: int = 0  # 3 bits
    is_emoji: int = 0  # 1 bit
    category: int = 0  # 5 bits
    is_emoji_presentation_base: int = 0  # 1 bit
    is_invalid: int = 0  # 1 bit
    is_non_rendered: int = 0  # 1 bit
    is_symbol: int = 0  # 1 bit
    is_combining_char: int = 0  # 1 bit
    is_word_char: int = 0  # 1 bit
    is_punctuation: int = 0  # 1 bit
    grapheme_break: int = 0  # 4 bits
    indic_conjunct_break: int = 0  # 2 bits
    is_extended_pictographic: int = 0  # 1 bit

    def pack(self) -> int:
        v = 0
        # bits 1..8: padding (8 bits) — leave as 0
        v |= (self.is_emoji_presentation & 0x1) << 0
        v |= (self.shifted_width & 0x7) << 9
        v |= (self.is_emoji & 0x1) << 12
        v |= (self.category & 0x1F) << 13
        v |= (self.is_emoji_presentation_base & 0x1) << 18
        v |= (self.is_invalid & 0x1) << 19
        v |= (self.is_non_rendered & 0x1) << 20
        v |= (self.is_symbol & 0x1) << 21
        v |= (self.is_combining_char & 0x1) << 22
        v |= (self.is_word_char & 0x1) << 23
        v |= (self.is_punctuation & 0x1) << 24
        v |= (self.grapheme_break & 0xF) << 25
        v |= (self.indic_conjunct_break & 0x3) << 29
        v |= (self.is_extended_pictographic & 0x1) << 31
        return v

    def grapheme_segmentation_property(self) -> int:
        """Top 7 bits: grapheme_break:4 + indic_conjunct_break:2 + is_extended_pictographic:1"""
        return (
            (self.grapheme_break & 0xF)
            | ((self.indic_conjunct_break & 0x3) << 4)
            | ((self.is_extended_pictographic & 0x1) << 6)
        )


def compute_char_props_table(
    categories: dict[int, str],
    east_asian: dict[int, str],
    other_default_ignorable: set[int],
    grapheme_break: dict[int, str],
    incb: dict[int, str],
    extended_pictographic: set[int],
    emoji: set[int],
    emoji_presentation: set[int],
    emoji_variation_base: set[int],
) -> list[int]:
    """Compute packed CharProps for all codepoints 0..MAX_UNICODE."""

    print("  computing widths...", file=sys.stderr)

    # --- Width table (priority order, first assigned wins) ---
    widths: dict[int, int] = {}

    # 9. Default: everything gets 1 first (lowest priority)
    for cp in range(MAX_UNICODE + 1):
        widths[cp] = 1

    # 8. Not assigned (Cn) → -4
    for cp in range(MAX_UNICODE + 1):
        cat = categories.get(cp, "Cn")
        if cat == "Cn":
            widths[cp] = -4

    # 7. Private use (Co) → -3
    for cp in range(MAX_UNICODE + 1):
        cat = categories.get(cp, "Cn")
        if cat == "Co":
            widths[cp] = -3

    # 6. Ambiguous (EastAsianWidth A) → -2
    for cp in range(MAX_UNICODE + 1):
        if east_asian.get(cp) == "A":
            widths[cp] = -2

    # 5. Non-printing (Cc, Cs, non-characters, Cf) → -1
    non_chars = set(range(0xFDD0, 0xFDEF + 1))
    for plane in range(17):
        non_chars.add(plane * 0x10000 + 0xFFFE)
        non_chars.add(plane * 0x10000 + 0xFFFF)
    for cp in range(MAX_UNICODE + 1):
        cat = categories.get(cp, "Cn")
        if cat in ("Cc", "Cs", "Cf") or cp in non_chars:
            widths[cp] = -1

    # 4. Combining marks + NUL + Cf → 0
    for cp in range(MAX_UNICODE + 1):
        cat = categories.get(cp, "Cn")
        if cat in ("Mn", "Mc", "Me", "Cf") or cp == 0:
            widths[cp] = 0

    # 3. Wide emoji (default emoji presentation) → 2
    for cp in emoji_presentation:
        if cp <= MAX_UNICODE:
            widths[cp] = 2

    # 2. EastAsianWidth W/F → 2
    for cp in range(MAX_UNICODE + 1):
        ea = east_asian.get(cp)
        if ea in ("W", "F"):
            widths[cp] = 2

    # 1. Regional indicators → 2 (highest priority)
    for cp in range(0x1F1E6, 0x1F1FF + 1):
        widths[cp] = 2

    print("  computing char props...", file=sys.stderr)

    result: list[int] = []
    for cp in range(MAX_UNICODE + 1):
        cat = categories.get(cp, "Cn")
        cat_idx = CATEGORY_INDEX.get(cat, 0)

        w = widths[cp]
        shifted_w = w + WIDTH_SHIFT  # store w+4 so -4..2 → 0..6

        gbp_str = grapheme_break.get(cp, "None")
        # Map to our enum; unknown → None
        gbp_idx = GBP_INDEX.get(gbp_str, GBP_INDEX["None"])

        incb_str = incb.get(cp, "None")
        incb_idx = INCB_INDEX.get(incb_str, INCB_INDEX["None"])

        is_ep = 1 if cp in extended_pictographic else 0
        is_em = 1 if cp in emoji else 0
        is_epb = 1 if cp in emoji_variation_base else 0
        is_empr = 1 if cp in emoji_presentation else 0

        # is_invalid: surrogates (Cs) and non-characters
        is_invalid = 1 if (cat == "Cs" or cp in non_chars) else 0

        # is_non_rendered: Cc, Cf, Mn, Me, Mc, ZWJ, combining
        is_non_rendered = 1 if (cat in ("Cc", "Cf") or w == 0) else 0

        # is_symbol: So, Sm, Sc, Sk
        is_symbol = 1 if cat in ("So", "Sm", "Sc", "Sk") else 0

        # is_combining_char: Mn, Mc, Me
        is_combining = 1 if cat in ("Mn", "Mc", "Me") else 0

        # is_word_char: letters, digits, connector punctuation
        is_word = (
            1 if cat in ("Lu", "Ll", "Lt", "Lm", "Lo", "Nd", "Nl", "No", "Pc") else 0
        )

        # is_punctuation: all P categories
        is_punct = 1 if cat.startswith("P") else 0

        p = CharProps(
            is_emoji_presentation=is_empr,
            shifted_width=shifted_w,
            is_emoji=is_em,
            category=cat_idx,
            is_emoji_presentation_base=is_epb,
            is_invalid=is_invalid,
            is_non_rendered=is_non_rendered,
            is_symbol=is_symbol,
            is_combining_char=is_combining,
            is_word_char=is_word,
            is_punctuation=is_punct,
            grapheme_break=gbp_idx,
            indic_conjunct_break=incb_idx,
            is_extended_pictographic=is_ep,
        )
        result.append(p.pack())

    return result


# ---------------------------------------------------------------------------
# Splitbins — 3-level multistage table compression
# ---------------------------------------------------------------------------


def splitbins(values: list[int], shift: int) -> tuple[list[int], list[int], list[int]]:
    """Build a 3-level multistage table.

    t1[cp >> shift] → block_index
    t2[(block_index << shift) + (cp & mask)] → t3_index
    t3[t3_index] → value

    Returns (t1, t2, t3).
    """
    mask = (1 << shift) - 1
    block_size = 1 << shift

    # Split values into blocks of block_size
    n = len(values)
    # Pad to multiple of block_size
    padded = list(values)
    while len(padded) % block_size != 0:
        padded.append(0)

    num_blocks = len(padded) // block_size

    # Deduplicate blocks → t2 (flat) + t1 (block index per input block)
    block_map: dict[tuple[int, ...], int] = {}
    t2_flat: list[int] = []
    t1: list[int] = []

    for b in range(num_blocks):
        block = tuple(padded[b * block_size : (b + 1) * block_size])
        if block not in block_map:
            idx = len(t2_flat) >> shift  # index of this block in t2
            block_map[block] = idx
            t2_flat.extend(block)
        t1.append(block_map[block])

    # t2_flat contains indices into t3 (after deduplication of values)
    # Deduplicate values → t3
    val_map: dict[int, int] = {}
    t3: list[int] = []
    t2: list[int] = []

    for v in t2_flat:
        if v not in val_map:
            val_map[v] = len(t3)
            t3.append(v)
        t2.append(val_map[v])

    return t1, t2, t3


def splitbins_char_props(packed: list[int]) -> tuple[list[int], list[int], list[int]]:
    """3-level table for CharProps. shift=8."""
    return splitbins(packed, CHAR_PROPS_SHIFT)


# ---------------------------------------------------------------------------
# GraphemeSegmentation state machine
# ---------------------------------------------------------------------------


@dataclass
class GraphemeSegState:
    grapheme_break: int = 0  # 4 bits (GBP of prev char)
    incb_consonant_extended: int = 0  # 1 bit
    incb_consonant_extended_linker: int = 0  # 1 bit
    incb_consonant_extended_linker_extended: int = 0  # 1 bit
    emoji_modifier_sequence: int = 0  # 1 bit
    emoji_modifier_sequence_before_last_char: int = 0  # 1 bit

    def encode(self) -> int:
        """Encode to 10-bit integer."""
        v = self.grapheme_break & 0xF
        v |= (self.incb_consonant_extended & 1) << 4
        v |= (self.incb_consonant_extended_linker & 1) << 5
        v |= (self.incb_consonant_extended_linker_extended & 1) << 6
        v |= (self.emoji_modifier_sequence & 1) << 7
        v |= (self.emoji_modifier_sequence_before_last_char & 1) << 8
        return v

    @staticmethod
    def decode(v: int) -> "GraphemeSegState":
        return GraphemeSegState(
            grapheme_break=v & 0xF,
            incb_consonant_extended=(v >> 4) & 1,
            incb_consonant_extended_linker=(v >> 5) & 1,
            incb_consonant_extended_linker_extended=(v >> 6) & 1,
            emoji_modifier_sequence=(v >> 7) & 1,
            emoji_modifier_sequence_before_last_char=(v >> 8) & 1,
        )


@dataclass
class GraphemeSegProps:
    """7-bit input: grapheme_break:4 + indic_conjunct_break:2 + is_extended_pictographic:1"""

    grapheme_break: int = 0
    indic_conjunct_break: int = 0
    is_extended_pictographic: int = 0

    def encode(self) -> int:
        v = self.grapheme_break & 0xF
        v |= (self.indic_conjunct_break & 0x3) << 4
        v |= (self.is_extended_pictographic & 0x1) << 6
        return v

    @staticmethod
    def decode(v: int) -> "GraphemeSegProps":
        return GraphemeSegProps(
            grapheme_break=v & 0xF,
            indic_conjunct_break=(v >> 4) & 0x3,
            is_extended_pictographic=(v >> 6) & 0x1,
        )


def grapheme_seg_result_encode(new_state: int, add_to_current_cell: int) -> int:
    """Encode GraphemeSegmentationResult as 16-bit."""
    return (new_state & 0x3FF) | ((add_to_current_cell & 1) << 10)


def compute_grapheme_seg_transition(state_enc: int, props_enc: int) -> int:
    """Compute the grapheme segmentation transition for (state, props).
    Returns encoded GraphemeSegmentationResult (16-bit).
    Implements UAX #29 rules GB3-GB13, GB999 as Kitty does.
    """
    state = GraphemeSegState.decode(state_enc)
    p = GraphemeSegProps.decode(props_enc)

    prop = GBP_NAMES[p.grapheme_break]
    incb = INCB_NAMES[p.indic_conjunct_break]
    prev_prop = GBP_NAMES[state.grapheme_break]

    add_to_cell = False

    # AtStart: first character of a new grapheme cluster — always starts a new cell
    if prev_prop == "AtStart":
        # If RI, set state to Private_Expecting_RI
        new_gbp = (
            GBP_INDEX["Private_Expecting_RI"]
            if prop == "Regional_Indicator"
            else p.grapheme_break
        )
        # Compute new state fields
        new_incb_cel = (incb == "Consonant") or (
            state.incb_consonant_extended and incb in ("Linker", "Extend")
        )
        new_incb_cell_linker = state.incb_consonant_extended and incb == "Linker"
        new_incb_cell_linker_ext = new_incb_cell_linker or (
            state.incb_consonant_extended_linker_extended
            and incb in ("Linker", "Extend")
        )
        new_ems = (state.emoji_modifier_sequence and prop == "Extend") or bool(
            p.is_extended_pictographic
        )
        new_ems_prev = state.emoji_modifier_sequence

        new_state = GraphemeSegState(
            grapheme_break=new_gbp,
            incb_consonant_extended=int(new_incb_cel),
            incb_consonant_extended_linker=int(new_incb_cell_linker),
            incb_consonant_extended_linker_extended=int(new_incb_cell_linker_ext),
            emoji_modifier_sequence=int(new_ems),
            emoji_modifier_sequence_before_last_char=int(new_ems_prev),
        )
        return grapheme_seg_result_encode(new_state.encode(), 0)

    # Compute add_to_cell based on UAX #29 rules
    add_to_cell = False

    # GB3: CR × LF
    if prev_prop == "CR" and prop == "LF":
        add_to_cell = True

    # GB4: (Control | CR | LF) ÷
    elif prev_prop in ("Control", "CR", "LF"):
        add_to_cell = False

    # GB5: ÷ (Control | CR | LF)
    elif prop in ("Control", "CR", "LF"):
        add_to_cell = False

    # GB6: L × (L | V | LV | LVT)
    elif prev_prop == "L" and prop in ("L", "V", "LV", "LVT"):
        add_to_cell = True

    # GB7: (LV | V) × (V | T)
    elif prev_prop in ("LV", "V") and prop in ("V", "T"):
        add_to_cell = True

    # GB8: (LVT | T) × T
    elif prev_prop in ("LVT", "T") and prop == "T":
        add_to_cell = True

    # GB9: × (Extend | ZWJ)
    elif prop in ("Extend", "ZWJ"):
        add_to_cell = True

    # GB9a: × SpacingMark
    elif prop == "SpacingMark":
        add_to_cell = True

    # GB9b: Prepend ×
    elif prev_prop == "Prepend":
        add_to_cell = True

    # GB9c: InCB=Consonant [InCB=Extend InCB=Linker]* × InCB=Consonant
    elif incb == "Consonant" and state.incb_consonant_extended_linker_extended:
        add_to_cell = True

    # GB11: \p{Extended_Pictographic} Extend* ZWJ × \p{Extended_Pictographic}
    elif (
        prev_prop == "ZWJ"
        and p.is_extended_pictographic
        and state.emoji_modifier_sequence_before_last_char
    ):
        add_to_cell = True

    # GB12/GB13: Regional_Indicator pairs
    # Private_Expecting_RI means we already have one RI, so second RI completes the pair
    elif prev_prop == "Private_Expecting_RI" and prop == "Regional_Indicator":
        add_to_cell = True

    # GB999: break everywhere else
    else:
        add_to_cell = False

    # Compute new state
    new_gbp: int
    if prop == "Regional_Indicator":
        if prev_prop == "Private_Expecting_RI":
            # Pair complete, reset to None (next RI starts new pair)
            new_gbp = GBP_INDEX["None"]
        else:
            new_gbp = GBP_INDEX["Private_Expecting_RI"]
    else:
        new_gbp = p.grapheme_break

    new_incb_cel_linker = bool(state.incb_consonant_extended and incb == "Linker")
    new_incb_cel_linker_ext = new_incb_cel_linker or bool(
        state.incb_consonant_extended_linker_extended and incb in ("Linker", "Extend")
    )
    new_incb_cel = (incb == "Consonant") or bool(
        state.incb_consonant_extended and incb in ("Linker", "Extend")
    )

    new_ems_prev = bool(state.emoji_modifier_sequence)
    new_ems = bool(state.emoji_modifier_sequence and prop == "Extend") or bool(
        p.is_extended_pictographic
    )

    new_state = GraphemeSegState(
        grapheme_break=new_gbp,
        incb_consonant_extended=int(new_incb_cel),
        incb_consonant_extended_linker=int(new_incb_cel_linker),
        incb_consonant_extended_linker_extended=int(new_incb_cel_linker_ext),
        emoji_modifier_sequence=int(new_ems),
        emoji_modifier_sequence_before_last_char=int(new_ems_prev),
    )
    return grapheme_seg_result_encode(new_state.encode(), int(add_to_cell))


def compute_grapheme_seg_table() -> list[int]:
    """Precompute all 2^17 entries (10-bit state × 7-bit props = 2^17).
    Key = (state_enc << 7) | props_enc.
    Returns list of 2^17 uint16_t values.
    """
    print("  computing grapheme segmentation table (2^17 entries)...", file=sys.stderr)
    size = (1 << 10) * (1 << 7)  # 131072
    table: list[int] = [0] * size
    for state_enc in range(1 << 10):
        for props_enc in range(1 << 7):
            key = (state_enc << 7) | props_enc
            table[key] = compute_grapheme_seg_transition(state_enc, props_enc)
    return table


# ---------------------------------------------------------------------------
# Splitbins for grapheme seg table
# ---------------------------------------------------------------------------


def splitbins_grapheme_seg(
    table: list[int],
) -> tuple[int, list[int], list[int], list[int]]:
    """Find optimal shift for grapheme seg table and return (shift, t1, t2, t3)."""
    best_shift = 7
    best_size = None
    best_tables = None

    for shift in range(4, 12):
        t1, t2, t3 = splitbins(table, shift)
        total = len(t1) + len(t2) + len(t3)
        if best_size is None or total < best_size:
            best_size = total
            best_shift = shift
            best_tables = (t1, t2, t3)

    assert best_tables is not None
    return best_shift, best_tables[0], best_tables[1], best_tables[2]


# ---------------------------------------------------------------------------
# C++ code generation
# ---------------------------------------------------------------------------


def _format_array(name: str, ctype: str, values: list[int], per_line: int = 20) -> str:
    lines: list[str] = []
    lines.append(f"static constexpr {ctype} {name}[{len(values)}]")
    lines.append("{")
    for i in range(0, len(values), per_line):
        chunk = values[i : i + per_line]
        row = ", ".join(str(v) for v in chunk)
        comma = "," if (i + per_line) < len(values) else ""
        lines.append(f"    {row}{comma}")
    lines.append("};")
    return "\n".join(lines)


def _enum_decl(name: str, underlying: str, members: list[str]) -> str:
    lines = [f"enum class {name} : {underlying}"]
    lines.append("{")
    for i, m in enumerate(members):
        comma = "," if i < len(members) - 1 else ""
        lines.append(f"    {m}{comma}")
    lines.append("};")
    return "\n".join(lines)


def generate_header(
    unicode_version: str,
    cp_t1: list[int],
    cp_t2: list[int],
    cp_t3: list[int],
    gs_shift: int,
    gs_t1: list[int],
    gs_t2: list[int],
    gs_t3: list[int],
) -> str:
    lines: list[str] = []

    lines.append("#pragma once")
    lines.append(f"// Unicode data, built from the Unicode Standard {unicode_version}")
    lines.append("// Code generated by gen_char_props.py, DO NOT EDIT.")
    lines.append("")
    lines.append("#include <cstdint>")
    lines.append("")

    # Enums
    lines.append(_enum_decl("GraphemeBreakProperty", "uint8_t", GBP_NAMES))
    lines.append("")
    lines.append(_enum_decl("IndicConjunctBreak", "uint8_t", INCB_NAMES))
    lines.append("")
    lines.append(_enum_decl("UnicodeCategory", "uint8_t", CATEGORY_NAMES))
    lines.append("")

    # CharProps constants
    lines.append(
        f"static constexpr uint32_t CHAR_PROPS_SHIFT {{ {CHAR_PROPS_SHIFT} }};"
    )
    lines.append(f"static constexpr uint32_t CHAR_PROPS_MASK  {{ {CHAR_PROPS_MASK} }};")
    lines.append(f"static constexpr uint32_t MAX_UNICODE       {{ 0x10FFFF }};")
    lines.append(f"static constexpr int      WIDTH_SHIFT       {{ {WIDTH_SHIFT} }};")
    lines.append("")

    # CharProps table
    lines.append("// CharProps 3-level multistage table")
    lines.append(
        "// Lookup: charPropsT3[charPropsT2[(charPropsT1[cp >> CHAR_PROPS_SHIFT] << CHAR_PROPS_SHIFT) + (cp & CHAR_PROPS_MASK)]]"
    )
    lines.append(_format_array("charPropsT1", "uint8_t", cp_t1))
    lines.append("")
    lines.append(_format_array("charPropsT2", "uint16_t", cp_t2))
    lines.append("")
    lines.append(_format_array("charPropsT3", "uint32_t", cp_t3))
    lines.append("")

    # GraphemeSegmentation constants
    gs_mask = (1 << gs_shift) - 1
    lines.append(f"static constexpr uint8_t  GRAPHEME_SEG_SHIFT {{ {gs_shift} }};")
    lines.append(f"static constexpr uint16_t GRAPHEME_SEG_MASK  {{ {gs_mask} }};")
    lines.append("")

    # GraphemeSegmentation table
    lines.append("// GraphemeSegmentation 3-level multistage table")
    lines.append("// Key = (state_enc << 7) | props_enc  (17-bit)")
    lines.append(
        "// Lookup: graphemeSegT3[graphemeSegT2[(graphemeSegT1[key >> GRAPHEME_SEG_SHIFT] << GRAPHEME_SEG_SHIFT) + (key & GRAPHEME_SEG_MASK)]]"
    )
    lines.append(_format_array("graphemeSegT1", "uint8_t", gs_t1))
    lines.append("")
    lines.append(_format_array("graphemeSegT2", "uint16_t", gs_t2))
    lines.append("")
    lines.append(_format_array("graphemeSegT3", "uint16_t", gs_t3))
    lines.append("")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------


def lookup_char_props_packed(
    cp: int, t1: list[int], t2: list[int], t3: list[int]
) -> int:
    block = t1[cp >> CHAR_PROPS_SHIFT]
    t2_idx = (block << CHAR_PROPS_SHIFT) + (cp & CHAR_PROPS_MASK)
    t3_idx = t2[t2_idx]
    return t3[t3_idx]


def unpack_width(packed: int) -> int:
    shifted = (packed >> 9) & 0x7
    return shifted - WIDTH_SHIFT


def unpack_grapheme_seg_props(packed: int) -> int:
    gbp = (packed >> 25) & 0xF
    incb = (packed >> 29) & 0x3
    ep = (packed >> 31) & 0x1
    return gbp | (incb << 4) | (ep << 6)


def lookup_grapheme_seg(
    key: int, shift: int, t1: list[int], t2: list[int], t3: list[int]
) -> int:
    mask = (1 << shift) - 1
    block = t1[key >> shift]
    t2_idx = (block << shift) + (key & mask)
    t3_idx = t2[t2_idx]
    return t3[t3_idx]


def validate_widths(t1: list[int], t2: list[int], t3: list[int]) -> bool:
    ok = True
    cases = [
        (0x0020, 1, "space"),
        (0x0000, 0, "NUL"),
        (0x4E00, 2, "CJK U+4E00"),
        (0x1F600, 2, "emoji U+1F600"),
        (0x0300, 0, "combining U+0300"),
        (0x1F1E6, 2, "regional indicator U+1F1E6"),
    ]
    for cp, expected, label in cases:
        packed = lookup_char_props_packed(cp, t1, t2, t3)
        got = unpack_width(packed)
        status = "OK" if got == expected else "FAIL"
        if got != expected:
            ok = False
        print(
            f"  width U+{cp:04X} ({label}): expected={expected} got={got} [{status}]",
            file=sys.stderr,
        )
    return ok


def validate_grapheme_break_test(
    tests: list[tuple[list[list[int]], str]],
    cp_t1: list[int],
    cp_t2: list[int],
    cp_t3: list[int],
    gs_shift: int,
    gs_t1: list[int],
    gs_t2: list[int],
    gs_t3: list[int],
) -> tuple[int, int]:
    """Run GraphemeBreakTest.txt tests. Returns (passed, total)."""
    passed = 0
    total = 0

    at_start_state = GraphemeSegState(grapheme_break=GBP_INDEX["AtStart"]).encode()

    for clusters, comment in tests:
        total += 1
        # Flatten all codepoints in order
        all_cps = [cp for cluster in clusters for cp in cluster]
        if not all_cps:
            passed += 1
            continue

        # Simulate segmentation
        state = at_start_state
        got_clusters: list[list[int]] = []
        current: list[int] = []

        for cp in all_cps:
            packed = lookup_char_props_packed(cp, cp_t1, cp_t2, cp_t3)
            props_enc = unpack_grapheme_seg_props(packed)
            key = (state << 7) | props_enc
            result = lookup_grapheme_seg(key, gs_shift, gs_t1, gs_t2, gs_t3)
            add_to_cell = (result >> 10) & 1
            new_state = result & 0x3FF

            if add_to_cell:
                current.append(cp)
            else:
                if current:
                    got_clusters.append(current)
                current = [cp]
            state = new_state

        if current:
            got_clusters.append(current)

        if got_clusters == clusters:
            passed += 1
        else:
            # Only print first few failures
            if (total - passed) <= 5:
                print(f"  FAIL: {comment}", file=sys.stderr)
                print(f"    expected: {clusters}", file=sys.stderr)
                print(f"    got:      {got_clusters}", file=sys.stderr)

    return passed, total


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate CharPropsData.h")
    parser.add_argument(
        "--output",
        default="Source/terminal/data/CharPropsData.h",
        help="Output file path (relative to repo root or absolute)",
    )
    args = parser.parse_args()

    output_path = Path(args.output)
    if not output_path.is_absolute():
        # Resolve relative to script's parent (repo root)
        script_dir = Path(__file__).resolve().parent
        repo_root = script_dir.parent
        output_path = repo_root / args.output

    print("=== gen_char_props.py ===", file=sys.stderr)
    print(f"Output: {output_path}", file=sys.stderr)

    # --- Load Unicode data ---
    print("\n[1/6] Loading Unicode data...", file=sys.stderr)
    unicode_version = load_unicode_version()
    print(f"  Unicode version: {unicode_version}", file=sys.stderr)

    categories = load_categories()
    east_asian = load_east_asian_width()
    other_default_ignorable = load_prop_list("Other_Default_Ignorable_Code_Point")
    grapheme_break = load_grapheme_break()
    incb = load_incb()
    extended_pictographic = load_extended_pictographic()
    emoji, emoji_presentation_base = load_emoji_data()
    emoji_variation_bases = load_emoji_variation_bases()

    # --- Compute CharProps ---
    print("\n[2/6] Computing CharProps table...", file=sys.stderr)
    packed_props = compute_char_props_table(
        categories,
        east_asian,
        other_default_ignorable,
        grapheme_break,
        incb,
        extended_pictographic,
        emoji,
        emoji_presentation_base,
        emoji_variation_bases,
    )

    # --- Compress CharProps ---
    print("\n[3/6] Compressing CharProps (splitbins shift=8)...", file=sys.stderr)
    cp_t1, cp_t2, cp_t3 = splitbins_char_props(packed_props)
    print(f"  charPropsT1: {len(cp_t1)} entries (uint8_t)", file=sys.stderr)
    print(f"  charPropsT2: {len(cp_t2)} entries (uint16_t)", file=sys.stderr)
    print(f"  charPropsT3: {len(cp_t3)} entries (uint32_t)", file=sys.stderr)
    cp_total_bytes = len(cp_t1) + len(cp_t2) * 2 + len(cp_t3) * 4
    print(
        f"  total: {cp_total_bytes} bytes (vs {(MAX_UNICODE + 1) * 4} flat)",
        file=sys.stderr,
    )

    # Validate t1 fits in uint8_t
    assert max(cp_t1) <= 255, f"charPropsT1 overflow: max={max(cp_t1)}"
    assert max(cp_t2) <= 65535, f"charPropsT2 overflow: max={max(cp_t2)}"

    # --- Compute GraphemeSegmentation ---
    print("\n[4/6] Computing GraphemeSegmentation table...", file=sys.stderr)
    gs_table = compute_grapheme_seg_table()

    # --- Compress GraphemeSegmentation ---
    print("\n[5/6] Compressing GraphemeSegmentation (splitbins)...", file=sys.stderr)
    gs_shift, gs_t1, gs_t2, gs_t3 = splitbins_grapheme_seg(gs_table)
    print(f"  shift: {gs_shift}", file=sys.stderr)
    print(f"  graphemeSegT1: {len(gs_t1)} entries (uint8_t)", file=sys.stderr)
    print(f"  graphemeSegT2: {len(gs_t2)} entries (uint16_t)", file=sys.stderr)
    print(f"  graphemeSegT3: {len(gs_t3)} entries (uint16_t)", file=sys.stderr)
    gs_total_bytes = len(gs_t1) + len(gs_t2) * 2 + len(gs_t3) * 2
    print(
        f"  total: {gs_total_bytes} bytes (vs {len(gs_table) * 2} flat)",
        file=sys.stderr,
    )

    assert max(gs_t1) <= 255, f"graphemeSegT1 overflow: max={max(gs_t1)}"
    assert max(gs_t2) <= 65535, f"graphemeSegT2 overflow: max={max(gs_t2)}"

    # --- Validate ---
    print("\n[6/6] Validating...", file=sys.stderr)
    print("  Width checks:", file=sys.stderr)
    widths_ok = validate_widths(cp_t1, cp_t2, cp_t3)

    print("  GraphemeBreakTest:", file=sys.stderr)
    try:
        tests = load_grapheme_break_test()
        passed, total = validate_grapheme_break_test(
            tests, cp_t1, cp_t2, cp_t3, gs_shift, gs_t1, gs_t2, gs_t3
        )
        print(f"  {passed}/{total} tests passed", file=sys.stderr)
        if passed < total:
            print(
                f"  WARNING: {total - passed} grapheme break tests failed",
                file=sys.stderr,
            )
    except Exception as e:
        print(f"  WARNING: could not run grapheme break tests: {e}", file=sys.stderr)

    # --- Generate header ---
    print("\nGenerating header...", file=sys.stderr)
    header = generate_header(
        unicode_version,
        cp_t1,
        cp_t2,
        cp_t3,
        gs_shift,
        gs_t1,
        gs_t2,
        gs_t3,
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(header, encoding="utf-8")
    print(f"Written: {output_path}", file=sys.stderr)
    print(f"  {len(header)} bytes", file=sys.stderr)


if __name__ == "__main__":
    main()
