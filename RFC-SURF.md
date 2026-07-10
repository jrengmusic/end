# RFC — SURF
## Sovereign URL Rendering Framework

**For:** ARCHITECT, COUNSELOR  
**Date:** July 2026  
**Status:** Ready for COUNSELOR handoff  
**Depends on:** Whelmed completion (includes CSS→LookAndFeel parser, jam::markdown, jam::mermaid)

---

## Problem Statement

END is a developer-centric environment where terminal workflows routinely require a browser — reading documentation, browsing GitHub issues, reviewing remote git repos, authenticating OAuth flows, checking Stack Overflow. Each context switch breaks flow. Full browser engines (WebKit, Chromium) are architecturally hostile: opaque lifecycle, async black box, unbounded state, massive binary footprint, all violating BLESSED.

SURF solves this by extracting readable content from web pages and rendering it natively through END's existing Whelmed/jam::markdown pipeline. It is not a browser. It is a reader with fetch.

---

## Architecture Overview

### Pipeline

```
URL (user input or link click)
  │
  ▼
Fetch ── juce::URL (GET, TLS, redirects, cookies)
  │
  │ raw HTML bytes
  ▼
Extraction Engine ── juce::JavascriptEngine (QuickJS) or Node >=22
  │                    │
  │                    ├── linkedom (pure JS DOM, parses HTML via htmlparser2)
  │                    ├── Defuddle (pure JS content extractor)
  │                    └── polyfills (self, Buffer.from, URL, atob, performance.now)
  │
  │ Markdown string
  ▼
jam::markdown ── native parse
  │
  ▼
Whelmed ── native render (juce::Graphics, proportional text, LookAndFeel)
```

Five stages. JS owns extraction. Native owns rendering. Clean boundary.

### Extraction Engine — Tiered Runtime

Same JS bundle, different host. Isomorphic contract — identical to jam's GPU tier:

| Capability | Available | Absent |
|---|---|---|
| Vulkan | LLGC (GPU path) | juce::Graphics |
| Node >=22 + sandbox | Spawn process, V8 JIT | QuickJS in-process |

```
Node >=22 + --permission succeeds  →  sandboxed Node (V8, JIT, faster)
Node >=22 + --permission fails     →  QuickJS (safe fallback)
Node <22 or absent                 →  QuickJS (always works)
```

Node is not a dependency. It is an environment capability, detected and exploited when present. Configurable:

```lua
surf = {
    use_node = "auto",       -- "auto" | "never"
    fetch_image = true,
    fetch_image_lazy = true,
}
```

`auto` = detect and use if sandboxing succeeds. `never` = user deliberately opts out.

END never executes Node without `--permission` sandboxing. If sandboxing fails for any reason, END falls back to QuickJS silently. The reader component never knows which tier ran.

### Component Architecture

SURF is a PaneView entity — the same base component shared by terminal and other END views. SURF is Whelmed on steroids: same rendering architecture, same `jam::markdown` backend, different input source and mutability.

**Encapsulation:**

```
Whelmed (editor)  →  local files    →  read/write  →  jam::markdown → juce::Graphics
SURF   (reader)   →  local/remote   →  read-only   →  jam::markdown → juce::Graphics
```

Same renderer. Different input. Different mutability. Rendering pipeline is SSOT.

SURF can read local files (e.g. doxygen HTML/CSS output) and remote URLs. The extraction pipeline is the same regardless of source.

---

## Key Discovery — linkedom + Defuddle in QuickJS

The original RFC proposed a C++ DOM shim, a C++ Readability port, and vendored gumbo-parser. All eliminated.

**linkedom** is a pure JavaScript DOM implementation (~250KB). Triple-linked list architecture. Implements `document`, `querySelector`, `querySelectorAll`, `createElement`, `textContent`, `innerHTML`, `className`, `parentNode`, `childNodes`, `getAttribute`, DOM traversal, DOM mutation. Designed for DOM-less environments. Not spec-complete, not trying to be. Bundles `htmlparser2` for HTML parsing — no external native parser needed.

**Defuddle** is a pure JavaScript content extractor by Kepano (Obsidian creator). Successor to Mozilla's Readability. Extracts article content, strips navigation/ads/chrome, outputs clean HTML or Markdown directly. Uses linkedom as its DOM. ~430KB bundled together with linkedom.

**go-defuddle** (by vaayne) proved this stack runs inside QuickJS without Node.js or browser. Bundled as a single ~430KB JS file via webpack with five trivial polyfills. Performance: ~450ms cold start (one-time), ~95ms per parse.

