#[[*****************************************************************************
                        Codegen Annotated Source of Truth
————————————————————————————————————————————————————————————————————————————————

            ░░████████████░░████████████░░████████████░░████████████
            ░░████  ░░████░░████  ░░████░░████  ░░████    ░░████
            ░░████        ░░████  ░░████░░████            ░░████
            ░░████        ░░████████████░░████████████    ░░████
            ░░████        ░░████  ░░████        ░░████    ░░████
            ░░████  ░░████░░████  ░░████░░████  ░░████    ░░████
            ░░████████████░░████  ░░████░░████████████    ░░████

————————————————————————————————————————————————————————————————————————————————
                         FOR YOUR EYES ONLY, DO NOT EDIT
******************************************************************************]]

set(CAST_RELEASE_VERSION "0.0.1")
set(CAST_RELEASE_TAG "v${CAST_RELEASE_VERSION}")

if(APPLE)
    set(CAST_PACK_folder "../Release")
elseif(WIN32)
    set(CAST_PACK_folder "../Release")
endif()
if(APPLE)
    set(CAST_PACK_format "pkg")
elseif(WIN32)
    set(CAST_PACK_format "exe")
endif()
if(APPLE)
    set(CAST_PACK_platform "macOS")
elseif(WIN32)
    set(CAST_PACK_platform "Windows")
endif()

file(GLOB CAST_RELEASE_ASSETS "${CMAKE_CURRENT_LIST_DIR}/${CAST_PACK_folder}/END v${CAST_RELEASE_VERSION} *")

if(NOT CAST_RELEASE_ASSETS)
    message(FATAL_ERROR "CAST: no release asset for ${CAST_RELEASE_TAG} in ${CAST_PACK_folder}")
endif()

execute_process(COMMAND gh release view "${CAST_RELEASE_TAG}" --repo "jrengmusic/end" RESULT_VARIABLE CAST_RELEASE_VIEW OUTPUT_QUIET ERROR_QUIET)

if(CAST_RELEASE_VIEW EQUAL 0)
    execute_process(COMMAND gh release upload "${CAST_RELEASE_TAG}" ${CAST_RELEASE_ASSETS} --repo "jrengmusic/end" --clobber COMMAND_ERROR_IS_FATAL ANY)
else()
    execute_process(COMMAND gh release create "${CAST_RELEASE_TAG}" ${CAST_RELEASE_ASSETS} --repo "jrengmusic/end" --title "${CAST_RELEASE_TAG}" --notes-file "${CMAKE_CURRENT_LIST_DIR}/RELEASE.md" COMMAND_ERROR_IS_FATAL ANY)
endif()
