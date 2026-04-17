# SPRINT-LOG.md

**Project:** ___display___  
**Repository:** /Users/jreng/Documents/Poems/dev/end/___display___  
**Started:** 2026-03-01

**Purpose:** Long-term context memory across sessions. Tracks completed work, technical debt, and unresolved issues. Written by PRIMARY agents only when ARCHITECT explicitly requests.

---

## Handoff: Rebuild `fonts/Display/*.ttf` with correct name-table metadata

**From:** COUNSELOR (END project, Sprint 24 — Ctrl+Q confirmation dialog)
**Date:** 2026-04-17

### Problem

END resolves the Action::List action-name font and the new `Terminal::Dialog` via `juce::FontOptions().withName("Display Bold")`. Lookup fails — JUCE substitutes to a fallback face. Root cause is broken metadata in the three proportional TTFs at `___display___/fonts/Display/`.

Inspection of the TTF `name` tables (via fontTools):

| File                  | ID 1 Family | ID 2 Subfamily | ID 4 Full Name   | ID 6 PostScript    | ID 16 | ID 17 |
|-----------------------|-------------|----------------|------------------|--------------------|-------|-------|
| `Display Book.ttf`    | `Display`   | **`BOOK`**     | `Display BOOK`   | `Display-BOOK`     | N/A   | N/A   |
| `Display Medium.ttf`  | `Display`   | **`MEDIUM`**   | `Display MEDIUM` | `Display-MEDIUM`   | N/A   | N/A   |
| `Display Bold.ttf`    | `Display`   | **`BOLD`**     | `Display BOLD`   | `Display-BOLD`     | N/A   | N/A   |

Contrast with correctly-built `Display Mono Bold.ttf` (produced by `build_fonts.py`): ID 2 = `Bold`, ID 4 = `Display Mono Bold`, ID 16 = `Display Mono`, ID 17 = `Bold`. DirectWrite/JUCE match by Full Name or Family+Subfamily; the uppercase subfamily + missing preferred-family entries break the non-mono Display lookup path.

### Recommended Solution

**Metadata-only rewrite.** The glyph outlines are correct — only the `name` table needs repair. No SVG rebuild required (these three TTFs are external imports, not built from the local GlyphSheet pipeline at `build_fonts.py`).

Write a one-shot Python script (`___display___/fix_display_names.py`) that, for each of the three TTFs, opens via `fontTools.ttLib.TTFont`, rewrites the Windows-platform (platformID=3, platEncID=1, langID=0x0409) entries of the `name` table to:

| Name ID | Value                                                     |
|--------:|-----------------------------------------------------------|
| 1       | `Display` (RIBBI) — or `Display {Style}` for non-RIBBI    |
| 2       | `Bold` for RIBBI Bold; `Regular` otherwise                |
| 4       | `Display {Style}` (`Display Book`, `Display Medium`, `Display Bold`) |
| 6       | `Display-{Style}` (`Display-Book`, etc.)                  |
| 16      | `Display` (typographic family — always)                   |
| 17      | `{Style}` — `Book`, `Medium`, `Bold`                      |

`Bold` is RIBBI; `Book` and `Medium` are non-RIBBI. RIBBI logic matches `build_fonts.py:644-646` exactly — copy the pattern:

```python
style = "Book" | "Medium" | "Bold"
is_ribbi = style in ("Regular", "Bold", "Italic", "Bold Italic")
win_family = FAMILY if is_ribbi else f"{FAMILY} {style}"
win_style  = style  if is_ribbi else "Regular"
```

Also remove any stale `name` entries at IDs 1, 2 (matching `build_fonts.py:670-677`) before re-inserting, so DirectWrite doesn't see mixed old+new rows.

Save over the existing files in-place (overwriting `fonts/Display/Display {Style}.ttf`). Re-run `Projucer` (or equivalent JUCE binary-resource refresh) so `BinaryData::DisplayBook_ttf` / `DisplayMedium_ttf` / `DisplayBold_ttf` pick up the new bytes — currently baked into the END build via `Source/Main.cpp:569-574`.

