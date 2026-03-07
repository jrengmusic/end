#!/usr/bin/env bash
# emoji_test.sh — Comprehensive Unicode/emoji width test cases
# Each section prints chars with | separators and an expected-width marker row.
# XX = width 2, X = width 1, 0 = width 0 (combining)

SEP="|"

# ─────────────────────────────────────────────────────────────────────────────
# 1. BASIC EMOJI (single codepoint, width 2)
# ─────────────────────────────────────────────────────────────────────────────
printf '\n=== 1. Basic Emoji (single codepoint, width 2) ===\n'

printf 'Smileys:\n'
printf '%s' "$SEP"
printf '\U0001F600%s\U0001F603%s\U0001F604%s\U0001F601%s\U0001F606%s\U0001F605%s\U0001F923%s\U0001F602%s\U0001F642%s\U0001F643%s' \
    "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP"
printf '\n'
printf '|XX|XX|XX|XX|XX|XX|XX|XX|XX|XX|\n'

printf 'Hands:\n'
printf '%s' "$SEP"
printf '\U0001F44B%s\U0001F91A%s\U0001F590%s\u270B%s\U0001F596%s\U0001F44C%s\U0001F90C%s\U0001F90F%s\u270C\uFE0F%s\U0001F91E%s' \
    "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP"
printf '\n'
printf '|XX|XX|XX|XX|XX|XX|XX|XX|XX|XX|\n'

printf 'Hearts:\n'
printf '%s' "$SEP"
printf '\u2764\uFE0F%s\U0001F9E1%s\U0001F49B%s\U0001F49A%s\U0001F499%s\U0001F49C%s\U0001F5A4%s\U0001F90D%s\U0001F90E%s\U0001F494%s' \
    "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP"
printf '\n'
printf '|XX|XX|XX|XX|XX|XX|XX|XX|XX|XX|\n'

printf 'Animals:\n'
printf '%s' "$SEP"
printf '\U0001F436%s\U0001F431%s\U0001F42D%s\U0001F439%s\U0001F430%s\U0001F98A%s\U0001F43B%s\U0001F43C%s\U0001F428%s\U0001F42F%s' \
    "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP"
printf '\n'
printf '|XX|XX|XX|XX|XX|XX|XX|XX|XX|XX|\n'

printf 'Food:\n'
printf '%s' "$SEP"
printf '\U0001F34E%s\U0001F350%s\U0001F34A%s\U0001F34B%s\U0001F34C%s\U0001F349%s\U0001F347%s\U0001F353%s\U0001FAD0%s\U0001F348%s' \
    "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP" "$SEP"
printf '\n'
printf '|XX|XX|XX|XX|XX|XX|XX|XX|XX|XX|\n'

# ─────────────────────────────────────────────────────────────────────────────
# 2. VS16 — text codepoint + U+FE0F → emoji presentation (width 2)
# ─────────────────────────────────────────────────────────────────────────────
printf '\n=== 2. VS16 (U+FE0F forces emoji presentation, width 2) ===\n'
printf 'Format: without VS16 | with VS16  (right column should be wider)\n'
printf '\n'

printf 'U+263A  SMILING FACE:\n'
printf '  text: |%s|   emoji: |%s|\n' \
    "$(printf '\u263A')" "$(printf '\u263A\uFE0F')"
printf '         |X|            |XX|\n'

printf 'U+2665  BLACK HEART SUIT:\n'
printf '  text: |%s|   emoji: |%s|\n' \
    "$(printf '\u2665')" "$(printf '\u2665\uFE0F')"
printf '         |X|            |XX|\n'

printf 'U+2600  BLACK SUN:\n'
printf '  text: |%s|   emoji: |%s|\n' \
    "$(printf '\u2600')" "$(printf '\u2600\uFE0F')"
printf '         |X|            |XX|\n'

printf 'U+2B50  WHITE MEDIUM STAR:\n'
printf '  bare: |%s|   +VS16: |%s|\n' \
    "$(printf '\u2B50')" "$(printf '\u2B50\uFE0F')"
printf '         |XX|           |XX|\n'

printf 'U+260E  BLACK TELEPHONE:\n'
printf '  text: |%s|   emoji: |%s|\n' \
    "$(printf '\u260E')" "$(printf '\u260E\uFE0F')"
printf '         |X|            |XX|\n'

printf 'U+270F  PENCIL:\n'
printf '  text: |%s|   emoji: |%s|\n' \
    "$(printf '\u270F')" "$(printf '\u270F\uFE0F')"
printf '         |X|            |XX|\n'

