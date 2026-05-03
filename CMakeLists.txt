# CMakeLists.txt - END: Ephemeral Nexus Display
# ============================================================================
# GPU-accelerated terminal emulator using JUCE
# ============================================================================

cmake_minimum_required(VERSION 4.2.0)

# ============================================================================
# JAM BUILD SYSTEM
# ============================================================================

set(JAM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../jam")
list(APPEND CMAKE_MODULE_PATH "${JAM_ROOT}/cmake")
include(BuildSetup)

# ============================================================================
# PROJECT
# ============================================================================

if(APPLE)
    set(_EXTRA_LANGS OBJC OBJCXX)
else()
    set(_EXTRA_LANGS "")
endif()

project(END
    VERSION 0.0.1
    LANGUAGES C CXX ${_EXTRA_LANGS}
    DESCRIPTION "Ephemeral Nexus Display - GPU Terminal Emulator"
)

# ============================================================================
# BINARY DATA AUTO-DISCOVERY (END-specific)
# ============================================================================

file(GLOB_RECURSE _end_svg CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.svg")
file(GLOB_RECURSE _end_xml CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.xml")
file(GLOB_RECURSE _end_png CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.png")
file(GLOB_RECURSE _end_fonts CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.otf"
)
file(GLOB_RECURSE _end_lua CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.lua")
file(GLOB_RECURSE _end_shell CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.zsh"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.bash"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.fish"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.ps1"
)
file(GLOB_RECURSE _end_js CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.js")
file(GLOB_RECURSE _end_html CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.html")
file(GLOB_RECURSE _end_shaders CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/**/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/**/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/**/*.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/**/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/**/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/**/*.glsl"
    "${JAM_ROOT}/**/*.frag"
    "${JAM_ROOT}/**/*.vert"
    "${JAM_ROOT}/**/*.glsl"
)

set(END_BINARY_FILES
    ${_end_svg} ${_end_xml} ${_end_png} ${_end_fonts}
    ${_end_lua} ${_end_shell} ${_end_shaders} ${_end_js} ${_end_html}
)

# Windows ConPTY sideload binaries
if(WIN32)
    list(APPEND END_BINARY_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/resources/windows/conpty.dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/resources/windows/OpenConsole.exe"
    )
endif()

# ============================================================================
# BUILD
# ============================================================================

configure_app(
    TARGET_NAME      END_App
    PRODUCT_NAME     END
    VERSION          "${PROJECT_VERSION}"
    COMPANY          JRENG
    COMPANY_WEBSITE  "https://jrengmusic.com"
    BUNDLE_ID        "com.JRENG.END"
    ICON_BIG         "${CMAKE_CURRENT_SOURCE_DIR}/Source/icons/end-icon.png"
    ICON_SMALL       "${CMAKE_CURRENT_SOURCE_DIR}/Source/icons/end-icon.png"
    TARGET_TYPE      GUIApp
    MODULES
        juce_core
        juce_data_structures
        juce_events
        juce_graphics
        juce_gui_basics
        juce_gui_extra
        juce_opengl
    JAM_MODULES
        jam_core
        jam_fuzzy
        jam_style
        jam_data_structures
        jam_fonts
        jam_freetype
        jam_graphics
        jam_gui
        jam_look_and_feel
        jam_javascript
        jam_lua
        jam_markdown
        jam_animation
        jam_debug
        jam_tui
    BINARY_FILES     ${END_BINARY_FILES}
    EXTRA_DEFINES
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        HAVE_FREETYPE
        $<$<NOT:$<PLATFORM_ID:Windows>>:HAVE_UNISTD_H>
        $<$<NOT:$<PLATFORM_ID:Windows>>:HAVE_FCNTL_H>
)

# ============================================================================
# END-SPECIFIC CONFIGURATION
# ============================================================================

set(TARGET_NAME "END_App")

# Suppress tmpnam deprecation from vendored Lua C source — scoped to jam_lua only
if(NOT MSVC)
    set_source_files_properties(
        "${JAM_ROOT}/jam_lua/jam_lua.cpp"
        PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations"
    )
endif()

# ============================================================================
# PLATFORM CONFIGURATION
# ============================================================================

if(APPLE)
    set_target_properties(${TARGET_NAME} PROPERTIES
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_NAME "END"
    )

    target_link_libraries(${TARGET_NAME} PRIVATE "-weak_framework UserNotifications")

    if(TARGET ${TARGET_NAME}_BinaryData)
        set_target_properties(${TARGET_NAME}_BinaryData PROPERTIES
            XCODE_GENERATE_SCHEME OFF
        )
    endif()

elseif(WIN32)
    target_link_libraries(${TARGET_NAME} PRIVATE dwmapi)
endif()

message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
message(STATUS "Configuration complete!")
message(STATUS "Generator: ${CMAKE_GENERATOR}")
message(STATUS "Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
