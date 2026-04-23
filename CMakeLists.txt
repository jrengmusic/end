# CMakeLists.txt - END: Ephemeral Nexus Display
# ============================================================================
# GPU-accelerated terminal emulator using JUCE + libtsm
#
# Usage:
#   mkdir -p Builds/Xcode && cd Builds/Xcode && cmake -G Xcode ../..
#
# Directory Structure:
#   dev/
#   ├── end/
#   │   ├── CMakeLists.txt      ← This file
#   │   ├── Source/             ← Your .cpp/.h files
#   │   └── modules/            ← Local JUCE modules (optional)
#   └── JUCE/                   ← JUCE framework (sibling)
# ============================================================================

cmake_minimum_required(VERSION 4.2.0)


# ============================================================================
# CONFIGURATION
# ============================================================================

set(PROJECT_NAME "END")
set(PROJECT_VERSION "0.0.1")
set(PROJECT_COMPANY "JRENG")
set(PROJECT_WEBSITE "https://jrengmusic.com")

set(TARGET_TYPE "GUI")

set(JUCE_MODULES
    juce_core
    juce_data_structures
    juce_events
    juce_graphics
    juce_gui_basics
    juce_gui_extra
    juce_opengl
)

include("$ENV{HOME}/Documents/Poems/dev/jam/Jam.cmake")
jam_set_root()

set(SOURCE_MODE "AUTO_GLOB")
set(SOURCE_FILES)

set(AUTO_BINARY_DATA ON)

set(MACOS_DEPLOYMENT_TARGET "10.14")
set(MACOS_ARCHITECTURES "x86_64;arm64")
set(BUNDLE_ID_PREFIX "com.${PROJECT_COMPANY}")

# ============================================================================
# JUCE MODULE SOURCE GROUPS
# ============================================================================
set(JUCE_ENABLE_MODULE_SOURCE_GROUPS ON CACHE BOOL "Show JUCE module sources in IDE")

# ============================================================================
# JUCE FRAMEWORK DISCOVERY
# ============================================================================

set(JUCE_FOUND FALSE)

if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(STATUS "✓ JUCE found: ${JUCE_PATH} (CMake variable)")
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
    set(JUCE_FOUND TRUE)

elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE")
    message(STATUS "✓ JUCE found: ${JUCE_PATH} (sibling directory)")
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
    set(JUCE_FOUND TRUE)

elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../JUCE/CMakeLists.txt")
    set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../JUCE")
    message(STATUS "✓ JUCE found: ${JUCE_PATH} (cousin directory)")
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
    set(JUCE_FOUND TRUE)

elseif(DEFINED ENV{JUCE_PATH} AND EXISTS "$ENV{JUCE_PATH}/CMakeLists.txt")
    set(JUCE_PATH "$ENV{JUCE_PATH}")
    message(STATUS "✓ JUCE found: ${JUCE_PATH} (environment variable)")
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
    set(JUCE_FOUND TRUE)
endif()

if(NOT JUCE_FOUND)
    message(FATAL_ERROR
        "JUCE framework not found!\n"
        "\n"
        "Searched:\n"
        "  1. JUCE_PATH CMake variable: ${JUCE_PATH}\n"
        "  2. ${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/\n"
        "  3. ${CMAKE_CURRENT_SOURCE_DIR}/../../JUCE/\n"
        "  4. JUCE_PATH environment variable\n"
        "\n"
        "Solutions:\n"
        "  - Clone JUCE to ${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/\n"
        "  - Pass -DJUCE_PATH=/path/to/JUCE\n"
    )
endif()

message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

# ============================================================================
# PROJECT DECLARATION
# ============================================================================

if(APPLE)
    set(_EXTRA_LANGS OBJC OBJCXX)
else()
    set(_EXTRA_LANGS "")
endif()

project("${PROJECT_NAME}"
    VERSION ${PROJECT_VERSION}
    LANGUAGES C CXX ${_EXTRA_LANGS}
    DESCRIPTION "Ephemeral Nexus Display - GPU Terminal Emulator"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Target type: ${TARGET_TYPE}")
message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

# ============================================================================
# SOURCE FILE ORGANIZATION
# ============================================================================

if(SOURCE_MODE STREQUAL "AUTO_GLOB")
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.m"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.mm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.hpp"
    )

    message(STATUS "Source files (auto-discovered):")
    foreach(SRC ${SOURCES})
        file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" "${SRC}")
        message(STATUS "  ${REL_PATH}")
    endforeach()