printf 'U+2702  BLACK SCISSORS:\n'
printf '  text: |%s|   emoji: |%s|\n' \
    "$(printf '\u2702')" "$(printf '\u2702\uFE0F')"
printf '         |X|            |XX|\n'

printf 'U+26A1  HIGH VOLTAGE:\n'
printf '  bare: |%s|   +VS16: |%s|\n' \
    "$(printf '\u26A1')" "$(printf '\u26A1\uFE0F')"
printf '         |XX|           |XX|\n'

# ─────────────────────────────────────────────────────────────────────────────
# 3. VS15 — U+FE0E forces text presentation (width 1)
# ─────────────────────────────────────────────────────────────────────────────
printf '\n=== 3. VS15 (U+FE0E forces text presentation, width 1) ===\n'
printf 'Format: emoji (no VS) | +VS15 text form\n'
printf '\n'

printf 'U+263A:\n'
printf '  emoji: |%s|   text: |%s|\n' \
    "$(printf '\u263A\uFE0F')" "$(printf '\u263A\uFE0E')"
printf '          |XX|          |X|\n'

printf 'U+2665:\n'
printf '  emoji: |%s|   text: |%s|\n' \
    "$(printf '\u2665\uFE0F')" "$(printf '\u2665\uFE0E')"
printf '          |XX|          |X|\n'

printf 'U+2600:\n'
printf '  emoji: |%s|   text: |%s|\n' \
    "$(printf '\u2600\uFE0F')" "$(printf '\u2600\uFE0E')"
printf '          |XX|          |X|\n'

printf 'U+260E:\n'
printf '  emoji: |%s|   text: |%s|\n' \
    "$(printf '\u260E\uFE0F')" "$(printf '\u260E\uFE0E')"
printf '          |XX|          |X|\n'

printf 'U+270F:\n'
printf '  emoji: |%s|   text: |%s|\n' \
    "$(printf '\u270F\uFE0F')" "$(printf '\u270F\uFE0E')"
printf '          |XX|          |X|\n'

printf 'U+2702:\n'
printf '  emoji: |%s|   text: |%s|\n' \
    "$(printf '\u2702\uFE0F')" "$(printf '\u2702\uFE0E')"
printf '          |XX|          |X|\n'

# ─────────────────────────────────────────────────────────────────────────────
# 4. ZWJ SEQUENCES (U+200D joins multiple codepoints → single glyph, width 2)
# ─────────────────────────────────────────────────────────────────────────────
printf '\n=== 4. ZWJ Sequences (U+200D joined, width 2 each) ===\n'
printf '%s' "$SEP"

# 👨‍👩‍👧‍👦  U+1F468 ZWJ U+1F469 ZWJ U+1F467 ZWJ U+1F466
printf '\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466%s' "$SEP"

# 👩‍💻  U+1F469 ZWJ U+1F4BB
printf '\U0001F469\u200D\U0001F4BB%s' "$SEP"

# 🏳️‍🌈  U+1F3F3 VS16 ZWJ U+1F308
printf '\U0001F3F3\uFE0F\u200D\U0001F308%s' "$SEP"

# 👨‍🍳  U+1F468 ZWJ U+1F373
printf '\U0001F468\u200D\U0001F373%s' "$SEP"

# 🧑‍🚀  U+1F9D1 ZWJ U+1F680
printf '\U0001F9D1\u200D\U0001F680%s' "$SEP"

# 👩‍❤️‍👨  U+1F469 ZWJ U+2764 VS16 ZWJ U+1F468
printf '\U0001F469\u200D\u2764\uFE0F\u200D\U0001F468%s' "$SEP"

# 🐻‍❄️  U+1F43B ZWJ U+2744 VS16
printf '\U0001F43B\u200D\u2744\uFE0F%s' "$SEP"

printf '\n'
printf '|XX|XX|XX|XX|XX|XX|XX|\n'
printf '(family)(woman-tech)(rainbow-flag)(man-cook)(astronaut)(couple-heart)(polar-bear)\n'

# ─────────────────────────────────────────────────────────────────────────────
# 5. FLAG SEQUENCES (regional indicator pairs, width 2 each)
# ─────────────────────────────────────────────────────────────────────────────
printf '\n=== 5. Flag Sequences (regional indicator pairs, width 2) ===\n'
printf '%s' "$SEP"

