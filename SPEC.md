# SPEC.md — END: Ephemeral Nexus Display

**Version:** 0.0.1
**Date:** 2026-06-04 (original), updated 2026-11-01
**Identity revision:** END is an audio plugin host, end to end. Its default plugin is
EVE, the terminal, under the ENAX contract. ARCHITECT: *"END is audio plugin host.
through and through audio plugins host end-to-end. its default plugin is EVE with ENAX
(developed with all audio plugin format, one by one), later will be formalised with
CLAP"*.

---

## 0. Identity

END is a **JUCE GUI application** that hosts audio plugins in a tabbed, splittable
pane workspace, rendered by the JAM Vulkan engine. The formats are VST2, VST3 and AU
through JUCE and CLAP through jam_clap (`Nexus::addFormats`, `Nexus.cpp:77-87`). Each new
pane loads EVE (`Session.cpp:71-120`, `cast/files.md:31`). EVE is checked in one format
at a time. ARCHITECT: *"ALL formats means ONE at a time, for EVE. any valid audio format
SHOULD WORK. any 3rd audio plugin with "ALL FORMAT" must be able to be loaded in END."*

It is not a terminal emulator. The VT engine, the document model, and the terminal
widgets that earlier versions of this SPEC described now live outside this
repository. They return as hosted plugins:

- **eve** — the terminal plugin.
- **whelmed** — the markdown editor/viewer plugin.

END supplies the window, the workspace (tabs, panes, splits, focus), the config
system, the rendering engine, and the plugin lifecycle. Plugins supply content.

### Priority Order

1. **JUCE GUI application.** Component hierarchy, ValueTree state, native
   windowing. Every architectural decision starts here.
2. **Audio plugin host.** JUCE's formats and jam_clap host the plugins. One plugin
   instance per pane, each a node of one audio graph. The host stays
   content-ignorant. ENAX is formalised later with CLAP (`DEBT.md`,
   DEBT-20261025T135707). END now holds `title`, `foregroundProcess` and `cwd` on each
   pane row, because the hosted plugin tells them (`EditorView.cpp:17-18`).
3. **GPU-first rendering.** The JAM Vulkan engine renders every window. Shader
   pipeline (Shadertoy background, Slang post-process, OBJ mesh) is host-level
   decoration.

### Why This Order Is Non-Negotiable

The previous iteration fought JUCE because its mental model was "terminal
emulator", not "JUCE application". That lesson stands for the host: END is a JUCE
application first. The host never absorbs plugin responsibilities. When a feature
belongs to content, it belongs to a plugin.

---

## 1. Architecture

### 1.0 Layer Order (top → bottom)

```
Application → Config → Nexus → Session → hosted audio plugins (EditorView panes)
```

- **ENDApplication** (Main.cpp) — orchestrator. Constructs `jam::Window` directly
  (there is no ENDWindow class). Owns config and the Nexus lane.
- **Config** (`Source/config/`) — ConfigDirectory, ConfigModel and its per-domain
  subclasses (theme, shader). Four-phase lifecycle. Data source is markdown.
- **Nexus** (`Source/plugins/Nexus.h`) — host singleton. Owns the sessions Model
  state (`Id::sessions` tree: the `focusedSession` parameter, `Nexus.cpp:41-45`), the
  plugin format manager, the one audio graph, and the ENAX extensions
  (`Nexus.h:121-122`). Offloads the plugins from BinaryData to `~/.config/end/plugins/`
  (`Nexus.cpp` `offloadPlugins`).
- **Session** — uuid-keyed container of hosted plugin instances for one window's
  session. A write of `Id::plugin` to a PANE row runs `loadPlugin (paneRow)`, which creates
  the plugin asynchronously; the creation callback runs `addPlugin (paneRow, instance)`
  (`Session.cpp:53-63`, `:71-120`). `removePlugin (uuid)` removes the node
  (`Session.cpp:141-148`).
- **EditorView panes** — each pane hosts one plugin editor
  (`juce::AudioProcessorEditor` from the hosted instance). A pane with no loaded
  plugin renders bare (outline only).

### 1.1 View Composition

```
ENDView                                  ← app surface; key dispatch; registers the actions (ENDApplication owns `ENDActions`, `Main.h:56`)
  ├── background (VulkanShaderComponent) ← config-driven background shader
  ├── SessionView : jam::TabbedComponent ← one-visible tab owner, adopts SESSION row
  │     └── TabView[N] : jam::MatrixComponent  ← binary-space pane graph per tab
  │           ├── EditorView[N] : jam::PaneComponent  ← one hosted plugin per pane
  │           └── jam::PaneEdge[N]       ← EDGE rows, draggable seams
  └── MessageOverlay                     ← status/preview overlay painting
```

