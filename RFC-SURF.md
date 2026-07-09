# RFC — SURF
## Sovereign URL Rendering Framework

**For:** ARCHITECT, COUNSELOR  
**Date:** July 2026  
**Status:** Ready for COUNSELOR handoff  
**Depends on:** Whelmed completion, CSS→LookAndFeel parser, jam::markdown, jam::mermaid

---

## Problem Statement

END is a developer-centric environment where terminal workflows routinely require opening a browser — reading documentation, browsing GitHub issues, checking Stack Overflow, authenticating OAuth flows, reviewing remote git repos. Each context switch to a browser breaks flow. A full browser engine (WebKit, Chromium) is architecturally hostile: opaque lifecycle, async black box, unbounded state, massive binary footprint. All of these violate BLESSED.

SURF is a bloat-free reader that lives inside END as a PaneView entity. It fetches web content, extracts readable text, and renders it natively through the existing Whelmed/jam::markdown pipeline. It is not a browser. It is a reader with fetch.

---

## Architecture Overview

```
            ┌─────────────────────────────────────────────┐
            │                   SURF                       │
            │              (PaneView entity)               │
            ├─────────────────────────────────────────────┤
            │                                              │
            │   URL Bar ─── Navigation Stack ─── Config    │
            │       │        (ephemeral)        (lua)      │
            │       ▼                                      │
            │   ┌───────┐                                  │
            │   │ Fetch  │  juce::URL / libcurl            │
            │   └───┬───┘                                  │
            │       │ raw HTML                             │
            │       ▼                                      │
            │   ┌───────┐                                  │
            │   │ Parse  │  jam::gumbo (vendored)          │
            │   └───┬───┘                                  │
            │       │ GumboNode* tree                      │
            │       ▼                                      │
            │   ┌────────────┐                             │
            │   │ Readability│  Tier 1: QuickJS + DOM shim │
            │   │  Extract   │  Tier 2: node (if >=22)     │
            │   └───┬────────┘                             │
            │       │ clean HTML subtree                   │
            │       ▼                                      │
            │   ┌──────────┐                               │
            │   │ Convert  │  HTML → Markdown              │
            │   └───┬──────┘                               │
            │       │ Markdown string                      │
            │       ▼                                      │
            │   ┌──────────┐                               │
            │   │ Whelmed  │  jam::markdown / jam::mermaid │
            │   │ Render   │  CSS→LookAndFeel styling      │
            │   └──────────┘  juce::Graphics paint         │
            │                                              │
            └─────────────────────────────────────────────┘
```

Six pure transforms. Each stage's output is the next stage's only input. No persistent state between them.

---

## Components

### 1. Fetch

**Owner:** SURF component  
**Implementation:** `juce::URL` for simple GET. Consider libcurl for advanced cookie handling, streaming, and redirect control via `CURLOPT_FOLLOWLOCATION`.

**Cookie jar:** `juce::StringPairArray` held in memory. Injected into request headers. Populated from HTTP `Set-Cookie` response headers and from auth popup cookie extraction. Dies with SURF instance. Never persisted to disk.

**Redirects:** Handled by transport layer. Transparent to pipeline.

**Threading:** Fetch runs off the message thread. Standard JUCE async work pattern — fetch on background thread, deliver result to message thread for parse.

### 2. Parse — jam::gumbo

**Owner:** jam module  
**Implementation:** Google's gumbo-parser vendored as `jam::gumbo`. Pure C99, zero external dependencies. Single compilation unit.

**Input:** Raw HTML bytes (UTF-8).  
**Output:** `GumboOutput*` containing `GumboNode*` tree.

**Contract:** Parse is synchronous, deterministic, allocation-bounded. Gumbo handles malformed HTML per HTML5 error recovery spec. The tree is read-only after parse — no mutation by downstream stages except Readability (which operates on its own copy or through the JS shim).

### 3. Readability Extract — Tiered Engine

Content extraction uses Mozilla's Readability algorithm: score DOM nodes by text density, tag type, classname heuristics, link ratio. Strip navigation, ads, sidebars, footers. Output clean HTML subtree containing only the article content.

#### Tier 1 — QuickJS (always available)

`juce::JavascriptEngine` (QuickJS, ES2023 compliant, already vendored in JUCE 8).

**DOM shim:** Expose `GumboNode*` tree to QuickJS via `registerNativeObject`. Minimal surface — only the DOM APIs Readability.js actually touches:

- Tree navigation: `parentNode`, `childNodes`, `nextSibling`, `previousSibling`
- Properties: `tagName`, `className`, `id`, `textContent`, `innerHTML`
- Query: `querySelector`, `querySelectorAll` (CSS selector subset)
- Attributes: `getAttribute`, `setAttribute`
- Mutation: `remove()`, `replaceChild()`, `appendChild()`
- Creation: `createElement`, `createTextNode`

**Readability.js** bundled as binary resource. Loaded into QuickJS at extraction time. Output retrieved via `evaluate()`.

**Security:** QuickJS runs in-process. No filesystem, no network, no OS access. Only the DOM surface explicitly exposed via `registerNativeObject`. `maximumExecutionTime` prevents infinite loops. Attack surface limited to gumbo parser bugs (fuzz-tested, battle-hardened).

#### Tier 2 — Node.js (if available, >=22 LTS)

**Detection:** `which node` at SURF initialization. Version check: parse `node --version` output, gate on `>=22.0.0`.

**Invocation:**

```cpp
juce::ChildProcess proc;
proc.start ("node --permission --allow-fs-read=/path/to/surf-extract.js /path/to/surf-extract.js");
proc.getProcessInput()->write (htmlBytes, htmlSize);
proc.getProcessInput()->close();
auto result = proc.readAllProcessOutput();
```

**Bundled script:** `surf-extract.js` — single file, esbuild-bundled, containing linkedom (~100KB, lighter than JSDOM) + Readability.js. Embedded as binary resource, written to known path with integrity hash verification on first use.

**Security mitigations:**
- `--permission` flag restricts filesystem, network, child process
- `--allow-fs-read` scoped to script path only
- No `--allow-fs-write`, no `--allow-child-process`, no `--allow-worker`
- `runScripts` never enabled in linkedom — `<script>` tags parsed as inert DOM nodes
- Script integrity verified via embedded hash before invocation

**Lifecycle:** `juce::ChildProcess` owns the node process. SURF owns the `ChildProcess`. Bounded, RAII. Process spawned per extraction, reaped on completion. No long-lived daemon.

**Contract parity:** Both tiers accept raw HTML, output clean HTML. SURF component never knows which tier ran. Same downstream pipeline.

#### Tier Selection

```cpp
// At SURF initialization
const bool nodeAvailable = detectNode();  // which node + version >= 22

// At extraction time
juce::String cleanHtml = nodeAvailable
    ? extractWithNode (rawHtml)
    : extractWithQuickJS (rawHtml);
```

Detection once at init, not per call. Stored as `const bool`. No runtime switching.

**Isomorphic contract — same pattern as jam's GPU tier:**

| Capability     | Available          | Absent              |
|----------------|--------------------|----------------------|
| Vulkan         | LLGC (GPU path)    | juce::Graphics       |
| Node >=22      | Spawn + linkedom   | QuickJS + DOM shim   |

Enhanced experience when available. Never fails when absent.

### 4. Convert — HTML to Markdown

**Input:** Clean HTML subtree from Readability.  
**Output:** Markdown string.

Tree walk on the clean HTML. Tag-to-syntax mapping:

| HTML              | Markdown                          |
|-------------------|-----------------------------------|
| `<h1>`–`<h6>`    | `#`–`######`                      |
| `<p>`             | Text block + blank line           |
| `<a href="...">`  | `[text](url)`                     |
| `<img>`           | `![alt](src)`                     |
| `<strong>`, `<b>` | `**text**`                        |
| `<em>`, `<i>`     | `*text*`                          |
| `<code>`          | `` `text` ``                      |
| `<pre><code>`     | Fenced code block                 |
| `<ul>/<li>`       | `- item`                          |
| `<ol>/<li>`       | `1. item`                         |
| `<blockquote>`    | `> text`                          |
| `<table>`         | GFM table syntax                  |
| `<hr>`            | `---`                             |

Deterministic tree walk. No state between elements. Pure function: `juce::String htmlToMarkdown (const GumboNode* cleanTree)`.

### 5. Render — Whelmed

Existing Whelmed component powered by `jam::markdown` and `jam::mermaid`. CSS→LookAndFeel styling applied. `juce::Graphics` paint. Proportional text, images, code blocks, diagrams — all through established pipeline.