For SURF: same pattern, JUCE host. Bundle linkedom + Defuddle + polyfills via esbuild into one `.js` file. Embed as binary resource in END. Load into `juce::JavascriptEngine` (QuickJS, ES2023 compliant, vendored in JUCE 8). Feed HTML string in, get Markdown string out.

**What this eliminates:**

| Originally planned | Status |
|---|---|
| jam::gumbo (vendored C99 HTML parser) | KILLED — linkedom bundles htmlparser2 |
| C++ DOM shim for QuickJS (~15 APIs) | KILLED — linkedom IS the DOM |
| Readability C++ port (~1500 lines) | KILLED — Defuddle runs unmodified |
| CSS querySelector engine in C++ | KILLED — linkedom implements it |
| Native HTML→Markdown converter | KILLED — Defuddle outputs Markdown |
| Node.js as capability tier for DOM | DEMOTED — optimization only, not capability |

---

## Components

### 1. Extraction Bundle

**File:** `surf-extract.js` — single esbuild-bundled file.  
**Contents:** linkedom + Defuddle + polyfills.  
**Size:** ~430KB.  
**Storage:** Embedded as binary resource in END. Written to known path on first use with SHA-256 integrity check.

**Entry point:**

```javascript
function extract(html) {
    const { document } = parseHTML(html);
    const result = new Defuddle(document).parse();
    return result.markdown;
}
```

Pure function. Document created, scored, converted, returned, discarded. No state between calls.

**Polyfills required (five, all trivial):**

| Global | Purpose |
|---|---|
| `self` | linkedom expects global reference |
| `Buffer.from()` | Base64 encoding in some extraction paths |
| `URL` | URL parsing/resolution for relative links |
| `atob()` | Base64 decoding |
| `performance.now()` | Timing (can stub to `Date.now()`) |

**Build process:**

```bash
npx esbuild surf-entry.js --bundle --platform=neutral --outfile=surf-extract.js
```

Custom entry point required — Defuddle's shipped bundles assume browser DOM or Node `require()`, neither of which exist in QuickJS standalone.

### 2. Engine Host — juce::JavascriptEngine

**Lifecycle:** One global `juce::JavascriptEngine` instance, owned by END (not by SURF). Bundle loaded once at END startup. ~450ms cold start amortized across entire session. Every `evaluate("extract(html)")` call runs warm. Instance outlives any SURF PaneView.

**Ownership:** END → JavascriptEngine (singleton). SURF components call into it. Engine is shared, stateless between calls. Each extraction is a pure function invocation.

**JUCE 8 confirmation:** `juce::JavascriptEngine` is a wrapper around QuickJS, ES2023 compliant. Not the pre-JUCE-8 toy interpreter. Full closures, classes, async/await, Promises, generators, modules. Verified in JUCE BREAKING_CHANGES.md.

### 3. Node Tier

**Detection at END startup:**

```cpp
// Pseudocode
const auto nodePath = findExecutableOnPath ("node");
const auto nodeVersion = parseVersion (runProcess (nodePath, "--version"));
const bool nodeAvailable = nodePath.isNotEmpty() && nodeVersion >= Version (22, 0, 0);
```

**Invocation:**

```cpp
juce::ChildProcess proc;
proc.start (nodePath + " --permission --allow-fs-read=" + scriptPath + " " + scriptPath);
proc.getProcessInput()->write (htmlBytes, htmlSize);
proc.getProcessInput()->close();
auto markdown = proc.readAllProcessOutput();
```

**Security — mandatory, non-negotiable:**

| Control | Enforcement |
|---|---|
| Minimum version | `>=22 LTS` — checked at detection |
| Sandbox | `--permission` flag — mandatory |
| Filesystem | `--allow-fs-read` scoped to script path only |
| No write | No `--allow-fs-write` |
| No spawn | No `--allow-child-process` |
| No workers | No `--allow-worker` |
| No network | No `--allow-net` (stdin/stdout only) |
| Script integrity | SHA-256 of embedded resource verified before invocation |
| Sandbox failure | Any `--permission` failure → fallback to QuickJS, never unsandboxed Node |

**Same bundle.** Node runs identical `surf-extract.js`. Same linkedom, same Defuddle, same polyfills. Output parity by design.

**Lifecycle:** `juce::ChildProcess` owns the node process. SURF call site owns the `ChildProcess`. Spawned per extraction, reaped on completion. No long-lived daemon. RAII.