The Owner/Owned composite is jam's layout law:

- `jam::OwnedComponent` publishes its own bounds to `Id::bounds` and holds the
  `Id::focus` parameter of its own row (`jam_OwnedComponent.cpp:22-38`). It never
  reads or writes keyboard focus. The child stays dumb.
- `jam::OwnerComponent` owns the focused-child parameter, the one truth of focus.
  The owner writes it: the derived `childAdded` hook on add
  (`MatrixComponent::childAdded`; `TabbedComponent` through `changeCallback`), the
  survivor on remove (`MatrixComponent::childRemoved`), a click inside a child
  (`OwnerComponent::mouseDown`), and the focus-move action (`MatrixComponent::focusPane`, run by the
  `map::MatrixCommandId` pane commands, `jam_MatrixComponent.cpp:385-406`).
- Focus is told downward only. A child row's `Id::focus` is 1 when the child is
  its owner's focused child and the owner's own row `Id::focus` is 1
  (`OwnerComponent::valueTreePropertyChanged`). ENDView tells the active session
  row (`ENDView.cpp:148`). Keyboard focus never writes the Model.
- `jam::MatrixComponent` (TabView base) mutates the binary-space graph: `split`
  targets the focused child. An EDGE row holds its head and tail spaces as child
  rows 0 and 1.

### 1.2 State — jam::Model Contract

`jam::Model` is the APVTS-analog state owner (owned ValueTree by value, atomic
`Parameter<T>` store, adaptive-rate flush timer, `createAndAddParameter<T>`).
Contract points that bind END:

- Parameter-backed properties accept raw `setProperty` writes on the message
  thread; the adapter reverse-syncs VT→atomic, and the flush timer is CAS-gated —
  it never reverts a raw ValueTree write.
- Two independent trees: the runtime Model (Nexus-owned sessions state) and the
  config Model. No config values on the runtime tree, no runtime state on the
  config tree.
- Focus lane: a click in a pane → `OwnerComponent::mouseDown` writes the TAB
  row's `focusedPane` → the owner tells each pane row `Id::focus` 1 or 0 →
  `EditorView` sets its plugin's `focus` host parameter
  (`EditorView::valueTreePropertyChanged`) → jam `PluginEditor` grabs keyboard
  focus when the parameter turns on (`PluginEditor::addCallbacks`). Actions and
  splits read the active tab's focused child
  (`SessionView::getActiveTabView()->getFocusedChild()`). No other copy of the
  focused pane exists.

### 1.3 Config — Markdown Tables, Cast Toolchain

Config data lives in markdown grid tables, parsed by jam_markdown
(`jam::ConfigDocument::parse` → `getValueTree`, validated by
`jam::ConfigValidator`).

- **Sources (deployed, live):** `~/.config/end/display.md`, `keys.md`,
  `themes/<name>/theme.md`. Shipped defaults live at `Source/config/`.
- **Load path (shared with EVE):** each file starts with a front matter
  `version: 0.0.1`. `jam::ConfigDocument::getOrCreateDefaultFile` writes the binary
  default when the disk file is missing or its version is older. Agents never change
  a version. `jam::ConfigDocument::addTables` validates the disk file and appends
  each table whose columns equal the binary table's; `setValuesFrom` lays those
  rows over the binary default. A failure (bad cell, changed columns, unknown table)
  goes to the message overlay with `file:line (column)`; the binary rows stay; nothing
  asserts on a disk file (`Source/config/ConfigModel.cpp`).
- **Value typing** is per-row (`integer`, `float`, `bool`, `string`, `colour`,
  `numbers`) via `ConfigDocument::getValueTypes`. A number cell must be a number
  (`Format::isNumber`).