### Acceptance Criteria

- [ ] `python -c "from fontTools.ttLib import TTFont; f = TTFont(r'___display___/fonts/Display/Display Bold.ttf'); [print(r.nameID, repr(str(r))) for r in f['name'].names]"` shows:
  - ID 1 = `Display`
  - ID 2 = `Bold`
  - ID 4 = `Display Bold`
  - ID 16 = `Display`
  - ID 17 = `Bold`
- [ ] Same shape for `Display Book.ttf` (ID 1 = `Display Book`, ID 2 = `Regular`, ID 16 = `Display`, ID 17 = `Book`) and `Display Medium.ttf` (ID 1 = `Display Medium`, ID 2 = `Regular`, ID 16 = `Display`, ID 17 = `Medium`) — non-RIBBI.
- [ ] END is rebuilt (BinaryData refreshed, JUCE rebuild).
- [ ] Launch END, open Action::List — action name labels render in the Display Bold face (not fallback).
- [ ] Launch END, press Ctrl+Q — `Terminal::Dialog` message label and Yes/No buttons render in Display Bold.
- [ ] Visual comparison: glyph shapes match the pre-rebuild outlines exactly (metadata-only change; outlines untouched).

### Files to Modify

- `___display___/fonts/Display/Display Book.ttf` — overwrite (metadata only)
- `___display___/fonts/Display/Display Medium.ttf` — overwrite (metadata only)
- `___display___/fonts/Display/Display Bold.ttf` — overwrite (metadata only)
- NEW `___display___/fix_display_names.py` — one-shot rewrite script (idempotent, safe to re-run)

**Do NOT modify** `build_fonts.py` (correct as-is) nor any END C++ source — once the TTF metadata is clean, `actionListNameFamily = "Display Bold"` in `Source/config/Config.cpp:255` resolves correctly with no code change.

### Notes

- The uppercase subfamily (`BOLD`, `MEDIUM`, `BOOK`) is almost certainly an artifact of the source font's original generator. `build_fonts.py` avoids this by writing names explicitly via `setName`/`setupNameTable`.
- `fix_nf_names.py` already exists at `___display___/` but is empty (0 bytes). Fill it with the Display-prop rewrite logic, OR use a new filename (`fix_display_names.py`) — either is acceptable; match whichever the Display-project prefers.
- The END Sprint 24 work (Ctrl+Q dialog) unblocks itself with `Display Bold` once TTFs are rebuilt. Until then, the dialog renders in JUCE's fallback. No blocker for merging END Sprint 24 — the dialog logic is correct; only the glyph surface is temporarily off.

---

## 📖 Notation Reference

**[N]** = Sprint Number (e.g., `1`, `2`, `3`...)

**Sprint:** A discrete unit of work completed by one or more agents, ending with ARCHITECT approval ("done", "good", "commit")

---

## ⚠️ CRITICAL RULES

**AGENTS BUILD CODE FOR ARCHITECT TO TEST**
- Agents build/modify code ONLY when ARCHITECT explicitly requests
- ARCHITECT tests and provides feedback
- Agents wait for ARCHITECT approval before proceeding

**AGENTS NEVER RUN GIT COMMANDS**
- Write code changes without running git commands
- Agent runs git ONLY when user explicitly requests
- Never autonomous git operations
- **When committing:** Always stage ALL changes with `git add -A` before commit
  - ❌ DON'T selectively stage files (agents forget/miss files)
  - ✅ DO `git add -A` to capture every modified file

**SPRINT-LOG WRITTEN BY PRIMARY AGENTS ONLY**
- **COUNSELOR** or **SURGEON** write to SPRINT-LOG
- Only when user explicitly says: `"log sprint"`
- No intermediate summary files
- No automatic logging after every task
- Latest sprint at top, keep last 5 entries

