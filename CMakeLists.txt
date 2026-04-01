cmake_minimum_required(VERSION 3.16)

# ---------------------------------------------------------------------------
# N8: Out-of-tree build guard
# The ESP-IDF build system enforces its own out-of-tree build directory, but
# we add an explicit guard for any direct cmake invocations.
# ---------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are forbidden. "
        "Use 'idf.py build' or 'cmake -B build' from the project root.")
endif()

# ---------------------------------------------------------------------------
# ESP-IDF project declaration.
# MUST appear AFTER cmake_minimum_required() and BEFORE project().
# ---------------------------------------------------------------------------
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(fiestaquest)

# ---------------------------------------------------------------------------
# Architecture boundary enforcement (documentation).
#
# The CMake include path isolation is enforced by each component's
# CMakeLists.txt via idf_component_register(REQUIRES ...).
#
# Dependency graph (arrows = allowed dependency direction):
#
#   main -> game, presentation, platform, connectivity
#   presentation -> game
#   platform -> hal
#   game -> (nothing — pure functions only)
#   connectivity -> platform
#   hal -> ESP-IDF SDK
#
# FORBIDDEN cross-dependencies:
#   game -> hal, presentation, connectivity         (Priority 0 determinism)
#   presentation -> hal                             (host-compilable contract)
#   connectivity -> game                            (N1c boundary)
#
# Enforced at configure time by test/host/CMakeLists.txt try_compile checks.
# ---------------------------------------------------------------------------