- **Authoring rules:** every key cell is lower-case words (`always on top`,
  `close pane`). The reader formats it to the reserved identifier with
  `Format::toCamelCase` when it creates the row. ARCHITECT: *"END and EVE keys all
  using user friendly case. lower case with whitespace. althought the exact word is
  internally reserved and FORMATTED."* A value cell whose text collides with
  markdown block syntax is backslash-escaped (`\-`, `` \` ``, `\\`) — the parser
  resolves the escape to the literal.
- **Theme colours:** `theme.md` `## colours` names every colour (JFS palette name,
  else jam's nearest colour name) with a `0xAARRGGBB` value. An alpha variant takes a
  word prefix on the base name: `clear`, `faint`, `glass`. Every other theme table has
  `key | type | light | dark | choices | description`; a colour cell holds a
  `## colours` key. The reader also takes a jam colour name or a direct `0xAARRGGBB`,
  so a user theme can use them; the default gfx theme uses neither. ARCHITECT: *"using
  direct AARRGGBB hex should work as is"*; *"no direct hex allowed in gfx theme.md, all
  colours must be named, and all consumer only use named colour"*. An empty `dark` is
  the light value. `display.md` `appearance` (AUTO, LIGHT, DARK) picks the column; AUTO
  follows the OS (`ConfigModel::getRowValue`).
- **Pane padding:** `theme.md` `## pane` `padding` is the one inset of the focused
  outline and of the hosted editor, which sits inside the outline stroke
  (`ENDLookAndFeel::getPanePadding`, `EditorView::resized`).
- **Constants pipeline:** `cast/*.md` (project-info, identifiers, bimaps, files)
  → `cast cast/spell.md` → `Source/generated/{ProjectInfo,Identifiers,Bimaps,
  Files,Generated}.h` (global `Id::`, `map::`, `files::`). The generated lexicon
  defines only what jam's lexicon does not — shared identifiers have exactly one
  owner: jam.
- **Key bindings** (`keys.md`): each row names a command and its key. The row never
  names a parameter or a command id. `ENDView` is END's one binder: it registers its
  commands and gives the KEYS tree to `jam::CommandManager::setKeyPresses`
  (`ENDView.cpp:13`, `:31`), again on each KEYS change (`:460`).
  `jam::CommandManager::invokeKey` is END's one key entry (`ENDView.cpp:57-60`).
  Inside the caller's layer it matches a value of one or two characters as a text
  sequence and a longer value as a key press. The `layer` column names a row's
  layer; an empty cell is the default layer. A row whose name is a layer name
  enters that layer (`prefix`, `` ` ``). In a layer, the prefix key clears the layer
  and passes through; any other unmatched key is dropped. In the default layer an
  unmatched key passes through. `sequenceTimeout` (default 1000 ms, owned by
  `CommandManager`) ends a sequence or a layer through one `juce::Timer`.
  ARCHITECT: *"ALWAYS one entry point, always SSOT"*.

### 1.4 Rendering — First-Class Vulkan Architecture

Two JAM engines are the rendering authority. Each is a `jam::Instance` and a
`juce::DeletedAtShutdown` (`jam_CpuGraphicsEngine.h:30-31`,
`jam_VulkanGraphicsEngine.h:72-74`). `ENDApplication` creates the Vulkan engine at
init (`VulkanGraphicsEngine::getOrCreate`, `Main.cpp:262`). Its `getOrCreate` creates
the CPU engine first (`jam_VulkanGraphicsEngine.h:167-170`).

- **`jam::CpuGraphicsEngine`** owns `jam::Typeface`, `jam::GlyphAtlas`,
  `jam::GlyphArrangement`, the image loader and the CPU image operations
  (`jam_CpuGraphicsEngine.h:155-171`).
- **`jam::VulkanGraphicsEngine`** owns the `jam::VulkanDevice`, the texture, geometry
  and pipeline caches, the image renderers, and one `jam::VulkanGraphics` for each
  peer (`jam_VulkanGraphicsEngine.h:528-578`).
- **Context dispatch.** The Vulkan engine sets
  `juce::ComponentPeer::externalContextFactory` to its `createContext`
  (`jam_VulkanGraphicsEngine.cpp:18-20`; on Windows only while the GPU is available,
  `:275-281`). On macOS, `createContext` falls back to the CPU engine's context when
  it makes no Vulkan context (`jam_VulkanGraphicsEngine.cpp:442-443`). The CPU lane
  draws glyphs through `jam::CpuLowLevelGraphicsContext`
  (`jam_CpuLowLevelGraphicsContext.h:50-51`).
- **macOS:** MoltenVK (vendored SDK under `Poems/Vulkan/macOS/`), CAMetalLayer
  surface attached once at swapchain creation.
- **Shader pipeline:** background and post-process shader projects under
  `~/.config/end/shaders/<name>/` (Shadertoy convention; optional `.slangp`
  resource manifest for OBJ mesh + textures). Runtime compilation via shaderc;
  engine templates splice with `@token@` placeholders through
  `jam::Format::replaceholder` with an explicit at-sign delimiter.
- Paint dispatch is message-thread-synchronous; no dedicated GPU thread.

### 1.5 Plugin Hosting — JUCE Formats, jam_clap and ENAX

- Formats: JUCE's default formats (VST2, VST3, AU) and `jam::ClapPluginFormat`
  (`Nexus::addFormats`, `Nexus.cpp:77-87`; `Nexus::registerExtensions` fills the
  extensions that it passes, `Nexus.cpp:61-75`).
- Plugins live in `~/.config/end/plugins/`. `Nexus::offloadPlugins` writes them from
  BinaryData when the build-time hash of the archives differs from the stamp file
  (`Nexus.cpp:178-206`). On macOS, when `files::evePlugin` names a `.component`, the
  `Nexus` constructor registers that one bundle in the process after the offload
  (`Nexus::registerEVE`, `Nexus.cpp:89-106`; `Nexus::registerAudioUnit`, `:237-258`). Formats are tested one at a time, so no other
  bundle loads. ARCHITECT: *"i dont want to put "END's in-house plugin" into system
  audio plugins path"*.
- A write of `Id::plugin` to a PANE row makes `Session::loadPlugin` create the plugin
  asynchronously (`createPluginInstanceAsync`). The creation callback runs
  `Session::addPlugin`, which adds the plugin as a node of the one graph
  (`Session.cpp:53-63`, `:71-120`). The format is the first registered format whose
  `fileMightContainThisPluginType` accepts the plugin file (`Session.cpp:36-51`).
  Each plugin owns its clock.
- `EditorView::createProcessorEditor` hosts the instance's `AudioProcessorEditor`
  when the pane row receives `Id::name` (`EditorView.cpp:172-214`). Resizable editors
  track the pane bounds.
- **Host identity.** END is a known JUCE host: `juce::PluginHostType::END` and
  `isEND()` (`jam/patch/juce-plugin-host-type-hook.patch`). ARCHITECT: *"we NEED END
  as one of the KNOWN type, this is the SSOT seam"*.
- **ENAX channel.** END exports one function, `enax_get_extension (const char* id)`
  (`Main.cpp:29-32`). It returns the `Nexus::extensions` entry for the id, or
  nullptr. The ids are the context factory, the peer-deleted callback, the key hook and
  the mouse hook. A profiler build adds `enax.prompt-painted` and `enax.manifest-file`.
  State never crosses here. ARCHITECT: *"must be cross format. hence this is END's contract
  as host to have ONE EXACT SEAM to handle all format, the only exceptions when there's a
  format that legitimately could not adhere to design, then ENAX is the path"*.
- **State seam.** `PluginState` (`plugins/PluginState.cpp`) is the one seam for the state of
  a hosted instance, in every format. `Session` owns one for each instance from the same
  company (`Session::addPlugin`). Its `juce::AudioProcessorListener` receives
  `audioProcessorChanged`, reads the raw state through one format lookup, and copies the
  `pluginState` rows to the `pluginState` child of the pane row. Each changed row gives one
  `valueTreePropertyChanged`. `setRows` writes rows through the same lookup. VST2 uses the
  chunk, VST3 the `IComponent` data, AU the `jucePluginState` key, CLAP the raw state.
- **ENAX rules.** The plugin side, `jam::ENAXClientExtensions`, holds the END-only
  rules in one table, selected by one `isEND()` probe. Outside END, the plugin keeps
  its standalone contract. ARCHITECT: *"as standalone, plugins that loaded into
  whatever known hosts other than END, EVE SPEC CONTRACT must stands as is"*.
- **Keys.** The key hook gives END every key before the plugin's components
  (`jam/patch/juce-key-pressed-hook.patch:21-23`, `Nexus.cpp:7-16`). The `engine` rule
  installs the key and mouse hooks with the other peer hooks
  (`jam_ENAXClientExtensions.cpp:15-37`). Under END, the plugin's `keys` rule removes
  the text editor's `normal`, `prefix`, `open` and `list` rows
  (`jam_ENAXClientExtensions.cpp:39-45`), so the hosted text editor holds no layer
  binding; END's prefix layer and its `normal`, `open` and `list` commands drive the mode. The plugin's concrete editor applies the rule to the text
  editor's defaults before it binds (`EVEView::getViewDefaultKeys`). ARCHITECT: *"when EVE (with
  its TextEditor) hosted in END, it SHOULD NOT register any keyboard shortcut for any
  state-command, BUT we KEPT the state machine"*.
- **Focus.** END owns focus. The focused pane writes the `focus` row of its plugin
  from its pane row (`EditorView::valueTreePropertyChanged`). EVE grabs keyboard focus
  only when the row turns on (`EVEView::registerTerminalEvents`), and
  passes it to its default component (`EVEView::focusGained`). The plugin never
  decides focus itself. ARCHITECT: *"when hosted at END the FOCUS parameter always
  being told by END always sync with Pane focus … Plugin doesnt grab its own focus,
  only being told"*.

### 1.6 Main Goal — Shared Vulkan Engine Across Host and Plugins

Hosted plugin instances share the host's Vulkan engine: one device for host chrome
and plugin content. Under END, the ENAX `engine` rule gives the plugin END's context
factory, peer-deleted, key and mouse callbacks, so the plugin paints through END's
engine. Outside
END, the plugin creates its own `VulkanGraphicsEngine`
(`jam_PluginEditorLayout.h:139-143`). Each binary keeps its own CPU engine
(`jam_PluginEditorLayout.h:137`).

### 1.7 Plugin State and Tab Names

END holds the state of its plugin in the `pluginState` child of each pane row. ARCHITECT:
*"END is the host. it holds its plugin instance stateInfo."*; *"the correct design is not
`terminal`, but the state instance of `EVE`. `EVE` could be any plugin"*. `terminal` is EVE's own
tree. EVE maps it to `pluginState` at `getStateInformation` and `setStateInformation`.

- `PluginState` replaces the rows of that child from the plugin (section 1.5).
- A new pane takes the `cwd` of the focused pane of its tab (`getInheritedDirectory`).
- A new tab takes the `cwd` of the focused pane of the active tab (`ENDView`, the `newTab`
  command).
- `Session::addPlugin` writes the `pluginState` child to the plugin before `graph.addNode`, so
  the shell starts in that `cwd`.
- A plugin from another company has no `PluginState`, and its pane holds no rows from it.
- END holds the module of each plugin with a `PluginState` until `Nexus` ends (`Nexus::addPluginModule`).
  END's graphics context interns a typeface that the plugin built, and JUCE unloads a
  bundle whose retain count is 1 (`juce_VSTPluginFormatImpl.h:785-786`).
- The tab name order is: rename, title, application name (`foregroundProcess`), last
  folder of `cwd`, plugin name (`SessionView::getTabName`, `SessionView.cpp:96-125`).
  A change of the rename, the focused pane or a report applies the tab name
  (`SessionView.cpp:77-94`).

---

## 2. Actions and Input

- **Commands.** A command id is a callback id, owned by the owner of the state that
  its callback changes. `commandManager` is the one registry of operations. ARCHITECT:
  *"commandManager is the SSOT of `operation`."* END's ids are `map::CommandId`
  (`cast/bimaps.md:56-93`). `ENDView::perform` dispatches through its `commands` table,
  keyed by `juce::CommandID`. Each operation goes through `invokeDirectly`, from a key, a menu,
  a gesture or a Model row. The pane focus commands are `map::MatrixCommandId`
  (`jam/cast/gui.md:226-242`), and `MatrixComponent::perform` runs them
  (`jam_MatrixComponent.cpp:385-398`). Every command of `map::CommandId` has a table entry,
  except `prefix`: a layer-entry row enters its layer in `invokeKey` and is never performed.
- Workspace verbs: `newTab`, `addPane` (split by edge or append), `closeTerminal`, `closePane`
  (pane → tab → window cascade), `closeTerminalPane`, `splitVertical`/`splitHorizontal`, pane
  join/swap, pane focus (`map::MatrixCommandId`), pane resize by `paneStep`, tab next/prev.
- **A close has one order.** `OwnerComponent::remove` destroys the child while its row is whole.
  Its `childRemoved` hook then runs, so a focus write reaches only the children that stay.
  `TabView` calls `Session::removePlugin` from that hook, after the editor is gone. When an owner
  holds no child, its hook tells the next owner: a tab with no pane leaves its session, and a
  session with no tab quits the application. ARCHITECT: *"closing pane, exit, clean dtor must be
  deterministic, SSOT. one COMMAND"*; *"last pane quit app"*.
- **`loadPlugin` loads any plugin into the focused pane.** `files::evePlugin` is its default
  argument. A file drop overrides it (`EditorView::DropTarget::filesDropped`). ARCHITECT: *"there
  will be command that load any plugin, override eve as the default arg"*.
- **The pane model.** ARCHITECT, verbatim:
  - *"a pane is just blank canvas for ANY PLUGIN"*
  - *"END "features" is A PLUGIN"*
  - *"default plugin is EVE"*
  - *"any pane could be any plugin"*
  - *"`exit` is terminal command that trigger chain of closing pane with its eve that received self dtor chain"*
  - *"cmd+w is closing pane, any pane, by default trigger `exit` chain command, the focused pane"*
  - *"by default cmd+w IS `exit`"*
- **Three close commands, in `map::CommandId`.** ARCHITECT: *"close terminal should be one
  commandId (kills eve instance CLEANLY)"*; *"close pane should be one commandId"*; *"close terminal
  pane should be one commandId (calls closeTerminal then, closePane)"*; *"cmd+w, `exit`, should call
  close terminal pane"*.
  - `closeTerminal` removes the `plugin` row of the pane. The pane deletes its editor, then
    `Session` removes the plugin.
  - `closePane` removes the pane, or the tab when it is the only pane.
  - `closeTerminalPane` invokes `closeTerminal`, then `closePane`.
- **`EditorView` is the command target of the close commands.** `perform` acts on its own pane by
  identity, focused or not. The key path asks the focused pane.
- **A shell exit closes its pane.** The `shellExited` row reaches the `terminal` child of the
  pane row. `EditorView` invokes `closeTerminalPane` on itself asynchronously, so the removal runs
  outside the plugin's call stack. ARCHITECT: *"`exit` should close END's pane (exit eve)"*.
- **END tells a hosted plugin through rows.** ENDView tells the panes; it never reaches into a
  plugin. `EditorView::setRows` builds a `pluginState` tree with the one row, and `Session::setRows`
  writes it through `PluginState`. The mode keys are jam's `map::EditorMode`.
  - `zoomIn` / `zoomOut` add or subtract `zoomStep` (`display.md`) to the `zoom` row.
    `zoomReset` sets its default value (`EditorView::setZoom`). EVE holds the limit. The targets
    are every pane of the session (`SessionView::setZoom` → `TabView::setZoom`), or the focused
    pane only when `exclusiveZoom` is true.
  - `normal`, `open` and `list` set the `mode` row of the focused plugin to the matching key of
    `map::EditorMode` (`EditorView::setMode`).
  - `focus` follows the pane row.
  - `EditorView::resized` writes the `width` and `height` rows from the pane bounds.
    `EditorView::setEditorBounds` only moves an editor that is not resizable, such as the VST2
    host window, so the plugin sets its own size from the rows.
    ARCHITECT: *"it's still the same tree but adhere to how audio plugin host-client contract"*.
- Splits and pane verbs act on the active tab's focused pane, the TAB row's
  `focusedPane` — never on positional or last-created assumptions.
- The pane menu's splits invoke the same `splitVertical` and `splitHorizontal` commands
  as the keys (`TabView.cpp`, `handleAreaOptionsResult`).

---

## 3. Roadmap

Iteration proceeds both ways between host and plugins:

1. **Freeze current host state** — workspace, config, rendering, focus contract
   verified (this revision).
2. **eve** — terminal as an audio plugin, END's default plugin under ENAX. Carries the VT engine, document model,
   CodeView rendering, PTY transport formerly specified here. The terminal-era
   SPEC is preserved verbatim as eve's baseline: `dev/eve/SPEC.md`.
3. **whelmed** — markdown editor/viewer as an audio plugin.
4. **Host iteration** — plugin lifecycle hardening, focus/keyboard routing with
   live editors, session persistence.
5. **Shared Vulkan engine** (§1.6) — the main goal: plugin instances render
   through the host's engine.

---

## 4. Platform

| Platform | Compiler | Graphics |
|----------|----------|----------|
| macOS (Intel + ARM) | Xcode clang | Vulkan via MoltenVK (vendored) |
| Windows 10/11 | MSVC + clang-cl (MSYS2) | Vulkan (D3D11 composition lane for the swapchain-less window path) |
| Linux | GCC/clang | Vulkan (Xlib surface) |

---

*This SPEC is the ground of truth for END the host. Content specifications
(terminal semantics, markdown rendering) belong to the plugins that implement
them.*
