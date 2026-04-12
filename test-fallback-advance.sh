#!/usr/bin/env bash
#
# Visual test for fallback glyph advance.
# Mono ASCII alternates with fallback codepoints (arrows / symbols).
# Each row should align column-perfect with the row above.
# If a fallback glyph bleeds past its cell, columns shift right of it.

cat <<'EOF'
mono-fallback alignment test
============================

ruler:  0123456789012345678901234567890123456789
mono:   |a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|
arrows: |→|↔|←|↑|↓|⇒|⇔|⇐|⇑|⇓|➜|➔|➤|➢|➣|⟶|⟵|⟷|⟹|
mixed:  |a|→|c|↔|e|←|g|↑|i|↓|k|⇒|m|⇔|o|⇐|q|⇑|s|
boxes:  |█|▓|▒|░|■|□|▪|▫|◆|◇|◈|●|○|◉|◎|★|☆|✓|✗|
dingb:  |✦|✧|✪|✩|✬|✭|✮|✯|✰|❀|❁|❂|❃|❄|❅|❆|❇|❈|❉|

cluster stress (no separators):
→→→→→→→→→→
↔↔↔↔↔↔↔↔↔↔
mono next: hello
arrow next: →world
EOF
