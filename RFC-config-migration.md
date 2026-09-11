# RFC: END Config Migration — Lua → Markdown/CSS Document Pipeline

**Status**: Accepted — ARCHITECT-ruled 2026-08-28 (jam session `jam-header-TU`)
**Applies to**: END `Source/config/`, shell integration, terminal-protocol dispatch.
**Companion**: `jam/PLAN-module-split.md` Step 5 (jam_lua deletion) + Successor section.

---

## 1. Abstract

END drops Lua entirely. Configuration moves to markdown tables parsed by
`jam::MarkdownDocument`; styles move to CSS parsed by `jam::CssDocument`; both flow
through one new `Document → juce::ValueTree` conversion. User-facing scripting moves
to the shell via an END-tagged terminal protocol on the already-shipping SKiT dispatch
path. No scripting language is embedded, now or later.

## 2. Rulings (ARCHITECT, 2026-08-28)

1. **Drop Lua entirely.** `jam_lua` (binding layer + vendored Lua 5.4.7) is deleted
   from the jam tree — jam `PLAN-module-split.md` Step 5. Verified before ruling:
   END is the sole consumer of jam_lua (grep across end/tit/cake/whatdbg/caroline —
   only `end/CMakeLists.txt`, `end/Source/generated/Lexicon.*`; the lone cake hit is
   a generated glob cache). Zero in-jam dependents.
2. **END breakage accepted.** Core engine is untouched; the refactor is confined to
   the config side, which is already encapsulated (see §4).
3. **Actions become data.** User-defined actions are declarative macro rows
   (REAPER/SWS Cycle Action / Photoshop Action model), not programs.
4. **Extensibility is protocol, not language.** The api is exposed as END-tagged
   terminal escape sequences (§7) — scaled from the existing SKiT precedent — plus,
   at the module level, CLAP (END-as-HOST direction; `jam_clap` host side exists).

## 3. Motivation — why Lua loses on every axis

| Axis | Lua (current) | Document pipeline (target) |
|---|---|---|
| Parse engine | Vendored VM, single-pass source→bytecode (`lparser.c`) — **no AST exists** | Sealed `jam::Document` AST, complete at creation |
| SSOT | Would need a second hand-written parser for any `LuaDocument`; VM stays for closures → two truths for one format | One Document family (Markdown/Xml/Css), CAST-proven |
| Determinism | Config read = arbitrary execution; `luaL_openlibs` opens io/os (`jam_LuaState.h:133`) | Static parse; same bytes → same AST → same tree |
| Error lines | Bytecode-decoding VM hook (`lineHook` + `LineMapBuilder`, `jam_LuaState.h:55-110`) exists only to recover lines execution throws away | `Id::line` stamped on table rows at parse (`jam_MarkdownDocument.h:66-82`) |
| Typing | `Tag`-typed values → typed `jam::Parameter` (`jam_lua_utils.cpp:60-115`) | Typed once at the single conversion point via existing `Validators` |
| Lean/YAGNI | Entire language runtime retained for one consumer's config | Deleted |

The only capability Lua uniquely delivered — user-authored logic in actions — was
never used: the sole shipped actions (`display.lua:312-332`, `split_thirds_h/v`) are
constant-argument api call sequences. No conditional, no computed value, no state in
the observed corpus. The `function()` wrapper contributed nothing but the VM
dependency.

## 4. Current State (verified inventory)

**Lua usage in END is confined to `Source/config/`:**
- `ConfigModel.h` / `ConfigModel.cpp` — `jam::lua::fromLua` call sites;
  `ConfigTheme` builds its THEMES tree via `fromLua` from `Id::FileThemes`
  BinaryData lua (`ConfigModel.h:95-100`).
- `ConfigDirectory.h` — lua-parse error channel in the `loadFromPath` contract.
- `Source/generated/Lexicon.h/.cpp` — jam_lua references.
- `end/CMakeLists.txt` — module link.

**Config corpus (5 files):**
- `Source/config/lua/display.lua` — scalars + nested tables (graphics, shell,
  terminal, hyperlinks, image) + `actions` table with `execute = function()` closures.
