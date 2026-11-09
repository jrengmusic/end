# PLAN: END Theme on the Style Contract

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-11-09
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE + JAM. Only Lean is adapted (LANGUAGE.md:25-38, one object, one translation unit).

Delivery: on approval, this file replaces `end/PLAN-style-driven-theme.md`, which it supersedes.

## Overview

END's theme gets one truth and one flow. `style.css` holds every font, colour and metric. The carried default is written once to disk, disk wins, and the Validator reports errors. StyleManager registers the one tree, and every style read goes through Style. The markdown emit becomes the inverse of the parse, at the callee.

## Context: what is wrong

- The crash: `gfx.css:2083` names `SymbolsNerdFont-Regular.ttf`; `end/Source/fonts/` has six Display faces. `jam_StyleManager.cpp:105-107` fails.
- Shadow state: embolden (`theme.md:47`, `gfx.css:1366`), thickness (`theme.md:57`, `gfx.css:891`), and two runtime trees (`ConfigTheme.cpp:100`, `ENDLookAndFeel.cpp:495-496`).
- The disk never reaches Style: `ENDLookAndFeel.cpp:495-496` reads binary css once. `ConfigModel.cpp:110-113` never calls `registerStyle`.
- SCREAMING values on disk: `jam_Css.cpp:411-412` stores a tag; `jam_ConfigDocument.cpp:542` formats the key; `:465` writes the value raw.
- `colour id` on disk: the parse appends the suffix (`:139-146`); the working tree has no emit strip. The diff removed `getKeyWords`, `getCellText`, `getPropertyNames`, `addPropertyTable`, `addSheetTables`.
- 26 words are in both lexicons with the suffix removed (`cast/style.md`, for example `markdown text`, `mermaid diagram text`). `getIdWords` tries colour first (`:150-152`).
- `ENDLookAndFeel` pokes config for style rows (`:24-25`, `:120`, `:128-129`, `:136`, `:146`, `:167`, `:172`, `:207`, `:225-227`, `:240`, `:245`, `:267`).
- `jam_StyleCustom.cpp:258-259` branches on `isEND()` for a config table.

## ARCHITECT's contract (verbatim, this session)

1. *"css contract: ALL RELATED STYLE SSOT - fonts - colour - metrics"*
2. *"colourId is juce native contract, the string format for css var is table generated, the enum, is table generated for the widget Component to declare referenced from the generated"*; *"fontId is our extension to juce native contract, identical to colourId"*
3. *"StyleManager build the map - StyleCustom consume downstream, style related `config` always through Style, never by poking config directly."*
4. *"config - file watcher listener events - style changed - component repaint"*
5. *"ConfigDocument should not doing any garbage defensive programming with bullshit jassert, ideally as consumer it should not write any jassert AT ALL"*
6. *"error should never be failure. carried binary always provide the default fallback. validation is always Document::Validator job. ConfigDocument should only trust validated document"*
7. *"binary write to virgin state to disk, disk always win."*; *"if disk was invalid, rendering never fails because guarantee of default fallback. BUT Validator must show error."*
8. *"md must be using plain words without any redudancy of `colour id`, `font id` etc. those keyword could be registered generated suffix to be appended at consumer, use jam::Format"*; *"DATA ALWAYS dictate logic"*; *"the contract is at CALLEE. starts from SSOT cast table manifest, down to config generated. the pattern exists. the problem is INCONSISTENCY"*
9. *"there are canon KEY parameters-value UPPERCASE such as appearance, UI scale"* (the XML-conformance exception stays).
10. *"you will break JFS-KANJUT if you changed StyleManager"*; *"JFS with KANJUT is the canon contract shape"*
11. *"END gains style.css"*; *"END + EVE + jam"*
12. *"merging theme.md as SSOT for END and EVE (hosted in END only, ENAX, pluginHostType isEND())"*
13. *"there are other agents working on Vulkan at jam ... be VERY CAREFUL"*

## Hard boundaries

- No file under `jam_vulkan`. No file outside each step's Scope. No git command by Engineer.
- No change to `jam_StyleManager.{h,cpp}` or to any KANJUT or JFS file.
- No version bump (end/CLAUDE.md:43-45).
- Comments and doxygen only in the post-audit pass.

## Dependency & API Inventory

