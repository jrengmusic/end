# Request For Comment — jam::vk::Shader + jam::vk::RenderPass
Date: 2026-06-29
Status: Ready for COUNSELOR handoff

---

## Problem Statement

END's background shader system (Shadertoy-compatible, multi-pass GLSL) was built on OpenGL. The Vulkan LLGC rewrite owns the full rendering stack. Shader rendering and post-processing need to be re-expressed in Vulkan terms without regressing the existing OpenGL feature set: multi-pass, ping-pong buffers, uniform/channel bimap, runtime compilation.

Additionally, post-processing must be applied at the LLGC flush boundary after all component painting is complete, without leaking shader knowledge into the Peer.

---

## Research Summary

- Existing OpenGL implementation already solves: iChannelN wiring (bimap), custom uniforms, hot-reload, multi-pass topology discovery. Logic is IDENTICAL — only the GPU API layer changes.
- `OpenGLShaderProgram(OpenGLContext&)` is the direct precedent for device resource injection.
- Shadertoy pass execution order: BufferA → BufferB → BufferC → BufferD → Image. Image is mandatory. Others are optional and presence-discovered from files on disk.
- Common is a shared GLSL include, injected at shaderc compile time into all passes. It is not a RenderPass.
- `VkRenderPass` is a first-class Vulkan concept — no renaming needed. `jam::vk::RenderPass` wraps it directly.

---

## Principles & Rationale

### jam::vk::Shader is SSOT for all shader state
One object owns File discovery, shaderc compilation, RenderPass ownership, and uniform/channel binding. Call sites diverge only at the final dispatch — paint vs post. The object itself is call-site agnostic.

### Topology is runtime-discovered, not caller-declared
Files present on disk determine which passes exist. No explicit pass count. Lean — build what the files define, nothing more.

### VulkanContext& is the resource root
`jam::vk::Shader(VulkanContext&)` mirrors `OpenGLShaderProgram(OpenGLContext&)`. `VulkanContext` owns `VkDevice` + VMA. LLGC holds `VulkanContext&`. Shader holds `VulkanContext&`. No resource ownership ambiguity.

### Component owns timer and frame advancement
`juce::Timer` is platform-abstracted and per-instance. Multiple components with different shaders at different frame rates require zero coordination. Component calls `repaint()` on tick. `iTime` is derived at draw time from `juce::Time::getMillisecondCounterHiRes()` — not stored anywhere.

### Post-processing belongs to LLGC::flush(), not ComponentPeer
Peer is platform window plumbing. It has no business knowing shaders exist. LLGC already has the signal: painting is done when `flush()` is called. Post shader runs there, reads the accumulated offscreen image, writes to swapchain.

### RenderPass wraps VkRenderPass directly
No renaming, no aliasing. `jam::vk::RenderPass` is the C++ owner of `VkRenderPass` + `VkFramebuffer`. Buffer passes additionally own a ping-pong `VkImage` pair.

---

## Scaffold

### jam::vk::Shader

```cpp
namespace jam::vk {

class Shader
{
public:
    explicit Shader (VulkanContext&);
    ~Shader();

    // Discovery + compilation. Called once on load or hot-reload.
    // Reads: Common (optional), Image (required), BufferA–D (optional).
    bool loadFromFiles (const juce::File& shaderDirectory);

    // Uniforms and channels — bimap logic carried over from OpenGL implementation verbatim.
    void setUniform  (const juce::String& name, float value);
    void setChannel  (int index, VkImageView);

    // Called by VulkanGraphics::paintShader — renders all passes into provided target region.
    void paint (VkCommandBuffer, VkImageView target, juce::Rectangle<int> clipRect);

    // Called by LLGC::flush — renders all passes into provided offscreen image.
    void applyPost (VkCommandBuffer, VkImageView offscreen, VkExtent2D extent);

private:
    VulkanContext&                    context;
    std::vector<RenderPass>           passes;   // BufferA..D then Image, presence-ordered
    std::string                       commonSource; // injected at compile time, not a pass

    void compilePass (RenderPass&, const std::string& glsl);
    void injectCommon (std::string& glsl) const;
    void updateBuiltinUniforms (VkCommandBuffer, juce::Rectangle<int> rect);
};

} // jam::vk
```