- `Source/config/lua/keys.lua` — purely declarative key-binding strings.
- `Source/config/lua/popup.lua` — declarative.
- `Source/config/lua/theme/gfx/theme.lua`, `whelmed.lua` — theme values.

**Swap point:** the `loadFromPath (const juce::var& path, juce::String& errors)`
override family on `ConfigDirectory` subclasses. Downstream of it, everything is
`juce::ValueTree` + `jam::Model::createAndAddParameter` — untouched by this RFC.

**Existing jam machinery this migration consumes (nothing new invented):**
- `jam::MarkdownDocument` — tokenizer/tree constructor over `jam::Document` + the
  markdown **table query API** (`jam_MarkdownDocument.h:10-14`); CAST is the working
  precedent for table-driven data.
- `jam::CssDocument : Document` (`jam_Css.h:7`) — CSSOM-shaped `StyleDeclaration`
  read API.
- `jam::XmlDocument::toDocument` (`jam_XML.h:87`) — XmlElement→Document (reverse
  bridge, for round-tripping; not on the config read path).
- `juce::ValueTree::createXml() / fromXml()` — framework persistence round trip,
  free. Note: `fromXml` yields all-string properties — it is NOT a substitute for
  the typed conversion below.
- `Validators` machinery keyed (tag → id) — currently populated by
  `jam_lua_utils.cpp:85-120`; survives as the typing authority.

## 5. Design — config read path

```
config .md / .css (BinaryData seed or disk)
    → MarkdownDocument::parse / CssDocument parse      (sealed AST, Id::line stamped)
    → Document → juce::ValueTree conversion             (ONE new method, jam-side)
    → jam::Model::createAndAddParameter                 (existing, unchanged)
```

- **One new method** (jam-side): recursive walk of the Document Element tree
  (root sentinel, link-chain children, typed property variants) → ValueTree. Same
  shape as the retired `Type::forEach` walk in `jam_lua_utils.cpp`, sourced from a
  sealed AST instead of a live VM.
- **Typing authority**: applied exactly once, at this method, via the `Validators`
  map. Markdown cells arrive as text; the validator/schema types them. One method,
  one typing point — SSOT holds; D follows.
- **Error reporting**: `Id::line` from the AST replaces the entire `LineMap`
  machinery; error lines keep their current `"<line> 'key: message'"` shape.
- **Styles**: theme values that are visual styling move to CSS via `CssDocument`'s
  `StyleDeclaration`; scalar config stays in markdown tables. The split boundary
  (what is "style" vs "config") is an ARCHITECT call during PLAN.
- Config comments/prose live as ordinary markdown around the tables — the
  documentation-heavy character of the current lua files (generated with extensive
  comments) is preserved natively.

## 6. Design — actions as macro rows

Actions become one markdown table per macro: ordered `(step, api, args)` rows —
the Cycle Action model. The shipped examples translate as:

```
## split_thirds_h — Split Horizontal Thirds

| step | api              | args            |
|------|------------------|-----------------|
| 1    | split_with_ratio | vertical, 0.333 |
| 2    | split_with_ratio | vertical, 0.5   |
```

Name/description/modal/global columns (or adjacent rows) carry the current action
fields (`display.lua:301-308`). A condition column is deliberately deferred — YAGNI
until a real use case arrives. The api vocabulary is the existing one
(`display.lua:286-299`): `split_horizontal`, `split_vertical`, `split_with_ratio`,
`new_tab`, `close_tab`, `next_tab`, `prev_tab`, `focus_pane`, `close_pane`.

## 7. Design — api over terminal protocol (scripting without a language)

**Shipped precedent (verified in-tree):** the SKiT filepath preview protocol already
carries an END-tagged verb end-to-end — payload prefix `END;` on DCS (final byte
`q`) and OSC 1337, `GEND;` on APC → `sendPreviewFile()` → `Id::previewFile` event →
GUI (`jam_TerminalSkit.h:72-73, 90-91, 105-106`). Opt-in by construction: no
trampoline assigned → member no-ops, passthrough untouched (`jam_TerminalSkit.h:22-23`).

**Scaling it — wire format:**

