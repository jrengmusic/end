# PLAN: The Theme File Is the Style Sheet in Markdown

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-11-09
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE + JAM. Only Lean is adapted (LANGUAGE.md:25-38).

## Overview

`theme.md` is a second serialization of one Model, the `CssDocument`. Each css block is one
markdown table, and each table is one css block. Both serializations fill the same document
sections, and the same `CssDocument::getValueTree` assembly builds the tree. StyleManager,
KANJUT and JFS do not change.

## Context: the cause

- `CssDocument` keeps the tiers as sections: `colours` (`:root`), `appearance` (+ `DARK`), `metrics` + `unit`, `window`, `fonts` (`@font-face`), `style` (class blocks) — `jam_Css.h:170-180`, dispatch `jam_Css.cpp:641-700`, `:743-751`, `:776-782`.
- `addSheet` collapses them into one record for StyleManager — `jam_Css.cpp:2106-2151`. That shape is canon.
- The emit started from the collapsed tree (`ConfigTheme.cpp:42`) and guessed the lost tiers back with predicates and a `COMPONENT` side record (`jam_ConfigDocument.cpp:621-648`); the parse rebuilt them with `getSheetType`, `###` tables and a lexicon guess. MANIFESTO E: *"no downstream consumer ... re-derives what that operation already produced."*
- JUCE pattern: one structure, two serializations, 1:1 — `ValueTree::fromXml` / `toXmlString` (`juce_ValueTree.h:448-460`).

## ARCHITECT's contract (verbatim, this session)

1. *"css contract: ALL RELATED STYLE SSOT - fonts - colour - metrics"*
2. *"theme.md user should not care about hierarchy. fonts is fonts, colours is colours, metrics is metrics."*; *"there should be only ONE metrics table"*
3. *"md must be using plain words without any redudancy of `colour id`, `font id` etc. those keyword could be registered generated suffix to be appended at consumer, use jam::Format"*; *"DATA ALWAYS dictate logic"*; *"the contract is at CALLEE"*
4. *"ConfigDocument should not doing any garbage defensive programming"*; *"validation is always Document::Validator job"*; *"binary write to virgin state to disk, disk always win."*; *"if disk was invalid, rendering never fails ... BUT Validator must show error."*
5. *"you will break JFS-KANJUT if you changed StyleManager"*
6. *"UNIVERSAL and GENERIC. no harcoding of assumptions ... must be ABSTRACTION"*; *"if ConfigDocument need to be rewritten from scratch ... FIX IT NOW. no more hack and patch"*
7. *"there are other agents working on Vulkan at jam ... be VERY CAREFUL"*

## The mirror (each row: css block ↔ markdown table ↔ document section)

| css | `theme.md` | section | key cell | value cell |
|---|---|---|---|---|
| `:root` | `## colours`, rows with a hex value | `colours` | palette words | `0xAARRGGBB` |
| `.appearance` colour ids (+ `.appearance.DARK`) | `## colours`, rows with a name value, `light` / `dark` | `appearance` (+ `DARK`) | words without the colour-id suffix | palette words |
| `.appearance` font ids | `## fonts`, rows with a name value | `appearance` | words without the font-id suffix | face words |
| `@font-face` | `## fonts`, face rows; descriptors as columns, as in css | `fonts` | face words | family, style, src, height, kerning, rasterizer, gamma, contrast, embolden |
| `.metrics` | `## metrics`, unit as type | `metrics` + `unit` | words | number |
| `.window` | `## window` | `window` | words | text or number |
| `.<class>` (+ `.DARK`) | `## <class>` | `style` child | as `## colours` / `## fonts` rows | as above |

- Which table a row belongs to is the lexicon of its id: the table names it (`colours` = `map::ColourId`, `fonts` = `map::FontId`). A row is an id row when its words plus that lexicon's suffix are an entry; else it is a palette row (`colours`) or a face row (`fonts`). The 26 words in both lexicons never meet in one table.
- Suffix words come from the generated lexicon (`getLexiconSuffix`, `jam_ConfigDocument.cpp:127-132`); words ↔ keys through `jam::Format`.
- A word that names no entry and no row is a Validator failure, not a guess.

## Settings rows

`## tab` (`position`, `always visible`, `uppercase`) and `## pane` (`split line`) are not style (contract 1). They move to `display.md`, read by `ConfigModel` as today. `theme.md` holds only the style sheet.

## Hard boundaries

- No file under `jam_vulkan`; no change to `jam_StyleManager.{h,cpp}`, KANJUT or JFS; no version bump; no jassert in the document readers; comments only in the post-audit pass.

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and the locked PLAN decisions (no deviation, no scope drift). @Auditor runs ONCE, after the final step, covering the whole sprint — never per step.

## Steps