**Images:** URLs extracted as Markdown `![alt](src)` syntax. Whelmed's image handler fetches `juce::URL` → `juce::Image`. Lazy by default — fetch triggered when image component enters `juce::Viewport` visible bounds via `visibleAreaChanged()`.

**Configuration (whelmed.lua):**

```lua
surf = {
    fetch_image = true,
    fetch_image_lazy = true,
}
```

`fetch_image = false` renders `alt` text only. `fetch_image_lazy = false` fetches all images at parse time (eager).

### 6. Navigation

**Stack:** `jam::Array<juce::URL>` + `int currentIndex`.  
**Operations:** Back (decrement index, re-fetch or cache), forward (increment), navigate (push URL, truncate forward stack).

**Ephemeral.** Stack lives and dies with the SURF PaneView instance. No history written to disk. No session restore. Closing SURF destroys all navigation state, cookie jar, and cached content. Identical to incognito semantics.

**Link handling:** Clickable regions in Whelmed rendered content. Click triggers navigation — push URL to stack, fetch, parse, extract, render.

### 7. Auth Popup

For pages requiring authentication (OAuth, login forms).

**Implementation:** Scoped `juce::WebBrowserComponent` inside an END popup. Short-lived: create → load login URL → user interacts → extract cookies → destroy.

**Cookie bridge:** After auth completes, extract cookies from `WKHTTPCookieStore` (macOS) / WebView2 cookie manager (Windows). Write into SURF's in-memory cookie jar. Unidirectional flow — WebView is auth source, SURF cookie jar is SSOT for the reader's fetch session.

**Lifecycle:** Popup owns the WebBrowserComponent. Popup is modal to SURF. Popup close triggers cookie extraction then destruction. No long-lived WebView.

---

## Existing Building Blocks

### Have — zero work:

| Browser Concern          | Existing Solution                              |
|--------------------------|------------------------------------------------|
| HTTP fetch + TLS         | `juce::URL`                                    |
| HTML-like tree structure | `juce::XmlElement`                             |
| Flexbox layout           | `juce::FlexBox`                                |
| Grid layout              | `juce::Grid`                                   |
| Proportional text        | `juce::Graphics` + `juce::Font` + `juce::AttributedString` |
| Rich text editing        | `jam::TextEditor`                              |
| Image decode             | `juce::Image` (PNG, JPEG, GIF)                 |
| Scrollable viewport      | `juce::Viewport`                               |
| Component hierarchy      | `juce::Component`                              |
| Theming                  | `juce::LookAndFeel` + colourId                 |
| SVG shapes               | StyledGraphics (juce::Path, colour, strokes)   |
| Markdown render          | `jam::markdown`                                |
| Diagram render           | `jam::mermaid` (Descriptor-based)              |
| JSON/XML parse           | `juce::JSON` / `juce::XmlDocument`             |
| Native windows/popups    | jam (glassmorphism, NSSheet, popups)            |
| GPU rendering            | LLGC (Vulkan, vendored SDK)                    |
| JavaScript engine        | `juce::JavascriptEngine` (QuickJS, ES2023)     |
| Child process management | `juce::ChildProcess`                           |

### Need to build:

| Component                    | Scope                                            |
|------------------------------|--------------------------------------------------|
| `jam::gumbo`                 | Vendor gumbo-parser as jam module                |
| DOM shim for QuickJS         | Minimal DOM surface for Readability.js (~15 APIs)|
| Readability.js bundle        | Embed as binary resource                         |
| `surf-extract.js`            | esbuild bundle: linkedom + Readability.js        |
| HTML→Markdown converter      | Pure tree walk, tag-to-syntax map                |
| Node detection + gating      | `which node` + version parse, `>=22`             |
| Cookie jar                   | `juce::StringPairArray`, in-memory, ephemeral    |
| SURF PaneView component      | Navigation stack, URL bar, Whelmed integration   |
| Auth popup                   | Scoped WebBrowserComponent + cookie bridge       |

---

## Security Model

### Threat Surface

SURF accepts untrusted HTML from the internet. The pipeline must ensure that untrusted content never achieves code execution or state mutation beyond the renderer.

### Guarantees

1. **No `<script>` execution.** Website JavaScript is never executed. In Tier 1, QuickJS only runs the bundled Readability.js. In Tier 2, linkedom does not execute script tags.

2. **No eval of untrusted strings.** Only the bundled extraction script is loaded into either engine. Raw HTML is data, never code.

