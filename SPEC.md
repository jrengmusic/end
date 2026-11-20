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
through JUCE and CLAP through jam_clap (`Nexus::addFormats`). Each new
pane loads EVE (`TabView::loadPlugin`, `Session::loadPlugin`, `Session::addPlugin`). EVE is checked in one format
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
   DEBT-20261025T135707). The tree of a hosted plugin that runs jam is attached under the
   `plugin` child of its pane row. It is then a part of the tree of END (section 1.5, plugin
   state).
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
- **Config** (`Source/config/`) — ConfigDirectory, its subclasses ConfigTheme and
  ConfigShader, and ConfigModel. Four-phase lifecycle. Data source is markdown.
- **Nexus** (`Source/plugins/Nexus.h`) — host singleton. Owns the sessions Model
  state (`Id::sessions` tree: the `focusedSession` parameter, `Nexus` constructor), the
  plugin format manager, the one audio graph, the ENAX extensions (`Nexus::extensions`) and
  the `jam::ValueTreeServer` of the lane. Offloads the plugin from BinaryData to
  `~/.config/end/plugins/` (`Nexus::getOrCreatePlugin`).
- **Session** — uuid-keyed container of hosted plugin instances for one window's
  session. A write of `Id::plugin` to a PANE row runs `loadPlugin (paneRow)`, which creates
  the plugin asynchronously; the creation callback runs `addPlugin (paneRow, instance)`
  (`Session::registerEvents`, `Session::loadPlugin`). `removePlugin (uuid)` removes the node.
- **EditorView panes** — each pane hosts one plugin editor
  (`juce::AudioProcessorEditor` from the hosted instance). A pane with no loaded
  plugin renders bare (outline only).

### 1.1 View Composition

```
ENDView                                  ← app surface; key dispatch; registers the actions (`ENDView::registerActions`; ENDApplication owns `Nexus` and `ConfigModel`, `Main.h:54-55`)
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
  row (`ENDView.cpp:147`). Keyboard focus never writes the Model.
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
  a version. `ConfigDirectory::addFileTables` parses each disk file and validates it with
  the rules of `jam::ConfigValidator` and the rules of the binary default document. A
  document that passes gives its tables to the target tree through the file-local
  `addTables`. END rejects a document that fails as a whole: none of its tables enter
  the tree. ARCHITECT: *"Reject whole document"*. `setValuesFrom` lays the accepted rows
  over the binary default. A failure (bad cell, changed columns, unknown table) goes to
  the message overlay with `file:line (column)`; the binary rows stay; nothing asserts
  on a disk file (`Source/config/ConfigDirectory.cpp`). A flex SVG that fails
  `jam::XmlValidator` keeps its binary default (`addFileFlex` in
  `Source/config/ConfigTheme.cpp`).
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
- **Theme style:** `Source/config/theme/gfx/style.css` is the one source of every font,
  colour and metric. ARCHITECT: *"css contract: ALL RELATED STYLE SSOT - fonts - colour -
  metrics"*. `theme.md` is the second serialization of that `jam::CssDocument`. Each css
  block is one markdown table, and each table is one css block. The carried `theme.md` is
  the front matter only. The default text is the carried `theme.md`, a newline, then
  `jam::ConfigDocument::toDocument (CssDocument)` written by `jam::MarkdownWriter`
  (`ConfigTheme.cpp`, `getOrCreateDefaultText`). The tab rows (`position`, `always
  visible`, `uppercase`) and the pane row (`split line`) are settings, not style. They are
  in `display.md` `## tab` and `## pane`.
- **Theme on disk:** the default is written to a virgin disk, and disk wins. ARCHITECT:
  *"binary write to virgin state to disk, disk always win."* The tables are `## colours`
  (palette rows `colour` + `0xAARRGGBB`, and colour-id rows `alias` + palette words in
  `light` / `dark`), `## fonts` (face rows with `value`, `style`, `src`, `height`,
  `kerning`, and the rasterizer columns on the first face; font-id rows `alias` + face
  words), one `## metrics` (the unit is the type, `em` or `float`), `## window`,
  and one table per css class. Each record of `.appearance` or of a class (`DARK`) is one
  column after the light column. ARCHITECT: *"fonts is fonts, colours is colours, metrics is
  metrics."* `key` cells are plain words. `jam::ConfigDocument` drops the `colour id` and
  `font id` suffix at write, and adds it from the generated lexicon at read. ARCHITECT:
  *"md must be using plain words without any redudancy of `colour id`, `font id` etc."*
  An empty `dark` is the light value. `display.md` `appearance` (AUTO, LIGHT, DARK) picks
  the column; AUTO follows the OS.