else()
    set(SOURCES)
    foreach(SRC ${SOURCE_FILES})
        list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${SRC}")
    endforeach()

    message(STATUS "Source files (explicit):")
    foreach(SRC ${SOURCE_FILES})
        message(STATUS "  ${SRC}")
    endforeach()
endif()

if(NOT SOURCES)
    message(FATAL_ERROR "No source files found!")
endif()

message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

# ============================================================================
# BINARY DATA AUTO-DISCOVERY
# ============================================================================

set(BINARY_DATA_FILES)

if(AUTO_BINARY_DATA)
    file(GLOB_RECURSE SVG_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.svg")
    file(GLOB_RECURSE XML_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.xml")
    file(GLOB_RECURSE PNG_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.png")
    file(GLOB_RECURSE FONT_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.otf"
    )
    file(GLOB_RECURSE LUA_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.lua")
    file(GLOB_RECURSE SHELL_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.zsh"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.bash"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.fish"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.ps1"
    )
    file(GLOB_RECURSE JS_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.js")
    file(GLOB_RECURSE HTML_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.html")

    # Add shader files from Source and modules
    file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS
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

    list(APPEND BINARY_DATA_FILES
        ${SVG_FILES} ${XML_FILES} ${PNG_FILES} ${FONT_FILES} ${LUA_FILES} ${SHELL_FILES} ${SHADER_FILES} ${JS_FILES} ${HTML_FILES}
    )

    # Windows ConPTY sideload binaries (MIT-licensed, from Microsoft Terminal project).
    # conpty.dll + OpenConsole.exe are extracted at runtime to ~/.config/end/conpty/
    # so that the modern ConPTY (with PSEUDOCONSOLE_WIN32_INPUT_MODE support) is used
    # instead of the inbox conhost.exe on Windows 10 22H2 and earlier.
    if(WIN32)
        list(APPEND BINARY_DATA_FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/windows/conpty.dll"
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/windows/OpenConsole.exe"
        )
    endif()
endif()

if(BINARY_DATA_FILES)
    message(STATUS "Binary data files:")
    foreach(FILE ${BINARY_DATA_FILES})
        file(RELATIVE_PATH REL "${CMAKE_CURRENT_SOURCE_DIR}" "${FILE}")
        message(STATUS "  ${REL}")
    endforeach()
else()
    message(STATUS "Binary data: none")
endif()

message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

# ============================================================================
# TARGET CREATION
# ============================================================================

set(TARGET_NAME "${PROJECT_NAME}_App")

juce_add_gui_app(${TARGET_NAME}
    PRODUCT_NAME "${PROJECT_NAME}"
    VERSION "${PROJECT_VERSION}"
    COMPANY_NAME "${PROJECT_COMPANY}"
    COMPANY_WEBSITE "${PROJECT_WEBSITE}"
    BUNDLE_ID "${BUNDLE_ID_PREFIX}.${PROJECT_NAME}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Source/icons/end-icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Source/icons/end-icon.png"
)

target_sources(${TARGET_NAME} PRIVATE ${SOURCES})

target_include_directories(${TARGET_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${JAM_ROOT}/jam_freetype/freetype/include"
    "${JUCE_PATH}/modules/juce_graphics/fonts/harfbuzz"
)

message(STATUS "Target created: ${TARGET_NAME}")
message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

# ============================================================================
# JAM MODULE REGISTRATION + LINKING
# ============================================================================

jam_add_modules(${TARGET_NAME}
    jam_core
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
)

# sol3 headers use <sol/...> includes; expose jam_lua/ so they resolve
target_include_directories(${TARGET_NAME} PRIVATE "${JAM_ROOT}/jam_lua")

# Suppress tmpnam deprecation from vendored Lua C source — scoped to jam_lua only
if(NOT MSVC)
    set_source_files_properties(
        "${JAM_ROOT}/jam_lua/jam_lua.cpp"
        PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations"
    )
endif()

# Suppress ONE_PIXEL macro-redefined from vendored FreeType amalgamation — scoped to jam_freetype only
if(NOT MSVC)
    set_source_files_properties(
        "${JAM_ROOT}/jam_freetype/jam_freetype.mm"
        "${JAM_ROOT}/jam_freetype/jam_freetype.cpp"
        PROPERTIES COMPILE_FLAGS "-Wno-macro-redefined"
    )
endif()

message(STATUS "Lua: jam_lua module (vendored)")
message(STATUS "sol2: jam_lua module (jam::lua:: namespace)")
message(STATUS "FreeType: ${JAM_ROOT}/jam_freetype (JAM module, vendored)")
message(STATUS "HarfBuzz: ${JUCE_PATH}/modules/juce_graphics/fonts/harfbuzz (JUCE internal)")

message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

# ============================================================================
# SOURCE GROUPS FOR IDE
# ============================================================================

foreach(ABS_SRC ${SOURCES})
    file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" "${ABS_SRC}")
    get_filename_component(REL_DIR "${REL_PATH}" DIRECTORY)

    if(NOT REL_DIR STREQUAL "")
        source_group("${REL_DIR}" FILES "${ABS_SRC}")
    else()
        source_group("Source Files" FILES "${ABS_SRC}")
    endif()
endforeach()

# ============================================================================
# JUCEHEADER PRE-GENERATION
# ============================================================================

function(_end_pre_generate_juce_header target)
    get_target_property(juce_library_code ${target} JUCE_GENERATED_SOURCES_DIRECTORY)
    if(NOT juce_library_code)
        return()
    endif()

    file(MAKE_DIRECTORY "${juce_library_code}")

    if(NOT TARGET juce::juceaide)
        message(WARNING "juce::juceaide not found")
        return()
    endif()

    get_target_property(juceaide_exe juce::juceaide IMPORTED_LOCATION)

    if(NOT juceaide_exe)
        foreach(config Release Debug RelWithDebInfo MinSizeRel)
            get_target_property(juceaide_exe juce::juceaide IMPORTED_LOCATION_${config})
            if(juceaide_exe)
                break()
            endif()
        endforeach()
    endif()

    if(NOT juceaide_exe)
        set(juceaide_exe $<TARGET_FILE:juce::juceaide>)
    endif()

    get_target_property(compile_defs ${target} COMPILE_DEFINITIONS)
    set(module_defs "")
    if(compile_defs)
        foreach(def ${compile_defs})
            if(def MATCHES "^JUCE_MODULE_AVAILABLE_" OR def MATCHES "^DONT_SET_USING_JUCE_NAMESPACE")
                list(APPEND module_defs "${def}")
            endif()
        endforeach()
    endif()

    string(ASCII 30 RS)
    string(ASCII 31 US)

    set(defs_file "${juce_library_code}/Defs.txt")
    set(defs_content "MODULE_DEFINITIONS${US}")
    foreach(def ${module_defs})
        string(APPEND defs_content "${def};")
    endforeach()
    string(APPEND defs_content "${RS}")
    file(WRITE "${defs_file}" "${defs_content}")

    set(juce_header "${juce_library_code}/JuceHeader.h")
    if(NOT EXISTS "${juce_header}")
        execute_process(
            COMMAND "${juceaide_exe}" header "${defs_file}" "${juce_header}"
            RESULT_VARIABLE result
            ERROR_VARIABLE error
            OUTPUT_QUIET
        )

        if(result EQUAL 0)
            message(STATUS "✓ Pre-generated JuceHeader.h")
        else()
            message(WARNING "Failed to pre-generate JuceHeader.h: ${error}")
        endif()
    endif()
endfunction()

_end_pre_generate_juce_header(${TARGET_NAME})
juce_generate_juce_header(${TARGET_NAME})

message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

# ============================================================================
# BINARY DATA
# ============================================================================

if(BINARY_DATA_FILES)
    juce_add_binary_data(${TARGET_NAME}_BinaryData
        NAMESPACE "BinaryData"
        SOURCES ${BINARY_DATA_FILES}
    )

    target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_NAME}_BinaryData)
    message(STATUS "BinaryData target created")
endif()

message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

# ============================================================================
# MODULE LINKING
# ============================================================================

message(STATUS "Linking JUCE modules:")

foreach(MODULE ${JUCE_MODULES})
    if(TARGET ${MODULE})
        target_link_libraries(${TARGET_NAME} PRIVATE ${MODULE})
        message(STATUS "  ✓ ${MODULE}")
    else()
        message(FATAL_ERROR "JUCE module not found: ${MODULE}")
    endif()
endforeach()

target_compile_definitions(${TARGET_NAME} PRIVATE
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    DONT_SET_USING_JUCE_NAMESPACE=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${TARGET_NAME},JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${TARGET_NAME},JUCE_VERSION>"
    HAVE_FREETYPE
    $<$<NOT:$<PLATFORM_ID:Windows>>:HAVE_UNISTD_H>
    $<$<NOT:$<PLATFORM_ID:Windows>>:HAVE_FCNTL_H>
    $<$<PLATFORM_ID:Darwin>:GL_SILENCE_DEPRECATION>
)

if(COMMAND _juce_fixup_module_source_groups)
    _juce_fixup_module_source_groups()
endif()

message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

# ============================================================================
# PLATFORM CONFIGURATION
# ============================================================================

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${MACOS_DEPLOYMENT_TARGET}" CACHE STRING "")
    set(CMAKE_OSX_ARCHITECTURES "${MACOS_ARCHITECTURES}" CACHE STRING "")

    message(STATUS "Platform: macOS")
    message(STATUS "  Deployment: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
    message(STATUS "  Architectures: ${CMAKE_OSX_ARCHITECTURES}")

    set_target_properties(${TARGET_NAME} PROPERTIES
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_NAME "${PROJECT_NAME}"
    )

    target_link_libraries(${TARGET_NAME} PRIVATE "-weak_framework UserNotifications")

    if(TARGET ${TARGET_NAME}_BinaryData)
        set_target_properties(${TARGET_NAME}_BinaryData PROPERTIES
            XCODE_GENERATE_SCHEME OFF
        )
    endif()

elseif(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_link_libraries(${TARGET_NAME} PRIVATE dwmapi)
    message(STATUS "Platform: Windows")

elseif(UNIX)
    message(STATUS "Platform: Linux")
endif()

message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

# ============================================================================
# COMPILER FLAGS
# ============================================================================

if(APPLE OR UNIX)
    target_compile_options(${TARGET_NAME} PRIVATE
        -Wno-unused-parameter
        -Wno-sign-conversion
        -Wno-shadow
    )
endif()

# ============================================================================
# CLANGD CONFIGURATION (Auto-generated for LSP)
# ============================================================================

set(CLANGD_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/.clangd")
set(CLANGD_CONTENT "CompileFlags:\n  CompilationDatabase: ${CMAKE_CURRENT_SOURCE_DIR}/Builds/Ninja/\n  Add:\n")

if(APPLE)
    string(APPEND CLANGD_CONTENT "    - -D__APPLE__=1\n")
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        string(APPEND CLANGD_CONTENT "    - -DTARGET_OS_IPHONE=1\n")
        string(APPEND CLANGD_CONTENT "    - -DJUCE_IOS=1\n")
        string(APPEND CLANGD_CONTENT "    - -DJUCE_MAC=0\n")
    else()
        string(APPEND CLANGD_CONTENT "    - -DTARGET_OS_IPHONE=0\n")
        string(APPEND CLANGD_CONTENT "    - -DJUCE_MAC=1\n")
        string(APPEND CLANGD_CONTENT "    - -DJUCE_IOS=0\n")
    endif()
    string(APPEND CLANGD_CONTENT "    - -DJUCE_WINDOWS=0\n")
    string(APPEND CLANGD_CONTENT "    - -DJUCE_LINUX=0\n")
elseif(WIN32)
    string(APPEND CLANGD_CONTENT "    - -D_WIN32=1\n")
    string(APPEND CLANGD_CONTENT "    - -DJUCE_WINDOWS=1\n")
    string(APPEND CLANGD_CONTENT "    - -DJUCE_MAC=0\n")
    string(APPEND CLANGD_CONTENT "    - -DJUCE_IOS=0\n")
    string(APPEND CLANGD_CONTENT "    - -DJUCE_LINUX=0\n")
elseif(UNIX)
    string(APPEND CLANGD_CONTENT "    - -D__linux__=1\n")
    string(APPEND CLANGD_CONTENT "    - -DJUCE_LINUX=1\n")
    string(APPEND CLANGD_CONTENT "    - -DJUCE_MAC=0\n")
    string(APPEND CLANGD_CONTENT "    - -DJUCE_IOS=0\n")
    string(APPEND CLANGD_CONTENT "    - -DJUCE_WINDOWS=0\n")
endif()

if(DEFINED JAM_ROOT AND EXISTS "${JAM_ROOT}")
    string(APPEND CLANGD_CONTENT "    - -I${JAM_ROOT}\n")
endif()

string(APPEND CLANGD_CONTENT "Diagnostics:\n  MissingIncludes: None\n  UnusedIncludes: None\n")

file(WRITE "${CLANGD_CONFIG}" "${CLANGD_CONTENT}")
message(STATUS "✓ Generated .clangd for LSP")

message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
message(STATUS "Configuration complete!")
message(STATUS "Generator: ${CMAKE_GENERATOR}")
message(STATUS "Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