---

### jam::vk::RenderPass

```cpp
namespace jam::vk {

class RenderPass
{
public:
    enum class Type { Buffer, Image };

    RenderPass (VulkanContext&, Type);
    ~RenderPass();

    // Image pass: target injected at draw time (component rect or LLGC offscreen).
    // Buffer pass: owns ping-pong images, target is internal.
    void begin  (VkCommandBuffer, VkImageView injectTarget = VK_NULL_HANDLE);
    void end    (VkCommandBuffer);
    void advance();  // swaps ping-pong index; no-op on Image pass

    // Previous frame output — bound as iChannelN input to downstream passes.
    VkImageView outputView() const;

    VkPipeline  pipeline = VK_NULL_HANDLE;

private:
    VulkanContext& context;
    Type           type;

    // Buffer pass only
    std::array<VkImage,     2> images     {};
    std::array<VkImageView, 2> imageViews {};
    size_t                     pingPong   = 0;

    VkRenderPass   renderPass   = VK_NULL_HANDLE;
    VkFramebuffer  framebuffer  = VK_NULL_HANDLE;
};

} // jam::vk
```

---

### Call sites

```cpp
// Component — component owns Shader instance and juce::Timer
void MyComponent::timerCallback() { repaint(); }

void MyComponent::paint (juce::Graphics& g)
{
    VulkanGraphics::paintShader (g, shader);
    // remaining JUCE paint calls follow
}

// LLGC — single post slot, set once by whoever owns the LLGC
llgc.setPostShader (&postShader); // nullptr = no post pass

// LLGC::flush (internal)
void VulkanLowLevelGraphicsContext::flush()
{
    // ... submit component draw calls ...
    if (postShader)
        postShader->applyPost (cmd, offscreenView, extent);
    present();
}
```

---

## BLESSED Compliance Checklist

- [x] **Bounds** — `Shader` owns `RenderPass` vector. `RenderPass` owns its `VkImage` pair. `VulkanContext` owns device + allocator. Lifetimes are explicit and non-overlapping.
- [x] **Lean** — Topology discovered from files. No speculative pass slots. Template `RenderPass<N>` rejected — runtime vector is sufficient.
- [x] **Explicit** — No implicit pass ordering. No hidden uniform injection. `advance()` is an explicit call, not a side effect of `end()`.
- [x] **SSOT** — `jam::vk::Shader` is the single source for all shader state. Uniform/channel bimap logic carried from OpenGL implementation unchanged.
- [x] **Stateless** — `VulkanGraphics::paintShader` is a free function, no state. `iTime` derived at call time, not stored.
- [x] **Encapsulation** — `VkRenderPass`, `VkFramebuffer`, ping-pong images are private to `RenderPass`. Call sites see no Vulkan internals.
- [x] **Deterministic** — Same files + same uniforms = same output. Ping-pong is deterministic per `advance()` call sequence.

---

## Open Questions

None. Uniform/channel bimap, hot-reload, and custom uniform API are carried from the existing OpenGL implementation unchanged.

---

## Handoff Notes

- Uniform/channel bimap implementation lives in the existing OpenGL shader system. COUNSELOR should port that logic verbatim into `jam::vk::Shader` — do not redesign.
- `Common` source injection happens at `shaderc::Compiler` call time via source string prepend or `#include` resolution, not as a `RenderPass`.
- `VulkanGraphics::paintShader` is a free function performing `static_cast<VulkanLowLevelGraphicsContext&>(g.getInternalContext())` — same pattern as the existing OpenGL helpers.
- `setPostShader` accepts `nullptr` — LLGC skips post pass if unset. This is the toggle for the feature at runtime.
- Component is responsible for constructing its own `Shader` instance, loading files, and owning `juce::Timer`. LLGC has no opinion on component-side shaders.
