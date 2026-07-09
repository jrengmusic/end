# CMakeLists.txt — jam_terminal conformance suite (standalone, Catch2)
# ============================================================================
# Revives the b3f0fea ALL-GREEN conformance pattern permanently inside END.
# The suite lives in END's tree as a standalone target — own CMakeLists.txt,
# own target, NOT wired into END's app build (END_App never add_subdirectory
# this tree). It exercises JAM's jam_terminal module, relocated here from
# JAM's own repo (jam_terminal/tests/) because JAM's
# JUCE_ENABLE_MODULE_SOURCE_GROUPS ON makes JUCE's module GLOB_RECURSE sweep
# every file under jam_terminal/**, misclassifying any .cpp living inside
# that tree as a module header (HEADER_FILE_ONLY TRUE, never compiled).
# Living outside any JUCE module folder avoids that entirely.
# Catch2 v2.13.10 single-header vendored at catch2/catch.hpp.
#
# ModelTests.cpp (RFC-terminal-editor.md S7.4 validation gate) additionally
# validates END's own `TerminalModel` (Source/terminal/TerminalModel.h — header-only
# P12 SSOT, distinct from JAM's jam::terminal::Model consumed elsewhere in
# this suite) — schema shape, Direction A store()->flush() sync, Direction B
# setter->parameterChanged wake seam, jam::ParameterText round-trip/
# truncation. TerminalModel is header-only (straight-line
# createAndAddParameter registration, no storage TU); END's Identifier.h
# constants are likewise header-only `static inline const` — the added
# "../Source" include directory is all resolution requires.
#
# Usage (ARCHITECT only — agents never build/run):
#   cmake -S tests -B tests/build -G Ninja
#   cmake --build tests/build
#   ./tests/build/jam_terminal_tests
# ============================================================================

cmake_minimum_required(VERSION 3.22)

# JAM_ROOT — two levels up from tests/ (tests -> end -> dev), then jam.
set(JAM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../jam")

# Explicit JUCE_PATH — checked first by BuildSetup.cmake's discovery order.
# Auto-discovery (JAM_ROOT/../JUCE, sibling/cousin of CMAKE_CURRENT_SOURCE_DIR)
# does not reach three levels up from tests/ to Poems/JUCE, so this suite
# sets it explicitly rather than relying on a discovery path shaped for
# one-level consumer projects (END's own app CMakeLists.txt lives at
# Poems/dev/end/ and reaches Poems/JUCE via the two-level "cousin" discovery
# option; this target lives one level deeper, at Poems/dev/end/tests/).
set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../JUCE")

list(APPEND CMAKE_MODULE_PATH "${JAM_ROOT}/cmake")
include(BuildSetup)

project(jam_terminal_tests
    VERSION 0.0.1
    LANGUAGES C CXX
    DESCRIPTION "jam_terminal conformance suite — Catch2, standalone"
)

configure_app(
    TARGET_NAME      jam_terminal_tests_App
    PRODUCT_NAME     jam_terminal_tests
    VERSION          "${PROJECT_VERSION}"
    COMPANY          JRENG
    TARGET_TYPE      ConsoleApp
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/TestMain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestTerm.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/WritePathTests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/SyncOutputTests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/MarkTests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/HyperlinkTests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/DispatchTests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/LookupTableTests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/ModelTests.cpp"
    MODULES
        juce_core
        juce_events
        juce_data_structures
        juce_graphics
        juce_gui_basics
    JAM_MODULES
        jam_core
        jam_vulkan
        jam_data_structures
        jam_graphics
        jam_terminal
)

target_include_directories(jam_terminal_tests_App PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Source"
)

message(STATUS "jam_terminal_tests: standalone conformance suite configured (not wired into any app build)")