**NAMING RULE (CODE VOCABULARY)**
- All identifiers must obey project-specific naming conventions (see NAMING-CONVENTION.md)
- Variable names: semantic + precise (not `temp`, `data`, `x`)
- Function names: verb-noun pattern (initRepository, detectCanonBranch)
- Struct fields: domain-specific terminology (not generic `value`, `item`, `entry`)
- Type names: PascalCase, clear intent (CanonBranchConfig, not BranchData)

**BEFORE CODING: ALWAYS SEARCH EXISTING PATTERNS**
- ❌ NEVER invent new states, enums, or utility functions without checking if they exist
- ✅ Always grep/search the codebase first for existing patterns
- ✅ Check types, constants, and error handling patterns before creating new ones
- **Methodology:** Read → Understand → Find SSOT → Use existing pattern

**TRUST THE LIBRARY, DON'T REINVENT**
- ❌ NEVER create custom helpers for things the library/framework already does
- ✅ Trust the library/framework - it's battle-tested

**FAIL-FAST RULE (CRITICAL)**
- ❌ NEVER silently ignore errors (no error suppression)
- ❌ NEVER use fallback values that mask failures
- ❌ NEVER return empty strings/zero values when operations fail
- ❌ NEVER use early returns
- ✅ ALWAYS check error returns explicitly
- ✅ ALWAYS return errors to caller or log + fail fast

**⚠️ NEVER REMOVE THESE RULES**
- Rules at top of SPRINT-LOG.md are immutable
- If rules need update: ADD new rules, don't erase old ones

---

## Quick Reference

### For Agents

**When user says:** `"log sprint"`

1. **Check:** Did I (PRIMARY agent) complete work this session?
2. **If YES:** Write sprint block to SPRINT-LOG.md (latest first)
3. **Include:** Files modified, changes made, alignment check, technical debt

### For User

**Activate PRIMARY:**
```
"@CAROL.md COUNSELOR: Rock 'n Roll"
"@CAROL.md SURGEON: Rock 'n Roll"
```

**Log completed work:**
```
"log sprint"
```

**Invoke subagent:**
```
"@oracle analyze this"
"@engineer scaffold that"
"@auditor verify this"
```

**Available Agents:**
- **PRIMARY:** COUNSELOR (domain specific strategic analysis), SURGEON (surgical precision problem solving)
- **Subagents:** Pathfinder, Oracle, Engineer, Auditor, Machinist, Librarian

---

<!-- SPRINT HISTORY STARTS BELOW -->
<!-- Latest sprint at top, oldest at bottom -->
<!-- Keep last 5 sprints, rotate older to git history -->

## SPRINT HISTORY

## Sprint 4: Donor/NF Scaling Fix, Metric Bounds, Icon Size Analysis

**Date:** 2026-03-05

### Agents Participated
- **COUNSELOR:** Claude Opus 4 — Analysis, planning, metric investigation
- **Engineer** (invoked by COUNSELOR) — Code edits, font builds, metric/glyph dumps
- **Oracle** (invoked by COUNSELOR) — NF scaling failure mode analysis

### Files Modified (2 total)

**Display Mono project:**
- `build_monolithic.py` — (1) Donor scaling fixed: now uses `min(scale_by_w, scale_by_h)` with `max_h = (ASC+LINE_GAP) + (abs(DESC)+LINE_GAP) = 1503`, centers both axes. Previously scaled to fill 100% width only, producing yMax=4521 on tall narrow glyphs. (2) NF scaling restored with same bounded logic — scale to fill available space without exceeding metric bounds, center both axes. (3) `translate_glyph` now supports dy parameter. (4) `fsSelection` reset in `patch_nf_names` to match base font (0xA0 for Bold, 0xC0 for others). (5) Restored `--mono` flag for NF patcher. (6) Restored NerdFontMono filenames in NF_NAME_SPECS.

**GGWP project:**
- No changes this sprint.

