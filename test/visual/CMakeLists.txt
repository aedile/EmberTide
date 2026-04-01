cmake_minimum_required(VERSION 3.16)
project(fiestaquest_visual_tests C)

# ---------------------------------------------------------------------------
# N8: Out-of-tree build guard
# ---------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are forbidden. "
        "Run: cmake -B build (from test/visual/) to configure out of tree.")
endif()

# ---------------------------------------------------------------------------
# N6: -Wall -Werror on all visual test targets (directory-wide default).
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Werror)

enable_testing()

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
set(PROJECT_ROOT   "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(GAME_INCLUDE   "${PROJECT_ROOT}/components/game/include")
set(GAME_SRC_DIR   "${PROJECT_ROOT}/components/game/src")
set(PRES_INCLUDE   "${PROJECT_ROOT}/components/presentation/include")
set(PRES_SRC_DIR   "${PROJECT_ROOT}/components/presentation/src")
set(MAIN_SRC_DIR   "${PROJECT_ROOT}/main")

# Collect sources. screen_mgr.c is device-only (calls hal_epaper) —
# excluded here to keep the visual harness host-compilable.
file(GLOB GAME_SOURCES        "${GAME_SRC_DIR}/*.c")
file(GLOB PRES_SOURCES        "${PRES_SRC_DIR}/*.c")
file(GLOB PRES_SCREEN_SOURCES "${PRES_SRC_DIR}/screens/*.c")
list(FILTER PRES_SOURCES EXCLUDE REGEX ".*screen_mgr\.c$")

# ---------------------------------------------------------------------------
# render_all_screens
#
# Renders every screen to an in-memory 1-bit framebuffer and writes PNGs
# to output/ for visual regression review. Phase 1: blank.png only.
#
# Compile flags note: stb_image_write.h triggers -Wsign-conversion and
# -Wunused-function in some compiler versions — suppressed for stb only.
# All project-authored code is still compiled at full -Wall -Werror.
# ---------------------------------------------------------------------------
add_executable(render_all_screens
    "${CMAKE_CURRENT_SOURCE_DIR}/render_all_screens.c"
    "${MAIN_SRC_DIR}/vm_builder.c"
    ${GAME_SOURCES}
    ${PRES_SOURCES}
    ${PRES_SCREEN_SOURCES}
)

target_include_directories(render_all_screens PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"   # resolves vendors/stb_image_write.h
    "${GAME_INCLUDE}"
    "${PRES_INCLUDE}"
    "${MAIN_SRC_DIR}"               # resolves vm_builder.h
)

target_compile_options(render_all_screens PRIVATE
    -Wall -Werror
    -Wno-sign-conversion
    -Wno-unused-function
)

# CTest: render_all_screens must exit 0 and write output/blank.png.
# Working directory is the build root so output/ lands inside build/.
add_test(NAME render_all_screens
         COMMAND render_all_screens
         WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

# ---------------------------------------------------------------------------
# test_asset_integration  (Phase 17)
#
# Blits a real sprite and renders real font text to a framebuffer, then
# writes output/asset_integration.png.
#
# Does NOT depend on vm_builder or screen_*.c — pure presentation test.
# Uses the generated sprite/font headers from
#   components/presentation/include/sprites/
#   components/presentation/include/fonts/
#
# stb_image_write.h triggers sign-conversion and unused-function warnings —
# suppressed only for this target; all project code compiled at -Wall -Werror.
# ---------------------------------------------------------------------------
add_executable(test_asset_integration
    "${CMAKE_CURRENT_SOURCE_DIR}/test_asset_integration.c"
    ${PRES_SOURCES}
)

target_include_directories(test_asset_integration PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"   # resolves vendors/stb_image_write.h
    "${PRES_INCLUDE}"
)

target_compile_options(test_asset_integration PRIVATE
    -Wall -Werror
    -Wno-sign-conversion
    -Wno-unused-function
)

# CTest: test_asset_integration must exit 0 and write output/asset_integration.png.
add_test(NAME test_asset_integration
         COMMAND test_asset_integration
         WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