```
ESC ] 1337 ; END ; <token> ; <verb> ; <arg> ; <arg> BEL
```

**Shell helper** — injected by END's existing shell-integration scripts
(`display.lua:195-202` mechanism), never hand-authored by users:

```zsh
end() { printf '\e]1337;END;%s;%s\a' "$END_TOKEN" "${(j:;:)@}" }
```

**User script** (replaces the Lua action verbatim in capability):

```zsh
end split_with_ratio vertical 0.333
end split_with_ratio vertical 0.5
```

Composes with the entire shell (loops, pipes, other CLIs) and traverses ssh
unchanged — capabilities config-scoped Lua never had.

**Security model (Guard Rule — threat named: escape injection).** The tty carries no
sender identity; `cat hostile.txt` writes the same stream, local or remote — ssh is
not a distinct threat class. Gates, both single-owner decisions at the dispatch
table:
1. **Capability allowlist** — opt-in per verb (kitty `allow_remote_control` / OSC 52
   model). Query verbs may default on; mutating verbs (split/close/spawn) default off.
2. **Shared-secret token** — END generates a session token, exports it into the
   child shell environment at spawn; mutating sequences must carry it (kitty
   password model). Integration scripts deliver it; injected files don't have it.
3. **Bounded grammar** — length-capped payloads; no verb executes or writes outside
   END's own state.

**SSOT:** the protocol verb table IS the api vocabulary. Macro table rows (§6)
reference the same verbs — config macros are recorded protocol. One vocabulary, two
entry points.

## 8. Deletions

| Artifact | Where |
|---|---|
| `jam_lua/` module (binding + vendored Lua 5.4.7) | jam tree — `PLAN-module-split.md` Step 5 |
| `fromLua` call sites, lua error channel | `end/Source/config/` |
| `Source/config/lua/*.lua` corpus | replaced by `.md`/`.css` equivalents |
| jam_lua link + Lexicon references | `end/CMakeLists.txt`, `Source/generated/Lexicon.*` |
| `lineHook`/`LineMapBuilder` machinery | dies with jam_lua |

Refactor-Rewrite Discipline applies: delete first, implement after; no coexistence.

## 9. Open decisions (ARCHITECT gates — NAMES.md Rule -1)

1. Config file names and layout (`config.md`? per-domain files mirroring the current
   display/keys/popup/theme split?).
2. The style/config boundary — which theme values become CSS vs markdown rows.
3. Verb names beyond the existing api vocabulary; the shell helper name (`end`);
   the token variable name (`END_TOKEN` used illustratively above).
4. Macro table schema (column names, where name/description/modal/global live).
5. The jam-side conversion method's name and home (Document member vs per-domain
   static) — nearest-sibling precedence applies once PLAN is drafted.
6. Typing schema location — Validators populated from where, now that
   `jam_lua_utils.cpp` dies.

## 10. Consequences

**Positive**: one parse pipeline for every jam text format; config read is static
and deterministic; VM/io/os attack+complexity surface gone; error lines improve;
automation lands on the shell where it composes; api SSOT spans config and protocol;
END moves cleanly toward HOST/CLAP where compiled plugins — not scripts — are the
strong extension form.

**Negative**: arbitrary user scripting inside config files is gone (by ruling — the
observed corpus never used it); END is broken from jam_lua deletion until this
migration lands; config seed files must be regenerated as markdown/CSS.

**Rejected alternatives** (with grounds):
- `jam::LuaDocument` — requires a second hand-written Lua parser (the VM has no AST
  to wire to) while the VM stays for closures: two truths for one format (S
  violation).
- markdown → XML → ValueTree read path — `Document → XmlElement` serializer doesn't
  exist either, and `ValueTree::fromXml` is all-string: the retyping pass it forces
  IS the direct conversion. XML remains the persistence face only.
- Keep Lua for actions only — an entire language runtime for two constant-argument
  call sequences (YAGNI).

---

*This RFC is the context handoff for the END-side COUNSELOR session. RFC fidelity
rule applies: every point above maps to a PLAN step or an explicit ARCHITECT
descope.*

**JRENG!**