- **Theme load:** `ConfigDirectory::addFileTables` validates the disk `theme.md` against
  the default document. `jam::CssWriter` writes a valid document as css text, and
  `CssDocument::parse` of that text gives the THEMES tree through `getValueTree`
  (`getStyleTree` in `ConfigTheme.cpp`). The css document is the only builder of its
  sections. An
  invalid document leaves the default tree and its error goes to the message overlay.
  ARCHITECT: *"if disk was invalid, rendering never fails ... BUT Validator must show
  error."*
- **Theme consumers:** the `Id::theme` event runs `StyleManager::registerStyle` on the
  THEMES tree, then `setAppearance`. Every style read goes through StyleManager
  (`ENDLookAndFeel.cpp`). ARCHITECT: *"style related `config` always through Style, never
  by poking config directly."*
- **Pane padding:** the `.metrics` row `--PaneComponent--padding` (`## metrics` `pane
  component padding`) is the one inset of the focused outline and of the hosted editor,
  which sits inside the outline stroke (`ENDLookAndFeel::getPanePadding`,
  `EditorView::resized`).
- **Constants pipeline:** `cast/*.md` (project-info, identifiers, bimaps, files)
  → `cast cast/spell.md` → `Source/generated/{ProjectInfo,Identifiers,Bimaps,
  Files,Generated}.h` (global `Id::`, `map::`, `files::`). The generated lexicon
  defines only what jam's lexicon does not — shared identifiers have exactly one
  owner: jam.
- **Key bindings** (`keys.md`): each row names a command and its key. The row never
  names a parameter or a command id. `ENDView` is END's one binder: it registers its
  commands and gives the KEYS tree to `jam::CommandManager::setKeyPresses`
  (`ENDView.cpp:13`, `:32`), again on each KEYS change (`:415`).
  `jam::CommandManager::invokeKey` is END's one key entry (`ENDView.cpp:55-58`).
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
init (`VulkanGraphicsEngine::getOrCreate`, `Main.cpp:261`). Its `getOrCreate` creates
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
  (`Nexus::addFormats`; `Nexus::registerExtensions` fills the extensions that it passes).
- Plugins live in `~/.config/end/plugins/`. END's CMake embeds exactly one plugin
  archive, the one of the host platform (`cast/cmake.cast:356-371`).
  `Nexus::getOrCreatePlugin` unpacks it from BinaryData when the build-time hash of the
  archive differs from the stamp file, or when its bundle is absent, and returns the
  bundle that the archive carries: its first entry. The format
  is the `pluginFormat` row of `project-info.md` (`CLAUDE.md`, ENAX Plugin Format). On
  macOS, when that bundle is a `.component`, the `Nexus` constructor registers that one
  bundle in the process after the offload (`Nexus::registerEVE`,
  `Nexus::registerAudioUnit`). Formats are tested one at a time, so no other
  bundle loads. ARCHITECT: *"i dont want to put "END's in-house plugin" into system
  audio plugins path"*.
- A write of `Id::plugin` to a PANE row makes `Session::loadPlugin` create the plugin
  asynchronously (`createPluginInstanceAsync`). The creation callback runs
  `Session::addPlugin`, which adds the plugin as a node of the one graph. The format is the
  first registered format whose `fileMightContainThisPluginType` accepts the plugin file
  (`Session::getPluginFormat`).
  Each plugin owns its clock.
- `EditorView::createProcessorEditor` hosts the instance's `AudioProcessorEditor`
  when the pane row receives `Id::name` (`EditorView::registerEvents`). Resizable editors
  track the pane bounds.
- **Host identity.** END is a known JUCE host: `juce::PluginHostType::END` and
  `isEND()` (`jam/patch/juce-plugin-host-type-hook.patch`). ARCHITECT: *"we NEED END
  as one of the KNOWN type, this is the SSOT seam"*.
