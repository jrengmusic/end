# PLAN: END and EVE config — correct markdown structure, named light/dark palette, one load path, pane padding

**RFC:** none — objective from ARCHITECT prompt, session 2026-11-02/03 (`end-eve-config`).
**Date:** 2026-11-03
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.14 (jam `patch/` lane) / JAM. CAST generates `CMakeLists.txt` and `Source/generated/*` in END, EVE and JAM. Agents never build and never run cast. ARCHITECT builds and runs cast.

## Overview

END and EVE read their config markdown through one jam load path. A disk file overlays the binary default row by row. An error shows in the overlay, the binary default stays, and nothing asserts on user input. Every key is lower-case words. Every theme table holds named colours in `light | dark` columns, with `0xAARRGGBB` for direct values. END carries no terminal rows. EVE's `eve.md` is written in END's organisation, not as css variables. The pane padding becomes one config row that both the outline and the hosted editor read.

## Rulings (ARCHITECT, verbatim)

Scope and intent:
- *"our current work is END-EVE. priority is END loaded on EVE."*
- *"we need PLAN/RFC hygiene. PLAN/RFC that already supersed, completed must be deleted."*
- *"end config should no longer carry anything about markdown/mermaid or terminal, it's now EVE config."*
- *"no handroll anything, this refactor probably mostly REMOVAL, not addition, not handroll anything"*
- *"END "style" actually organized as theme. this contract should not be changed. no runtime behaviour should change."*

Errors and version:
- *"agents always HACK by bumping version, WHICH IS NOT ALLOWED"*
- *"each time the table-parse structure changes, instead of showing error and keeping the default binary carried as default value and carry on, it always hit assert and crash. this is fucking bullshit"*
- *"we already have Document::Validator, we have a way to validate and show error. NOT fucking let it error to hit assertion bullshit"*
- *"so the error came from EVE? EVE config validator not designed like END? fix it"*
- *"END now should also have version check, identical, exactly SSOT validation same as EVE with frontmatter"*
- EVE error surface: *"EVEView overlay"*.

Structure, css and palette:
- *"cancel the css, but WRITE CORRECT md structure. BUT make eve config to organized like end. not just lazily write all css var with double dash into table."*
- *"there's no seed. eve is using plugin_bootstrap. css still the only SSOT. but the contract exactly like END. binary carry default, disk always win. so EVE need a specific way to author its default config by combining ViewLayout.md + css"*
- EVE path: *"~/.config/end/eve.md"*.
- *"EVE "theme" could be just defined in its eve.md"*
- *"colours should be defined with NAMES, with LIGHT/DARK theme. currently value and dark. CONFUSING."*
- *"then the actual "THEME" by default refer to the NAME, although direct hex AARRGGBB should work"*
- *"appearance is LIGHT. appearance.DARK is DARK. same thing different name"*
- Columns: *"light | dark everywhere"* — every theme table has `key | type | light | dark | description`; a non-colour row fills `light`; an empty `dark` means the same as light.
- Both projects: *"EVE and END both"*. END light/dark source: *"display.md row"*.
- *"the original palette of those names is at jreng-filter-strip style.css, fix all to conform to consistent colour-name"*
- *"palette that EVE and END use names the reference SSOT is JFS. other than that the name are on jam generated name"*
- Alpha: *"replace all consistently using 0xAARRGGBB"*.
- Unknown name in a cell: *"Fall back to ColourNames"*.
- Scope of eve.md tables: *"All components"*.
- TextEditor collision: *"Delete STYLE's TextEditor rows"*.
- *"what i dont understand why the fuck there so many definition of terminalIndex? that is so unnecessary, and should be internal mapping"* — indexes 0–15 stay in eve.md: *"0–15 in eve.md"*.

Keys:
- *"END and EVE keys all using user friendly case. lower case with whitespace. althought the exact word is internally reserved and FORMATTED."*
- *"no. why overcomplicatie? what already define? just reformat as we need. why now the format is inconsistent that's the issue. IIRC plugin_bootstrap with xml settings need tag with screaming snake case, that should not break. still eve could use anything, formatting is reader/parser responsibility"*

END theme content:
- `## code`: *"Delete code; keep embolden"*; *"embolden should stays at theme.md"*; *"embolden in theme, font table"*.
- Tab rename editor colours: *"Move to `## tab`"*.

Pane padding:
- *"padding value should be exposed as config."*
- *"padding value should be SSOT for EditorView to render INSIDE the outline"*
- *"the net effect is EVE's TextEditor will drawn its own padding, INSIDE the border. currently colliding"*

Document deletion: *"INVENTORY-end-plugin-host, carol/HANDOFF.md, eve/HANDOFF-profiling.md, eve/PLAN-eve-terminal-protocol"*, plus the two completed ENAX plans.

## Language / Framework Constraints

- CODING.md *One object, one translation unit*: every non-template body moves to the object's one `.cpp`.
- CODING.md *Submodule headers include nothing*: jam submodule files take no `#include`.
- CODING.md *No out-parameters*: the one exception is a value replacement in place. `juce::ValueTree` is a shared handle; a function that adds tables into a target tree follows the existing END pattern `addTables (juce::ValueTree target, ...)` (`ConfigModel.cpp:57`).
- MANIFESTO **E** *Complete at Creation*: `ConfigDocument::parse` sets each config row id while it creates the document. No consumer changes a row id later.
- MANIFESTO **D**: an assert stays only on an invariant the binary owns (the binary default is valid, `jam_PluginEditorLayout.cpp:23`, `ConfigModel.cpp:103`, `:185`; a row exists in the binary-built state, `ConfigModel.cpp:209`). No assert reads user input.
- CAROL.md Version Rule (END `CLAUDE.md`): no agent changes a `version:` value. Adding a front matter with `version: 0.0.1` to a file that has none changes no version.

