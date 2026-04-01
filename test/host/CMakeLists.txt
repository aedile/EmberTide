cmake_minimum_required(VERSION 3.16)
project(fiestaquest_host_tests C)

# ---------------------------------------------------------------------------
# N8: Out-of-tree build guard
# ---------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are forbidden. "
        "Run: cmake -B build (from test/host/) to configure out of tree.")
endif()

# ---------------------------------------------------------------------------
# N5: NO ESP-IDF headers allowed in the host test include paths.
# This comment is the explicit assertion required by spec-challenger N5.
# The include_directories() below list ONLY the game component sources.
# ESP-IDF paths are never added here. Any accidental ESP-IDF include will
# cause a compile error, not a silent build success.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# N6: Compiler flags — -Wall -Werror mandatory on both host test suites.
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Werror)

enable_testing()

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(GAME_INCLUDE  "${PROJECT_ROOT}/components/game/include")
set(GAME_SRC_DIR  "${PROJECT_ROOT}/components/game/src")

# Collect game component sources (all .c files under game/src/).
file(GLOB GAME_SOURCES "${GAME_SRC_DIR}/*.c")

# ---------------------------------------------------------------------------
# Helper macro: add a normal passing host test.
# ---------------------------------------------------------------------------
function(add_host_test target source)
    cmake_parse_arguments(ARG "NO_GAME_INCLUDES" "" "" ${ARGN})

    add_executable(${target} ${source})
    target_compile_options(${target} PRIVATE -Wall -Werror)

    if(NOT ARG_NO_GAME_INCLUDES)
        target_include_directories(${target} PRIVATE "${GAME_INCLUDE}")
        if(GAME_SOURCES)
            target_sources(${target} PRIVATE ${GAME_SOURCES})
        endif()
    endif()

    add_test(NAME ${target} COMMAND ${target})
endfunction()

# ---------------------------------------------------------------------------
# Boundary violation tests (E1, N1a, N1b, N1c)
#
# Architecture requires:
#   - game/      must NOT include hal_*.h        (E1)
#   - presentation/ must NOT include hal_*.h     (N1a)
#   - game/      must NOT include presentation   (N1b)
#   - connectivity/ must NOT include game/       (N1c)
#
# Strategy: use CMake's try_compile() at configure time to attempt to compile
# each boundary-violating source with NO relevant include path. Each attempt
# MUST fail to compile (result = FALSE). If try_compile succeeds, a hard
# FATAL_ERROR is raised — the boundary is broken.
#
# This provides the WILL_FAIL semantics without polluting the build targets.
# ---------------------------------------------------------------------------

# Scratch directory for try_compile artefacts.
set(BOUNDARY_TMP "${CMAKE_BINARY_DIR}/boundary_checks")

macro(check_boundary_violation label source_file)
    message(STATUS "Checking boundary violation: ${label}")
    try_compile(
        _${label}_compiled
        "${BOUNDARY_TMP}/${label}"
        "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}"
        CMAKE_FLAGS
            "-DCMAKE_C_STANDARD=11"
    )
    if(_${label}_compiled)
        message(FATAL_ERROR
            "BOUNDARY VIOLATION DETECTED: ${label}\n"
            "The file '${source_file}' compiled successfully — "
            "this means a forbidden header is reachable. "
            "Fix the CMake include paths to restore the architecture boundary.")
    else()
        message(STATUS "  ${label}: OK — compile failed as expected (boundary holds)")
    endif()
endmacro()

# E1: game/ must not include hal_*.h
check_boundary_violation(E1_game_no_hal
    "bound_e1_game_includes_hal.c")

# N1a: presentation/ must not include hal_*.h
check_boundary_violation(N1a_presentation_no_hal
    "bound_n1a_presentation_includes_hal.c")

# N1b: game/ must not include presentation headers
check_boundary_violation(N1b_game_no_presentation
    "bound_n1b_game_includes_presentation.c")

# N1c: connectivity/ must not include game/ headers
check_boundary_violation(N1c_connectivity_no_game
    "bound_n1c_connectivity_includes_game.c")

# ---------------------------------------------------------------------------
# E2: CTest failure escalation — proves ctest returns non-zero on failure.
# This is a runtime test (not compile-time) marked WILL_FAIL TRUE.
# ---------------------------------------------------------------------------
add_executable(bound_e2_always_fails
    "${CMAKE_CURRENT_SOURCE_DIR}/bound_e2_always_fails.c")
target_compile_options(bound_e2_always_fails PRIVATE -Wall -Werror)
add_test(NAME bound_e2_always_fails COMMAND bound_e2_always_fails)
set_tests_properties(bound_e2_always_fails PROPERTIES WILL_FAIL TRUE)

# ---------------------------------------------------------------------------
# N4: Partition arithmetic validation.
# PARTITIONS_CSV_PATH is injected as a compile-time macro.
# ---------------------------------------------------------------------------
set(PARTITIONS_CSV "${PROJECT_ROOT}/partitions.csv")
add_executable(test_partitions "${CMAKE_CURRENT_SOURCE_DIR}/test_partitions.c")
target_compile_options(test_partitions PRIVATE -Wall -Werror)
target_compile_definitions(test_partitions PRIVATE
    PARTITIONS_CSV_PATH="${PARTITIONS_CSV}")
add_test(NAME test_partitions COMMAND test_partitions)

# ---------------------------------------------------------------------------
# PHASE B — FEATURE TESTS (happy-path)
# ---------------------------------------------------------------------------

# Sanity test: 1 == 1.
add_host_test(test_sanity
    "${CMAKE_CURRENT_SOURCE_DIR}/test_sanity.c")