### Step 1: Delete the tree emit and the sheet parse
**Scope:** `jam/jam_markdown/document/jam_ConfigDocument.{h,cpp}`
**Action:** Delete the sheet branch of `toDocument (ValueTree)` and its helpers (`addSheetTables`, `addPropertyTable`, `getPropertyNames`, `getCellText`, `getKeyWords`, `getSheetComponent`), and the sheet parse (`getSheetType`, `addSubTable`, `getComponentWords`, the `###` path, the `COMPONENT` child). `ConfigDocument` stays the reader of config tables. The compiler lists every caller.
**Validation:** No tier is derived anywhere in `ConfigDocument`.

### Step 2: Markdown writer of a CssDocument
**Scope:** the module that sees both documents, by the module include order
**Action:** A writer returns the `ConfigDocument` of the mirror table above, built from the sections, never from a tree. Keys and values through `Format` and the lexicon suffix.
**Validation:** Each section writes exactly one table; nothing is guessed.

### Step 3: Markdown reader into a CssDocument
**Scope:** same file as Step 2
**Action:** A reader fills the same sections from the tables: id rows by lexicon, palette rows, face rows with their descriptor columns, metrics with unit, window, class tables into `style` children, `dark` into `DARK`.
**Validation:** Round trip css → markdown → css gives equal sections; `getValueTree` gives an equal tree.

### Step 4: Validator rules for the style tables
**Scope:** `jam/jam_markdown/document/jam_ConfigValidator.cpp`
**Action:** A row whose words name no lexicon entry and no palette or face row fails; an unknown `src` fails (existing rule). The table rule compares headers with the default document, as today.
**Validation:** Contract 4.

### Step 5: END theme lane
**Scope:** `end/Source/config/ConfigTheme.cpp`, `ConfigDirectory.{h,cpp}`, `ConfigModel.cpp`, `display.md`, `theme/gfx/theme.md`
**Action:** Default text = the shipped front matter + the markdown of `CssDocument::getOrCreate (style.css)`, written once to a virgin disk. Load: validate the disk document against the default; on pass, the reader's `getValueTree (…)`; on fail, the default tree and the error. StyleManager registers that tree. `## tab` and `## pane` rows go to `display.md`.
**Validation:** Contracts 1, 4. One tree, one flow.

### Step 6: EVE theme lane
**Scope:** `jam/jam_plugin_bootstrap/layout/jam_PluginEditorLayout.{h,cpp}`
**Action:** The same calls as Step 5: write the markdown of EVE's style sheet, read through the reader. Under END, read END's file.
**Validation:** EVE standalone and hosted read the same shape.

### Step 7: Documents, audit, doxygen, log
**Action:** `end/SPEC.md`, `end/ARCHITECTURE.md`; @Auditor once; validated findings resolved; doxygen pass; sprint log.

## Execution facts (read during execution)

1. Placement: `jam_web` cannot see `ConfigDocument`. `jam_markdown.h:26` includes `jam_style.h`, and `jam_style.h:39` includes `jam_web.h`. So the writer and the reader are `ConfigDocument` members:
   - `static ConfigDocument toDocument (const CssDocument& styleSheet);` (joins the `toDocument` family)
   - `struct CssWriter : Document::Writer` (joins the `MarkdownWriter` family): writes a theme document as css text. `CssDocument::parse` of that text builds the sections, so the css document stays the only builder of its sections (Auditor finding: MANIFESTO E *"Creation does not change a complete object"*).
   - `juce::Identifier getAliasKey (const Element& row) const;` (one lexicon rule for the writer and the Validator)
   - `static juce::Identifier CssDocument::getSheetType (const juce::String& styleSheet);` (the sheet type of a style sheet file)
2. `toDocument (const juce::ValueTree&)` stays for `jam_Profiler.cpp:172`, without the sheet branch.
3. The `COMPONENT` section of `CssDocument` is deleted. No reader outside `jam_Css.cpp` and `jam_ConfigDocument.cpp` read it, and markdown words cannot carry the component boundary.
4. The shipped `theme.md` keeps its front matter. `ConfigDocument::getOrCreateDefaultFile` compares the versions of the disk and the default (`jam_ConfigDocument.cpp:881-882`).
5. The reader chooses the lexicon of an alias row by what its value names: a face row of `## fonts` gives the font lexicon, else the colour lexicon.

## Verification (ARCHITECT builds and runs)

1. Fresh `theme.md`: `## colours`, `## fonts`, `## metrics`, `## window` (and EVE's class tables); plain words; one metrics table.
2. Edit a palette colour, a component colour, a face height, a metric: each applies live.
3. Corrupt a row: defaults render, the overlay shows the error.
4. Hosted EVE and standalone EVE; Markdown and Mermaid colours and fonts; dark appearance.