**Threading:** Spawn and read on background thread. Deliver Markdown string to message thread for jam::markdown parse and Whelmed render.

### 4. Fetch

**Implementation:** `juce::URL` for GET requests. Handles TLS, redirects.

**Cookie jar:** `juce::StringPairArray` held in memory. Injected into request headers. Populated from HTTP `Set-Cookie` response headers and from auth popup cookie extraction. Ephemeral — dies with SURF instance. Never persisted to disk.

**Threading:** Fetch on background thread, deliver to message thread.

### 5. Navigation

**Stack:** `jam::Array<juce::URL>` + `int currentIndex`.  
**Operations:** Back (decrement, re-fetch or cache), forward (increment), navigate (push, truncate forward).

**Ephemeral.** Stack lives and dies with the SURF PaneView instance. No history to disk. No session restore. Closing SURF destroys all navigation state, cookie jar, and cached content. Incognito semantics.

**Link handling:** Clickable regions in Whelmed rendered content. Click triggers navigation — push URL to stack, fetch, extract, render.

### 6. Image Handling

URLs extracted by Defuddle as Markdown image syntax `![alt](src)`. `jam::markdown` handles image nodes. Whelmed fetches `juce::URL` → `juce::Image`, renders inline with proportional layout.

**Lazy by default.** Fetch triggered when image component enters `juce::Viewport` visible bounds via `visibleAreaChanged()`.

**Configuration (whelmed.lua):**

```lua
surf = {
    fetch_image = true,       -- false = alt text only
    fetch_image_lazy = true,  -- false = eager fetch at parse time
}
```

**Rendering:** Standard `juce::Image` drawn by Whelmed component. No terminal protocol. No Sixel/Kitty. This is a JUCE GUI component.

### 7. Auth Popup

For pages requiring authentication (OAuth, login forms).

**Implementation:** Scoped `juce::WebBrowserComponent` inside an END popup (jam native popup). Short-lived: create → load login URL → user interacts → extract cookies → destroy.

**Cookie bridge:** After auth completes, extract cookies from platform cookie store:
- macOS: `WKHTTPCookieStore`
- Windows: `ICoreWebView2CookieManager`

Write into SURF's in-memory cookie jar. Unidirectional — WebView is auth source, SURF cookie jar is SSOT.

**Lifecycle:** Popup owns the WebBrowserComponent. Popup close triggers cookie extraction then destruction. No long-lived WebView. The WebBrowserComponent reliability problems (crashes, blank screens, re-instantiation failures) apply to long-lived WebViews in DAW hosts, not short-lived auth popups with bounded lifecycle.

**Scope:** Auth popup can ship after core SURF. SURF without auth covers the majority use case (public documentation, public repos, public discussions).

---

## What jam+JUCE Already Provides

| Browser Concern | Existing Solution |
|---|---|
| HTTP fetch + TLS + redirects | `juce::URL` |
| Flexbox layout | `juce::FlexBox` |
| Grid layout | `juce::Grid` |
| Proportional text | `juce::Graphics` + `juce::Font` + `juce::AttributedString` |
| Image decode | `juce::Image` (PNG, JPEG, GIF) |
| Scrollable viewport | `juce::Viewport` |
| Component hierarchy / render tree | `juce::Component` |
| Theming | `juce::LookAndFeel` + colourId |
| SVG shapes | StyledGraphics (juce::Path, colour, strokes) |
| Markdown render | `jam::markdown` |
| Diagram render | `jam::mermaid` (Descriptor-based) |
| CSS→LookAndFeel | Planned, ships with Whelmed |
| JSON/XML parse | `juce::JSON` / `juce::XmlDocument` |
| JavaScript engine (ES2023) | `juce::JavascriptEngine` (QuickJS) |
| Native windows/popups | jam (glassmorphism, NSSheet) |
| Child process management | `juce::ChildProcess` |
| GPU rendering | LLGC (Vulkan, vendored SDK) |

## What Needs to Be Built

| Component | Scope | Complexity |
|---|---|---|
| esbuild bundle (linkedom + Defuddle + polyfills) | Custom entry point, five polyfills | Low |
| Engine host integration | Load bundle into JavascriptEngine, expose `extract()` | Low |
| Node tier detection + invocation | `which node`, version parse, `--permission`, ChildProcess | Low |
| Script integrity verification | SHA-256 embed + verify | Low |
| SURF PaneView component | Navigation stack, URL bar, Whelmed integration | Medium |
| Cookie jar | `juce::StringPairArray`, in-memory, ephemeral | Low |
| Auth popup | WebBrowserComponent + cookie bridge, platform-specific extraction | Medium |
| Image lazy loading in Whelmed | `visibleAreaChanged()` gate on image fetch | Low |