### Root Cause Analysis
- Sprint 3 changed donor scaling from "scale down if too wide" to "scale all to fill 100% width". This caused tall narrow donor glyphs (Noto Symbols2) to scale up massively (e.g. U+2758 Light Vertical Bar: yMax=4521, 4x ascent). This poisoned all downstream steps including NF patching.
- NF patcher was not the cause of tall line gap — donor merge was.
- NF patcher with `--mono` was incorrectly removed and restored.
- Multiple metric formula changes (option 3 OpenType, hhea-only adjustments) were attempted without understanding the original working formula, compounding the problem.

### Key Discovery: NF Icon Sizing
- NF `--mono` sizes icons to fill cell WIDTH (615). Icons are already 615 wide.
- Cell is 615w x 1503h. Square icons fill width but only ~36% of height.
- Icons CANNOT be made larger in the font without overflowing advance_w (breaks monospace).
- Kitty renders built-in NF icons larger by detecting whitespace after icon and rendering across 2 cells. This is a RENDERER behavior, not achievable in the font.
- **Conclusion: NF icon sizing for visual fullness must be handled by END's renderer, not the font pipeline.**

### Vertical Metrics (SSOT — confirmed working)
```
hhea.ascent      = ASC + LINE_GAP  = 1131
hhea.descent     = DESC - LINE_GAP = -372
sTypoAscender    = ASC + LINE_GAP  = 1131
sTypoDescender   = DESC - LINE_GAP = -372
sTypoLineGap     = 0
usWinAscent      = ASC + LINE_GAP  = 1131
usWinDescent     = abs(DESC) + LINE_GAP = 372
fsSelection      = 0xA0 (Bold) / 0xC0 (others)
Total line height = 1503 (all three metric sets agree)
```

### Alignment Check
- [x] LIFESTAR: SSOT (metrics from sheet_config.py), Explicit (no magic numbers)
- [x] No early returns
- [x] Fail-fast error handling

### Technical Debt / Follow-up
- END renderer needs NF icon rendering strategy (2-cell detection like kitty, or dedicated icon rendering path)
- Donor scaling centers at `(ASC+DESC)/2 = 379` — may not be visual center for all glyph types. Monitor for misalignment.
- `scale_glyph`, `translate_glyph`, `decompose_glyph`, `recalc_bounds`, `is_empty_glyph` helpers remain in build_monolithic.py even though some are only used by donor merge. Consider cleanup if NF scaling is removed later.
- 5 dead files still on filesystem (emptied but not deleted)

**Status:** Line gap fixed. Donor scaling bounded. NF scaling bounded. Base font and Monolith both render correctly. NF icon size is a renderer concern, not font.

---

## Sprint 3: Cleanup, Donor Scaling, NF Pipeline, Metric Restoration

**Date:** 2026-03-05

### Agents Participated
- **COUNSELOR:** Claude Opus 4 — Planning, metric analysis, delegating to engineer/oracle
- **Engineer** (invoked by COUNSELOR) — Code edits, font builds, metric dumps
- **Oracle** (invoked by COUNSELOR) — Analyzed NF scaling failure modes, glyph bounds analysis

### Files Modified (8 total)

**Display Mono project:**
- `build_fonts.py` — Stripped underscore prefixes from all functions/variables (BASE_DIR, tokenize_path, chunks, arc_to_bezier, lig_name, FakeGlyf, cw/ch). Added blank cell skip (empty cells no longer produce glyphs). Vertical metrics restored to original aligned formula.
- `build_monolithic.py` — Stripped underscore prefixes. Donor glyph scaling changed to fill 100% advance_w (was 90% with downscale-only guard). Added translate_glyph dy support. Added fsSelection reset in patch_nf_names. Restored --mono flag for NF patcher. Restored vertical metrics to original aligned formula in patch_nf_names. Removed scale_nf_glyphs function and all related code (pre_patch_orders, scaling call) — NF patcher handles its own sizing. Removed dead code (throwaway pen draw in scale_glyph).
- `append_ligatures.py` — Emptied (dead code)
- `generate_ligature_cells.py` — Emptied (dead code)
- `fix_nf_names.py` — Emptied (superseded by build_monolithic.py)
- `migrate_to_grouped.py` — Emptied (one-time migration, done)
- `patch_nf.sh` — Emptied (superseded by build_monolithic.py)

