--	████████████████████  ████████████    ████  ████    ████    ████
--	████████████████████  ████████████    ████  ████    ████    ████
--	████░░░░░░░░░░░░████  ████░░░░████    ████  ░░░░    ░░░░    ████
--	████            ████  ████    ████    ████                  ████
--	████████████████████  ████    ████    ████  ████████████████████
--	████████████████████  ████    ████    ████  ████████████████████
--	████░░░░░░░░░░░░░░░░  ████    ████    ████  ████░░░░░░░░░░░░████
--	████                  ████    ████    ████  ████            ████
--	████████████████████  ████    ████████████  ████████████████████
--	████████████████████  ████    ████████████  ████████████████████
--	░░░░░░░░░░░░░░░░░░░░  ░░░░    ░░░░░░░░░░░░  ░░░░░░░░░░░░░░░░░░░░
--
--	                Ephemeral Nexus Display  v0.0.1
-- ============================================================================
-- Configuration
-- https://github.com/jrengmusic/end
-- ============================================================================
--
-- This file is auto-generated with default values on first launch.
-- Edit any value below to customise your terminal.
-- Invalid or missing values fall back to defaults silently.
-- Reload with Cmd+R (no restart needed).
--
-- Colour format: 0xAARRGGBB hex integer (JUCE native ARGB format).
--   - Alpha 0xFF = fully opaque, 0x00 = fully transparent.
--   - Example: 0xffa1d6e5 = frostbite teal, fully opaque.
--
-- Key binding format: "modifier+key" (e.g. "cmd+c", "ctrl+shift+t").
--   - Modifiers: cmd, ctrl, alt, shift
--   - Some keys use a two-step sequence: press the prefix key first, then the
--     action key. See the keys section below.
--
-- ============================================================================

END = {
    nexus    = require("nexus"),
    display  = require("display"),
    graphics = require("graphics"),
    whelmed  = require("whelmed"),
    keys     = require("keys"),
    popups   = require("popups"),
    actions  = require("actions"),
}