# Regional indicators: A=U+1F1E6 ... Z=U+1F1FF
# US: U+1F1FA U+1F1F8
printf '\U0001F1FA\U0001F1F8%s' "$SEP"
# GB: U+1F1EC U+1F1E7
printf '\U0001F1EC\U0001F1E7%s' "$SEP"
# JP: U+1F1EF U+1F1F5
printf '\U0001F1EF\U0001F1F5%s' "$SEP"
# KR: U+1F1F0 U+1F1F7
printf '\U0001F1F0\U0001F1F7%s' "$SEP"
# DE: U+1F1E9 U+1F1EA
printf '\U0001F1E9\U0001F1EA%s' "$SEP"
# FR: U+1F1EB U+1F1F7
printf '\U0001F1EB\U0001F1F7%s' "$SEP"
# IT: U+1F1EE U+1F1F9
printf '\U0001F1EE\U0001F1F9%s' "$SEP"
# ES: U+1F1EA U+1F1F8
printf '\U0001F1EA\U0001F1F8%s' "$SEP"
# BR: U+1F1E7 U+1F1F7
printf '\U0001F1E7\U0001F1F7%s' "$SEP"
# ID: U+1F1EE U+1F1E9
printf '\U0001F1EE\U0001F1E9%s' "$SEP"

printf '\n'
printf '|XX|XX|XX|XX|XX|XX|XX|XX|XX|XX|\n'
printf '(US)(GB)(JP)(KR)(DE)(FR)(IT)(ES)(BR)(ID)\n'

# ─────────────────────────────────────────────────────────────────────────────
# 6. SKIN TONE MODIFIERS (base + U+1F3FB-1F3FF, width 2)
# ─────────────────────────────────────────────────────────────────────────────
printf '\n=== 6. Skin Tone Modifiers (base + U+1F3FB-1F3FF, width 2) ===\n'

printf 'Waving hand:\n'
printf '%s' "$SEP"
printf '\U0001F44B\U0001F3FB%s' "$SEP"   # light
printf '\U0001F44B\U0001F3FC%s' "$SEP"   # medium-light
printf '\U0001F44B\U0001F3FD%s' "$SEP"   # medium
printf '\U0001F44B\U0001F3FE%s' "$SEP"   # medium-dark
printf '\U0001F44B\U0001F3FF%s' "$SEP"   # dark
printf '\n'
printf '|XX|XX|XX|XX|XX|\n'

printf 'Woman:\n'
printf '%s' "$SEP"
printf '\U0001F469\U0001F3FB%s' "$SEP"
printf '\U0001F469\U0001F3FC%s' "$SEP"
printf '\U0001F469\U0001F3FD%s' "$SEP"
printf '\U0001F469\U0001F3FE%s' "$SEP"
printf '\U0001F469\U0001F3FF%s' "$SEP"
printf '\n'
printf '|XX|XX|XX|XX|XX|\n'

# ─────────────────────────────────────────────────────────────────────────────
# 7. KEYCAP SEQUENCES (digit/# /* + U+FE0F + U+20E3, width 2)
# ─────────────────────────────────────────────────────────────────────────────
printf '\n=== 7. Keycap Sequences (digit + U+FE0F + U+20E3, width 2) ===\n'
printf '%s' "$SEP"
printf '0\uFE0F\u20E3%s' "$SEP"
printf '1\uFE0F\u20E3%s' "$SEP"
printf '2\uFE0F\u20E3%s' "$SEP"
printf '3\uFE0F\u20E3%s' "$SEP"
printf '4\uFE0F\u20E3%s' "$SEP"
printf '5\uFE0F\u20E3%s' "$SEP"
printf '6\uFE0F\u20E3%s' "$SEP"
printf '7\uFE0F\u20E3%s' "$SEP"
printf '8\uFE0F\u20E3%s' "$SEP"
printf '9\uFE0F\u20E3%s' "$SEP"
printf '#\uFE0F\u20E3%s' "$SEP"
printf '*\uFE0F\u20E3%s' "$SEP"
printf '\n'
printf '|XX|XX|XX|XX|XX|XX|XX|XX|XX|XX|XX|XX|\n'

# ─────────────────────────────────────────────────────────────────────────────
# 8. CJK WIDE CHARACTERS (width 2, not emoji)
# ─────────────────────────────────────────────────────────────────────────────
printf '\n=== 8. CJK Wide Characters (width 2, not emoji) ===\n'

printf 'Chinese (ni hao shi jie):\n'
printf '%s' "$SEP"
printf '\u4F60%s\u597D%s\u4E16%s\u754C%s' "$SEP" "$SEP" "$SEP" "$SEP"
printf '\n'
printf '|XX|XX|XX|XX|\n'