3. **Tier 1 blast radius:** END process only. QuickJS has no OS access beyond explicitly registered DOM shim functions. Equivalent risk to any bug in END itself.

4. **Tier 2 blast radius:** Mitigated by `--permission`. Node process cannot write filesystem, spawn children, or access network. Restricted to reading the bundled script file.

5. **Script integrity:** `surf-extract.js` embedded as binary resource with hash. Written to disk on first use. Hash verified before every invocation. Mismatch → fallback to Tier 1.

6. **Cookie jar isolation:** Memory only. No disk persistence. No cross-session leakage.

7. **Auth popup isolation:** WebBrowserComponent lifecycle bounded by popup. Cookie extraction is read-only from WebView → write to SURF jar. No reverse flow.

### Risk Acknowledgement

END owns the security posture of SURF. If crafted HTML triggers a parser vulnerability in gumbo (Tier 1) or linkedom (Tier 2), END facilitated the vector. Mitigations reduce blast radius but do not eliminate risk.

---

## BLESSED Compliance

| Pillar          | Status | Notes |
|-----------------|--------|-------|
| **B**ounds      | ✓      | SURF PaneView owns all child state. Cookie jar scoped to instance. Node process lifecycle RAII via `juce::ChildProcess`. Navigation stack scoped to PaneView lifetime. |
| **L**ean        | ✓      | No browser engine vendored. gumbo is one C99 file. QuickJS already in JUCE. DOM shim is the minimal surface Readability touches. Node path uses linkedom (~100KB) not JSDOM (~15MB). |
| **E**xplicit    | ✓      | Tier detection at init, not per-call. All configuration visible in `whelmed.lua`. No hidden globals. No magic values. Pipeline stages have explicit input→output contracts. |
| **S**SOT        | ✓      | Cookie jar is SSOT for auth state. Navigation stack is SSOT for history. `jam::markdown` is SSOT for rendered content. No shadow state between tiers. |
| **S**tateless   | ✓      | Each pipeline stage is a pure transform. Fetch → Parse → Extract → Convert → Render. No stage remembers previous invocations. Cookie jar is session state owned by SURF, not machinery state. |
| **E**ncapsulation | ✓   | SURF component exposes `navigate(juce::URL)`. Internals (tier selection, gumbo tree, DOM shim, extraction engine) are private. Whelmed receives Markdown, never raw HTML. |
| **D**eterministic | ✓   | Same HTML input → same Markdown output → same rendered content. Tier selection is `const bool` set at init. No runtime variance. |

---

## Open Questions

1. **DOM shim scope.** `querySelector`/`querySelectorAll` requires a CSS selector matching engine. Audit Readability.js source to determine the exact selector patterns used — may be a small subset (tag, class, id, attribute) rather than full CSS selector spec. This bounds the shim complexity.

2. **linkedom vs alternatives.** linkedom is ~100KB and sufficient for Readability. Verify Readability.js compatibility with linkedom vs JSDOM. If linkedom has gaps, evaluate `happy-dom` as alternative.

3. **Auth popup platform parity.** `WKHTTPCookieStore` cookie extraction is macOS API. Windows equivalent via WebView2 `ICoreWebView2CookieManager`. Linux (WebKitGTK) cookie access API needs verification.

4. **Whelmed image handler.** Does `jam::markdown` currently handle `![alt](url)` with async network fetch? If not, this is new capability in the markdown renderer, not just SURF.

5. **URL bar design.** Keyboard-driven? Autocomplete from navigation stack? Paste-only? This is a UX decision for ARCHITECT.

---

## Handoff Notes for COUNSELOR

- SURF is not a browser. Do not add features that make it one. The scope boundary is: fetch, extract readable content, render as Markdown. Everything else is out of scope.
- The DOM shim is the riskiest component. Start by auditing Readability.js to enumerate every DOM API it calls. Build the shim to that exact surface. Nothing more.
- Tier 2 (Node) is optional. SURF must be fully functional with Tier 1 only. Never add a hard dependency on Node.
- The auth popup is a separate component from the reader. It can ship later. SURF without auth covers the majority use case (public documentation, public repos, public discussions).
- `jam::gumbo` module setup should follow existing jam module conventions. Consult ARCHITECT for module boilerplate.
- The HTML→Markdown converter is the simplest piece. Implement first as a smoke test for the full pipeline.
- Cookie jar is intentionally ephemeral. If ARCHITECT later decides persistence is needed, that is a new RFC, not a modification to this one.