No external native dependencies. One JS bundle (linkedom + Defuddle), embedded as binary resource.

---

## Alternatives Evaluated and Rejected

### juce::WebBrowserComponent as Reader

Wraps platform WebView (WKWebView/WebView2/WebKitGTK). Black box. Async lifecycle END doesn't own. Known crash bugs in DAW contexts. Process model, GC, and navigation state opaque. Violates B (Bounds), E (Explicit), D (Deterministic). Rejected.

### V8 as Embedded JS Engine

~3-4MB stripped. JIT-compiled, faster than QuickJS. But: builds with Chromium's depot_tools/gn, not CMake-native. Per-isolate ~10-20MB memory. Cross-platform build painful. No DOM — same gap as QuickJS. Speed delta on a 1500-line extraction script is single-digit milliseconds. Irrelevant for this workload. QuickJS already vendored in JUCE 8 at zero cost. Rejected.

### Lightpanda as Embedded Library

Architecture philosophically aligned (DOM + JS, no renderer). But: written in Zig, no library API, no static lib export, no C ABI. Standalone binary only. Dependency chain: Zig + Rust (html5ever) + C++ (V8). Three foreign language boundaries. Beta stage. As a subprocess, functionally identical to Node tier but on nobody's machine. Rejected.

### litehtml as Layout Engine

C++ HTML/CSS layout engine with pluggable draw backend (`document_container`). Architecturally clean. But solves a problem SURF doesn't have — SURF converts to Markdown and renders through jam::markdown, not through CSS box model layout. litehtml would be the right tool for pixel-accurate HTML rendering, which is explicitly not SURF's goal. Rejected.

### C++ Readability Port + gumbo-parser + Native DOM Shim

Original RFC approach. ~1500 lines C++ port of Readability algorithm, vendored gumbo-parser (C99), C++ DOM shim implementing ~15 DOM APIs including querySelector (requires CSS selector engine). All eliminated by the discovery that linkedom + Defuddle runs inside QuickJS directly. The JS path is less code, less maintenance, upstream-updatable, and proven by go-defuddle. Rejected in favor of pure JS extraction.

### Node.js as Hard Dependency

Would provide JSDOM + Readability with zero bundling work. But: external runtime END doesn't own. B violation. 50MB+ of V8/libuv if vendored. END must never fail when Node is absent. Rejected as dependency; accepted as optional environment capability with mandatory sandboxing.

---

## Security Model

### Threat Surface

SURF accepts untrusted HTML from the internet. The pipeline must ensure untrusted content never achieves code execution or state mutation beyond the renderer.

### Guarantees

1. **No website JavaScript execution.** `<script>` tags are parsed as DOM nodes by linkedom. Never executed. Defuddle operates on DOM structure only.

2. **No eval of untrusted strings.** Only the bundled extraction script runs. Raw HTML is data input to `parseHTML()`, never code input to `eval()`.

3. **Tier 1 (QuickJS) blast radius:** END process only. QuickJS runs in-process via `juce::JavascriptEngine`. No filesystem, no network, no OS access. The only APIs available are those registered via `registerNativeObject()` — none for this workload. `maximumExecutionTime` prevents infinite loops. Risk equivalent to any bug in END itself.

4. **Tier 2 (Node) blast radius:** Mitigated by `--permission`. Node process restricted: read-only access to script file, stdin/stdout I/O only. No filesystem write, no child process spawn, no worker threads, no network access.

5. **Sandbox failure policy:** If `--permission` invocation fails for any reason — version incompatibility, platform edge case, permission error — SURF falls back to QuickJS. Node is never invoked without sandboxing. No exceptions.

6. **Script integrity:** `surf-extract.js` embedded as binary resource in END with SHA-256 hash. Written to known path on first use. Hash verified before every Node invocation. Mismatch → QuickJS fallback.

7. **Cookie isolation:** Memory only. No disk persistence. No cross-session leakage. Dies with SURF instance.

8. **Auth popup isolation:** WebBrowserComponent lifecycle bounded by popup. Cookie extraction is read-only from WebView → write to SURF jar. No reverse flow.

### Risk Acknowledgement