**GGWP project:**
- `build_fonts.py` — Stripped underscore prefixes. Added blank cell skip. Vertical metrics restored to original aligned formula.
- `GLYPHSHEET.md` — Updated parser algorithm and validation sections to reflect blank cell skip.

### Alignment Check
- [x] LIFESTAR: SSOT (all constants from sheet_config.py), Explicit (no magic numbers)
- [ ] NAMING-CONVENTION: Underscore prefixes removed per ARCHITECT directive (rejects Python private convention)
- [x] No early returns
- [x] Fail-fast error handling

### Problems Solved
- Dead files cleaned up (5 emptied)
- Underscore prefix convention removed across all scripts
- Blank cells (e.g. `==`) no longer produce empty glyphs in font
- Donor glyph scaling now fills full cell width instead of 90% downscale-only

### Problems Created (by COUNSELOR — documented for accountability)
- Vertical metrics were broken across multiple iterations. COUNSELOR kept guessing at "correct" OpenType metric formulas instead of understanding the original working formula. Each fix compounded the problem. Wasted ~1 hour of ARCHITECT's time across multiple rebuild/test cycles.
- NF glyph scaling was attempted (scale_nf_glyphs) but produced glyphs with yMax=4521 (4x ascent) because scaling was applied without proper vertical repositioning. The entire approach was wrong — NF patcher with --mono already handles sizing. Function removed.
- COUNSELOR repeatedly ran full pipeline (build_fonts + build_monolithic = 6+ min) instead of building base font only for incremental testing.
- COUNSELOR made autonomous decisions about what to build/run instead of waiting for ARCHITECT direction.

### Technical Debt / Follow-up
- `<=>` ligature cell has artwork in all 3 weights (session notes incorrectly said empty — verified present)
- `==` ligature intentionally empty (no artwork drawn)
- NF icons are still at NF patcher's default sizing (--mono --careful). If ARCHITECT wants larger icons, need a different approach than post-processing scaling.
- Donor glyph scaling (Noto) fills 100% width — needs visual testing
- 5 dead files emptied but not deleted from filesystem (no rm capability)
- translate_glyph now supports dy parameter but only used by removed scale_nf_glyphs — kept for future use

### Vertical Metrics (SSOT — original working formula)
```
hhea.ascent      = ASC + LINE_GAP  = 1131
hhea.descent     = DESC - LINE_GAP = -372
sTypoAscender    = ASC + LINE_GAP  = 1131
sTypoDescender   = DESC - LINE_GAP = -372
sTypoLineGap     = 0
usWinAscent      = ASC + LINE_GAP  = 1131
usWinDescent     = abs(DESC) + LINE_GAP = 372
Total line height = 1503 (all three metric sets agree)
```

**Status:** Metrics restored. Base font verified correct. Monolith needs rebuild and visual test.

---

## Sprint 1: Project Setup and Initial Planning ✅

**Date:** 2026-01-11  
**Duration:** 14:00 - 16:30 (2.5 hours)

### Agents Participated
- **COUNSELOR:** Kimi-K2 — Wrote SPEC.md and ARCHITECTURE.md
- **ENGINEER** (invoked by COUNSELOR) — Created project structure
- **AUDITOR** (invoked by COUNSELOR) — Verified spec compliance

### Files Modified (8 total)
- `SPEC.md:1-200` — Complete feature specification with all flows
- `ARCHITECTURE.md:1-150` — Initial architecture patterns documented
- `src/core/module.cpp:10-45` — Core module scaffolding with proper initialization
- `src/core/module.h:1-30` — Core module header with explicit dependencies
- `tests/core_test.cpp:1-50` — Test scaffolding following Testable principle
- `CMakeLists.txt:1-25` — Build configuration with explicit targets
- `README.md:1-20` — Project overview