- **ENAX channel.** END exports one function, `enax_get_extension (const char* id)`
  (`Main.cpp:29-32`). It returns the `Nexus::extensions` entry for the id, or
  nullptr. The ids are the context factory, the peer-deleted callback, the key hook, the
  mouse hook and `enax.server-port`, the address of the lane. A profiler build adds `enax.prompt-painted` and `enax.manifest-file`.
  State never crosses here. ARCHITECT: *"must be cross format. hence this is END's contract
  as host to have ONE EXACT SEAM to handle all format, the only exceptions when there's a
  format that legitimately could not adhere to design, then ENAX is the path"*.
- **Plugin state.** The tree of a hosted plugin is attached to the tree of END, under the
  `plugin` child of its pane row. It is then the tree of END. END listens to it and writes to
  it. The plugin tree stays in sync. ARCHITECT: *"EVE tree is SSOT / EVE tree attached to
  END's tree / END's tree is the whole state. / the whole state could run in background as
  daemon."*; *"when END load EVE, EVE's tree attached to END, now it's END's tree. END can
  listen and do whatever, EVE always synced. that's the contract"*; *"ONE SHAPE, ONE PATH, ONE
  LANE, ONE SERIALISATION, ONE SSOT."*
  - The lane. END is always the IPC server. `Nexus` owns one `jam::ValueTreeServer` on the
    tree of `ENDModel`. It listens on the loopback address, on a port that the system gives.
    ARCHITECT: *"END is always IPC server / Daemon is IPC server on background"*. The port
    reaches the plugin through the ENAX extension `enax.server-port`. The ENAX rule `server`
    connects the plugin's own `jam::ValueTreeConnection`, which syncs the model tree of the
    plugin. The connection is one `juce::InterprocessConnection` and one
    `juce::ValueTreeSynchroniser`. Each change of a tree reaches the other side as the encoded
    change of the synchroniser. The synchroniser does not send a change back to the side that
    applied it (`jam/patch/juce-value-tree-synchroniser-hook.patch`). When
    `setStateInformation` swaps the model root, the connection follows the new root through
    `valueTreeRedirected`. Audio never crosses the lane. Every host-to-plugin value crosses
    this lane. ARCHITECT: *"enax extension or IPC, CHOOSE ONE FOR ALL"*; *"nothing wrong with
    IPC, it's our lane for parameters that is not AudioParameter"*.
  - Identity. The plugin writes one `jam::UUID` on its model root as `id`. `Session::addPlugin`
    reads the raw chunk one time (`getChunkData`). It appends a held row with the type and the
    `id` of that chunk as the `plugin` child of the pane row. The first message of the
    connection is the complete tree of the plugin. The server connection creates its
    synchroniser first. Then it replaces the held row with the synced tree. A tree with no `id`
    goes under the root of END's tree. Every reaction is a `juce::ValueTree::Listener` event.
    ARCHITECT: *"why dont we trust ValueTreeSynchroniser? ValuTree::Listener?"*.
  - END to plugin. At the attach, `EditorView::valueTreeChildAdded` writes on the attached
    `terminal` row, in this order: the `cwd` of a new pane row, the `focus` of the pane row,
    then `width` and `height`. Later, END writes `focus` (`Session::registerEvents`), `mode`
    from the `normal` and `open` commands (`EditorView::setMode`), an empty `markdownFile`
    from the close key (`closeTerminalPane`), and the size (`EditorView::resized`).
    `Session::getTerminal` gives the `terminal` row of a pane. ARCHITECT: *"END will be the
    sole author of winsize"*.
  - A plugin that does not run jam has no connection and no held row.
- **ENAX rules.** The plugin side, `jam::ENAXClientExtensions`, holds the END-only
  rules in one table, selected by one `isEND()` probe. Outside END, the plugin keeps
  its standalone contract. ARCHITECT: *"as standalone, plugins that loaded into
  whatever known hosts other than END, EVE SPEC CONTRACT must stands as is"*.
- **Keys.** The key hook gives END every key before the plugin's components
  (`jam/patch/juce-key-pressed-hook.patch:21-23`, `Nexus.cpp:6-15`). The `engine` rule
  installs the key and mouse hooks with the other peer hooks
  (`jam_ENAXClientExtensions.cpp:9-31`). Under END, the plugin's `keys` rule removes
  the text editor's `normal`, `prefix` and `open` rows
  (`jam_ENAXClientExtensions.cpp:33-39`), so the hosted text editor holds no layer
  binding; END's prefix layer and its `normal` and `open` commands drive the mode. The plugin's concrete editor applies the rule to the text
  editor's defaults before it binds (`EVEView::getViewDefaultKeys`). ARCHITECT: *"when EVE (with
  its TextEditor) hosted in END, it SHOULD NOT register any keyboard shortcut for any
  state-command, BUT we KEPT the state machine"*.