printf 'Japanese Hiragana (konnichiwa):\n'
printf '%s' "$SEP"
printf '\u3053%s\u3093%s\u306B%s\u3061%s\u306F%s' "$SEP" "$SEP" "$SEP" "$SEP" "$SEP"
printf '\n'
printf '|XX|XX|XX|XX|XX|\n'

printf 'Katakana:\n'
printf '%s' "$SEP"
printf '\u30AB%s\u30BF%s\u30AB%s\u30CA%s' "$SEP" "$SEP" "$SEP" "$SEP"
printf '\n'
printf '|XX|XX|XX|XX|\n'

printf 'Korean (hangugeo):\n'
printf '%s' "$SEP"
printf '\uD55C%s\uAD6D%s\uC5B4%s' "$SEP" "$SEP" "$SEP"
printf '\n'
printf '|XX|XX|XX|\n'

printf 'Fullwidth Latin:\n'
printf '%s' "$SEP"
printf '\uFF21%s\uFF22%s\uFF23%s' "$SEP" "$SEP" "$SEP"
printf '\n'
printf '|XX|XX|XX|\n'

# ─────────────────────────────────────────────────────────────────────────────
# 9. BOX DRAWING / LINE CHARACTERS (width 1, EAW Ambiguous)
# ─────────────────────────────────────────────────────────────────────────────
printf '\n=== 9. Box Drawing / Line Characters (width 1, EAW Ambiguous) ===\n'
printf '\u250C\u2500\u252C\u2500\u2510\n'
printf '\u2502 \u2502 \u2502\n'
printf '\u251C\u2500\u253C\u2500\u2524\n'
printf '\u2502 \u2502 \u2502\n'
printf '\u2514\u2500\u2534\u2500\u2518\n'
printf 'Expected: each box char = width 1\n'

# ─────────────────────────────────────────────────────────────────────────────
# 10. COMBINING MARKS (width 0 — grapheme sidecar)
# ─────────────────────────────────────────────────────────────────────────────
printf '\n=== 10. Combining Marks (combining = width 0) ===\n'
printf 'Format: precomposed | decomposed (should look identical)\n'
printf '\n'

# é: precomposed U+00E9 vs e + U+0301
printf 'e + acute (U+0301):\n'
printf '  precomposed: |%s|   decomposed: |%s|\n' \
    "$(printf '\u00E9')" "$(printf 'e\u0301')"
printf '               |X|               |X|\n'

# ñ: precomposed U+00F1 vs n + U+0303
printf 'n + tilde (U+0303):\n'
printf '  precomposed: |%s|   decomposed: |%s|\n' \
    "$(printf '\u00F1')" "$(printf 'n\u0303')"
printf '               |X|               |X|\n'

# ö: precomposed U+00F6 vs o + U+0308
printf 'o + diaeresis (U+0308):\n'
printf '  precomposed: |%s|   decomposed: |%s|\n' \
    "$(printf '\u00F6')" "$(printf 'o\u0308')"
printf '               |X|               |X|\n'

# å: precomposed U+00E5 vs a + U+030A
printf 'a + ring above (U+030A):\n'
printf '  precomposed: |%s|   decomposed: |%s|\n' \
    "$(printf '\u00E5')" "$(printf 'a\u030A')"
printf '               |X|               |X|\n'

# ü: precomposed U+00FC vs u + U+0308
printf 'u + diaeresis (U+0308):\n'
printf '  precomposed: |%s|   decomposed: |%s|\n' \
    "$(printf '\u00FC')" "$(printf 'u\u0308')"
printf '               |X|               |X|\n'

# ─────────────────────────────────────────────────────────────────────────────
# 11. NERD FONT ICONS (PUA, width 1)
# ─────────────────────────────────────────────────────────────────────────────
printf '\n=== 11. Nerd Font Icons (PUA U+E000-U+F8FF, width 1) ===\n'
printf '(Requires a Nerd Font — will show replacement glyphs otherwise)\n'
printf '\n'
printf '%s' "$SEP"
printf '\uE0A0%s' "$SEP"   # git branch
printf '\uE0B0%s' "$SEP"   # powerline arrow right
printf '\uE0B2%s' "$SEP"   # powerline arrow left
printf '\uF013%s' "$SEP"   # gear
printf '\uF015%s' "$SEP"   # home
printf '\uF07C%s' "$SEP"   # folder open
printf '\uF120%s' "$SEP"   # terminal
printf '\n'
printf '|X|X|X|X|X|X|X|\n'
printf '(branch)(arrow-r)(arrow-l)(gear)(home)(folder)(terminal)\n'

printf '\n=== END OF EMOJI TEST ===\n'