### Alignment Check
- [x] LIFESTAR principles followed (Lean, Immutable, Findable, Explicit, SSOT, Testable, Accessible, Reviewable)
- [x] NAMING-CONVENTION.md adhered (semantic names, verb-noun functions, no type encoding)
- [x] ARCHITECTURAL-MANIFESTO.md principles applied (no layer violations, explicit dependencies)
- [x] No early returns used
- [x] Fail-fast error handling implemented

### Problems Solved
- Established project foundation following domain-specific patterns
- Defined clear module boundaries preventing layer violations

### Technical Debt / Follow-up
- Error handling needs refinement in module.cpp (marked with TODO)
- Performance requirements not yet defined for real-time constraints

**Status:** ✅ APPROVED - All files compile, tests scaffold in place

---

## Sprint 2: Unified Grid, Ligature Cells, Config SSOT ✅

**Date:** 2026-03-03  
**Duration:** ~2 hours

### Agents Participated
- **COUNSELOR:** Claude Opus 4 — Requirements, planning, script updates
- **Pathfinder** (invoked by COUNSELOR) — Investigated missing glyph elements in old SVGs
- **Engineer** (invoked by COUNSELOR) — Ran generate/transfer/build scripts

### Files Modified (5 total)
- `sheet_config.py` (NEW) — Single source of truth for all grid, metric, font metadata, ligature, and appearance constants. All scripts import from here.
- `generate_unified_sheets.py` — Rewritten: imports from sheet_config.py, generates blank SVGs with 10px gutters + ligature rows (23 two-cell, 12 three-cell)
- `transfer_glyphs.py` — Rewritten: imports from sheet_config.py, remaps glyphs from old grid (COL_STEP=140, ROW_STEP=243) to new grid (COL_STEP=130, ROW_STEP=253), handles both `<path>` and `<rect>` elements
- `build_fonts.py` — Updated: all hardcoded _GRID/WEIGHTS/metadata replaced with imports from sheet_config.py
- `append_ligatures.py` — Created but superseded (ligatures now built into generate script). Can be deleted.

### Key Decisions
- **10px gutter** both horizontal and vertical (was 20px H / 0px V)
- **COL_STEP=130** (CELL_W=120 + GUTTER=10), **ROW_STEP=253** (CELL_H=243 + GUTTER=10)
- **sheet_config.py** is SSOT — no derived values hardcoded in scripts
- **Ligature layout**: 2-cell = 5 per row (rows 10-14), 3-cell = 3 per row (rows 15-18)
- **Ligature cell width**: N * CELL_W + (N-1) * GUTTER
- **`<rect>` glyph handling** added to transfer script (catches -, _, |, . characters)

### Alignment Check
- [x] LIFESTAR: Single Source of Truth (sheet_config.py), Explicit (no hidden derived values), Findable (all constants in one file)
- [x] No early returns
- [x] Fail-fast error handling

### Problems Solved
- Missing glyphs (- _ | .) were `<rect>` elements, not `<path>` — transfer script now handles both
- Vertical gutter was 0px — cells were stacked flush. Now 10px consistent both directions
- Grid constants were hardcoded and duplicated across 3 scripts — now single config file
- Old/new grid had different spacing — transfer script remaps per-cell with dx/dy offsets

### Technical Debt / Follow-up
- `append_ligatures.py` is dead code — delete it
- `fix_nf_names.py` and `patch_nf.sh` not yet updated for new config pattern
- GGWP repo needs same refactor (see handoff below)
- Ligature GSUB table generation not yet implemented in build_fonts.py
- Font build from new unified sheets not yet tested with actual glyphs (0 glyphs on blank sheets verified OK)

**Status:** ✅ APPROVED — All 3 scripts import from sheet_config.py, generate/transfer/build pipeline verified

---