## Dependency & API Inventory

jam_markdown (the reader):
- `MarkdownDocument::parse` assigns a table id `Format::toValidID (heading text)` (`jam_MarkdownBlockParser.cpp:1804-1814`; `## text editor` → `text_editor`, `jam_Format.cpp:104-127`). It assigns a row id from the raw key cell, or `Id::tableRow` when the cell is not a valid Identifier (`jam_MarkdownBlockParser.cpp:1767-1773`).
- `ConfigDocument` reads `key`, `type`, `value`, `dark` (`jam_ConfigDocument.cpp:57-132`). `addRowChild` drops `dark` (`:64-81`; header claim at `jam_ConfigDocument.h:61-64`). `colour` parses hex only (`:33-34`). `toDocument` writes the inverse (`:134-305`).
- `ConfigValidator` rules `type`, `value`, `choices` (`jam_ConfigValidator.cpp:5-86`) over `MarkdownValidator` rules (`jam_MarkdownValidator.cpp:16-84`). No numeric rule.
- cast compiles `jam_markdown` (`cast/CMakeLists.txt:222-224`, `jam_markdown.cpp:9`). cast's doxygen index has 0 references to `ConfigDocument` or `ConfigValidator` and 326 to `MarkdownDocument` table use (`cast/docs/xml`). This plan changes `ConfigDocument` and `ConfigValidator` only; `MarkdownDocument` and its block parser keep their behaviour.

jam_core:
- `jam::Format::toCamelCase`, `toKebabCase`, `toScreamingSnakeCase`, `dashToWhitespace`, `from`, `upTo`, `toValidID` (`jam_Format.cpp:104-127`, `:201-244`, `:537-569`, `:644-653`, `:696-700`). `forEachWord` splits on whitespace and on camel boundaries (`jam_Format.h:90-156`).
- `Id::toTag` / `Id::toType` = `toScreamingSnakeCase` (`jam_Id.h:16-30`).
- `Format::isVersionOld` (`jam_Format.cpp:785-810`); `MarkdownDocument::getVersion` (`jam_MarkdownDocument.cpp:1399-1406`).

jam_data_structures:
- `jam::Model::setValuesFrom` (`jam_Model.h:937-940`, `jam_Model.cpp:246-267`) over `Model::applyFunctionRecursively` (two-tree walk, `jam_ModelUtils.cpp:21-39`).
- `map::Appearance` AUTO/LIGHT/DARK (`jam_DataStructures.h:107-136`).

jam_graphics / jam_style:
- `map::ColourNames`, a `Bimap<uint32_t>` of camelCase names (`jam_Graphics.h:146-149`, `:534`); `Bimap::find (name)` (`jam_Bimap.h:168-172`).
- `jam::Colours::getNearestNameFormatted`, `getDistance` (`jam_Colours.cpp:118-147`).
- `jam::StyleManager::isDark` (`jam_StyleManager.cpp:168-174`), `setAppearance` (`:176-200`, string slot only at `:193`), `getColour` (`:153-165`).
- `jam::StyleCustom` getter family `getTabPadding` (`jam_StyleCustom.h:109`), `getPaneEdgeSize` (`:122`), `getWindowPadding` (`:190`); `drawPaneOutline` (`:119`).

jam_web:
- `CssDocument::getValueTree` builds the sheet tree: `STYLE` and `TEXT_EDITOR` sheet children, palette `--name` int64, alias `--Component--nameColourId` string `--palette`, `DARK` and `UNIT` children, `FONTS` records (`jam_Css.cpp:2045-2216`).

jam_plugin_bootstrap:
- `PluginEditorLayout::getOrCreateSettings<DocumentType>` (`jam_PluginEditorLayout.cpp:18-45`); markdown-lane rules (`:292-356`); `getDefaultSettings` (`:521-526`); `getSettingsTree` (`:528-539`); XML-lane `getValueTree` puts the css tree first and a `settings` child second (`:541-551`); markdown-lane `getValueTree` (`:553-556`); `populateTree` (`:50-55`, `:574-581`).
- `SettingsDocument<ConfigDocument>::getSettingsValue` / `setSettingsValue` (`jam_SettingsDocument.cpp:67-115`); XML lane (`:5-51`), unchanged.
- `PluginEditor::fileChanged` re-reads settings and calls `populateTree` (`jam_PluginEditor.cpp:419-436`).

jam_gui:
- `jam::MessageOverlay (overlayState, fontCallback)` and `registerParameters (model)` need an OVERLAY row with an `Id::message` `ParameterText` (`jam_MessageOverlay.h:93-129`). END's owner pattern: `ENDModel.cpp:8-9`, `ENDView.cpp:5`.
- `jam::PaneComponent::edgePadding` (`jam_PaneComponent.h:104`), read only at `ENDLookAndFeel.cpp:524` (LSP, 2 references).

END:
- `ConfigModel` `getOrCreateDefaultFile` (`ConfigModel.cpp:38-51`), `addTables` (`:57-88`), `loadFromPath` (`:225-248`), `getRowValue` (`:205-212`).
- `ENDLookAndFeel::setColours` (`ENDLookAndFeel.cpp:554-581`), `registerEvents` (`:620-637`), `drawPaneOutline` (`:516-528`).
- `EditorView::resized` (`EditorView.cpp:20-57`).
- `jam::CommandManager::setKeyPresses` matches a keys row by its Identifier (`jam_CommandManager.cpp:12-14`, `:133`).

EVE:
- `EVEProcessor` builds the layout with `files::viewLayout` as the default resource (`EVEProcessor.h:91-97`) and binds the settings lane (`:99-104`).
- `EVEView` registers component-property callbacks (`EVEView.cpp:156-191`) and builds the theme (`:47-53`).

