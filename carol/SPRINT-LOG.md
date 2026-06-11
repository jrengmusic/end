# SPRINT-LOG

---

## Sprint 12: button::SVG + StyledGraphics Foundation, Scoped Colour Distributor ✅

**Date:** 2026-06-12
**Duration:** ~08:00

### Agents Participated
- COUNSELOR: orchestration, design discussion (StyledGraphics/Shape semantics, static Flex, kuassa button model, JUCE-native colourId rule, nested registry distributor), fact-checking (juce::Drawable capability matrix, TextButton/TabbedButtonBar ColourIds, jam::ID coverage), trivial fixes (jam::IDtype shadowing, Window background key, duplicate identifier)
- Engineer: font-key renames, SVG module move, Flex rewrite→stubs, jam::button::SVG, Bar rework, colour registry restructure
- Pathfinder: jam button/LAF/colour-consumer surveys
- Librarian: juce::Drawable vs custom StyledGraphics capability research (juce_SVGParser.cpp evidence)

### Files Modified (~30 total)

**JAM:**
- `jam_core/identifier/jam_identifier_svg.h` — font/font_size/font_family entries removed
- `jam_core/identifier/jam_identifier_appearance.h` — fontFamily ("font-family"), fontSize ("font-size") added
- `jam_core/jam_core.h` / `jam_core.cpp` — SVG includes removed (moved to jam_graphics)
- `jam_graphics/svg/jam_svg.h` / `.cpp` — moved from jam_core/xml (byte-verbatim)
- `jam_graphics/svg/jam_svg_button.h` / `.cpp` — moved from jam_core/xml
- `jam_graphics/svg/jam_svg_flex.h` / `.cpp` — REWRITE: instance class deleted; static-only struct — getStyledGraphics (parser STUB), paint (9-slice STUB), getName (implemented)
- `jam_graphics/styled_graphics/jam_styled_graphics.h` — new: StyledGraphics { Shape { path, stroke, colour, colourId, bounds }, shapes, width, height }; attributed_path/ deleted
- `jam_graphics/jam_graphics.h` / `.cpp` — SVG section before graphics utilities (declaration order), styled_graphics include
- `jam_gui/button/jam_button_svg.h` — new: jam::button::SVG : juce::Button — array<StyledGraphics, 8>, setGraphics, getStyledGraphics, getStateCount, paintButton → Custom::drawTabButton
- `jam_gui/button/jam_button_bar.h` / `.cpp` — Tab class DELETED; Orientation + ColourIds (backgroundColourId/outlineColourId/highlightColourId) on Bar; TabInfo holds mouse::Events<SVG>; lambda-wired click/drag-reorder/right-click; getBestTabLength(name, depth) on Bar
- `jam_gui/layout/jam_tabbed_component.h` / `.cpp` — createTabButton type + Orientation references updated
- `jam_gui/jam_gui.h` — jam_button_svg.h include
- `jam_gui/view/jam_view_content.h`, `jam_data_structures/view/jam_view_manager.cpp`, `jam_view_manager_panel.cpp`, `jam_markdown/mermaid/jam_mermaid_svg_parser.cpp` — IDref::font_size/font_family → fontSize/fontFamily
- `jam_markdown/jam_markdown.h` — jam_graphics dependency + include added

**END:**
- `Source/Identifier.h` — IDtype::code added; codeFamily/codeStyle/codeSize, family, indicator, inactiveText, inactiveBackground, selection, foreground, outline, background duplicates removed; caret, highlight, textOn, textOff, buttonOn added
- `Source/Main.cpp` — registerTypefaces reads ID::fontFamily/fontSize from IDtype::code node
- `Source/config/Config.cpp` — getFont→getDisplay(IDtype::code); ID::fontSize (Windows scale)
- `Source/config/lua/display.lua` — key renames: colours.{caret,text,highlight}, tab.{button,button_on,text_off,text_on,highlight}, window.background, overlay.{background,text}
- `Source/config/lua/whelmed.lua` — code_family/code_style/code_size block deleted
- `Source/lookAndFeel/LookAndFeel.h` / `.cpp` — colourIds → nested HashMap<node, HashMap<property, vector<int>>> distributor; scoped setColours; vtpc nested lookup; tab paint stubbed (loadGraphics/drawBarBackground/drawBarIndicator TODO); drawTabButton test fill via TextButton button/buttonOn ids; selectionColourId removed
- `Source/end/MessageOverlay.h` — Label::backgroundColourId/textColourId; font from overlay node (jam::IDtype::overlay)
- `Source/end/Window.cpp` — style dispatcher keyed/reads jam::ID::background (was jam::ID::colour)
- `Source/end/Tabs.cpp` — variable-length tab names retained; no Tab:: references

### Alignment Check
- [x] BLESSED principles followed (registry = SSOT distributor; Flex stateless; button owns its bank)
- [x] NAMES.md adhered (all new names ARCHITECT-gated: StyledGraphics, Shape, setGraphics, highlightColourId)
- [x] MANIFESTO.md principles applied

