# design

## Identity

- **Name:** design
- **Type:** Source code project
- **Language:** Python, shell
- **Root:** `/Users/jreng/Documents/Poems/dev/end/design`
- **Version:** none found

## Current State

- **Last sprint:** none. `carol/SPRINT-LOG.md` has no sprint entries.
- **Active ODE:** none.
- **Active debt:** none. `DEBT.md` does not exist.

## Layout

- `build_fonts.py`, `build_monolithic.py`, `generate_unified_sheets.py`, `generate_ligature_cells.py`, `append_ligatures.py`, `fix_nf_names.py`, `migrate_to_grouped.py`, `sheet_config.py` — Python scripts at the root.
- `patch_nf.sh` — shell script at the root.
- `FontPatcher/` — font patcher. Contains `src/` and `bin/`.
- `designer/` — design files with the `.af` extension.
- `donors/` — donor font files (`.ttf`).
- `fonts/` — font directory (`Display Mono`).
- `svg-mono/` — SVG glyph sheets.
- `carol/` — CAROL contract files and `SPRINT-LOG.md`.
- `.claude/` — Claude Code settings, agents, and commands.

## Key Documents

| Document | Purpose |
|----------|---------|
| `carol/SPRINT-LOG.md` | Cross-session memory. Holds sprint entries. |

## API Docs

None. No doxygen or other generator config found.

## Build

None. No build or package manifest found at the root.