- `Format::toWords`, `toTag`, `toType`, `toCamelCase` (`jam_Format.h:309-351`).
- `map::ColourId`, `map::FontId` (`cast/style.md:77`, `:855`). Suffix: `getLexiconSuffix` (`jam_ConfigDocument.cpp:117-122`), `getIdSuffix` (`:131-137`).
- Parse tiers: `getSheetType` (`:353-372`), `getComponentWords` (`:164-170`), `addProperty` (`:319-340`), `Id::alias` (`:55-61`).
- `Css::getOrCreate` memoized (`jam_Css.h:185`); `Css::getValueTree` (`:233`); sheet typed by file name (`:227`); `.window` (`jam_Css.cpp:679-685`).
- `ConfigDocument::getOrCreate` over `SharedDocuments` (`:5-21`); `getOrCreateDefaultFile` (`:593-607`).
- `ConfigValidator::addRules`, `addTableRules` (`jam_ConfigValidator.h:34-60`).
- `StyleManager`: `registerStyle` (`.h:117`), `setAppearance` (`:94`), `getFont` (`:45`), `getMetrics` (`:145`), `getWindowStyle` (`:193`), `getWindowStyleString` (`:207`), `getStyleSheet` (`:260`).
- Sibling forms: `StyleTheme.cpp:15` (`setAppearance` on `getStyleSheet()`), `:42-69` (window reads), `:20-31` (font styles). Reload order: `kuassa_PluginEditor.cpp:426-432`. Canon default: `kuassa_PluginEditorLayout.cpp:601-603`.
- Hosted EVE reads END's theme and overlays its own css defaults (`jam_PluginEditorLayout.cpp:94-106`, `:130`), on sheet `STYLE` (`jam_StyleManager` `getStyleSheet`).

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and the locked PLAN decisions (no deviation, no scope drift). @Auditor runs ONCE, after the final step, covering the whole sprint — never per step.

## Steps

### Step 1: ConfigDocument trusts the Validator
**Scope:** `jam/jam_markdown/document/jam_ConfigDocument.cpp`
**Action:** Delete `jassert` at `:568` and at `:603`, and the `[[maybe_unused]]` local at `:602`.
**Validation:** Contract 5. The file has no `jassert`.

### Step 2: The emit is the inverse of the parse
**Scope:** `jam/jam_markdown/document/jam_ConfigDocument.{h,cpp}`
**Action:**
1. Pathfinder pastes the HEAD text of `getKeyWords`, `getCellText`, `getPropertyNames`, `addPropertyTable`, `addSheetTables` and HEAD `toDocument` into the Engineer prompt. These are the starting point.
2. Key: a row whose words pass `getIdSuffix` is written without the suffix and without the component words of its table.
3. Value: an alias of a row with an id suffix is written with `Format::toWords`. This is the inverse of `Id::alias` (`:55-61`). Every reference row has one type, `string`, which `:323` reads as an alias.
4. Tiers: a sheet is one `##` heading. Under it, `###` tables: `colours`, one table per component with the colour-id rows, `fonts` with every font-id row, `metrics`. The fonts table selects the font lexicon (`:312`), so the 26 shared words never compete.
5. A table that holds both properties and child rows (`FONTS`): the child rows stay the `##` table, and the properties are a `### metrics` table under it. `getSheetType` puts them on the same tree (`:400-405`), and `getComponentWords` adds no prefix (`:166`).
**Validation:** Contract 8. Round trip: css tree → markdown → tree, equal for colours, aliases, metrics, units, dark, components, faces and font attributes. No `colour id`, `font id` or SCREAMING value in the output.

### Step 3: Carried default memoized under one name
**Scope:** `jam/jam_markdown/document/jam_ConfigDocument.{h,cpp}`
**Action:** Add overload `getOrCreate (const juce::Identifier& name, const juce::String& text)`, which parses `text` once per name into `SharedDocuments`. The one-argument form calls it with the BinaryData text.
**Validation:** Family `getOrCreate` (NAMES Rule 5). One parse per name.

### Step 4: `style.css` holds every font, colour and metric
**Scope:** rename `end/Source/config/theme/gfx/gfx.css` to `style.css`; `end/Source/config/theme/gfx/theme.md`; `end/cast/files.md`; `end/cast/identifiers.md`
**Action:**
1. The file name gives sheet `STYLE`, so hosted EVE and END share one sheet (contracts 11, 12). `files::themeStyleSheet` value is `style.css`.
2. Delete the `symbols` face (`:2081-2083`). END ships no such file.
3. Add `.window { blur: 32; mac: backgroundBlur; win: blurBehind; }`. Add `--ResizableWindow--backgroundColourId: var(--glassNarwhalGrey)` to `.appearance`.
4. Add to `.metrics`: `--Tab--depth`, `--Tab--textPadding`, `--Tab--paddingTop`, `--Tab--paddingRight`, `--Tab--paddingBottom`, `--Tab--paddingLeft`, `--PaneComponent--resizeBarThickness`, `--PaneComponent--padding`, with the values of `theme.md:83-91`, `:111-113`.
5. Add `@font-face` `tab` (DisplayBook, height 12, kerning 0.075) and `overlay` (DisplayMonoMedium, height 14).
6. `theme.md` keeps only rows that are not a font, colour or metric: `## tab` (`position`, `always visible`, `uppercase`) and `## pane` (`split line`). Version stays. Menu opacity goes: the colour is `PopupMenu::backgroundColourId` (`gfx.css:72`).
7. Add ids `tabDepth`, `tabTextPadding`, `tabPaddingTop`, `tabPaddingRight`, `tabPaddingBottom`, `tabPaddingLeft`, `paneComponentResizeBarThickness`, `paneComponentPadding` (family `scrollBarThickness`, `StyleCustom.cpp:261`). Delete ids with no reader.
**Validation:** Contract 1. Each row is in one source. Every `src` names a file in `Source/fonts/`.