END owns the security posture of SURF. If crafted HTML triggers a parser vulnerability in htmlparser2 (via linkedom in Tier 1) or linkedom/Defuddle (in Tier 2), END facilitated the vector. Mitigations reduce blast radius but do not eliminate risk. QuickJS Tier 1 contains risk to END's own process — same as any parser bug in any native code path.

---

## BLESSED Compliance

| Pillar | Status | Notes |
|---|---|---|
| **B** Bounds | ✓ | SURF PaneView owns navigation stack, cookie jar, fetch state. JavascriptEngine owned by END (singleton, outlives SURF). Node process RAII via `juce::ChildProcess`. Auth popup owns WebBrowserComponent. All lifecycles traceable. |
| **L** Lean | ✓ | No browser engine. No native DOM. No native HTML parser. One ~430KB JS bundle. QuickJS already in JUCE. Node uses what's on the machine. Zero vendored native dependencies. |
| **E** Explicit | ✓ | Tier detection at startup, stored as `const bool`, not per-call. Configuration in `whelmed.lua`. Pipeline stages have explicit input→output contracts. No hidden globals. `--permission` flags visible in invocation. |
| **S** SSOT | ✓ | Rendering pipeline (jam::markdown → Whelmed → juce::Graphics) is SSOT shared with editor. Cookie jar is SSOT for auth state. Navigation stack is SSOT for history. One JS bundle serves both tiers — output parity by design, not testing. |
| **S** Stateless | ✓ | Each extraction is a pure function: HTML string in, Markdown string out. No state between calls. Engine holds library definitions only (functions, classes), not call state. Cookie jar is session state owned by SURF, not machinery state. |
| **E** Encapsulation | ✓ | SURF exposes `navigate(juce::URL)`. Tier selection, JS engine, extraction internals all private. Whelmed receives Markdown string, never raw HTML. Reader/editor distinction is input source and mutability, not renderer. |
| **D** Deterministic | ✓ | Same HTML → same Markdown → same render. Both tiers execute identical JS bundle. Tier selection is `const bool`. No runtime variance in output. |

---

## Open Questions

1. **Whelmed image handler.** Does `jam::markdown` currently handle `![alt](url)` with async network fetch, or is this new capability? If new, it benefits both SURF and the Markdown editor (remote images in `.md` files).

2. **Auth popup platform parity.** `WKHTTPCookieStore` (macOS) and `ICoreWebView2CookieManager` (Windows) cookie extraction APIs need verification. Linux (WebKitGTK) cookie access API unconfirmed.

3. **URL bar UX.** Keyboard-driven? Paste-only? Autocomplete from navigation stack? ARCHITECT decision.

4. **Defuddle upstream tracking.** Bundle is a snapshot. Update strategy: pin version and manually update, or automate bundle rebuild in CI?

5. **Relative URL resolution.** Defuddle's Markdown output may contain relative URLs for links and images. Resolution against base URL needed before jam::markdown receives the string. Verify whether Defuddle handles this or if post-processing is required.

---

## Handoff Notes for COUNSELOR

- **SURF is not a browser.** The scope boundary is: fetch HTML, extract readable content to Markdown, render natively. Do not add features that make it a browser.
- **JS boundary is extraction only.** Everything after the Markdown string is native C++/JUCE/jam. The JS engine is a black box that takes HTML and returns Markdown. Nothing more.
- **Start with Tier 1 (QuickJS) only.** Get the full pipeline working with `juce::JavascriptEngine` before adding Node detection and invocation. Tier 2 is an optimization, not a capability.
- **Auth popup ships separately.** Core SURF (fetch → extract → render) covers public content. Auth popup is a distinct component that can follow.
- **One esbuild bundle.** Custom entry point required — Defuddle's shipped bundles assume browser DOM or Node `require()`. go-defuddle's approach (custom webpack entry + polyfills) is the reference implementation. Adapt for esbuild.
- **One global JavascriptEngine.** Owned by END, not SURF. Load bundle once at startup. SURF calls `evaluate("extract(html)")`. Engine is shared, extraction is stateless.
- **Test the polyfills.** The five shims (`self`, `Buffer.from`, `URL`, `atob`, `performance.now`) must be verified against JUCE 8's QuickJS. go-defuddle proved them in Wazero's QuickJS — JUCE's wrapper may have different globals.
- **whelmed.lua config keys:** `surf.use_node`, `surf.fetch_image`, `surf.fetch_image_lazy`. Respect existing whelmed.lua conventions.
- **Node tier is never trusted without sandbox.** If `--permission` fails, log it and fallback. No code path ever runs unsandboxed Node.