Established patterns confirmed for new members:
- Getter family on `StyleCustom`: `getTabPadding`, `getWindowPadding`, `getPaneEdgeSize` → `getPanePadding`.
- END Identifier family: `textFontSize` with literal `fontSize` (`end/cast/identifiers.md:184`) → `textFontFamily` with literal `fontFamily`.
- `getOrCreateDefaultFile` and `addTables` keep their END names when they move to jam (NAMES Rule 5).
- `Model::setValuesFrom` gains a static two-tree form with the same name; the member delegates to it.

## Names for ARCHITECT approval (NAMES Rule -1)

Approval of this PLAN ratifies these names. No other new name enters the code.
1. `jam::ConfigDocument::getOrCreateDefaultFile (const juce::File& file, const juce::String& defaultText)` — moved from END `ConfigModel.cpp:38`; adds the version gate.
2. `jam::ConfigDocument::addTables (juce::ValueTree target, const ConfigDocument& defaultDocument, const ConfigDocument& userDocument, const Document::Validator::Rules& rules)` — moved from END `ConfigModel.cpp:57`.
3. `static void jam::Model::setValuesFrom (juce::ValueTree& target, const juce::ValueTree& source)`.
4. `virtual int jam::StyleCustom::getPanePadding() const noexcept`.
5. END `Id::textFontFamily` with literal `fontFamily`.
6. jam cast table `## AnsiColour` in `jam/cast/terminal.md`, generating `map::AnsiColour` (index 0–15 ↔ `black`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white`, `brightBlack`, `brightRed`, `brightGreen`, `brightYellow`, `brightBlue`, `brightMagenta`, `brightCyan`, `brightWhite`).
7. File-static functions in `jam_PluginEditorLayout.cpp`: `getStyleDocument (const juce::ValueTree& settingsTree)` returns the `ConfigDocument` of the css tree in the eve.md structure; `setStyleValues (juce::ValueTree& settingsTree, const juce::ValueTree& configTree)` sets the css tree's palette and alias values from the config tables.
8. `juce::var jam::ConfigDocument::getColourValue (const juce::String& cellText) const` — the one colour resolution of Step 4 (hex, then the document's `colours` table, then `map::ColourNames`); a void var when the text does not resolve. Step 5's validator calls it. Family: `getValueTypes`, `getValueTree`, `getValueText` (NAMES Rule 5), added during execution.
9. Functions in `jam_ConfigDocument`, added during execution to remove repeated logic (MANIFESTO S): `juce::Identifier ConfigDocument::getValueColumn (const Element& row) const` (public, because the validator's `choices` rule needs it too) returns `Id::light` when the row has a `light` cell, else `Id::value`; `getCellValue (const ConfigDocument&, const juce::Identifier& type, const juce::String& text)` returns `getColourValue (text)` for a `colour` type, else `getValueTypes().get (type, view)`. Family: `getValueTypes`, `getValueText`, `getColourValue` (NAMES Rule 5).
11. jam `text::English` rows in `jam/cast/core.md` beside `failEmptyValue` / `failNotInSet` (`:2650-2652`), added during execution so each Step 5 failure says what failed: `failNotNumber` (`not a number: `), `failNotColour` (`unresolved colour: `), `failEmptyKey` (`empty key`), and for Step 6's structure check `failTableStructure` (`table differs from the default: `). Family: `fail*` (NAMES Rule 5). `Format::isNumber` (`jam_Format.cpp:703-712`) is corrected at its owner to its own doc (*"Checks if a string is numeric"*): non-empty, at least one digit, `-` only first, at most one `.`; LSP shows no other caller (`jam_Format.h:609`).
12. `jam::SharedInstance<map::Appearance> appearance { std::in_place };` in `ENDLookAndFeel` (beside `buttonState`, `position`, `windowFX`, `ENDLookAndFeel.h:272-274`) and in `ConfigModel`, because both call `map::Appearance` (`darkModeSettingChanged`; `StyleManager::isDark` in `getRowValue`). Family: the per-map `SharedInstance` members (NAMES Rule 5); jam sibling `jam_PluginEditor.h:289`. Added during execution; the Engineer introduced it without a prompt name (CODING "No identifier latitude"), COUNSELOR ratified it by family.
13. File-static functions in `jam_PluginEditorLayout.cpp`, added during execution for MANIFESTO S (one alias→table/key rule for writer and overlay) and L (one responsibility per function): `getStyleTable (const juce::Identifier& alias)` and `getStyleKey (const juce::Identifier& alias)` return the config table type and row key of a css alias; `addStyleRows`, `addTerminalRows` (writer sections of `getStyleDocument`); `setStyleRows`, `setTerminalRows`, `setFontRecords` (overlay sections of `setStyleValues`); `addStyleRow` / `setStyleRow` hold the one row rule that the component and terminal sections share (the Engineer added them without a prompt name; COUNSELOR ratified them as the singular of `addStyleRows` / `setStyleRows`). Family: `getStyleDocument` / `setStyleValues`, verbs `get` / `add` / `set` (NAMES Rule 1, 5).
14. ARCHITECT: *"why there's mono and --mono? that's violation of SSOT. there's only ONE `mono`"* — EVE `style.css` deletes the `"--mono"` `@font-face` (`:933-940`); `--Markdown--codeFontId` uses `var(mono)` (`:723`). Scope widened by ARCHITECT: *"no copies allowed. fix all now"* — the same fix in `jam/resources/markdown.css`, `kuassa/user_modules/resources/markdown.css` (delete the `"--mono"` block) and `cast/`, `ggwp/`, `scout/Source/markdown.css` (rename `"--mono"` to `mono`).
15. Names from the Auditor fixes (Step 15), each from an existing family (NAMES Rule 5): `jam::Format::toWords` (the `to*` converters; ARCHITECT's "lower case with whitespace"); `ConfigDocument::getConfigRow` (`getTableRow`); file-statics `isConfigTable`, `isHexColour` (`is*`); validator `getTypeFailures`, `getValueFailures`, `getNumberFailures`, `getColourFailures`, `getKeyFailures`, `getChoicesFailures` (`getFailures`) and `forEachValueCell` (`forEachCell`); END `addFileTables` (renamed END loop), `ConfigModel::getEventKey`, `EditorView::setEditorBounds` / `setSizeParameters` (`set*`), `menuItemIcons` (NAMES Rule 8 table noun). Audit finding 1 corrected the Step 2 / Step 13 mechanism: no row id changes after creation (MANIFESTO *Complete at Creation*); the reader formats keys where it reads and looks them up (`getConfigRow`).
10. Palette names computed in Step 9: each colour without a JFS name takes `jam::Colours::getNearestNameFormatted` of its hex. Computed (script `scratchpad/nearest_palette.py`, map order = insertion order, `jam_HashMap.h:2328-2335`; no tie, no collision): `A1D6E5` crystal, `00DDEE` coldLightOfDay, `8FC6D0` continentalWaters, `6B9099` cavoloNero, `273233` aztec, `001A20` void (`Void` → `lowerFirstChar`), `2D3B40` vulcan, `DDDDDD` steam, `121212` dreamlessSleep. JFS exact: `090D12` bunker, `FC704C` preciousPersimmon, `C5F0E9` gentleCold, `F3F5C5` silkStar, `8CC9D9` dolphin, `519299` lagoon, `699DAA` tranquiliTeal, `33535B` mediterranea, `BAFFFD` paleSky, `FEFFD2` mattWhite, `67DFEF` poseidonJr, `01C2D2` caribbeanBlue, `00C8D8` blueBikini, `2C4144` littleMermaid, `4E8C93` paradiso.

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and the locked PLAN decisions (no deviation, no scope drift). @Auditor runs ONCE, after the final step, covering the whole sprint — never per step.

## Steps

### Step 1: Plan hygiene
**Scope:** `end/PLAN-enax-plugin-host-type.md`, `end/PLAN-enax-command-id.md`, `end/INVENTORY-end-plugin-host.md`, `end/carol/HANDOFF.md`, `eve/HANDOFF-profiling.md`, `eve/PLAN-eve-terminal-protocol.md`; END `CLAUDE.md` "Open PLANs".
**Action:** @Engineer deletes the six files with `rm`. COUNSELOR replaces the END `CLAUDE.md` "Open PLANs" list with this PLAN.
**Validation:** The six paths do not exist. No other file changes.

### Step 2: jam reader — keys formatted at creation
**Scope:** `jam_markdown/document/jam_ConfigDocument.h` / `.cpp`.
**Action:**
1. `ConfigDocument::parse` sets, while it creates the document, the id of each row of each config table (header has `key` and `type`) to `juce::Identifier { Format::toCamelCase (key cell text) }`. `close pane` → `closePane`; `ui scale` → `uiScale`; a single-word key keeps its text.
2. `addRowChild` and `addProperty` take the key from the row id, not from the raw cell.
3. `toDocument` writes each key as words: `Format::dashToWhitespace (Format::toKebabCase (key))` (`fontSize` → `font size`). It writes each table heading the same way from the table type's words (`TEXT_EDITOR` → heading from the original component words, see Step 6).
**Validation:** No change in `MarkdownDocument` or its block parser. Every key path reads the row id. Format calls only; no character loop.

### Step 3: jam reader — `light | dark` columns
**Scope:** `jam_ConfigDocument.h` / `.cpp`.
**Action:**
1. `getConfigColumns` holds `key`, `type`, `value`, `light`, `dark`, `description` (`Id::light`, `jam_Style.cpp:27`).
2. A row's primary value comes from `light` when the header has it, else from `value`.
3. A non-empty `dark` cell gives the dark value in both paths: `addProperty` keeps its `DARK` child (`:98-100`); `addRowChild` sets a `dark` property on the row child. An empty `dark` cell gives no dark value.
4. `toDocument` writes `light` and `dark` for a table whose tree carries a `DARK` child or `dark` properties, else `value`.
5. Correct the header doc at `jam_ConfigDocument.h:61-64` in the doxygen pass (post-audit).
**Validation:** Both table paths treat `dark` the same. No new column name other than `light`.

### Step 4: jam reader — colour cells by name
**Scope:** `jam_ConfigDocument.cpp`.
**Action:** A `colour` cell resolves in this order:
1. `0x` plus 8 hex digits → the ARGB value (`getHexValue64`, as today).
2. A key of the same document's `colours` table → that row's resolved value. The cell text goes through `Format::toCamelCase` before the lookup.
3. A name of `map::ColourNames` → `map::ColourNames::getInstance()->find (camelCase name)` (`jam_Bimap.h:168-172`).
The `colours` table resolves first, so other tables can refer to it.
**Validation:** The lookup uses `map::ColourNames` and the document's own table only. No string compare with `==`.

### Step 5: jam validator — no user input reaches an assert
**Scope:** `jam_markdown/document/jam_ConfigValidator.h` / `.cpp`.
**Action:** Add rules in `ConfigValidator::addRules`:
1. `number`: an `integer`, `floatingPoint`, `em` or `numbers` cell (in `value`, `light` or `dark`) must parse fully. The rule uses the parse that `Format::getNumber` uses, so a cell that passes never reaches the asserts at `jam_Format.h:535`, `:541`.
2. `colour`: a `colour` cell must resolve by Step 4. An unresolved name fails with its location.
3. The `value` rule (`:31-54`) also checks `light`.
Each failure text uses `getLocation` (`jam_MarkdownValidator.cpp:5-14`).
**Validation:** Every failure is a `juce::Result`; no assert in the validator.

### Step 6: jam shared load path
**Scope:** `jam_ConfigDocument.h` / `.cpp`; `jam_data_structures/model/jam_Model.h` / `.cpp`; END `Source/config/ConfigModel.cpp`.
**Action:**
1. Move `getOrCreateDefaultFile` from `ConfigModel.cpp:38-51` to `jam::ConfigDocument::getOrCreateDefaultFile (file, defaultText)`. It writes `defaultText` when the file is missing, or when `Format::isVersionOld (user version, default version)`. It returns the file.
2. Move `addTables` from `ConfigModel.cpp:57-88` to `jam::ConfigDocument::addTables (target, defaultDocument, userDocument, rules)`:
   - It validates `userDocument` with `Document::Validator::isValid (rules, userDocument)`.
   - For each config table of the user document, the header set must equal the header set of the same table in `defaultDocument`. A table that differs fails with its location; a table absent from the default fails with its location.
   - It appends each passing table of `userDocument.getValueTree (rootType of target)` to `target`.
   - It returns the joined failure text, empty when none.
3. Add `static void jam::Model::setValuesFrom (juce::ValueTree& target, const juce::ValueTree& source)`. The body is the lambda of `jam_Model.cpp:246-267`. The member `setValuesFrom` calls it with `state` and keeps its message-thread assert and lock.
**Validation:** One body for each operation. END compiles against the moved functions (Step 7). No assert on user input.

### Step 7: END on the shared load path, with front matter
**Scope:** END `Source/config/ConfigModel.h` / `.cpp`, `ConfigDirectory.h`; `Source/config/display.md`, `keys.md`, `theme/gfx/theme.md`.
**Action:**
1. Each of the three files gets a front matter `---` / `version: 0.0.1` / `---` before its prose.
2. `ConfigModel` and `ConfigTheme` build their state from the binary text through `ConfigDocument::parse (...).getValueTree (...)`. The binary-validity asserts at `:103`, `:185` stay.
3. `saveToPath` / `ConfigTheme::saveToPath` call `jam::ConfigDocument::getOrCreateDefaultFile (file, BinaryData text)`.
4. `loadFromPath` / `ConfigTheme::loadFromPath` call `jam::ConfigDocument::addTables (disk, binary document, disk document, jam::ConfigValidator {}.getRules())`, then `setValuesFrom (disk)` (`:164`, `:242`). The joined errors go to `appModel.setMessage` (`:244-247`).
5. Delete the END copies of `getOrCreateDefaultFile` and `addTables`.
**Validation:** END reads the same values as before. A changed table structure on disk gives an overlay message and keeps the binary rows.

### Step 8: END keys, names and tables
**Scope:** END `Source/config/display.md`, `keys.md`, `theme/gfx/theme.md`; `cast/identifiers.md`; `Source/lookAndFeel/ENDLookAndFeel.h` / `.cpp`; `Source/Main.cpp`; `Source/ui/TabView.cpp`.
**Action:**
1. Every key cell in the three files becomes lower-case words (`alwaysOnTop` → `always on top`, `closePane` → `close pane`, `font-family` → `font family`, `fontSize` → `font size`, `gpu` stays `gpu`). Every heading stays its single word or becomes lower-case words.
2. `cast/identifiers.md`: add `textFontFamily` with literal `fontFamily`. `ENDLookAndFeel.cpp:117`, `:130`, `:136` read `Id::textFontFamily`.
3. `theme.md`: delete `## code`. Add `## font` with the row `embolden`; `Main.cpp:132` reads `Id::toType (Id::font)`. Move `caret`, `editor background`, `editor outline` into `## tab`; `ENDLookAndFeel.cpp:558`, `:560`, `:561` read `Id::toType (Id::tab)`.
4. Delete `getCodeFont`, `getCodeMetrics`, `CodeMetrics`, `selectionCursorColourId` and the `setColours` row at `:559`. `TabView::paintSplitPreview` / `paintJoinPreview` (`TabView.cpp:169-210`) stop using the code metrics. Delete each `cast/identifiers.md` row that no reader uses after the change (the compile gives the list).
**Validation:** Every keys row still matches a command (`jam_CommandManager.cpp:133`). No visible change except the previews (ruled).

### Step 9: END palette and light/dark
**Scope:** END `theme/gfx/theme.md`, `display.md`; `ConfigModel.cpp`; `ENDLookAndFeel.h` / `.cpp`.
**Action:**
1. `theme.md` gets `## colours` (`key | type | value | description`). Each opaque colour of the file is one row: its JFS name when the hex is in JFS `:root` (`jreng-filter-strip/Source/layout/style.css:48-81`), else `jam::Colours::getNearestNameFormatted (hex)`, written as words. Values are `0xAARRGGBB`.
2. Every other theme table takes `key | type | light | dark | choices | description` (choices only where the table has it). A colour cell names a `## colours` row; an alpha colour (`0xbf090d12`, `0x802c4144`, `0x2000ddee`) stays a direct `0xAARRGGBB`. `dark` is empty, so END looks the same as today.
3. `display.md` `## display` gets `appearance | string | AUTO | AUTO, LIGHT, DARK | ...`.
4. `ConfigModel::getRowValue` returns a row's dark value when `jam::StyleManager::isDark (appearance)` and the row has one, else its value.
5. `ENDLookAndFeel` re-runs `initialiseColours` on an `Id::appearance` change (`registerEvents`, `:620-637`) and, while the appearance is AUTO, on a desktop dark-mode change (`juce::DarkModeSettingListener`, the pattern of `jam_PluginEditor.cpp:219`, `:343-347`).
**Validation:** With `dark` empty every colour equals today's value. The names come from JFS or the jam table only; the Step report lists each computed name.

### Step 10: Pane padding
**Scope:** `jam_style/jam_StyleCustom.h` / `.cpp`; `jam_gui/layout/jam_PaneComponent.h`; END `theme.md` `## pane`, `ENDLookAndFeel.h` / `.cpp`, `Source/ui/EditorView.cpp`.
**Action:**
1. `theme.md` `## pane` gets `padding | int | 8 | | ...`.
2. `jam::StyleCustom` gets `virtual int getPanePadding() const noexcept;` with a default of 0. `ENDLookAndFeel` overrides it from `## pane` `padding`.
3. Delete `jam::PaneComponent::edgePadding` (`jam_PaneComponent.h:100-104`). `ENDLookAndFeel::drawPaneOutline` uses `getPanePadding()` in place of it (`:524`).
4. `EditorView::resized` places the hosted editor inside the outline stroke: bounds `getLocalBounds().reduced (padding + 2 × lineThickness)` for a resizable editor, top-left at that inset for any other editor. The `width` / `height` parameters take the inner size (`:47-55`).
**Validation:** One padding value in config; the outline and the editor both read it. EVE's own window padding (`ViewLayout.md:99`) stays inside the hosted editor.

### Step 11: EVE css colour names
**Scope:** `eve/Source/layout/style.css`, `eve/Source/textEditor.css`.
**Action:**
1. `style.css`: delete the four `--TextEditor--` lines (`:13-14`, `:368-369`).
2. Rename each palette name that is not a colour name to its JFS name, else to `jam::Colours::getNearestNameFormatted (hex)`: `--terminalIndexed0` … `--terminalIndexed15`, `--endCodeText`, `--endCodeBackground`, and the names `style.css` takes from `textEditor.css` (`--terminalIndexed11`, `13`, `14`, `15`, `233`). Every `var()` follows the rename. Indexes 16–255 keep their entries for the internal mapping.
3. A palette entry with alpha (`--endCodeHighlight: #00DDEE20`) keeps a css name, because a css alias needs a `var()`; the eve.md writer gives it as a direct `0xAARRGGBB` (Step 12).
**Validation:** A `diff` shows only renames and the four deletions. Every `var()` target is declared.

### Step 12: EVE default config in END's organisation
**Scope:** `jam_plugin_bootstrap/layout/jam_PluginEditorLayout.h` / `.cpp`; `jam/cast/terminal.md` (`## AnsiColour`); `eve/Source/layout/ViewLayout.md`.
**Action:**
1. `ViewLayout.md`: every key becomes lower-case words (`previewWidth` → `preview width`, `sequenceTimeout` → `sequence timeout`, `UI_SCALE` → `ui scale`, `HISTORY_CAPACITY` → `history capacity`). The version stays `0.0.1`.
2. `getStyleDocument (settingsTree)` writes the css tree as config tables:
   - `## colours`: each palette name that a written row refers to, as words, type `colour`, value `0xAARRGGBB`.
   - One table per component of the `.appearance` aliases (`--Component--nameId`, parts by `Format::from` / `Format::upTo` on `--`): heading = words of `Component`; key = words of `name` without its `Id` suffix (`textColourId` → `text colour`, `textFontId` → `text font`); type `colour` for a ColourId, `string` for a FontId; `light` = the `.appearance` value; `dark` = the `.appearance.DARK` value when it differs, else empty. An alpha palette value is written as its `0xAARRGGBB`.
   - `## terminal`: indexes 0–15 only, keyed by `map::AnsiColour`. Indexes 16–255 stay in the binary css.
   - `## metrics` and `## fonts`: the `.metrics` values and the `@font-face` records, as eve.md carries them today (`## STYLE` metrics, `## FONTS`).
3. `getDefaultSettings (SettingsDocument<ConfigDocument>*)` (`:521-526`) returns `ViewLayout.md` text followed by `MarkdownWriter {}.getText (getStyleDocument (getSettingsTree (...)))`.
4. `getValueTree (SettingsDocument<ConfigDocument>&)` (`:553-556`) follows the XML lane's shape (`:541-551`): it starts from `getSettingsTree`, calls `setStyleValues (settingsTree, configTree)`, and appends the non-style tables. `setStyleValues` walks the css tree's palette entries and aliases forward, finds the config value at the table and key that Step 12.2 writes for each, and sets it in the sheet (and its `DARK` child; an empty dark takes the light value) where the css tree holds that entry. A resolved colour is set as int64.
5. `StyleManager::setAppearance` (`jam_StyleManager.cpp:193-194`) accepts an int64 slot value as the colour, beside the string alias path.
**Validation:** A first launch writes an eve.md with no `--` key. A launch with that file renders as today, except the four deleted STYLE TextEditor lines (ruled).

### Step 13: EVE on the shared load path
**Scope:** `jam_PluginEditorLayout.cpp`; `jam_SettingsDocument.cpp` (markdown lane only); EVE `Source/EVEView.h` / `.cpp`, `Source/layout/parameters.md`.
**Action:**
1. `getOrCreateSettings<DocumentType>` (`:18-45`) calls `ConfigDocument::getOrCreateDefaultFile (userSettings, defaultText)`, then `ConfigDocument::addTables (disk, defaultDocument, userDocument, rules)` on a copy of the default tree, then `Model::setValuesFrom (defaultTree, disk)`. Delete the assert at `:42` and the debug-only log at `:37-40`. The XML lane keeps its behaviour.
2. The failure text travels as `Id::message` on the returned settings root. `populateTree` writes it to the model's component property `Id::toTag (Id::message)` (`AudioModel::getOrCreateComponentProperty`, the pattern of `EVEView.cpp:241`).
3. The settings lane compares settings keys in tag form: the rules at `:311-349`, `getSettingsValue` and `setSettingsValue` match a row when `Id::toTag (row id)` equals the tag. `populateTree` adds each settings property under `Id::toTag (name)`. A new settings row gets its key as words (`juce::String::replaceCharacter (Chars::underscore, Chars::space).toLowerCase()` of the tag).
4. `EVEView` owns a `jam::MessageOverlay`. It registers the callback `Id::toType (Id::message)` (the pattern of `EVEView.cpp:177-181`), which shows the property text, and it shows the current text once after construction.
**Validation:** A changed table structure in eve.md shows an overlay message, keeps the default and does not assert. The XML lane's SCREAMING tags are unchanged.

### Step 14: Docs
**Scope:** END `SPEC.md`, `ARCHITECTURE.md`, `README.md`, `CLAUDE.md`; EVE `SPEC.md`, `README.md`, `CLAUDE.md`; `eve/PLAN-Grand-Scheme.md` §5 Gap 2 note (`:1219-1229`).
**Action:** COUNSELOR records the config contract of this PLAN in each document. END `SPEC.md` §1.3 authoring rule (`:138-142`, *"table key cells are property names verbatim. A property consumed through a jam lexicon identifier uses jam's spelling"*) is superseded by ARCHITECT's key ruling and is rewritten. The Grand Scheme note gets an `[AMENDED 2026-11-03 — config]` block with the rulings above.
**Validation:** Every claim cites file:line in the code.

### Step 15: Audit, doxygen pass, log
**Action:** @Auditor sweeps once. COUNSELOR resolves every validated finding. @Pathfinder writes the doxygen pass over every changed header. COUNSELOR logs the sprint per `/log` and writes the commit messages in chat.

## Amendment — 2026-11-03: canon settings lane, keyword key form

### Rulings (ARCHITECT, verbatim)
- *"KANJUT PluginEditorLayout is the canon. both plugin_bootstrap are identical, will be synced"*
- *"no bloating Style concern into PluginEditorLayout"*
- *"DONT CHANGE xml Settings contract, VERBATIM as KANJUT"*
- *"keyword is keyword, jam::Format always gives the correct case"*
- *"that should not be hardcoded to any format so whichever format needed it's always on reader/parser responsible to format, DETERMINISTICALLY"*
- *"hardcoding case into generated definition is the problem. format case is C++ responsibility always"*
- *"if the config invalid DONT FUCKING CRASH. show the error, load default value from binary."* / *"END has the pattern to show the error, NOTHING IS HARDCODED. because eve default md config IS the fallback not only for writing on disk when the file not exists, but also as fallback when the file on disk INVALID"*
- *"dont use juce::String API, use jam::Format. juce::String allocates"*

### Superseded
- Step 12 (the style translation layer in `jam_PluginEditorLayout.cpp`) and the `getValueTree (juce::ValueTree)` rewrite of Step 13 are removed. The four files `jam_PluginEditorLayout.{h,cpp}`, `jam_SettingsDocument.{h,cpp}` are back on the KANJUT canon text (done).
- Step 2's camelCase key (`getRowKey` = `Format::toCamelCase`) was COUNSELOR's inference, not a ruling. It is replaced by K3.

### Steps
**K1 (done).** Canon restore of the four bootstrap files; markdown-lane rules, `getSettingsValue`, `setSettingsValue` find rows through `ConfigDocument::getConfigRow`, which compares `Id::toType` of both keys; `addParameters` adds each settings property under `Id::toTag (name)`.
**K2 (done).** `Format::toWords (std::string_view) -> std::string`, no juce::String inside: any case to lower-case words; an all-upper word in mixed-case text is an abbreviation and stays. `setSettingsValue` writes a new row's key cell with it.
**K3. One key form.** `Id::toTag (word)` becomes `Format::toScreamingSnakeCase` of `Format::toWords (word)`, so every form of a keyword gives one tag: `--Slider--thumbColourId`, `slider thumb colour id`, `sliderThumbColourId` → `SLIDER_THUMB_COLOUR_ID`; `UIScale`, `ui scale`, `UI_SCALE` → `UI_SCALE`. A camelCase input gives the same tag as before.
**K4. The css parser stores the key form.** `CssDocument` stores every custom-property name and every `var()` reference as `Id::toType (name)` on all paths (`jam_Css.cpp:389-412`, `:414-430`, `:543-572`, `:758-770`, `getVariable` `:210-224`). The css files do not change.
**K5. Definitions carry no case.** cast `## ColourId` (`cast/style.md:66`, 755 rows), `## FontId` (`:837`, 85 rows), `## TerminalIndexed` (`cast/terminal.md:121`, 256 rows): each `value` becomes the lower-case words of the keyword (`` `--Slider--thumbColourId` `` → `` `slider thumb colour id` ``). Scripted edit under the Destructive-Edit protocol.
**K6. StyleManager looks up by the key form.** Every lookup formats its incoming name with `Id::toType`: `getFontsStyleValue`, `getMetrics`, `getWindowStyle`, `getWindowStyleString`, `getColour`, `getFont` slot, `setAppearance` (`colourId` match through `Format::toWords` of the key against the map value).
**K7. ConfigDocument stores the key form.** `getRowKey` returns `Id::toType (key cell)`. The writer writes `Format::toWords (key)`. Every END reader of the config tree reads with `Id::toType (Id::x)`.
**K8. EVE markdown lane on END's flow.** The binary default is the fallback for a missing file and for an invalid file: `ConfigDocument::addTables` overlays each valid user table on the default, every error goes to the `Id::message` settings property and EVE's overlay, and no assert fires on user input. The XML lane stays canon.
**Validation:** EVE and END launch from an empty `~/.config/end/`; colours, fonts, metrics and the scrollbar match the css; an invalid eve.md shows the error and keeps the defaults.

### Audit 2 triage (COUNSELOR, against CONTRACT)
**Accepted (resolved this sprint):**
- 1, 2: a key cell or colour cell that formats to no words asserts (`juce::Identifier {""}`). K8. The key rule rejects a key with no words; `getConfigRow` compares tag text and never builds an empty Identifier.
- 3: a `numbers` cell whose count differs from the default row is indexed past its end. K8. `addTables` rejects it.
- 4: user ranges reach JUCE range asserts (`jam_Viewport.cpp:13`, `jam_PluginEditor.cpp:154`). K8.
- 5: `rasterizer` and `appearance` text reaches an assert and a throwing `Bimap::get`. K8. Settings rules check them against their maps, the pattern of the UIScale rule.
- 6: eve.md style cells that reference a palette or font name, and font `src` cells, reach StyleManager asserts and throws. K8. `addTables` rejects a reference that names no key; a settings rule checks `src` against BinaryData (the pattern of the image rule).
- 7: `setSettingsValue` dereferences a missing `settings` table (`jam_SettingsDocument.cpp:97-102`). K8.
- 8, 39: an unknown `mode` token asserts (`jam_CommandManager.cpp:33-34`). K8. A settings rule checks `mode` against `map::EditorMode`.
- 9: `Id::toType` on the audio thread (`EVETerminalProcessor.cpp:116`). MANIFESTO deadline threads.
- 11, 12: table ids and the `size` rule match raw ids. K1, K3.
- 13: an unknown or duplicate key is overlaid or dropped silently. MANIFESTO "Silent fail".
- 14: `mpekeyboard` rows in cast `ColourId` do not match their css names. K5.
- 15: a second key form at `jam_ViewManager.cpp:531`. K3.
- 16: an empty FIFO range builds an empty Identifier (`EVEView.cpp:297`). MANIFESTO fail fast on external input.
- 17, 32: font rasterization and embolden are style operations in `PluginEditorLayout`. Ruling "no bloating Style concern into PluginEditorLayout"; the canon keeps them on the theme.
- 19, 30: `Id::toTag` round-trips through juce::String twice and is an inline free function in a header. Ruling "juce::String allocates"; CODING one object one translation unit.
- 22: `state.createCopy()` of the whole config to read three values. MANIFESTO E "No copy".
- 23: EVE's overlay uses a registry callback where `MessageOverlay::registerParameters` exists (END uses it). MANIFESTO established pattern.
- 24: menu icon ordinals repeat `TabView::AreaOption` as magic numbers. MANIFESTO E, S.
- 25: `ENDView::valueTreePropertyChanged` repeats `ConfigModel::getEventKey`. MANIFESTO S.
- 28: `getRowKey` is public with no outside caller. MANIFESTO E "Private by default".
- 29: `map::AnsiColour` has no reader after Step 12 was superseded. YAGNI.
- 31: `failNotInSet` texts read `value not in {X`. Malformed message.
- 34: a missing theme folder gives no error. MANIFESTO "Silent fail".
- 44: `defaultTables.isEmpty()` tested twice. Simpler form.

**Rejected:**
- 10: ARCHITECT ruled *"if the config invalid … show the error, load default value from binary"*; the whole-file fallback is that ruling.
- 18: the formatting builds a lookup key; it copies, allocates or changes none of the data read. The paint and message threads are not deadline threads.
- 20, 38: `forEachWord` takes `juce::StringRef` and does not split at `_` or `-`; ARCHITECT ruled *"NOT with juce::String"*. One responsibility, so MANIFESTO L:72 applies to the length.
- 21: the binary default validated at its owner is MANIFESTO D's permitted assert; jassert plus a positive branch is MANIFESTO's own CORRECT example (MANIFESTO.md:113-126).
- 26: the `GRAPHICS` parent check at `Main.cpp:86` resolves the shared `background` row.
- 27: MANIFESTO.md:113-126 pairs `jassert` with positive nesting as the CORRECT form.
- 33: the version gate replaces a file without a version by ARCHITECT's design; a failed disk write is not user input.
- 35: `ENAXClientExtensions::set` runs the rule at once (`jam_ENAXClientExtensions.h:37-43`); nothing stores the lambda.
- 36: no existing name fits the numeric-cell rule better; a new term needs ARCHITECT.
- 37: one responsibility (config tables to and from markdown); the placement of the file lane is PLAN Step 6.
- 40: `Format::getNumber` asserts and cannot report a failure; `std::from_chars` is the standard API.
- 41, 42, 43: Lean findings with no read that confirms a contract clause.
- 23 (rejected after the read): registry callbacks keyed by component id are plugin_bootstrap's established listener path (`kuassa_PluginEditor.cpp:341-349`, `:370-379`); `MessageOverlay::registerParameters` needs a `ParameterText` in the model (`jam_MessageOverlay.h:115-129`), which the settings lane does not write.
- 24 (rejected after the read): keying by `TabView::AreaOption` makes `ENDLookAndFeel` include `TabView`, while `TabView.h:10` includes `ENDLookAndFeel.h`. MANIFESTO E layer rule.

**Names added during resolution:** `getMergedValue` (local lambda, `ConfigModel.cpp:234`, family `get` + noun); `aliasType` (local, `jam_StyleVariDisplay.cpp`).

## BLESSED Alignment

- **B:** the binary default is the one owner of every row; the disk only overlays it.
- **L:** removal first: END's code table and getters, the css-variable dump, the duplicate load code, the constant `edgePadding`.
- **E:** errors are `juce::Result` text shown in an overlay; no silent skip and no assert on user input.
- **S (SSOT):** one load path for END and EVE; css is the one style source for EVE; one padding value; one key format at creation.
- **S (Stateless):** the error text travels in the data (settings root, component property); no object keeps a flag.
- **E (Encapsulation):** the reader formats keys; consumers read formatted ids. The settings lane formats its own tags.
- **D:** same file in, same tree out; formatting at creation only.

## Risks / Open Questions

None.