### Step 5: One default, one tree
**Scope:** `end/Source/config/ConfigTheme.{h,cpp}`
**Action:**
1. A file-static builds the default text: BinaryData `theme.md`, a newline, then `MarkdownWriter {}.getText (ConfigDocument::toDocument (Css::getValueTree (Format::toType (Id::settings), styleSheets)))`. This is `kuassa_PluginEditorLayout.cpp:601-603`. It registers the text through the Step 3 overload before any table is read.
2. `saveToPath` writes that text with `getOrCreateDefaultFile`.
3. `loadFromPath` is unchanged in shape: `addFileTables` validates, the disk overlays the defaults, errors return.
**Validation:** Contracts 6, 7. An invalid disk keeps the default and returns the message.

### Step 6: Style registers the one tree, on every change
**Scope:** `end/Source/lookAndFeel/ENDLookAndFeel.{h,cpp}`
**Action:**
1. `initialiseStyle` creates StyleManager from the THEMES tree of `ConfigModel`, not from a css tree.
2. `applyColours` calls `setAppearance (jam::StyleManager::getStyleSheet(), *this, appearanceText)` (`StyleTheme.cpp:15`).
3. The `Id::theme` event (`ConfigTheme.cpp:101`) runs `registerStyle`, `applyColours`, `applyFontStyles`, `loadGraphics`, in the order of `kuassa_PluginEditor.cpp:430-432`, then the LookAndFeel cascade. Delete the per-table colour events.
**Validation:** Contracts 3, 4. One flow for init and watcher.

### Step 7: Consumers read Style
**Scope:** `end/Source/lookAndFeel/ENDLookAndFeel.{h,cpp}`; `jam/jam_style/jam_StyleCustom.cpp`; `jam/jam_web/css/jam_Css.{h,cpp}`
**Action:**
1. Tab and overlay fonts: `getFont (Id::tab)`, `getFont (Id::overlay)`.
2. Depth, paddings, pane sizes: `getMetrics (StyleManager::getStyleSheet(), Id::…)`.
3. Window: `getWindowStyle<float> (Id::blur)`, `getWindowStyleString (Id::mac / Id::win)`, background `findColour (juce::ResizableWindow::backgroundColourId)`.
4. Delete the menu opacity read; the popup uses its colour id.
5. `position`, `alwaysVisible`, `uppercase`, `splitLine` stay `getRowValue`, with typed `var` casts in place of the `.toString()` parses.
6. `jam_StyleCustom.cpp`: delete the `isEND()` branch (`:258-259`). `:261` is the one path.
7. `jam_Css`: delete the comma-list case and `getStyleAttributeType`, which have no remaining need.
**Validation:** Contract 3. No `getRowValue` for a font, colour or metric in END.

### Step 8: Documents
**Scope:** `end/SPEC.md:164-178`, `end/ARCHITECTURE.md:286-288`, `:662`, `:694-698`, `end/PLAN-style-driven-theme.md`
**Action:** Record the contract. Replace the old plan with this one.
**Validation:** ARCHITECTURE mirrors the code.

### Step 9: Build, audit, doxygen, log
**Action:** ARCHITECT runs `cast` in jam, eve, end, and builds; ARCHITECT deletes `~/.config/end/themes/gfx/theme.md` (no version bump, so the old file stays). @Auditor once. Validated findings resolved. Doxygen pass by @Pathfinder on touched headers. Sprint log.
**Validation:** Verification list below.

## Verification (ARCHITECT builds and runs)

1. END starts with no config. `theme.md` appears with plain keys, word values, no suffix words.
2. Edit a colour, a metric and a font height on disk: each applies live and repaints.
3. Corrupt `theme.md`: END renders defaults, and the overlay shows the error.
4. Hosted EVE takes END's colours. Markdown and Mermaid render with correct colours and fonts, including the 26 shared words.
5. EVE standalone writes and reads its own `theme.md` in the same shape.
6. Dark appearance applies.

## BLESSED Alignment

- **B:** ConfigTheme owns the tree. StyleManager holds it by reference.
- **L:** No new machinery. One overload, one emit inverse.
- **E:** Tables name the lexicon; no guessing.
- **S (SSOT):** Each row has one source. One runtime tree.
- **S (Stateless):** Per-table events and hand reads are removed.
- **E (Encapsulation):** Format conversion is at the callee. Consumers ask Style.
- **D:** One flow for init and reload.

## Risks / Open Questions

Closed by the quotes above. Decisions derived, for ARCHITECT's approval with this plan:
- Rows that are not font, colour or metric stay in carried `theme.md` (contract 1 lists style only; canon form `kuassa_PluginEditorLayout.cpp:601-603`).
- `gfx.css` becomes `style.css` (contracts 11, 12; the old `gfx.css` name kept hosted EVE on a different sheet).
- Markdown and Mermaid rows stay in END's sheet: hosted EVE reads them from END's theme (contract 12).