## Handoff to COUNSELOR (next session): GGWP Update

**From:** COUNSELOR  
**Date:** 2026-03-03

### Objective
Port the sheet_config.py SSOT pattern and all improvements to the GGWP repo.

### What Changed in Display Mono (to port to GGWP)

**1. Config as SSOT**
- Display Mono now has `sheet_config.py` with all primitives (CELL_W, CELL_H, GUTTER) and derived values (COL_STEP, ROW_STEP)
- GGWP's `sheet_config.py` still uses `col_gap`/`row_gap` (should be single `gutter`), wrong `cell_height` (223, should be 243), and has `label_height` concept that Display Mono doesn't use

**2. Grid Math**
- GGWP: `row_step = cell_h + row_gap + label_h` — Display Mono: `row_step = cell_h + gutter`
- GGWP has separate `label_height` in row_step — Display Mono bakes label into cell top (no separate height)
- GGWP uses `col_gap`/`row_gap` — should be unified `gutter`

**3. Ligature Support**
- GGWP has no ligature support
- Display Mono's `generate_unified_sheets.py` handles 2-cell and 3-cell ligature rows
- Ligature cell width = `N * CELL_W + (N-1) * GUTTER`
- Layout: 2-cell ligatures N per row (COLS//2), 3-cell ligatures M per row (COLS//3)

**4. `<rect>` Glyph Handling**
- GGWP's `build_fonts.py` already has `draw_rect_to_pen()` — good
- But GGWP's `generate_sheet.py` uses `<line>` for reflines — Display Mono uses `<path d="M x,y L x,y">`
- GGWP wraps glyphs in `<g id="glyph-U+XXXX">` — Display Mono doesn't (Affinity exports don't use these)

**5. Transfer Script**
- GGWP has no transfer script — it's Display Mono specific (old grid → new grid remapping)
- Not needed in GGWP unless we want a generic "reflow grid" tool

### Files to Modify in GGWP (`~/Documents/Poems/dev/ggwp/`)
- `sheet_config.py` — Replace `col_gap`/`row_gap` with `gutter`, fix `cell_height` to 243, add `ligatures_2`/`ligatures_3` config, remove `label_height`
- `generate_sheet.py` — Unified gutter math, ligature cell generation, use `<path>` for reflines instead of `<line>`, remove `label_height` from row_step
- `build_fonts.py` — Match new grid math (no `label_height` in row_step), verify `<rect>` parsing works with new grid

### Ligature List (35 total)
**2-cell (23):** `->` `=>` `<-` `==` `!=` `>=` `<=` `&&` `||` `!!` `::` `<<` `>>` `/*` `*/` `//` `|>` `<|` `+=` `-=` `*=` `/=` `%=`
**3-cell (12):** `<=>` `-->` `<--` `==>` `<==` `->>` `<<-` `>>=` `=<<` `===` `!==` `<<=`

### Current State of Display Mono Scripts (reference implementations)
- `sheet_config.py` — SSOT config
- `generate_unified_sheets.py` — blank sheet generator with ligatures
- `transfer_glyphs.py` — old→new grid remapper (Display Mono specific)
- `build_fonts.py` — TTF builder importing from config

### Acceptance Criteria
- [ ] GGWP `sheet_config.py` uses single `gutter` value, correct `cell_height`, ligature config
- [ ] GGWP `generate_sheet.py` produces identical grid layout to Display Mono's generator
- [ ] GGWP `build_fonts.py` grid math matches (no `label_height` in row_step)
- [ ] GGWP generates blank sheets with ligature rows when ligatures configured
- [ ] GGWP builds fonts from sheets with new grid (test with Display Mono sheets if possible)

### Notes
- Display Mono is the proving ground; GGWP is the generalized tool
- Once GGWP is updated, Display Mono should eventually import from GGWP instead of its own sheet_config.py
- GSUB ligature table generation is NOT yet implemented anywhere — that's a separate sprint

---

<!-- End of sprint entries -->