- **Close.** The close key closes in this order: the plugin's markdown view, then the pane,
  then the tab, then the application. When the `markdownFile` of the attached tree is not
  empty, END writes it empty on the attached tree (EVE: *"a host clears it to close the view"*). Otherwise END
  closes the pane. ARCHITECT: *"close hierarchy always pane - tab - quit app"*; *"dont give
  all cmd+w to EVE, pass it through"*.
- **Focus.** END owns focus. `jam::OwnerComponent` writes the `focus` of each pane row: the
  focused child of a focused owner. When the `focus` of a pane row changes, `Session` writes it
  on the attached tree of that pane (`Session::registerEvents`). When the tree attaches,
  `EditorView::valueTreeChildAdded` writes the current value. EVE grabs keyboard focus
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

END holds the tree of its plugin under the `plugin` child of each pane row (section 1.5,
plugin state). ARCHITECT: *"END is the host. it holds its plugin instance stateInfo."*;
*"session restoration with daemon, definitely NEED ALL"*.

- **Save.** At shutdown, the process that hosts writes `Nexus::getSessionsState` to the
  `sessions` key of `END.settings`. It is a copy of the sessions tree. On each pane with a
  plugin, the copy holds the blob that `getStateInformation` gives, as `state`. The copy has no
  `plugin` child and no `name` on that pane (`Session::setPluginStates`). END never parses the
  blob. ARCHITECT: *"why we cant simply call setStateInfo() and getStateInfo() as is? why create
  workaround it?"*.
- **Restore.** `ENDApplication::host` reads that key before it builds the window.
  `Nexus::setSessionsState` moves each saved session row into the sessions tree and creates its
  `Session`. The `Session` loads the plugin of each pane (`Session::loadPlugins`).
  `Session::addPlugin` gives the `state` of the pane row to `setStateInformation` before the
  graph adds the plugin, then removes it. The plugin's connection follows the restored root, so
  the first sync carries the restored tree. The window builds from the rows (section 1.8).

- END has no start directory setting. ARCHITECT: *"END has no cwd config. END runtime state track
  last focused eve cwd, and tell eve to start at cwd, default always eve config. there is no
  fallback."*
- `Session` keeps the `cwd` of the last focused terminal as the `cwd` row of the sessions row.
  It writes the row when a pane gets focus, and when the `cwd` of the terminal of the focused
  pane changes (`Session::registerEvents`).
- The new-row constructor of `EditorView` writes that row to every new pane row as `cwd`,
  when it is not empty: an added pane, a split, a corner drag and a file drop all build their
  pane there. At the attach, `EditorView::valueTreeChildAdded` writes it on the `terminal`
  row of the plugin, before the size, and removes it from the pane row. A restored pane
  comes through the adopt constructor and keeps its own saved `cwd`. ARCHITECT: *"Own saved
  cwd"*. A restore also brings back the `cwd` row of the sessions row
  (`Nexus::setSessionsState`).
- EVE writes the start directory of its own config in its constructor. ARCHITECT: *"eve has
  its own config. nothing about that changed whatever the host is"*.
- Under END, EVE writes no editor size: the ENAX rule `size` replaces EVE's own size write.
  EVE opens its shell only when the winsize is valid and `cwd` is not empty
  (`EVETerminalProcessor::prepare`). For a new pane, the first valid winsize comes from END's
  size, and END writes `cwd` before the size. Thus a new pane's shell starts at END's size,
  in END's directory. A restored pane carries the saved `winsize`, `width` and `height`,
  which END wrote before the save (`parameters.md:57`, `EVETerminalProcessor::setState`). Its
  shell starts at the saved size, and END's first size write at the attach then resizes it.
- END holds the module of each plugin until `Nexus` ends (`Nexus::addPluginModule`).
  END's graphics context interns a typeface that the plugin built, and JUCE unloads a
  bundle whose retain count is 1 (`juce_VSTPluginFormatImpl.h:785-786`).
- The tab name is the rename, or else the `name` of the focused pane row, which is the plugin
  name (`SessionView::getTabName`). A change of `name` or `focusedPane` on a tab row or a
  pane row applies the tab name (`SessionView::valueTreePropertyChanged`).

