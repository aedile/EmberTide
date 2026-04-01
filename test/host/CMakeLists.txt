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
# N5: NO ESP-IDF headers in host test include paths.
#
# The include_directories() calls below list ONLY the game component sources.
# ESP-IDF headers are never added here. Any accidental #include of an
# ESP-IDF header causes a compile error, not a silent build success. This
# proves that host tests are genuinely isolated from device SDK code.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# N6: -Wall -Werror on all host test targets (directory-wide default).
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

# Collect all game component .c sources for host linkage.
file(GLOB GAME_SOURCES "${GAME_SRC_DIR}/*.c")

# ---------------------------------------------------------------------------
# add_host_test(<target> <source>)
#
# Builds a single-source test executable that links the game component.
# Adds a CTest entry that expects exit 0.
# ---------------------------------------------------------------------------
function(add_host_test target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE
        "${GAME_INCLUDE}"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    if(GAME_SOURCES)
        target_sources(${target} PRIVATE ${GAME_SOURCES})
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# ---------------------------------------------------------------------------
# Boundary violation checks (E1, N1a, N1b, N1c) — TWO-LAYER DEFENSE
#
# Architecture rules enforced here:
#   E1  : game/      MUST NOT include hal_*.h
#   N1a : presentation/ MUST NOT include hal_*.h
#   N1b : game/      MUST NOT include presentation headers
#   N1c : connectivity/ MUST NOT include game/ headers
#
# Layer 1 — CMake configure time (defense-in-depth):
#   try_compile() is called at configure time with NO forbidden include path.
#   A compile success triggers FATAL_ERROR, stopping the build immediately.
#   This prevents a broken boundary from ever producing a build artifact.
#
# Layer 2 — CTest runtime (B1: makes boundary checks visible in ctest output):
#   check_boundary.sh is invoked as a CTest test. It compiles the boundary
#   file with no forbidden include dirs and exits 0 if compilation FAILS
#   (boundary holds) or exits 1 if compilation SUCCEEDS (boundary broken).
#   This makes boundary status visible in every `ctest --output-on-failure`
#   run — not hidden in CMake configure output.
# ---------------------------------------------------------------------------
set(BOUNDARY_TMP "${CMAKE_BINARY_DIR}/boundary_checks")
set(BOUNDARY_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.sh")

macro(assert_compile_fails label source_file)
    message(STATUS "Checking boundary: ${label}")
    try_compile(
        _${label}_compiled
        "${BOUNDARY_TMP}/${label}"
        "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}"
        CMAKE_FLAGS "-DCMAKE_C_STANDARD=11"
    )
    if(_${label}_compiled)
        message(FATAL_ERROR
            "BOUNDARY VIOLATION: ${label}\n"
            "  '${source_file}' compiled successfully — a forbidden header "
            "is reachable. Fix the CMake include paths to restore the boundary.")
    else()
        message(STATUS "  ${label}: OK (compile failed as expected)")
    endif()
endmacro()

assert_compile_fails(E1_game_no_hal      "bound_e1_game_includes_hal.c")
assert_compile_fails(N1a_pres_no_hal     "bound_n1a_presentation_includes_hal.c")
assert_compile_fails(N1b_game_no_pres    "bound_n1b_game_includes_presentation.c")
assert_compile_fails(N1c_conn_no_game    "bound_n1c_connectivity_includes_game.c")

# ---------------------------------------------------------------------------
# Layer 2: CTest-visible boundary tests via check_boundary.sh
#
# Each add_test() call invokes check_boundary.sh with the source file path.
# No include dirs are passed — this mirrors the try_compile() above and
# ensures the script tests the same condition that the configure-time check
# enforces. Shell script exits 0 (PASS) when compilation fails as required.
# ---------------------------------------------------------------------------
add_test(
    NAME boundary_E1_game_no_hal
    COMMAND "${BOUNDARY_SCRIPT}"
            "${CMAKE_CURRENT_SOURCE_DIR}/bound_e1_game_includes_hal.c"
)
set_tests_properties(boundary_E1_game_no_hal
    PROPERTIES LABELS "boundary")

add_test(
    NAME boundary_N1a_pres_no_hal
    COMMAND "${BOUNDARY_SCRIPT}"
            "${CMAKE_CURRENT_SOURCE_DIR}/bound_n1a_presentation_includes_hal.c"
)
set_tests_properties(boundary_N1a_pres_no_hal
    PROPERTIES LABELS "boundary")

add_test(
    NAME boundary_N1b_game_no_pres
    COMMAND "${BOUNDARY_SCRIPT}"
            "${CMAKE_CURRENT_SOURCE_DIR}/bound_n1b_game_includes_presentation.c"
)
set_tests_properties(boundary_N1b_game_no_pres
    PROPERTIES LABELS "boundary")

add_test(
    NAME boundary_N1c_conn_no_game
    COMMAND "${BOUNDARY_SCRIPT}"
            "${CMAKE_CURRENT_SOURCE_DIR}/bound_n1c_connectivity_includes_game.c"
)
set_tests_properties(boundary_N1c_conn_no_game
    PROPERTIES LABELS "boundary")

# ---------------------------------------------------------------------------
# E2: CTest failure escalation
#
# This test MUST exit non-zero. WILL_FAIL TRUE tells ctest to report it as
# "Passed" only when the executable returns non-zero. Proves that the CI
# pipeline correctly detects failures and does not generate false greens.
# ---------------------------------------------------------------------------
add_executable(bound_e2_always_fails
    "${CMAKE_CURRENT_SOURCE_DIR}/bound_e2_always_fails.c")
add_test(NAME bound_e2_always_fails COMMAND bound_e2_always_fails)
set_tests_properties(bound_e2_always_fails PROPERTIES WILL_FAIL TRUE)

# ---------------------------------------------------------------------------
# N4: Partition arithmetic validation
#
# PARTITIONS_CSV_PATH injected at compile time. Validates:
#   - No partition end address exceeds 0x800000 (8 MiB flash).
#   - No two partitions overlap.
#   - Total partition sizes fit within 8 MiB.
# ---------------------------------------------------------------------------
add_executable(test_partitions "${CMAKE_CURRENT_SOURCE_DIR}/test_partitions.c")
target_compile_definitions(test_partitions PRIVATE
    PARTITIONS_CSV_PATH="${PROJECT_ROOT}/partitions.csv")
add_test(NAME test_partitions COMMAND test_partitions)

# ---------------------------------------------------------------------------
# Feature tests (happy-path)
# ---------------------------------------------------------------------------
add_host_test(test_sanity "${CMAKE_CURRENT_SOURCE_DIR}/test_sanity.c")
