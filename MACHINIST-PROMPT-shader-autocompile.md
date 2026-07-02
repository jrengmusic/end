@CAROL.md MACHINIST: Rock 'n Roll

Build tooling problem, confirmed root cause of a real runtime Vulkan bug this session — needs a build-automation fix.

## Problem

`~/Documents/Poems/dev/jam/cmake/AppBuilder.cmake:570-585` embeds `jam_vulkan`'s SPIR-V shaders as binary data:

```cmake
# Vulkan SPIR-V BinaryData — pre-baked shaders embedded as byte arrays
if(_ca_mod STREQUAL "jam_vulkan")
    file(GLOB_RECURSE _ca_vulkan_spv_files CONFIGURE_DEPENDS
        "${JAM_ROOT}/${_ca_mod}/spv/*.spv"
    )
    if(_ca_vulkan_spv_files)
        juce_add_binary_data(${_ca_mod}_ShaderData
            NAMESPACE   "jam::vulkan"
            HEADER_NAME "JamVulkanShaderData.h"
            SOURCES     ${_ca_vulkan_spv_files}
        )
        target_link_libraries(${_CA_TARGET_NAME} PRIVATE ${_ca_mod}_ShaderData)
    endif()
endif()
```

This globs whatever `.spv` files already exist under `jam_vulkan/spv/` and embeds them as-is. **There is zero compile step from `.vert`/`.frag` GLSL source (`jam_vulkan/shaders/*.vert`/`*.frag`) to `.spv`.** The `.spv` files are manually-maintained, checked-into-git artifacts (confirmed via `git status` — tracked, show as `M`/`D` alongside source edits) that nobody/nothing automatically regenerates.

**Confirmed real consequence (this session, exact timestamp evidence):**
- `jam_vulkan/shaders/background.frag` edited 2026-07-01 13:00:05 — `jam_vulkan/spv/background.frag.spv` last touched 2026-06-29 09:20:00, never recompiled, not even marked modified in git.
- `jam_vulkan/shaders/instanced.vert` (the shared vertex shader backing nearly every quad-drawing pipeline) was edited again at 18:38:39 — *after* its own `.spv` (stamped 13:40:32) was last compiled.

Runtime result (MoltenVK, actual crash log from ARCHITECT this session): `VK_ERROR_INITIALIZATION_FAILED: Render pipeline compile failed... Fragment input(s) 'user(locn0)' mismatching vertex shader output type(s) or not written by vertex shader.` — stale bytecode compiled against an old shader interface, while the current C++ pipeline code assumes the new one.

## Task

Add automatic recompilation of `jam_vulkan/shaders/*.vert`/`*.frag` → `jam_vulkan/spv/*.vert.spv`/`*.frag.spv` whenever the GLSL source is newer than (or the `.spv` doesn't yet exist for) the corresponding source, as part of the normal build (`ninja`) — not a separate manual step ARCHITECT has to remember.

Investigate first, don't assume:
- Is `glslc` (Vulkan SDK / shaderc's CLI) or `glslangValidator` already available on ARCHITECT's dev machines (macOS + Windows, both need to work)? Check `~/Documents/Poems/dev/jam/___sdk___/vulkan/` (the vendored Vulkan SDK path referenced elsewhere in this project) for either tool before assuming a new dependency is needed.
- CMake's proper mechanism for this is a `add_custom_command(OUTPUT <spv> COMMAND glslc <source> -o <spv> DEPENDS <source> ...)` per shader file, so `ninja` treats it as a real build dependency (recompiles only when source is newer, exactly like any other build step) — glob the `.vert`/`.frag` sources (mirroring the existing `file(GLOB_RECURSE ... CONFIGURE_DEPENDS ...)` pattern already used one block above at line 555-558 for fonts) and generate one custom command per file, feeding the resulting `.spv` list into the existing `juce_add_binary_data` call instead of (or in addition to) the current pre-existing-`.spv` glob.
- Confirm whether `.spv` files should remain checked into git (current convention) as a build cache, or whether committing generated artifacts should stop now that compilation is automatic — flag this as an open question for ARCHITECT rather than deciding unilaterally; this changes repo hygiene, not just build mechanics.
- Cross-platform: the custom command must work identically via Ninja on both macOS and Windows (MSVC/clang-cl) dev machines per this project's known dual-platform setup.

This is `~/Documents/Poems/dev/jam/cmake/AppBuilder.cmake` — shared JAM build infrastructure, not `end`-specific — changes here affect every project consuming `jam_vulkan`.

Do not touch shader *source* content (`.vert`/`.frag` GLSL) — this is a build-automation fix only, not a shader-logic change.