### 1.8 Server Mode

The lane is the same in every mode. Only the life of the IPC server changes. ARCHITECT: *"ONE
SHAPE, ONE PATH, ONE LANE, ONE SERIALISATION, ONE SSOT. the difference only the IPC server
lifecycle either live and die with the app, or running on bg"*.

- The `server` row of the `display` table of `display.md` is a bool. Its default is false.
  ARCHITECT: *"for `standalone` mode, default server is false"*.
- **`server` false (standalone).** The server lives and dies with the application. A quit ends
  the whole application. ARCHITECT: *"cmd+q close immediately, the app, the whole app."* The
  window close, the `quit` command and the close of the last tab take the same quit path
  (`ENDApplication::systemRequestedQuit`).
- **`server` true.** ARCHITECT: *"`server` mode enabled, it run on background with lock file,
  per app END instance"*. A quit destroys the window and hides the Dock icon. The process keeps
  `Nexus`, the audio graph, the editors and the shells. ARCHITECT: *"for Daemon mode, it
  destroy the GUI, keeping process running bg."* A quit that the system asks for when no
  window exists ends the process.
- **Launch with `server` true.** ARCHITECT: *"running new instance always check whether Daemon
  running on bg, attach, create new GUI from server (this is still isomorphic to tmux)"*.
  `ENDApplication::launch` takes the lock `END.server` (`juce::InterProcessLock`).
  - The lock is free. The launch releases it, starts the same executable with `--server` as a
    detached child (`jam::ChildProcess::startDetached`), and quits. The server is never the
    process that the system launched. The server starts `Nexus`, writes its port to the
    `port` key of `END.settings`, takes the lock for its life, and builds its window.
  - The lock is held. The launch reads the port, connects a `jam::ValueTreeConnection` on its
    own `ENDModel` tree, and quits. The root of that tree is `end`, with no `id`, so the
    server puts it under the root of END's tree. `ENDApplication::valueTreeChildAdded` sees
    that `end` child. It builds the window if none exists, shows the Dock icon, and brings the
    process to the front.
- **The window is a materialisation of the tree.** A new window builds its sessions, tabs,
  panes with their seams, and editors from the existing rows (`ENDView::addSessions`,
  `SessionView`, `jam::MatrixComponent::addChildrenFromState`, `EditorView::initialise`). It
  creates a first tab only when the focused session has none.

---

## 2. Actions and Input

- **Commands.** A command id is a callback id, owned by the owner of the state that
  its callback changes. `commandManager` is the one registry of operations. ARCHITECT:
  *"commandManager is the SSOT of `operation`."* END's ids are `map::CommandId`
  (`cast/bimaps.md:58-99`). `ENDView::perform` dispatches through its `commands` table,
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
- **`loadPlugin` loads EVE into the focused pane.** `TabView::loadPlugin` takes no argument
  (`TabView.h:59`). It sets the `plugin` row of the focused pane to the EVE file in the plugins
  folder (`Nexus::getOrCreatePlugin`). A file drop splits the pane at the drop edge and sets the
  `plugin` row of the new pane to the dropped file (`EditorView::DropTarget::filesDropped`).
  ARCHITECT: *"there will be command that load any plugin, override eve as the default arg"*.
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
- **END tells a hosted plugin through the host lane.** ENDView tells the panes; it never reaches
  into a plugin. Each tell is a write on the attached tree (section 1.5, plugin state). The mode
  keys are jam's `map::EditorMode`.
  - `normal` and `open` tell the `mode` row of the focused plugin the matching key of
    `map::EditorMode` (`EditorView::setMode`).
  - `focus` follows the `focus` of the pane row.
  - Size is the `width` and `height` rows of the attached `terminal` row. `EditorView::resized`
    writes the pane bounds, reduced by the inset, into them. It writes them again when a new tree
    attaches under the `plugin` row of the pane (`EditorView::valueTreeChildAdded`), because the
    attached tree holds EVE's default size. A hosted VST2 editor is not resizable by its host, so
    the rows are the one way that the pane size reaches the plugin. EVE sets the size of its editor
    from the rows, and its text editor writes its own winsize from that size. ARCHITECT: *"END will
    be the sole author of winsize, the pane size down to text editor size - winsize"*. END also
    sets the editor bounds (`EditorView::setEditorBounds`).
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
   live editors. Session persistence is built (section 1.7).
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