### Problems Solved
- Flex instance class was unreliable garbage (clone-filter XML at paint, Bucket dedup, bail-outs) → deleted; static parser/painter stubs await ARCHITECT handcode
- Tab button was a LAF-painted lookalike with 3× duplicated 8-state plumbing → jam::button::SVG IS a button (kuassa model); state plumbing collapsed to getState + getStateCount
- Colour collision: flat registry applied bare property names from every node (whelmed.background overwrote TabbedComponent background → transparent tab fill) → nested node-scoped distributor, collision impossible by construction
- juce::Drawable evaluated and rejected on evidence: no paint-time findColour, no collapse, no 9-slice, serif:id ignored
- Invisible window: Window style dispatcher read renamed lua key → jam::ID::background fix
- jam::IDtype shadowing (END IDtype hides jam's) → explicit jam::IDtype:: qualification

### Debts Paid
- None

### Debts Deferred
- None (side finding flagged in-session: registerTypefaces() defined but never called — awaiting ARCHITECT direction)

---

## Sprint 11: SVG Flex Layout System + LAF 1:1 Colour Chain ✅

**Date:** 2026-06-10
**Duration:** ~06:00

### Agents Participated
- COUNSELOR: orchestration, design discussion (Segment/Shape/Flex vocabulary, 1:1 chain, open vs fixed roles, String→Identifier keys, HashMap consolidation), fact-checking (jam SVG API, JUCE ColourIds, XmlElement iteration API)
- Engineer: code implementation (many delegations — jam SVG module, Flex, END LookAndFeel)
- Pathfinder: Phase 3 completion survey, jam SVG module API discovery

### Files Modified (14 total)

**JAM:**
- `jam_core/identifier/jam_identifier_svg.h` — stroke_width, stroke, topLeft, topRight, bottomLeft, bottomRight, outline, foreground entries added to IDENTIFIER_SVG X-macro.
- `jam_core/xml/jam_svg.h` — getElementPath (per-element path reader), parseStyle (generic CSS key-value from style attr), parseColour (hex decoder), Flex forward declaration. getEllipsePath/getCirclePath/getRectPath/getAllFoundPath doxygen updated to reflect delegation to getElementPath.
- `jam_core/xml/jam_svg.cpp` — parseStyle + parseColour implementations. getElementPath (path/rect/ellipse/circle dispatch). getRectPath/getEllipsePath/getCirclePath refactored to delegate to getElementPath. getStrokeWidth refactored onto parseStyle.
- `jam_core/xml/jam_svg_flex.h` — new file: SVG::Flex struct — Shape (path, PathStyle, colourId, colour, strokeWidth), Segment (vector\<Shape\>, bounds), Segments = HashMap\<Identifier, Segment\>, getName, getSegments, paint. Comprehensive doxygen.
- `jam_core/xml/jam_svg_flex.cpp` — new file: getName (serif:id/digit-strip), getSegments (collectShapes lambda walker — LAF groups via colourIds, self-styled via parseStyle+parseColour, bounds rect exclusion via getChildByName), paint (cornerTransform lambda, HashMap\<Identifier, AffineTransform\>, range-for over segments, findColour at paint time).
- `jam_core/jam_core.h` — jam_svg_flex.h include wired.
- `jam_core/jam_core.cpp` — jam_svg_flex.cpp unity include wired.
- `jam_gui/button/jam_button_bar.h` — Tab::ColourIds { backgroundColourId = 0x4200001 }, Bar::ColourIds { indicatorColourId = 0x4200100 }.

**END:**
- `Source/lookAndFeel/LookAndFeel.h` — barBackgroundColourId, frontBackgroundColourId, inactiveBackgroundColourId, frontTextColourId, inactiveTextColourId, tabOutlineColourId, indicatorColourId removed from ColourIds enum. Three Segments members → one flexGraphics HashMap\<Identifier, Segments\>. getTabFont() public accessor. Class + method doxygen rewritten (1:1 chain SSOT).
- `Source/lookAndFeel/LookAndFeel.cpp` — colourIds registry retargeted: background→TabbedComponent::backgroundColourId, outline→tabOutlineColourId, indicator→Bar::indicatorColourId, inactiveBackground→Tab::backgroundColourId, foreground→frontTextColourId, inactiveText→tabTextColourId. frontBackground entry deleted. loadGraphics populates flexGraphics via insert_or_assign. Draw virtuals lookup flexGraphics.at(key). drawTabButton text via TabbedButtonBar ids. vtpc narrowed (Array\<var\> relic deleted). getTabFont() SSOT extracted.
- `Source/end/Tabs.cpp` — updateTabBarVisibility: inline font construction replaced with LAF getTabFont() call.
- `Source/Identifier.h` — ID::frontBackground X-macro entry deleted.
- `Source/config/lua/display.lua` — front_background key + comment deleted from tab section. Outline comment rewritten. 1:1 chain note added.

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- SVG string members + cout diagnostic → parsed Segment/Shape cache (then → module-level jam::SVG::Flex)
- Array\<var\> event encoding relic in vtpc → plain property message dispatch
- Full recursive setColours on every vtpc → narrowed: colour key → setColours, SVG key → loadGraphics
- Hand-rolled SVG parsing (resolveGroupName, parseStrokeWidth, buildRolePath) → jam::SVG module APIs (getElementPath, parseStyle, parseColour)
- Inline Graphic struct + pointer-to-member dispatch → HashMap\<Identifier, Segment\> keyed by SVG group name
- Fixed outline/foreground/background role paths → dynamic Shape ops in document order, LAF-aware via colourIds registry
- Six duplicated END ColourIds → retargeted to JUCE TabbedComponent/TabbedButtonBar + jam::button component ids
- Font construction duplicated (Tabs.cpp + LookAndFeel.cpp) → getTabFont() SSOT
- Three separate Segments members → one flexGraphics HashMap
- String-keyed Segments → Identifier-keyed (hashed, interned)
- getRectPath bounds pollution (unions all child rects) → getChildByName single direct-child rect
- SegmentTransform struct + 5-entry array → cornerTransform lambda + HashMap\<Identifier, AffineTransform\>

### Debts Paid
- None

### Debts Deferred
- None

**Date:** 2026-06-10
**Duration:** ~08:00

### Agents Participated
- COUNSELOR: orchestration, design discussion, fact-finding, delegation
- Engineer: code implementation (many delegations)
- Pathfinder: codebase discovery (endless validation/message chain, jam::Map, Function::Map, config tree structure)
- Librarian: embedded Lua API research (lua_sethook, OP_SETFIELD decoding, chunk names, nil semantics, parseSVGPath, jam::SVG/XML)
- Auditor: mid-sprint validation (Map::Instance refactor)

### Files Modified (18 total)

**JAM:**
- `jam_core/map/jam_map.h` — Instance<T>::contains(value) added — O(1) key lookup via Map::getKey inversion.
- `jam_lua/jam_lua_types.h` — LineMap nested type (tag → property → line), LineMapBuilder (result + tableRegisters + pendingRegisters + flushRoot).
- `jam_lua/jam_lua_state.h` — getType(code, chunkName) overload (luaL_loadbufferx "@name" → real filenames in Lua errors). lineHook: LUA_MASKLINE hook decoding OP_NEWTABLE/OP_SETFIELD from savedpc[-1] via vendored internals, register-tracked key→line capture, LineMap* via lua_getextraspace. getLineMap/getLineMapBuilder.
- `jam_data_structures/lua/jam_lua_value_tree.h` — jam::lua::Validators (nested HashMap, tag → property → predicate). from() optional Validators* — registers type validators (int/double/string) during the same build walk. from() validation overload (validators + errors + lineMap) — rejects invalid values during parse, reports missing (nil/undefined) properties, line-numbered error lines via appendError tag+key lookup.
- `jam_data_structures/model/jam_model.h` — fromLua forwarding overloads for both validator modes.

**END:**
- `Source/end/Map.h` — config::File + config::Graphics relocated here as Map::Instance<T> CRTP (canonical location next to Boolean). New maps: GpuMode (auto/true/false), DropMode (space/newline), Position expanded to 5 values (top/bottom/left/right/center). TabOrientation removed — Position enum mirrors jam::button::Tab::Orientation 0–3.
- `Source/Main.h` — Context owners: config::File, config::Graphics, GpuMode, Position, DropMode. tabOrientationMap removed.
- `Source/Identifier.h` — loadMessage added to IDENTIFIER_CONFIG.
- `Source/config/Config.h` — Model API: load → loadFromPath, validators member, registerValidator (try_emplace/insert_or_assign — no bracket), graphicsCallbacks (Function::Map), loadMessage member + getLoadMessage(). Comprehensive doxygen rewrite.
- `Source/config/Config.cpp` — ctor CONTRACT: initialise → saveToPath → loadFromPath → startWatching. enumCheck<MapType> + getEnumValidator static resolvers (result returns, no if/else chain, no second map). loadFromPath: per-file getType with chunk name, flushRoot, validated fromLua, error accumulation, single atomic loadMessage + unconditional sendPropertyChangeMessage. buildGraphicsCallbacks: filename → sendPropertyChangeMessage dispatch built from config values per reload. fileChanged: lua → loadFromPath, svg → graphicsCallbacks direct lookup. All diagnostics removed.
- `Source/end/View.cpp` — messageOverlay enabled, ID::loadMessage → showMessage(config.getLoadMessage()) — message is event not state, no clearing.
- `Source/lookAndFeel/LookAndFeel.h` — drawBarIndicator + drawTabButton overrides, indicatorSegments + buttonSegments, parseTabBarSvg → parseSvg (generic).
- `Source/lookAndFeel/LookAndFeel.cpp` — parseSvg full implementation (role groups via serif:id/suffix-strip, bounds rects, parseSVGPath + addRectangle, style colour extraction). loadGraphics: all three SVGs → segment vectors. Three draw methods: scale-to-fit fill/stroke per segment.
- `Source/config/lua/graphics.lua` — tab_inactive/tab_active filenames set.
- `Source/config/svg/tab_inactive.svg`, `tab_active.svg` — wired through pipeline (BinaryData GLOB → seed → watch → parse → paint).

### Alignment Check
- [x] BLESSED principles followed (B: validators/lineMap/watcher owned by Model, hook lifetime scoped to getType. L: one walk builds tree + validators, one resolver. E: explicit CONTRACT ctor sequence, named registerValidator. S: BinaryData defaults are the single schema — validators derived from it. S: loadMessage is an event, no shadow state cycle. E: JAM knows var types only, END enriches with its Maps. D: same lua → same tree, same errors.)
- [x] NAMES.md adhered (loadFromPath/saveToPath verbs, getEnumValidator result-return resolver, Position/GpuMode/DropMode semantic nouns, no "node" anywhere)
- [x] MANIFESTO.md principles applied

### Problems Solved
- Lua values invalid but silently accepted — validators now built from BinaryData walk (type + enum), invalid values rejected before setValuesFrom, defaults survive.
- Nil/undefined lua values vanish silently (Lua spec: nil removes table key) — missing-property detection against registered validators.
- Same-value setProperty suppressed loadMessage notification — message moved to member + unconditional sendPropertyChangeMessage; message is an event, fires once per loadFromPath, always.
- Validation errors had no source location — LUA_MASKLINE hook decodes OP_SETFIELD from vendored Lua internals, nested LineMap (tag → key → line) eliminates same-key collisions across sections.
- Lua syntax errors showed `[string "..."]` — chunk name overload gives `display.lua:42:` format.
- `position = "center"` rejected by 2-value Position map — Position expanded to 5 values, single map for all positional values, TabOrientation eliminated.
- SVG hot-reload if/else chain — graphicsCallbacks Function::Map direct lookup, rebuilt per reload from live config values.
- idMap parallel-map garbage, flat validator collisions, isLuaValid post-build walk, enrichStringValidators second walk, registerStringEnumValidator template + 4-branch chain — all eliminated through ARCHITECT-led redesign: one walk, one validators map, one resolver.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 9: Tab Bar LookAndFeel Refactor + SVG Pipeline + Config Watcher ✅

**Date:** 2026-06-08
**Duration:** ~06:00

### Agents Participated
- COUNSELOR: orchestration, design discussion, plan, delegation
- Engineer: code implementation (multiple delegations)
- Pathfinder: codebase discovery (button::Bar, LookAndFeel, kuassa patterns, JUCE tab internals)
- Researcher: web research on 9-slice/3-slice rendering models (CSS border-image, Android NinePatch, macOS NSDrawThreePartImage/NinePartImage, Qt QSS, GTK CSS, JUCE capabilities)
- Auditor: mid-sprint validation

### Files Modified (24 total)

**JAM:**
- `jam_look_and_feel/jam_look_and_feel_custom.h` — renamed virtuals: drawButtonGroupTrack → drawBarBackground, drawButtonGroupSlidingIndicator → drawBarIndicator, drawTabButton signature unchanged. Added @brief doxygen per virtual.
- `jam_gui/button/jam_button_bar.h` — added Background nested class (LAF-aware, delegates to drawBarBackground), SlidingIndicator nested class (delegates to drawBarIndicator), background + animator members, snapIndicator/animateIndicator methods. Removed BehindFrontTabComp forward decl + unique_ptr member.
- `jam_gui/button/jam_button_bar.cpp` — removed BehindFrontTabComp class definition. Bar ctor: addAndMakeVisible(background + indicator), mouse-pass-through. addTab: indicator.toBehind(&background). setCurrentTabIndex: animateIndicator() after currentTabChanged. updateTabPositions: background.setBounds + snapIndicator at end. Renamed local `animator` → `desktopAnimator` (shadow fix). Removed behindFrontTab usage. Bar::paint now empty (background component paints itself).

**END:**
- `Source/Identifier.h` — IDENTIFIER_CONFIG: added graphics. IDENTIFIER_COMMON: added inactiveText, frontBackground, inactiveBackground, background (removed foreground — moved to DISPLAY). IDENTIFIER_DISPLAY: added tabBar, tabInactive, tabActive, tabBarSvg removed, outline + foreground added.
- `Source/config/Config.h` — File enum: added graphics. Model: added Watcher::Listener inheritance, fileChanged override, watcher member. Updated doxygen (8 sections, watcher ownership).
- `Source/config/Config.cpp` — File::map: added graphics entry. loadPath: creates graphics/ dir (replaced button/), starts watcher at end. Added fileChanged: .lua → load(), .svg → sendPropertyChangeMessage on matching graphics property.
- `Source/Main.h` — removed Watcher::Listener inheritance, fileChanged declaration, watcher member.
- `Source/Main.cpp` — removed watcher setup (addFolder/coalesceEvents/addListener), removed fileChanged method entirely. Application now config-only.
- `Source/lookAndFeel/LookAndFeel.h` — ColourIds: semantic names (barBackgroundColourId, frontBackgroundColourId, inactiveBackgroundColourId, frontTextColourId, inactiveTextColourId, tabOutlineColourId, indicatorColourId). Removed old IDs (tabBarBackgroundColourId, tabLineColourId, tabActiveColourId, tabIndicatorColourId). Added Segment struct, barSegments, parseTabBarSvg private. Removed loadTabBarSvg, draw3Slice, SlicePaths, buttonSlice/indicatorSlice. drawBarBackground + drawStretchableLayoutResizerBar overrides.
- `Source/lookAndFeel/LookAndFeel.cpp` — colourIds map: all semantic keys (ID::background, frontBackground, inactiveBackground, foreground, inactiveText, outline, indicator). drawBarBackground: empty skeleton (ARCHITECT fills). drawStretchableLayoutResizerBar: unchanged. setColours: unchanged. parseTabBarSvg: empty skeleton. Removed loadTabBarSvg, draw3Slice, SVG path cache, all 3-slice rendering code.
- `Source/config/lua/display.lua` — tab section: semantic keys (background, front_background, inactive_background, inactive_text, outline, indicator). Removed line, active, inactive (replaced), tab_bar_svg (moved to graphics.lua).
- `Source/config/lua/graphics.lua` — NEW: path = "gfx", tab_bar = "tab_bar.svg", tab_inactive = "", tab_active = "".
- `Source/config/lua/end.lua` — added graphics = require("graphics") after display.
- `Source/config/svg/tab_bar.svg` — tab bar SVG with 4 corner groups (top-left, top-right, bottom-left, bottom-right) + 1 flex group, each with outline/foreground role groups + bounds rect. Replaces old 3-slice tab.svg.

### Alignment Check
- [x] BLESSED principles followed (B: RAII ownership of Background/SlidingIndicator/Watcher. L: single-responsibility virtuals. E: semantic colour IDs, jam identifiers for all SVG parsing. S: config::Model SSOT for file watching. S: no shadow state. E: LAF decides how, Component decides what. D: same config → same render.)
- [x] NAMES.md adhered (Segment, Background, SlidingIndicator — semantic nouns. drawBarBackground/drawBarIndicator — verbs. No type suffixes.)
- [x] MANIFESTO.md principles applied

### Problems Solved
- Tab bar background was using tabLineColourId (a "line" colour) for full background fill — wrong semantics. Replaced with dedicated barBackgroundColourId from tab.background config.
- drawButtonGroupTrack/drawButtonGroupSlidingIndicator names were opaque — renamed to drawBarBackground/drawBarIndicator (role-based).
- BehindFrontTabComp (old JUCE V2 pattern) removed — replaced by Background component that delegates to LAF.
- Application::fileChanged coupled Application to LookAndFeel — moved watcher to config::Model, SVG changes routed through sendPropertyChangeMessage on ValueTree.
- button/ subfolder replaced by graphics/ (config contract: config::Model::loadPath creates it).
- 3-slice SVG approach abandoned (wrong model for tab bar that needs both horizontal + vertical orientation). Replaced with 9-slice corner+flex model.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 8: Phase 3 Pane Splits + Navigation + PaneManager Cleanup ✅

**Date:** 2026-06-07
**Duration:** 04:00

### Agents Participated
- COUNSELOR: plan, orchestration, bug diagnosis (focusedPane sync, resizer bar bounds, orphan scan)
- Engineer: identifier additions, SplitDirection map, PaneManager refactor, Panes/View/Tabs implementation, LookAndFeel override
- Pathfinder: codebase discovery (pane/split patterns, PaneManager API, endless tab bar height pattern)
- Auditor: (inline validation during Engineer delegation)

### Files Modified (16 total)

**JAM Framework:**
- `jam_core/identifier/jam_identifier_layout.h:92-98` — added panes, pane, direction, ratio, vertical, horizontal to IDENTIFIER_LAYOUT
- `jam_data_structures/map_instance/jam_map_split_direction.h` — NEW: SplitDirection Map::Instance (vertical/horizontal enum)
- `jam_data_structures/jam_data_structures.h:46` — registered jam_map_split_direction include
- `jam_gui/layout/jam_pane_manager.h:167-172` — NeededBar struct (node + isVertical + bounds), neededBars always tracks all required bars (fixes orphan scan), new bars get setBounds on creation
- `jam_gui/layout/jam_pane_manager.cpp:6-10,23-68,121-165` — removed 5 file-local statics, API takes jam::UUID + Identifier direction, direction comparison via ID::vertical.toString()

**END Project:**
- `Source/Identifier.h:126` — added closePane to IDENTIFIER_KEYS
- `Source/end/Panes.h:39-50` — split/removePane take jam::UUID, added focusPane
- `Source/end/Panes.cpp:17,28-39,57-117` — addLeaf(uuid) direct, split Attachment after layout + toFront, removePane UUID, focusPane bounds-based navigation
- `Source/end/Tabs.h:7,43-44` — config/Config.h include, tabFontRatio constant, updateTabBarVisibility declaration
- `Source/end/Tabs.cpp:10,22-24,37,55-69` — tab bar depth 0 at init, updateTabBarVisibility from config font family+size
- `Source/end/View.cpp:124-183` — 7 action handlers: splitHorizontal, splitVertical, closePane, paneLeft/Right/Up/Down
- `Source/end/PaneView.h` — unchanged (focus mechanism preserved)
- `Source/lookAndFeel/LookAndFeel.h:59-62` — drawStretchableLayoutResizerBar declaration
- `Source/lookAndFeel/LookAndFeel.cpp:87-95` — drawStretchableLayoutResizerBar implementation (paneBarColourId/paneBarHighlightColourId)
- `Source/config/lua/keys.lua:48,82,85` — cmd+w -> close_pane, split shortcuts corrected

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- PaneManager file-local statics promoted to jam identifier system (SSOT)
- PaneManager String-based UUID/direction replaced with typed jam::UUID + Identifier
- Attachment ordering bug: Attachment created before layout caused premature valueTreeChildAdded, overwrote focusedPane to void. Fix: Attachment after layout, toFront after Attachment
- PaneManager orphan scan bug: neededBars only tracked NEW bars, causing orphan scan to remove valid existing bars on each layout pass (odd/even bar visibility). Fix: neededBars tracks all required bars
- PaneManager new bar bounds: bars created during layout had zero bounds until next layout pass. Fix: NeededBar carries bounds, setBounds on creation
- Close hierarchy: cmd+w went straight to close_tab. Now close_pane (pane>tab>quit)
- Split shortcuts inverted: \ was horizontal, - was vertical. Corrected
- Tab bar visible with 1 tab. Now hidden when <=1, shown from config font metrics (endless pattern)

### Debts Paid
None

### Debts Deferred
None

---

## Sprint 7: Hierarchical Attachment + Focus Sync + Model Lean ✅

**Date:** 2026-06-07
**Duration:** Full session

### Agents Participated
- COUNSELOR: Sprint lead — hierarchical attachment design, Component/Attachment API, focus chain architecture, UUID design, AudioModel static attach migration
- Engineer: AnyMap::remove, Attachment refactors (3-arg→2-arg→template 1-arg), Component Model& storage, ComponentWithID removal, Tabs/Panes/PaneView rework, jam::UUID creation, Registry diagnostics removal, static attach migration to AudioModel, Identifier cleanup
- Pathfinder: Static attach consumer mapping, Tabs tree management survey
- Librarian: UUID cross-thread usage analysis in endless

### Files Modified — JAM

- `jam_core/utilities/jam_any_map.h` — added `remove(key)` + String overload for atomics cleanup on detach
- `jam_core/misc/jam_uuid.h` — NEW: 64-bit trivially copyable UUID, std::abs positive, atomic-compatible
- `jam_core/jam_core.h` — registered jam_uuid.h include
- `jam_data_structures/model/jam_model.h` — Component: Model& member (public), Property::defaultValue int→juce::var, removed onAttachment, removed virtual on getValueTree. Attachment: template ctor (1-arg, parent auto-discovery), stores Component& only, no Model&/VT members, no getState/registerAtomics/attachRecursively/detachRecursively. Removed ComponentWithID template entirely.
- `jam_data_structures/value_tree/jam_value_tree_utils.cpp` — Component ctor definitions, Attachment destructor (uses component.getValueTree()), removed old Attachment ctors/methods
- `jam_data_structures/model/jam_audio_model.h` — received static attach methods (getRoot, getParent, attach×3, attachChild)
- `jam_data_structures/model/jam_audio_model.cpp` — received static attach implementations
- `jam_gui/view/jam_view_panel.h:43` — Model::attach → AudioModel::attach

### Files Modified — END

- `Source/end/View.h` — removed private Model& (inherited), added valueTreeChildAdded override, model/config as VT members, Tabs init via ctor list
- `Source/end/View.cpp` — ARCHITECT's focus implementation: listens model tree, valueTreePropertyChanged syncs focused_pane on PANE focus change, valueTreeChildAdded syncs on new tab creation
- `Source/end/Tabs.h` — Model::Component type=IDtype::tabs, jam::Owner<Attachment>, removed model member
- `Source/end/Tabs.cpp` — addNewTab creates jam::UUID, Attachment 1-arg, Owner::add/remove, empty currentTabChanged
- `Source/end/Panes.h` — Model::Component (was ComponentWithID), jam::Owner<Attachment>, removed model member
- `Source/end/Panes.cpp` — jam::UUID, Model::Component base (no seeds), setName/setComponentID in body, Attachment 1-arg
- `Source/end/PaneView.h` — Model::Component (was ComponentWithID), jam::UUID param, focusGained/focusLost set PANE focus property, visibilityChanged toFront
- `Source/Identifier.h` — added IDtype::tabs, ID::focusedPane, ID::focus; removed ID::activeTab, ID::activePaneID (dead)
- `Source/action/Registry.cpp` — removed all jam::debug::Log::write diagnostics

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Hierarchical parent→child attachment: each parent owns Attachments for its children (View→Tabs→Panes→PaneView), RAII lifecycle
- Parent auto-discovery: Attachment walks getParentComponent() chain, finds nearest Model::Component, no explicit parent parameter
- Focus chain: PaneView::focusGained sets PANE.focus=1, View listens model tree and syncs VIEW.focused_pane; valueTreeChildAdded handles new tab creation
- Model& plumbing eliminated: Component stores Model& directly, derived classes inherit it
- ComponentWithID CRTP removed: derived classes call setName/setComponentID directly
- Static attach methods moved to AudioModel (audio plugin path only, not END)
- jam::UUID replaces juce::Uuid: 64-bit int64_t, trivially copyable, atomic-compatible, no string allocation for identity

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 6: Tab UX + Model architecture ✅

**Date:** 2026-06-07
**Duration:** Full session

### Agents Participated
- COUNSELOR: Sprint lead — tab system rename design, Model/Attachment architecture, PARAM removal, ValueTree absorption, orientation wiring
- Engineer: button::Bar/Tab rename, Owner<TabInfo> migration, drag-reorder, orientation support, Model cleanup (getRootTree/UniqueNodeMap/attach deletion), Attachment recursive graft, PARAM scrapping, accessor rework, CodeView cleanup, dangling declaration removal
- Pathfinder: Tab system survey (END, JAM, kuassa, JUCE), consumer analysis
- Librarian: JUCE ComponentAnimator API, TabBarButton drag patterns, PopupMenu right-click

### Files Modified — JAM (4 new, 10 modified, 4 deleted)

**New (4):**
- `jam_gui/button/jam_button_bar.h` — button::Bar + button::Tab (renamed from TabbedButtonBar/TabBarButton), Owner<TabInfo>, drag-reorder, orientation
- `jam_gui/button/jam_button_bar.cpp` — implementations: drag threshold 5px, moveTab with onTabMoved, axis-aware layout

**Deleted (4):**
- `jam_gui/layout/jam_tabbed_button_bar.h` — replaced by button/jam_button_bar.h
- `jam_gui/layout/jam_tabbed_button_bar.cpp` — replaced by button/jam_button_bar.cpp
- `jam_gui/button/jam_button_group.h` — dead code, replaced by button::Bar
- `jam_gui/button/jam_button_tab.h` — dead code, replaced by button::Tab

**Modified (10):**
- `jam_gui/layout/jam_tabbed_component.h` — uses button::Bar/Tab, setOrientation(int), getOrientation()
- `jam_gui/layout/jam_tabbed_component.cpp` — orientation switch in paint/resized, button::Tab references
- `jam_gui/jam_gui.h` — updated includes: button_bar.h replaces tabbed_button_bar.h, group.h, tab.h
- `jam_gui/jam_gui.cpp` — updated TU includes
- `jam_gui/code_editor/jam_code_view.h` — removed ValueTree::Component/Listener inheritance, stateAttachment, state, properties array, VTPC per SPEC §2.1
- `jam_gui/code_editor/jam_code_view.cpp` — simplified ctor (no VT), deleted VTPC impl, deleted properties array
- `jam_data_structures/model/jam_model.h` — absorbed ValueTree (Component, ComponentWithID, Attachment, all statics); deleted PARAM/addParameter/getValue/setValue/storeValue/loadValue/getRawParameterValue/attach/getRootTree/UniqueNodeMap; added addProperties (isInt64), recursive Attachment, nested AnyMap accessors
- `jam_data_structures/model/jam_model.cpp` — deleted PARAM definition, all old accessors, attach implementations; simplified addTextParameter; dual-tree applyFunctionRecursively in setValuesFrom
- `jam_data_structures/value_tree/jam_value_tree_utils.cpp` — recursive graft/ungraft implementations, registerAtomics delegates to addProperties, dual-tree overload
- `jam_data_structures/value_tree/jam_value_tree_json.cpp` — removed stale includes

### Files Modified — END (7 modified)

- `Source/end/View.h` — takes Model&, owns unique_ptr<Attachment>, applyTabOrientation renamed setTabOrientation
- `Source/end/View.cpp` — recursive attachment in ctor body after addAndMakeVisible, TabOrientation bimap, orientation from config
- `Source/end/Map.h` — TabOrientation bimap (Tab::Orientation enum values), typed get() returning Orientation
- `Source/end/Tabs.h` — jam::Model::Component (was ValueTree::Component)
- `Source/end/PaneView.h` — jam::Model::ComponentWithID (was ValueTree::ComponentWithID)
- `Source/Main.h` — deleted viewAttachment member
- `Source/Main.cpp` — View(model), no viewAttachment
- `Source/end/Window.cpp` — jam::Model::toColour (was ValueTree::toColour)
- `Source/lookAndFeel/LookAndFeel.cpp` — jam::Model::applyFunctionRecursively, jam::Model::toColour
- `Source/config/Config.cpp` — addProperties (was addParametersFromProperties), jam::Model::fromLua
- `Source/config/lua/display.lua` — tab.position renamed to tab.orientation
- `Source/Identifier.h` — added orientation identifier

### Alignment Check
- [x] BLESSED principles followed (SSOT: PARAM shadow eliminated, Bound: recursive Attachment RAII, Encapsulation: Model stops exposing state)
- [x] NAMES.md adhered (addProperties, setTabOrientation, button::Bar/Tab)
- [x] MANIFESTO.md principles applied

### Problems Solved
- Tab system naming: TabbedButtonBar/TabBarButton renamed to button::Bar/Tab, button::Group deleted
- PARAM shadow state: active_tab existed as both node property AND PARAM child — PARAM pattern scrapped entirely
- Model::attach exposed raw state tree — deleted, Attachment is the only graft path
- CodeView had ValueTree infrastructure violating SPEC §2.1 — removed, pure juce::Component
- registerAtomics created PARAM children — now delegates to addProperties (direct property binding)
- ValueTree struct absorbed into Model — single namespace for all state infrastructure
- Recursive Attachment: one construction grafts entire Component hierarchy + registers atomics at every level

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 5: TabbedButtonBar fork + bit_cast + Union + config pipeline ✅

**Date:** 2026-06-06
**Duration:** Full session

### Agents Participated
- COUNSELOR: Sprint lead — TabbedButtonBar analysis, bit_cast/Union design, config pipeline redesign, Window style collapse, colour format migration
- Engineer: TabbedButtonBar+TabBarButton fork, TabbedComponent fork, bit_cast, Union, CellFifo replacement, toInt replacement, lua::ValueTree, Window rewrites, LookAndFeel rewrites, Tabs simplification, View ValueTree::Component, colour conversion
- Pathfinder: button::Group internals, TabbedComponent wiring, VTPC flow
- Librarian: JUCE TabbedButtonBar/TabBarButton/TabbedComponent deep dive, juce::var internals, juce::Colour(uint32)
- Researcher: C++17 bit_cast implementation, packed value type patterns, existing library survey

### Files Modified — JAM (8 new, 7 modified)

**New (8):**
- `jam_core/utilities/jam_bit_cast.h` — constexpr __builtin_bit_cast polyfill
- `jam_core/utilities/jam_union.h` — variadic packed transport (uint32/uint64 backing, pack/unpack, structured bindings)
- `jam_gui/layout/jam_tabbed_button_bar.h` — TabBarButton + TabbedButtonBar fork from JUCE
- `jam_gui/layout/jam_tabbed_button_bar.cpp` — layout algorithm verbatim, Custom LAF paint
- `jam_data_structures/lua/jam_lua_xml.h` — moved from jam_lua, getBody rename, Text::numeric, quoted()
- `jam_data_structures/lua/jam_lua_value_tree.h` — moved from jam_lua, typed vars (int64/double), fromValueTree alias

**Modified (7):**
- `jam_core/utilities/jam_toInt.h` — bit_cast replaces C-style union type-pun, removes UB
- `jam_core/jam_core.h` — registered bit_cast, union includes
- `jam_gui/layout/jam_tabbed_component.h` — verbatim JUCE fork (content management, ButtonBar subclass)
- `jam_gui/layout/jam_tabbed_component.cpp` — changeCallback, clearTabs, addTab with content component
- `jam_gui/jam_gui.h` — registered jam_tabbed_button_bar.h
- `jam_gui/button/jam_button_group.h` — static_cast<Custom&> replaces dynamic_cast<Theme*>
- `jam_look_and_feel/jam_look_and_feel_custom.h` — drawButtonGroupTrack, drawButtonGroupSlidingIndicator virtuals
- `jam_terminal/transport/jam_cell_fifo.h` — packHeader/unpackHeader → Union<int32_t, uint8_t>
- `jam_data_structures/value_tree/jam_value_tree.h` — fromLua, toInt, toColour utilities
- `jam_data_structures/jam_data_structures.h` — jam_lua dependency, lua/ includes
- `jam_lua/jam_lua.h` — removed xml/value_tree includes (moved to jam_data_structures)

### Files Modified — END (4 new/rewritten, 8 modified)

**Modified (8):**
- `Source/end/Tabs.h` + `.cpp` — simplified: TabbedComponent manages content, no Owner<Panes>, removal selects next tab
- `Source/end/View.h` + `.cpp` — inherits ValueTree::Component (IDtype::view), owns tabsAttachment
- `Source/end/Window.h` + `.cpp` — removed ValueTree::Component, collapsed to setStyle(property) + registerStyleParameters, inline lambdas
- `Source/lookAndFeel/LookAndFeel.h` + `.cpp` — drawTabButton toggle visual, drawButtonGroupTrack, drawButtonGroupSlidingIndicator, fromRGBA removed, toColour
- `Source/Main.h` + `.cpp` — windowAttachment removed, viewAttachment added
- `Source/Identifier.h` — added size, view identifiers
- `Source/config/Config.cpp` — lua::ValueTree::from replaces Xml::from + fromXml
- `Source/config/lua/display.lua` — size={w,h} packed, all colours 0xAARRGGBB
- `Source/config/lua/whelmed.lua` — all colours 0xAARRGGBB
- `Source/config/lua/end.lua` — format comment updated

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered (node→state, getBody, blacklisted words)
- [x] MANIFESTO.md principles applied (SSOT for pack/unpack, Encapsulation for ValueTree ownership)

### Problems Solved
- Tab layout: FlexBox equal-width replaced with JUCE per-button variable width + proportional scaling
- Tab removal: proper index tracking via JUCE's removeTab + setCurrentTabIndex post-removal
- Tab painting: button::Group dynamic_cast<Theme*> → static_cast<Custom&>
- Colour pipeline: eliminated string→hex roundtrip (lua stores 0xAARRGGBB int, var(int64), direct Colour construction)
- Window size atomicity: single CSV property, single VTPC
- toInt UB: C-style union type-pun replaced with bit_cast
- CellFifo ad-hoc memcpy replaced with Union<int32_t, uint8_t>
- Config typed vars: jam::lua::ValueTree::from stores int64/double instead of strings
- Window style dispatch: three layers collapsed to two (registerStyleParameters + setStyle)
- Attachment ownership: View owns tabsAttachment, Window no longer ValueTree::Component

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 4: Phase 3 — Tabs + Panes + action::Registry

**Date:** 2026-06-06
**Duration:** Full session

### Agents Participated
- COUNSELOR: Sprint lead, design discussion (Registry architecture, LookAndFeel CRTP pattern, Owner API usage, tab lifecycle), direct edits on Registry/View/Tabs/LookAndFeel
- Engineer: PaneView stub, Panes container, Tabs component, Registry initial impl, View rewrite, LookAndFeel base change + drawTabButton, jam::LookAndFeel::Custom + Methods
- Pathfinder: Codebase survey (TabbedComponent, PaneManager, Owner, Function::Map APIs)
- Librarian: juce::KeyPress::createFromDescription API, juce::Component::getLookAndFeel() internals, JUCE LookAndFeelMethods pattern
- Researcher: LookAndFeelMethods patterns (CRTP, static registration, intermediate base, component-side virtual)

### Files Modified — JAM (1 new, 3 modified)

**New (1):**
- `jam_look_and_feel/jam_look_and_feel_custom.h` — Custom base (LookAndFeel_V4 + custom virtuals), Methods<Derived> CRTP with static_cast get()

**Modified (3):**
- `jam_look_and_feel/jam_look_and_feel.h` — include jam_look_and_feel_custom.h before Theme
- `jam_gui/button/jam_button_tab.h` — paintButton: static_cast<Custom&> replaces dynamic_cast<Theme*>
- `jam_data_structures/value_tree/jam_value_tree.h` — member renamed node→state

### Files Modified — END (7 new, 7 modified)

**New (7):**
- `Source/end/PaneView.h` — stub: juce::Component + ValueTree::ComponentWithID<PaneView>, UUID ctor
- `Source/end/Panes.h` — per-tab pane container: PaneManager + Owner<PaneView> + Owner<PaneResizerBar>
- `Source/end/Panes.cpp` — split, removePane, layout delegation
- `Source/end/Tabs.h` — TabbedComponent + ValueTree::Component, owns Owner<Panes>
- `Source/end/Tabs.cpp` — addNewTab (counter-named), removeCurrentTab (quit on last), currentTabChanged (state + visibility)
- `Source/action/Registry.h` — std::hash<KeyPress> injection, prefix key state machine, config listener
- `Source/action/Registry.cpp` — buildKeyMap from config KEYS (createFromDescription), run via Function::Map

**Modified (7):**
- `Source/end/View.h` — KeyListener + ValueTree::Listener, owns Tabs + Registry
- `Source/end/View.cpp` — registers actions via registry.actions.add([this]...), setSize from config, first tab
- `Source/end/Map.h` — removed Modifier/KeyName bimaps (replaced by juce::KeyPress::createFromDescription)
- `Source/Main.h` — tabsAttachment member, removed Modifier/KeyName contexts
- `Source/Main.cpp` — tabsAttachment wiring
- `Source/lookAndFeel/LookAndFeel.h` — inherits jam::LookAndFeel::Methods<LookAndFeel>, drawTabButton override
- `Source/lookAndFeel/LookAndFeel.cpp` — drawTabButton (rounded rect + text), setColours fixed: applyFunctionRecursively recursive walk

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied
- [ ] JRENG-CODING-STANDARD.md — multiple violations corrected mid-sprint (bail-out guards, hand-rolled parsers, manual booleans, forward declarations, stored pointer shadow state)

### Problems Solved
- LookAndFeel custom virtuals without dynamic_cast: jam::LookAndFeel::Custom + Methods<Derived> CRTP pattern — static_cast, zero runtime cost
- setColours broken (searched one level deep): replaced with applyFunctionRecursively recursive walk
- fromRGBA channel rotation was no-op: ARCHITECT fixed rotation order
- Boolean config parsing: end::Boolean::get() bimap replaces broken static_cast<int> on string vars
- Key binding parsing: juce::KeyPress::createFromDescription replaces hand-rolled parseKeyBinding
- Registry prefix key state machine: Timer IS the state, no manual isPrefixActive boolean
- Tab removal crash: reordered removeTab before tabPanes.remove (currentTabChanged fires while tabPanes intact)

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 3: Phase 3 — Config Infrastructure + Listener-Driven Window

**Date:** 2026-06-05 — 2026-06-06
**Duration:** Full session (2 days, context compaction mid-session)

### Agents Participated
- COUNSELOR: Sprint lead, design discussion (config overlay, styleParameters pattern, fromString refactor), fromRGBA bug diagnosis, direct verification of all engineer output
- Engineer: BackgroundBlur Backend expansion (jam), jam::Model::setValuesFrom, jam::Window setGlass 3-arg, end::Window + styleParameters, config::Model redesign, LookAndFeel listener, Identifier.h IDENTIFIER_BACKEND, lua config renames, call site updates (ModalWindow, GlassComponent, Dialog, Theme)
- Pathfinder: Codebase survey (kuassa glass machinery, ProcessorChain::parameters pattern, jam::Function::Map contract, config tree structure)

### Files Modified — JAM (2 new, 1 deleted, 25 modified)

**New (2):**
- `jam_core/identifier/jam_identifier_window.h` — X-macro IDENTIFIER_WINDOW (11 keys: mac, win, blurStyle, 4 macOS backends, 4 Windows backends)
- `jam_core/identifier/jam_identifier_terminal.h` — relocated from jam_terminal/identifier/

**Deleted (1):**
- `jam_terminal/identifier/jam_identifier_terminal.h` — moved to jam_core

**Modified (25):**
- `jam_style/background_blur/jam_background_blur.h` — 8-value Backend enum, shouldTintComponent, fromString (table-driven unordered_map + IDref), isGlassFXAvailable/isMicaAvailable/isAcrylic11Available, new Windows DWM constants
- `jam_style/background_blur/jam_background_blur.mm` — macOS platform: 4-case switch in enable(), applyBackgroundBlur, applyVisualFX, applyGlassFX (NSGlassEffectView), disable() with NSGlassEffectView cleanup
- `jam_style/background_blur/jam_background_blur.cpp` — Windows platform: 4-case switch in enable(), applyBlurBehind, applyAcrylic10, applyAcrylic11, applyMica, getWindowsBuildNumber delegation
- `jam_core/utilities/jam_platform.h` — extracted getWindowsBuildNumber(DWORD, cached), isWindows10 uses it
- `jam_core/identifier/jam_identifier.h` — includes jam_identifier_window.h, MAKE_VIEW includes IDENTIFIER_WINDOW
- `jam_data_structures/model/jam_model.h` — new public setValuesFrom(ValueTree), private overlay(ValueTree&, const ValueTree&)
- `jam_data_structures/model/jam_model.cpp` — setValuesFrom: isEquivalentTo early-out, per-property diff overlay. Removed debug log blocks
- `jam_gui/window/jam_window.h` — setGlass(Colour, float, Backend) 3-arg, setShowWindowButtons(bool), members: tintColour, blurRadius, glassBackend
- `jam_gui/window/jam_window.cpp` — setGlass shouldTintComponent branch, parentHierarchyChanged/visibilityChanged/handleAsyncUpdate pass stored Backend
- `jam_gui/window/jam_modal_window.h` — Backend as 6th ctor param
- `jam_gui/window/jam_modal_window.cpp` — setupWindow forwards Backend to setGlass
- `jam_gui/button/jam_button_dialog.h` — Dialog::show passes platform-default Backend
- `jam_gui/window/jam_glass_component.cpp` — handleAsyncUpdate passes platform-default Backend
- `jam_look_and_feel/theme/jam_look_and_feel_theme.cpp` — preparePopupMenuWindow passes platform-default Backend
- `jam_data_structures/value_tree/jam_parameter.h` — minor
- `jam_data_structures/value_tree/jam_parameter_text.h` — minor
- `jam_data_structures/value_tree/jam_value_tree.h` — minor
- `jam_lua/jam_lua_function.h` — minor
- `jam_lua/jam_lua_object.h` — minor
- `jam_lua/jam_lua_stack.h` — minor
- `jam_lua/jam_lua_state.h` — minor
- `jam_lua/jam_lua_types.h` — minor
- `jam_lua/jam_lua_xml.h` — minor
- `jam_terminal/jam_terminal.h` — include path update for relocated identifier
- `jam_terminal/tty/jam_tty.h` — minor
- `jam_terminal/tty/jam_tty.cpp` — minor
- `jam_terminal/video/jam_video.h` — minor

### Files Modified — END (2 new, 7 deleted, 11 modified)

**New (2):**
- `Source/endWindow.h` — end::Window : jam::Window + ValueTree::Listener, styleParameters (jam::Function::Map), registerStyleParameters/setStyle/applyStyleFor
- `Source/endWindow.cpp` — 8 styleParameter registrations (colour, blurRadius, alwaysOnTop, buttons, width, height, mac, win), ProcessorChain::parameters pattern verbatim

**Deleted (7):**
- `Source/config/lua/default_actions.lua` — renamed to actions.lua
- `Source/config/lua/default_display.lua` — renamed to display.lua
- `Source/config/lua/default_end.lua` — renamed to end.lua
- `Source/config/lua/default_keys.lua` — renamed to keys.lua
- `Source/config/lua/default_nexus.lua` — renamed to nexus.lua
- `Source/config/lua/default_popups.lua` — renamed to popups.lua
- `Source/config/lua/default_whelmed.lua` — renamed to whelmed.lua

**Modified (11):**
- `Source/Identifier.h` — IDENTIFIER_BACKEND X-macro (11 keys), integrated into END_MAKE_VIEW
- `Source/config/Config.h` — load(File, String&) overlay signature, loadPath returns StringArray
- `Source/config/Config.cpp` — build() seeds from BinaryData, load() uses setValuesFrom, loadPath() accumulates errors
- `Source/Main.h` — includes endWindow.h, window member std::unique_ptr<end::Window>
- `Source/Main.cpp` — initialise: Window reads own style, fileChanged loads single file only
- `Source/lookAndFeel/LookAndFeel.h` — setColours private, fromRGBA public static, config member with addListener/removeListener
- `Source/lookAndFeel/LookAndFeel.cpp` — fromRGBA channel rotation fix (RRGGBBAA → ARGB), setColours iterates colourIds map
- `Source/EndView.h` — simplified (transparent content)
- `Source/EndView.cpp` — paint() empty
- `SPEC.md` — blur_style config documentation
- `END.ode` — version bump

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md enforced (no bail-out guards, positive nesting, no captures in lambdas following ProcessorChain pattern)

### Problems Solved
- Config hot-reload was destructive (removeChild + appendChild) — redesigned to overlay-in-place via jam::Model::setValuesFrom with per-property diff
- Listener disconnection on tree reassignment — solved by never reassigning state, only mutating properties
- Duplicate children on reload — solved by property-only mutation (no appendChild in load path)
- "Fire-once glass" — end::Window now a ValueTree::Listener, reacts to every config change
- fromString if/else chain for BackgroundBlur::Backend — replaced with table-driven unordered_map + jam_identifier_window.h X-macro
- fromRGBA channel rotation bug — was a no-op (identity function), fixed to correctly rotate RRGGBBAA → ARGB
- Rvalue reference mismatch in Function::Map::get<ValueTree> — fixed with std::move on lvalue args
- const qualifier loss with forwarding references — fixed by dropping const on lambda params and local vars

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 2: Phase 2 — jam_gui Tab System Rewrite + PaneManager Fix

**Date:** 2026-06-05
**Duration:** Full session

### Agents Participated
- COUNSELOR: Sprint lead, plan writing, audit processing, direct fixes
- Engineer: Steps 1-7 implementation, audit finding fixes (F1-F17)
- Auditor: Final audit (24 findings)
- Pathfinder: Codebase survey (jam_gui, kuassa button::Group)

### Files Modified (18 total)

**Deleted (6):**
- `jam_gui/layout/jam_tab_bar_button.h` — old TabBarButton removed
- `jam_gui/layout/jam_tab_bar_button.cpp` — old TabBarButton impl removed
- `jam_gui/layout/jam_tabbed_button_bar.h` — old TabbedButtonBar removed
- `jam_gui/layout/jam_tabbed_button_bar.cpp` — old TabbedButtonBar impl removed
- `jam_gui/layout/jam_tabbed_component.h` — old TabbedComponent replaced
- `jam_gui/layout/jam_tabbed_component.cpp` — old TabbedComponent replaced

**Created (4):**
- `jam_gui/button/jam_button_options.h` — popup menu button, forked from kuassa
- `jam_gui/button/jam_button_tab.h` — TabButton with drag-reorder + inline rename
- `jam_gui/layout/jam_tabbed_component.h` — new TabbedComponent backed by button::Group
- `jam_gui/layout/jam_tabbed_component.cpp` — new TabbedComponent implementation

**Modified (8):**
- `jam_gui/jam_gui.h` — include order: button section before TabbedComponent, added Options + TabButton
- `jam_gui/jam_gui.cpp` — added TabbedComponent .cpp TU include
- `jam_gui/button/jam_button_group.h` — isFreeButton param, index API (getCurrentIndex/setCurrentIndex/removeButton/moveButton), right-click callback, buttons private with accessors (getButtonCount/getButtonAt/getButtonNames)
- `jam_gui/layout/jam_pane_resizer_bar.h` — Base class `juce::Component` → `mouse::Events<juce::Component>`. Removed forward declaration `class PaneManager;`, naked `PaneManager*` member, submodule include, mouseDown/mouseDrag overrides. `mouseDownPos` public (transient drag state for PaneManager callbacks)
- `jam_gui/layout/jam_pane_resizer_bar.cpp` — Removed submodule includes, mouseDown/mouseDrag implementations. Constructor drops `PaneManager*` parameter
- `jam_gui/layout/jam_pane_manager.h` — layout() non-static with resizer bar reconciliation (create/prune on layout, RAII-bound to split node), extracted storeBoundsProperties/findMatchingBar/layoutSplitNode helpers. Removed submodule include. Bar creation wires mouse::Events callbacks (onMouseDown/onMouseDrag), C++17 structured binding capture fix
- `jam_look_and_feel/theme/jam_look_and_feel_theme.h` — drawTabButton virtual, drawThreeSlice, setTabSVG/setTabSVGElementIds, 6 SVG slice members, tab ColourIds, extracted drawConnectedButtonBackground/drawStandaloneButtonBackground helpers
- `jam_look_and_feel/theme/jam_look_and_feel_theme.cpp` — 3-slice SVG infrastructure, drawTabButton fallback rendering, drawButtonGroupSlidingIndicator SVG path, alternative token cleanup (not/and/or), drawButtonBackground Lean decomposition

**Also modified (pre-existing fix):**
- `jam_data_structures/model/jam_model.cpp:333-349` — removed 4 redundant juce::String explicit instantiations (setValue x2, storeValue, loadValue)

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied
- [x] JRENG-CODING-STANDARD.md enforced (alternative tokens, bail-out guards, Lean decomposition, encapsulation)

### Problems Solved
- Old tab system (TabbedButtonBar/TabBarButton) replaced with button::Group-backed TabbedComponent
- PaneManager resizer bar lifecycle: bars now RAII-bound to split nodes via layout() reconciliation — orphan bars pruned, missing bars created automatically
- tabColours SSOT divergence on addTab/moveTab — colours now reorder with buttons
- getContentArea() bug — was returning tab strip instead of content area
- drawButtonBackground Lean violation (66→31 lines) — extracted connected/standalone helpers
- layoutNode Lean violation (103→~25 lines) — extracted storeBoundsProperties/findMatchingBar/layoutSplitNode
- Alternative token violations throughout jam_look_and_feel_theme.cpp — all !/ &&/ || replaced with not/and/or
- Group::buttons encapsulation — moved to private, added getButtonCount/getButtonAt/getButtonNames accessors
- Bail-out guards in setTabSVG — restructured to positive nesting
- Pre-existing jam_model.cpp warnings — removed redundant explicit instantiations after specializations
- PaneResizerBar naked `PaneManager*` pointer (BLESSED Bound) — base class changed to `mouse::Events<juce::Component>`, PaneManager wires callbacks
- PaneResizerBar circular dependency — broken by removing PaneManager dependency entirely
- Forward declaration `class PaneManager;` (JRENG forbidden) — eliminated
- Submodule include violations in resizer_bar.h/.cpp and pane_manager.h — all removed

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 1: Phase 1 — jam_terminal Extraction + SharedResource Redesign

**Date:** 2026-06-04
**Duration:** Full session

### Agents Participated
- COUNSELOR: Led extraction planning, PLAN authoring, identifier categorization, SharedResource redesign discussion, audit remediation. All decision gates with ARCHITECT.
- Pathfinder: Video event-firing site enumeration (33 sites, 28 distinct hooks across 7 files). Function::Map contract discovery.
- Engineer: Steps 1-9 implementation, SharedResource redesign, compile error fixes, pre-existing BLESSED violation fixes, doxygen remediation.
- Auditor: Step 1 audit (2 rounds), final sprint audit (38-item checklist, 12 findings).

### Files Modified — JAM (32 new, 11 modified)

**jam_core (2 modified)**
- `jam_core/utilities/jam_shared_resource.h` — Full redesign: split into `SharedResource` (non-template polymorphic entry base with virtual `operator==`/`hash()`) and `SharedResources<Derived>` (single-param CRTP container with `Owner<SharedResource>` storage). Methods use `const SharedResource&` params and `auto&` returns for CRTP deferred lookup.

**jam_graphics (11 modified, 3 new)**
- `jam_graphics/detail/jam_char.h` — Added 6 static methods (`fromCodepoint`, `width`, `isWordChar`, `isCombining`, `graphemeSegmentationStep`, `graphemeSegmentationInit`). Added private bit-layout constants (`BIT_WIDTH_SHIFTED`, `BIT_IS_COMBINING`, `BIT_IS_WORD_CHAR`, `BIT_GRAPHEME_SEG_PROPERTY`, `WIDTH_FIELD_BITS`, `GRAPHEME_SEG_PROPERTY_BITS`, `BOOL_FIELD_BITS`, `WIDTH_SHIFT`). Removed `GraphemeSegmentationResult` struct (moved to `Grapheme::SegmentationResult`).
- `jam_graphics/detail/jam_charset.cpp` — NEW. DEC line-drawing table + `Char::fromCodepoint` body.
- `jam_graphics/detail/jam_char_props.cpp` — NEW. charPropsT1/T2/T3 tables + `Char::charPropsFor`/`width`/`isWordChar`/`isCombining` bodies.
- `jam_graphics/detail/jam_grapheme_seg.cpp` — NEW. graphemeSegT1/T2/T3 tables + `Char::graphemeSegmentationStep` body.
- `jam_graphics/detail/jam_grapheme.h` — `Grapheme : SharedResources<Grapheme>`. Nested `Entry : SharedResource` (was top-level `GraphemeEntry`). Nested `SegmentationResult` (moved from `jam_char.h`). Added `@file` doxygen.
- `jam_graphics/detail/jam_stamp.h` — `Stamp : SharedResources<Stamp>`. Nested `Entry : SharedResource` (was top-level `StampEntry`). Widened `flags` `uint8_t`→`uint16_t`. Added `juce::Colour underline`, 3-bit underline style field, `OVERLINE`/`SUPERSCRIPT`/`SUBSCRIPT` bits. Removed old `UNDERLINE` single bit. Explicit constructor (virtual base breaks aggregate init).
- `jam_graphics/detail/jam_row.h` — `cells[]` → `chars[]` rename.
- `jam_graphics/jam_graphics.h` — Include order: `jam_grapheme.h` before `jam_char.h`.
- `jam_graphics/jam_graphics.cpp` — Aggregator: replaced `jam_char.cpp` with 3 split TUs.
- `jam_graphics/fonts/font/glyph/jam_glyph.cpp:31-32` — `0x01`/`0x02` → `Stamp::BOLD`/`Stamp::ITALIC`.
- `jam_graphics/fonts/font/glyph/jam_glyph_arrangement_shape.cpp:33-34` — Removed duplicate `sgrBold`/`sgrItalic` locals.
- `jam_graphics/fonts/font/glyph/jam_glyph_arrangement.h` — `uint8_t style` → `uint16_t style`.
- `jam_graphics/fonts/font/glyph/jam_glyph_graphics.h` — `const uint8_t* styles` → `const uint16_t* styles`.
- `jam_graphics/fonts/font/glyph/jam_glyph_graphics_cells.cpp` — `uint8_t` → `uint16_t` for style, `UNDERLINE_STYLE_MASK` test.
- `jam_graphics/fonts/typeface/jam_typeface.h` — `Typeface : SharedResource`. Added `hash()` override. `operator==` signature updated.
- `jam_graphics/fonts/typeface/jam_typeface_resources.h` — `SharedResources<TypefaceResources>`. Added `@file` doxygen.
- `jam_gui/code_editor/jam_caret_component.h:137` — Brace-init fix for widened `StampEntry`.

**jam_terminal (32 new)**
- `jam_terminal/jam_terminal.h` — Module header. Deps: `jam_core`, `jam_graphics`, `jam_data_structures`, `juce_core`, `juce_data_structures`, `juce_gui_basics`.
- `jam_terminal/jam_terminal.cpp` — Aggregator: 16 sub-TU includes.
- `jam_terminal/identifier/jam_identifier_terminal.h` — `IDENTIFIER_TERMINAL(X)` X-macro, 53 identifiers in `jam::terminal::ID::`.
- `jam_terminal/cell/jam_palette.h` — 256-slot mutable palette with `setPaletteColour`/`palette256At`.
- `jam_terminal/video/jam_screen.h` — `Screen : jam::Map::Instance<Screen>` CRTP.
- `jam_terminal/video/jam_winsize.h` — Terminal dimensions.
- `jam_terminal/video/jam_video.h` — Video base class (1743 lines). Ctor preserved: `Video(dims, events)`. 33+ fire sites renamed `id::` → `jam::terminal::ID::`.
- `jam_terminal/video/jam_video.cpp` — Core Video: constructor, flush, scroll, print, reset.
- `jam_terminal/video/jam_video_csi.cpp` — CSI dispatch + DECRQSS/DECRQM.
- `jam_terminal/video/jam_video_esc.cpp` — ESC dispatch.
- `jam_terminal/video/jam_video_sgr.cpp` — SGR dispatch. RFC-missing: underline styles/color, overline, super/subscript.
- `jam_terminal/video/jam_video_mode.cpp` — DEC mode handling.
- `jam_terminal/video/jam_video_edit.cpp` — Screen edit ops.
- `jam_terminal/video/jam_video_osc.cpp` — OSC dispatch. RFC-missing: OSC 4/10/11.
- `jam_terminal/video/jam_video_oscext.cpp` — OSC 8/133/1337.
- `jam_terminal/video/jam_video_dcs.cpp` — DCS/APC payload.
- `jam_terminal/video/jam_video_ops.cpp` — Cursor primitives, tab stops.
- `jam_terminal/parser/jam_csi.h` — CSI parameter accumulator.
- `jam_terminal/parser/jam_dispatch_table.h` — VT state machine dispatch table.
- `jam_terminal/parser/jam_parser.h` — Parser DFA.
- `jam_terminal/parser/jam_parser.cpp` — Process loop.
- `jam_terminal/parser/jam_parser_action.cpp` — Action dispatch.
- `jam_terminal/transport/jam_cell_fifo.h` — Lock-free cell transport. Bail-out guards refactored to positive nesting.
- `jam_terminal/keyboard/jam_keyboard.h` — Keyboard encoding.
- `jam_terminal/keyboard/jam_keyboard.cpp` — Windows keyboard encoder.
- `jam_terminal/tty/jam_tty.h` — TTY base class.
- `jam_terminal/tty/jam_tty.cpp` — TTY drain loop.
- `jam_terminal/tty/jam_unix_tty.h` — Unix PTY.
- `jam_terminal/tty/jam_unix_tty.cpp` — Unix PTY implementation.
- `jam_terminal/tty/jam_windows_tty.h` — Windows ConPTY. Decoupled from `lua::Engine` (ctor takes `const juce::File& conptyDir`).
- `jam_terminal/tty/jam_windows_tty.cpp` — Windows ConPTY implementation.
- `jam_terminal/protocol/jam_vt_vocabulary.h` — 166 named VT protocol constants.

**jam CMakeLists**
- `jam/CMakeLists.txt:25` — `jam_tui` → `jam_terminal`.

### Files Modified — END (4 total)
- `CMakeLists.txt:113` — `jam_tui` → `jam_terminal` in `JAM_MODULES`.
- `Source/Main.h` — `ENDApplication` with `public:` access specifier.
- `Source/Main.cpp` — `ENDApplication` fixes (semicolon, class name).
- `PLAN-jam-terminal-extraction.md` — Written and updated through session. Locked decisions, SharedResource redesign notes, `static_assert` drop.

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Extracted jam_terminal as reusable JUCE module (32 files, ~13K lines) — VT engine decoupled from END.
- Redesigned SharedResource into 2-type architecture: `SharedResource` (polymorphic entry base) + `SharedResources<Derived>` (single-param CRTP container). Entry types nest inside their owner. No top-level `GraphemeEntry`/`StampEntry`.
- Resolved C++ CRTP chicken-and-egg: method signatures use `SharedResource&`/`auto&`, bodies use deferred `Derived::Entry` lookup. `Owner<SharedResource>` polymorphic storage with virtual dispatch for hash/equality.
- Absorbed CharProps/Charset/CharPropsData into `jam::Char` static methods with TU-static lookup tables split into 3 files by table family.
- Widened `StampEntry::flags` to `uint16_t` with underline color, 3-bit underline style, overline/super/subscript.
- Consolidated 166 scattered VT protocol magic numbers into named `constexpr` constants in `jam_vt_vocabulary.h`.
- Decoupled WindowsTTY from `lua::Engine` (ctor takes `const juce::File& conptyDir`).
- Fixed 4 pre-existing BLESSED violations (magic numbers in jam_glyph.cpp, duplicate sgrBold/sgrItalic, CellFifo bail-out guards, Font::styleFlags investigation).

### Debts Paid
- None

### Debts Deferred
- None

### Known Residual (ARCHITECT-visible, not deferred)
- F7: Forward declaration `class Video;` in `jam_parser.h:78` — JRENG standard forbids forward decls; submodule zero-include rule prevents the alternative. Structural tension, PLAN-acknowledged.
- F8: `jam_screen.h:46` uses plain anonymous `enum` (Map::Instance convention) — JRENG requires `enum class`. Pre-existing pattern tension.
- F10: Multiple files exceed L (Lean) 300-line limit — data-dense/protocol-faithful files (Video.h 1743, WindowsTTY.cpp 1718, Keyboard.h 911, etc.).

---

## Sprint 0: END Rewrite — SPEC + ARCHITECTURE

**Date:** 2026-06-04
**Duration:** Full session

### Agents Participated
- COUNSELOR: Led SPEC writing through dialogue with ARCHITECT. Read all RFCs, old ARCHITECTURE.md, old DEBT.md, PLAN-WHELMED.md, jam source (PaneManager, Typeface, glyph pipeline, Markdown parser, SpectrumProcessor, TETRIS.md, identifier system, button::Group). Verified CONTRACT alignment.
- Pathfinder: Initial codebase survey (old END — 155 files, 46K LOC, 7 subsystems, 6 debts, last 20 commits all refactoring).
- Librarian: JUCE focus system research (getCurrentlyFocusedComponent, hasKeyboardFocus, focusOfChildComponentChanged, FocusChangeListener, focus on hide/remove/overlay — all from JUCE source with line citations).

### Files Modified (2 total)
- `SPEC.md` — Complete rewrite specification v0.0.1. 15 phases, full architecture, META-MVC, APVTS-analog, anti-mental-model, coordinate spaces, performance targets, incremental Model tracking per phase.
- `ARCHITECTURE.md` — Architectural contracts and mental model. Pre-implementation — no file details, contracts only.

### Alignment Check
- [x] BLESSED principles followed (each section references specific BLESSED pillars)
- [x] NAMES.md adhered (all new names discussed and approved: end::Model/View, config::Model, terminal::Controller/Model/View/Processor, PaneView, CodeView::Selection)
- [x] MANIFESTO.md principles applied (lock-free, unidirectional, SSOT, no shadow state, TETRIS contract for CodeView)

### Decisions Locked
1. **Priority order:** JUCE GUI app first, VT emulator second, niceties third.
2. **APVTS analog:** Spectrum analyzer pattern — reader pushes, message thread paints.
3. **META-MVC:** Recursive MVC layers, not three god objects.
4. **Plugin mapping:** Nexus=Host, Controller=AudioProcessor, Model=APVTS, View=PluginEditor, CellFifo=SpectrumFIFO, CodeModel=outputDB.
5. **Two independent trees:** config::Model (config constants) + end::Model (runtime state). Never mixed.
6. **Config SSOT:** Lua files on disk. config::Model is derived state. Init and reload are the same code path. No referTo.
7. **CodeView TETRIS contract:** Dumb widget, cell-space API, NOT jam::ValueTree::Component. Selection TYPE on TABS, selection COORDS transient in CodeView.
8. **Three coordinate spaces:** Video-grid, Document, Screen/pixel. jam::Cell::Point::fromPixel/toPixel is the ONLY converter. Manual arithmetic forbidden.
9. **Keyboard centralized at end::View** (KeyListener), mouse per-PaneView (JUCE delivery).
10. **activePaneID on TABS** authored by end::View (FocusChangeListener). Async delivery guarded.
11. **DisplayCallbacks eliminated.** Lua actions dispatch through action::Registry. Parameterized dispatch required.
12. **TTY moved to jam_terminal.** Constructor takes config path, no app coupling.
13. **Identifiers:** IDENTIFIER_TERMINAL X-macro in jam_terminal, expanded into jam::ID. Video event keys eliminated (virtual hooks replace string-keyed dispatch). END-specific identifiers in AppIdentifier.h.
14. **PaneManager resizer bar fix:** RAII-bound lifetime in Phase 2 (jam_gui).
15. **Namespace structure:** end:: (app), config:: (config), terminal:: (terminal), whelmed:: (markdown). Main.cpp not namespaced.
16. **Nexus ownership:** Nexus owns Controllers. Controllers survive View destruction (daemon mode). Minimal working Nexus in Phase 3.
17. **action::Registry functional in Phase 3** with prefix key state machine and keys.lua parsing.
18. **Whelmed two-pass pipeline:** jam::Markdown::Parser (proven, unchanged) → ParsedDocument IR → style resolution → jam::String with PROPORTIONAL Char → CodeModel → CodeView (edit) / TextView (read).
19. **Font/atlas GL-thread binding:** UNRESOLVED open seam. Must be designed before Phase 4.
20. **Anti-mental-model:** Explicit negation of terminal scanline model. Buffer<Row> is scratch, NOT document. CodeModel is SSOT. Width enters once at projection. Reader NEVER touches CodeModel.

### Problems Solved
- Identified root cause of old END's architectural rot: terminal-first mental model fighting JUCE.
- Identified font/atlas GL-thread race condition (use-after-free on reload) as principal blocker of old END.
- Identified PaneManager resizer bar lifecycle bug (RAII violation in remove()).
- Identified DisplayCallbacks as layer violation (config parser holding UI closures).
- Identified CodeView as jam::ValueTree::Component as layer violation (generic widget coupled to END state schema).

### Debts Paid
- None (Sprint 0 — specification only, no code)

### Debts Deferred
- None
