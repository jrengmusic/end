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
--	                Ephemeral Nexus Display  v%%versionString%%
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
-- Colour format: "#RRGGBB" (fully opaque) or "#RRGGBBAA" (with alpha).
--   - "#RGB" and "#RGBA" shorthand supported (e.g. "#F00" becomes "#FF0000").
--   - "rgba(r, g, b, a)" functional notation (a is 0.0 - 1.0).
--
-- Key binding format: "modifier+key" (e.g. "cmd+c", "ctrl+shift+t").
--   - Modifiers: cmd, ctrl, alt, shift
--   - Some keys use a two-step sequence: press the prefix key first, then the
--     action key. See the keys section below.
--
-- ============================================================================

END = {
    nexus   = require("nexus"),
    display = require("display"),
    whelmed = require("whelmed"),
    keys    = require("keys"),
    popups  = require("popups"),
    actions = require("actions"),
}
