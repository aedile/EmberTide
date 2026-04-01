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
# N6: -Wall -Werror mandatory on all host test executables.
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

# Collect sources (game + presentation, minus screen_mgr which is device-only).
file(GLOB GAME_SOURCES "${GAME_SRC_DIR}/*.c")
file(GLOB PRES_SOURCES "${PRES_SRC_DIR}/*.c")

# Exclude screen_mgr.c — it calls hal_epaper (device only, not host-compilable).
list(FILTER PRES_SOURCES EXCLUDE REGEX ".*screen_mgr\\.c$")

# ---------------------------------------------------------------------------
# render_all_screens executable
# ---------------------------------------------------------------------------
add_executable(render_all_screens
    "${CMAKE_CURRENT_SOURCE_DIR}/render_all_screens.c"
    ${GAME_SOURCES}
    ${PRES_SOURCES}
)

target_include_directories(render_all_screens PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"        # for vendors/stb_image_write.h
    "${GAME_INCLUDE}"
    "${PRES_INCLUDE}"
)

target_compile_options(render_all_screens PRIVATE
    -Wall -Werror
    # stb_image_write uses some patterns that trigger -Wsign-conversion and
    # -Wunused-function in some GCC versions. Suppress only those two.
    -Wno-sign-conversion
    -Wno-unused-function
)

# ---------------------------------------------------------------------------
# CTest: running render_all_screens must exit 0.
# The test changes directory to the build root so output/ is written there.
# ---------------------------------------------------------------------------
add_test(NAME render_all_screens
         COMMAND render_all_screens
         WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
