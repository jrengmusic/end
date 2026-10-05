# END v0.0.1

Ephemeral Nexus Display

## What's New

- Release lane: a signed macOS pkg and Windows x64 and arm64 installers, uploaded to the GitHub release.
- Signing data lives in cast/signing.md, and entitlements.plist is generated from it.
- The shader binary data depends on the glslc commands, so a clean build embeds fresh shaders.
- The engine starts through jam's getOrCreate: one Vulkan init contract.

## Platforms

| Platform | Architecture       | Format                            |
| -------- | ------------------ | --------------------------------- |
| macOS    | universal          | .pkg (signed, notarized)          |
| Windows  | x64                | .exe installer                    |
| Windows  | arm64              | .exe installer                    |

## Installation

macOS: open the .pkg. It installs `END.app` to `/Applications`.

Windows: run the installer. It installs `END.exe` in Program Files.
