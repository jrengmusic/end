"""
fix_nf_names.py — Rewrite name-table metadata for Display proportional TTFs.

Only the `name` table is touched. Outlines and all other tables are untouched.
Idempotent: safe to re-run.
"""

from fontTools.ttLib import TTFont

FONTS_DIR = "C:/Users/jreng/Documents/Poems/dev/end/___display___/fonts/Display"

CONFIGS = [
    {
        "path": f"{FONTS_DIR}/Display Book.ttf",
        "style": "Book",
        "is_ribbi": False,
    },
    {
        "path": f"{FONTS_DIR}/Display Medium.ttf",
        "style": "Medium",
        "is_ribbi": False,
    },
    {
        "path": f"{FONTS_DIR}/Display Bold.ttf",
        "style": "Bold",
        "is_ribbi": True,
    },
]

PLATFORM_ID = 3
PLAT_ENC_ID = 1
LANG_ID = 0x0409

FAMILY = "Display"

TARGET_IDS = (1, 2, 4, 6, 16, 17)


def build_name_map(style: str, is_ribbi: bool) -> dict:
    win_family = FAMILY if is_ribbi else f"{FAMILY} {style}"
    win_style = style if is_ribbi else "Regular"
    full_name = f"{FAMILY} {style}" if not is_ribbi else (
        FAMILY if style == "Regular" else f"{FAMILY} {style}"
    )
    ps_name = f"{FAMILY}-{style}"

    return {
        1: win_family,
        2: win_style,
        4: full_name,
        6: ps_name,
        16: FAMILY,
        17: style,
    }


def fix_font(cfg: dict) -> None:
    path = cfg["path"]
    style = cfg["style"]
    is_ribbi = cfg["is_ribbi"]

    font = TTFont(path)
    name_table = font["name"]

    name_map = build_name_map(style, is_ribbi)

    for name_id in TARGET_IDS:
        for r in list(name_table.names):
            if r.nameID == name_id and r.platformID in (1, 3):
                name_table.removeNames(
                    nameID=r.nameID,
                    platformID=r.platformID,
                    platEncID=r.platEncID,
                    langID=r.langID,
                )

    for name_id, value in name_map.items():
        name_table.setName(value, name_id, PLATFORM_ID, PLAT_ENC_ID, LANG_ID)

    font.save(path)
    print(f"Saved: {path}")


def verify_font(cfg: dict) -> None:
    path = cfg["path"]
    style = cfg["style"]
    is_ribbi = cfg["is_ribbi"]

    font = TTFont(path)
    name_table = font["name"]
    name_map = build_name_map(style, is_ribbi)

    print(f"\n--- {path} ---")
    all_ok = True
    for name_id, expected in name_map.items():
        record = name_table.getName(name_id, PLATFORM_ID, PLAT_ENC_ID, LANG_ID)
        actual = record.toUnicode() if record is not None else None
        status = "OK" if actual == expected else "FAIL"
        if status == "FAIL":
            all_ok = False
        print(f"  ID {name_id:2d}: {status}  expected={expected!r}  actual={actual!r}")

    if all_ok:
        print("  PASS")
    else:
        raise RuntimeError(f"Verification failed for {path}")


if __name__ == "__main__":
    for cfg in CONFIGS:
        fix_font(cfg)

    for cfg in CONFIGS:
        verify_font(cfg)
