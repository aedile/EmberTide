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
set(PRES_INCLUDE  "${PROJECT_ROOT}/components/presentation/include")
set(PRES_SRC_DIR  "${PROJECT_ROOT}/components/presentation/src")
set(CONN_INCLUDE  "${PROJECT_ROOT}/components/connectivity/include")
set(CONN_SRC_DIR  "${PROJECT_ROOT}/components/connectivity/src")

# Collect all game component .c sources for host linkage.
file(GLOB GAME_SOURCES "${GAME_SRC_DIR}/*.c")

# Also collect game/lib sources (sfxr.c -- vendored PCM generator).
set(GAME_LIB_DIR "${PROJECT_ROOT}/components/game/lib")
file(GLOB GAME_LIB_SOURCES "${GAME_LIB_DIR}/*.c")
if(GAME_LIB_SOURCES)
    list(APPEND GAME_SOURCES ${GAME_LIB_SOURCES})
endif()

# Collect presentation sources (exclude device-only screen_mgr.c).
file(GLOB PRES_SOURCES "${PRES_SRC_DIR}/*.c")
list(FILTER PRES_SOURCES EXCLUDE REGEX ".*screen_mgr\\.c$")

# ---------------------------------------------------------------------------
# CONN_SOURCES: intentionally enumerated (not globbed) to exclude
# wifi_service.c, which contains ESP-IDF BLE/WiFi headers that cannot be
# compiled in the host environment. Only the transport-agnostic protocol
# and sync modules are included here.
# ---------------------------------------------------------------------------
set(CONN_SOURCES
    "${CONN_SRC_DIR}/protocol.c"
    "${CONN_SRC_DIR}/sync.c"
)

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
# add_pres_test(<target> <source>)
#
# Builds a presentation-layer test that links both game and presentation
# sources (minus screen_mgr.c). Includes PRES_INCLUDE for fq_framebuffer.h,
# fq_sprite.h, fq_text.h.
# Architecture constraint: presentation/ MUST NOT include hal_*.h.
#
# NOTE: add_pres_test excludes screens/*.c from its source list. Tests that
# depend on screen renderers (screen_home.c, screen_inventory.c,
# screen_stats.c) or vm_builder.c MUST use add_vm_test instead.
# ---------------------------------------------------------------------------
function(add_pres_test target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE
        "${GAME_INCLUDE}"
        "${PRES_INCLUDE}"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    if(GAME_SOURCES)
        target_sources(${target} PRIVATE ${GAME_SOURCES})
    endif()
    if(PRES_SOURCES)
        target_sources(${target} PRIVATE ${PRES_SOURCES})
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
        CMAKE_FLAGS "-DCMAKE_C_STANDARD=11" "-DCMAKE_C_FLAGS=-Wall -Werror -Wimplicit-function-declaration"
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

# DET-NT-5: Float ban boundary test.
# bound_float_ban.c calls sin() without including <math.h>. Under -Wall -Werror
# the implicit function declaration is a hard error. The configure-time check
# proves the boundary holds even when compiled without game include dirs.
assert_compile_fails(N2_float_ban        "bound_float_ban.c")

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

# DET-NT-5 (Layer 2): CTest-visible float ban boundary check.
add_test(
    NAME boundary_N2_float_ban
    COMMAND "${BOUNDARY_SCRIPT}"
            "${CMAKE_CURRENT_SOURCE_DIR}/bound_float_ban.c"
)
set_tests_properties(boundary_N2_float_ban
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
# Phase-2 Bound tests (Rule 22 Phase A — BOUND RED)
# Written before any implementation to prove the boundary constraints.
# ---------------------------------------------------------------------------
add_host_test(test_prng_bounds       "${CMAKE_CURRENT_SOURCE_DIR}/test_prng_bounds.c")
add_host_test(test_crc32_bounds      "${CMAKE_CURRENT_SOURCE_DIR}/test_crc32_bounds.c")
add_host_test(test_progression_bounds "${CMAKE_CURRENT_SOURCE_DIR}/test_progression_bounds.c")
add_host_test(test_determinism_bounds "${CMAKE_CURRENT_SOURCE_DIR}/test_determinism_bounds.c")

# ---------------------------------------------------------------------------
# Phase-2 Feature tests (Rule 22 Phase B — FEATURE RED)
# Written before implementation to define the happy-path contract.
# ---------------------------------------------------------------------------
add_host_test(test_prng              "${CMAKE_CURRENT_SOURCE_DIR}/test_prng.c")
add_host_test(test_crc32             "${CMAKE_CURRENT_SOURCE_DIR}/test_crc32.c")
add_host_test(test_progression       "${CMAKE_CURRENT_SOURCE_DIR}/test_progression.c")
add_host_test(test_combat_determinism "${CMAKE_CURRENT_SOURCE_DIR}/test_combat_determinism.c")

# ---------------------------------------------------------------------------
# Feature tests (happy-path)
# ---------------------------------------------------------------------------
add_host_test(test_sanity "${CMAKE_CURRENT_SOURCE_DIR}/test_sanity.c")

# ---------------------------------------------------------------------------
# Phase-3 Bound tests (Rule 22 Phase A — BOUND RED)
# Written before any phase-3 implementation. These fail until types.h and
# save_format.h/save_format.c are fully implemented (GREEN phase).
# ---------------------------------------------------------------------------
add_host_test(test_types_bounds "${CMAKE_CURRENT_SOURCE_DIR}/test_types_bounds.c")

# ---------------------------------------------------------------------------
# Phase-3 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_host_test(test_save_format     "${CMAKE_CURRENT_SOURCE_DIR}/test_save_format.c")
add_host_test(test_save_corruption "${CMAKE_CURRENT_SOURCE_DIR}/test_save_corruption.c")

# ---------------------------------------------------------------------------
# Phase-4 Bound tests (Rule 22 Phase A — BOUND RED)
# Written before any phase-4 implementation. These fail until combat.h and
# combat.c are fully implemented (GREEN phase).
# ---------------------------------------------------------------------------
add_host_test(test_combat_bounds  "${CMAKE_CURRENT_SOURCE_DIR}/test_combat_bounds.c")

# ---------------------------------------------------------------------------
# Phase-4 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_host_test(test_combat_engine  "${CMAKE_CURRENT_SOURCE_DIR}/test_combat_engine.c")

# ---------------------------------------------------------------------------
# Phase-5 Bound tests (Rule 22 Phase A — BOUND RED)
# Written before any phase-5 item engine implementation.
# ---------------------------------------------------------------------------
add_host_test(test_item_bounds     "${CMAKE_CURRENT_SOURCE_DIR}/test_item_bounds.c")

# ---------------------------------------------------------------------------
# Phase-5 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_host_test(test_item_engine     "${CMAKE_CURRENT_SOURCE_DIR}/test_item_engine.c")
add_host_test(test_item_time_loop  "${CMAKE_CURRENT_SOURCE_DIR}/test_item_time_loop.c")

# ---------------------------------------------------------------------------
# Phase-6 Bound tests (Rule 22 Phase A — BOUND RED)
# Written before any phase-6 implementation. These fail until training.h,
# progression.h (new functions), and legacy.h/legacy.c are implemented.
# ---------------------------------------------------------------------------
add_host_test(test_training_bounds "${CMAKE_CURRENT_SOURCE_DIR}/test_training_bounds.c")
add_host_test(test_level_up_bounds "${CMAKE_CURRENT_SOURCE_DIR}/test_level_up_bounds.c")
add_host_test(test_legacy_bounds   "${CMAKE_CURRENT_SOURCE_DIR}/test_legacy_bounds.c")

# ---------------------------------------------------------------------------
# Phase-6 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_host_test(test_training        "${CMAKE_CURRENT_SOURCE_DIR}/test_training.c")
add_host_test(test_level_up        "${CMAKE_CURRENT_SOURCE_DIR}/test_level_up.c")
add_host_test(test_legacy          "${CMAKE_CURRENT_SOURCE_DIR}/test_legacy.c")

# ---------------------------------------------------------------------------
# Phase-7 Bound tests (Rule 22 Phase A — BOUND RED)
# Written before any phase-7 implementation. Prove the system REJECTS:
#   - OOB pixel coordinates (negative AND >=200)
#   - NULL pointer inputs on all fb/sprite/font functions
#   - Signed INT16_MIN coordinate underflow
#   - Degenerate line inputs (same point, all-OOB coords)
#   - MSB bit-order fidelity (pixel 0,0 == bit 7 of byte 0)
#   - 4-edge sprite clipping (negative x/y, past right/bottom)
#   - Zero-dimension sprites
#   - ASCII boundary enforcement (0x20-0x7E only)
#   - int16_t cursor not uint8_t
# ---------------------------------------------------------------------------
add_pres_test(test_fb_bounds     "${CMAKE_CURRENT_SOURCE_DIR}/test_fb_bounds.c")
add_pres_test(test_sprite_bounds "${CMAKE_CURRENT_SOURCE_DIR}/test_sprite_bounds.c")
add_pres_test(test_text_bounds   "${CMAKE_CURRENT_SOURCE_DIR}/test_text_bounds.c")

# ---------------------------------------------------------------------------
# Phase-7 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_pres_test(test_fb_feature     "${CMAKE_CURRENT_SOURCE_DIR}/test_fb_feature.c")
add_pres_test(test_sprite_feature "${CMAKE_CURRENT_SOURCE_DIR}/test_sprite_feature.c")
add_pres_test(test_text_feature   "${CMAKE_CURRENT_SOURCE_DIR}/test_text_feature.c")

# ---------------------------------------------------------------------------
# Phase-8: VM Builder sources (live in main/ — bridges game/ and presentation/)
# ---------------------------------------------------------------------------
set(MAIN_INCLUDE "${PROJECT_ROOT}/main")
set(VM_BUILDER_SOURCES "${PROJECT_ROOT}/main/vm_builder.c")

# Collect presentation screen sources (screens/ subdirectory).
file(GLOB PRES_SCREEN_SOURCES "${PRES_SRC_DIR}/screens/*.c")

# ---------------------------------------------------------------------------
# add_vm_test(<target> <source>)
#
# Builds a test that links game, presentation (including screens/), AND the
# vm_builder from main/. Includes GAME_INCLUDE, PRES_INCLUDE, and MAIN_INCLUDE
# so that view_models.h, vm_builder.h, and screen headers are all reachable.
# ---------------------------------------------------------------------------
function(add_vm_test target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE
        "${GAME_INCLUDE}"
        "${PRES_INCLUDE}"
        "${MAIN_INCLUDE}"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    if(GAME_SOURCES)
        target_sources(${target} PRIVATE ${GAME_SOURCES})
    endif()
    if(PRES_SOURCES)
        target_sources(${target} PRIVATE ${PRES_SOURCES})
    endif()
    if(PRES_SCREEN_SOURCES)
        target_sources(${target} PRIVATE ${PRES_SCREEN_SOURCES})
    endif()
    if(EXISTS "${PROJECT_ROOT}/main/vm_builder.c")
        target_sources(${target} PRIVATE "${PROJECT_ROOT}/main/vm_builder.c")
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# ---------------------------------------------------------------------------
# Phase-8 Bound tests (Rule 22 Phase A — BOUND RED)
# Written before any Phase-8 implementation.
# ---------------------------------------------------------------------------
add_vm_test(test_vm_bounds "${CMAKE_CURRENT_SOURCE_DIR}/test_vm_bounds.c")

# ---------------------------------------------------------------------------
# Phase-8 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_vm_test(test_vm_builder "${CMAKE_CURRENT_SOURCE_DIR}/test_vm_builder.c")

# ---------------------------------------------------------------------------
# Phase-9 Bound tests (Rule 22 Phase A — BOUND RED)
# Written before any Phase-9 implementation.
# ---------------------------------------------------------------------------
add_vm_test(test_p9_combat_bounds   "${CMAKE_CURRENT_SOURCE_DIR}/test_p9_combat_bounds.c")
add_vm_test(test_p9_dialogue_bounds "${CMAKE_CURRENT_SOURCE_DIR}/test_p9_dialogue_bounds.c")

# ---------------------------------------------------------------------------
# Phase-9 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_vm_test(test_p9_combat_feature   "${CMAKE_CURRENT_SOURCE_DIR}/test_p9_combat_feature.c")
add_vm_test(test_p9_dialogue_feature "${CMAKE_CURRENT_SOURCE_DIR}/test_p9_dialogue_feature.c")

# ---------------------------------------------------------------------------
# Phase-10: Connectivity protocol sources
#
# add_conn_test(<target> <source>)
#
# Builds a test that links game sources (for crc32, combat_hash, prng) AND
# the connectivity protocol/sync sources. Adds both GAME_INCLUDE and
# CONN_INCLUDE so that combat_hash.h and protocol.h/sync.h are both
# reachable.
#
# Architecture note: The test executable itself includes both game/ and
# connectivity/ headers, which is allowed at the APPLICATION layer (the
# test harness bridges both). The connectivity sources do NOT re-export
# game/ headers — they only use crc32.h internally (PRIV_REQUIRES pattern).
#
# N1c boundary is preserved: connectivity/include/ has no game/ in its
# PUBLIC include path (the boundary check tests this separately).
# ---------------------------------------------------------------------------
function(add_conn_test target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE
        "${GAME_INCLUDE}"
        "${CONN_INCLUDE}"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    if(GAME_SOURCES)
        target_sources(${target} PRIVATE ${GAME_SOURCES})
    endif()
    if(CONN_SOURCES)
        target_sources(${target} PRIVATE ${CONN_SOURCES})
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# ---------------------------------------------------------------------------
# Phase-10 Bound tests (Rule 22 Phase A — BOUND RED)
# Covers: NULL guards, nonce zero-XOR, round out-of-range, buffer underflow,
# magic mismatch, CRC mismatch, 1-bit PRNG avalanche, padding non-inclusion,
# DISCONNECT exact size, and unknown type (0xFF) rejection (QA-P10-02).
# ---------------------------------------------------------------------------
add_conn_test(test_p10_bounds "${CMAKE_CURRENT_SOURCE_DIR}/test_p10_bounds.c")

# ---------------------------------------------------------------------------
# Phase-10 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_conn_test(test_p10_protocol    "${CMAKE_CURRENT_SOURCE_DIR}/test_p10_protocol.c")
add_conn_test(test_p10_combat_sync "${CMAKE_CURRENT_SOURCE_DIR}/test_p10_combat_sync.c")

# ---------------------------------------------------------------------------
# Phase-11: Application Event Loop sources (live in main/)
#
# add_app_test(<target> <source>)
#
# Builds a test that links game, presentation (including screens/), vm_builder,
# event_bus, AND app_fsm from main/. Includes GAME_INCLUDE, PRES_INCLUDE,
# MAIN_INCLUDE, and test/ dir so that event_bus.h, app_fsm.h, and view_models.h
# are all reachable.
# ---------------------------------------------------------------------------
set(EVENT_BUS_SOURCE "${PROJECT_ROOT}/main/event_bus.c")
set(APP_FSM_SOURCE   "${PROJECT_ROOT}/main/app_fsm.c")

function(add_app_test target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE
        "${GAME_INCLUDE}"
        "${PRES_INCLUDE}"
        "${MAIN_INCLUDE}"
        "${CONN_INCLUDE}"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    if(GAME_SOURCES)
        target_sources(${target} PRIVATE ${GAME_SOURCES})
    endif()
    if(PRES_SOURCES)
        target_sources(${target} PRIVATE ${PRES_SOURCES})
    endif()
    if(PRES_SCREEN_SOURCES)
        target_sources(${target} PRIVATE ${PRES_SCREEN_SOURCES})
    endif()
    if(EXISTS "${PROJECT_ROOT}/main/vm_builder.c")
        target_sources(${target} PRIVATE "${PROJECT_ROOT}/main/vm_builder.c")
    endif()
    if(EXISTS "${EVENT_BUS_SOURCE}")
        target_sources(${target} PRIVATE "${EVENT_BUS_SOURCE}")
    endif()
    if(EXISTS "${APP_FSM_SOURCE}")
        target_sources(${target} PRIVATE "${APP_FSM_SOURCE}")
    endif()
    if(CONN_SOURCES)
        target_sources(${target} PRIVATE ${CONN_SOURCES})
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# ---------------------------------------------------------------------------
# Phase-11 Bound tests (Rule 22 Phase A — BOUND RED)
# Written before any Phase-11 implementation.
# ---------------------------------------------------------------------------
add_app_test(test_p11_bus_bounds "${CMAKE_CURRENT_SOURCE_DIR}/test_p11_bus_bounds.c")
add_app_test(test_p11_fsm_bounds "${CMAKE_CURRENT_SOURCE_DIR}/test_p11_fsm_bounds.c")

# ---------------------------------------------------------------------------
# Phase-11 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_app_test(test_p11_bus_feature "${CMAKE_CURRENT_SOURCE_DIR}/test_p11_bus_feature.c")
add_app_test(test_p11_fsm_feature "${CMAKE_CURRENT_SOURCE_DIR}/test_p11_fsm_feature.c")

# ---------------------------------------------------------------------------
# Phase-12: HAL E-Paper + Flash (host mocks)
#
# HAL headers live in components/fq_hal/include/ — this path is deliberately
# NOT added to the game/ or presentation/ include lists (boundary preserved).
# Mock .c files live in test/host/ and are linked directly into the test
# executables, replacing the real target implementations.
#
# add_hal_test(<target> <source>)
#
# Links a single HAL mock file (mock_hal_epaper.c or mock_hal_flash.c) with
# the test source. Only hal/include is added — no game/ or presentation/
# include paths, keeping HAL tests fully isolated at the bottom layer.
# ---------------------------------------------------------------------------
set(HAL_INCLUDE "${PROJECT_ROOT}/components/fq_hal/include")

function(add_hal_test target source mock_src)
    add_executable(${target} ${source} ${mock_src})
    target_include_directories(${target} PRIVATE
        "${HAL_INCLUDE}"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# ---------------------------------------------------------------------------
# Phase-12 Bound tests (Rule 22 Phase A — BOUND RED)
# Written before any Phase-12 implementation. Fail until headers+mocks exist.
# ---------------------------------------------------------------------------
add_hal_test(test_p12_hal_epaper_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p12_hal_epaper_bounds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_epaper.c")

add_hal_test(test_p12_hal_flash_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p12_hal_flash_bounds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_flash.c")

# ---------------------------------------------------------------------------
# Phase-12 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_hal_test(test_p12_hal_epaper_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p12_hal_epaper_feature.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_epaper.c")

add_hal_test(test_p12_hal_flash_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p12_hal_flash_feature.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_flash.c")

# ---------------------------------------------------------------------------
# Phase-13: HAL GPIO Buttons + Audio Piezo + Deep Sleep (host mocks)
#
# Same add_hal_test() pattern as Phase-12. Each test executable links its
# corresponding mock .c file which replaces the target stub at link time.
# HAL_INCLUDE is already set above — no game/ or presentation/ paths added,
# preserving the bottom-layer isolation boundary.
# ---------------------------------------------------------------------------

# Phase-13 Bound tests (Rule 22 Phase A — BOUND RED)
add_hal_test(test_p13_hal_gpio_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p13_hal_gpio_bounds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_gpio.c")

add_hal_test(test_p13_hal_audio_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p13_hal_audio_bounds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_audio.c")

add_hal_test(test_p13_hal_sleep_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p13_hal_sleep_bounds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_sleep.c")

# Phase-13 Feature tests (Rule 22 Phase B — FEATURE RED)
add_hal_test(test_p13_hal_gpio_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p13_hal_gpio_feature.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_gpio.c")

add_hal_test(test_p13_hal_audio_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p13_hal_audio_feature.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_audio.c")

add_hal_test(test_p13_hal_sleep_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p13_hal_sleep_feature.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_sleep.c")

# ---------------------------------------------------------------------------
# Phase-14: HAL BLE GATT + WiFi Captive Portal (host mocks)
#
# Same add_hal_test() pattern as Phase-12/13. Each test executable links its
# corresponding mock .c file which replaces the target stub at link time.
# HAL_INCLUDE is already set above — no game/ or presentation/ paths added,
# preserving the bottom-layer isolation boundary.
# ---------------------------------------------------------------------------

# Phase-14 Bound tests (Rule 22 Phase A — BOUND RED)
add_hal_test(test_p14_hal_ble_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p14_hal_ble_bounds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_ble.c")

add_hal_test(test_p14_hal_wifi_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p14_hal_wifi_bounds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_wifi.c")

# Phase-14 Feature tests (Rule 22 Phase B — FEATURE RED)
add_hal_test(test_p14_hal_ble_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p14_hal_ble_feature.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_ble.c")

add_hal_test(test_p14_hal_wifi_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p14_hal_wifi_feature.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_wifi.c")

# ---------------------------------------------------------------------------
# F-02 Remediation: fq_character_create
#
# character.c lives in game/ — use add_host_test() which links all GAME_SOURCES
# (the file(GLOB) at the top of this file picks up character.c automatically).
# ---------------------------------------------------------------------------

# Bound tests (Rule 22 Phase A — BOUND RED)
add_host_test(test_character_bounds "${CMAKE_CURRENT_SOURCE_DIR}/test_character_bounds.c")

# Feature tests (Rule 22 Phase B — FEATURE RED)
add_host_test(test_character "${CMAKE_CURRENT_SOURCE_DIR}/test_character.c")

# ---------------------------------------------------------------------------
# Phase-16: Hardware Bringup — failure injection bounds + feature tests.
#
# These tests use the add_hal_test() helper (same as Phase-12/13) but exercise
# the new failure injection APIs added to the mocks in this phase:
#   mock_epaper_inject_spi_error(), mock_epaper_inject_busy_timeout()
#   mock_flash_inject_write_error(), mock_flash_inject_mount_error()
# ---------------------------------------------------------------------------

# Phase-16 Bound tests (Rule 22 Phase A — BOUND RED)
add_hal_test(test_p16_hal_epaper_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p16_hal_epaper_bounds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_epaper.c")

add_hal_test(test_p16_hal_flash_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p16_hal_flash_bounds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_flash.c")

add_hal_test(test_p16_hal_gpio_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p16_hal_gpio_bounds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_gpio.c")

# Phase-16 Feature tests (Rule 22 Phase B — FEATURE RED)
add_hal_test(test_p16_hal_epaper_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p16_hal_epaper_feature.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_epaper.c")

add_hal_test(test_p16_hal_flash_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p16_hal_flash_feature.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_flash.c")

add_hal_test(test_p16_hal_gpio_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p16_hal_gpio_feature.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_gpio.c")

# ---------------------------------------------------------------------------
# Phase-17 Bound tests (Rule 22 Phase A — BOUND RED)
# Covers sprite array sizing, table count/dims, font bitmap size, NULL guards,
# off-screen blit, cursor advance, and descriptor field invariants.
# ---------------------------------------------------------------------------
add_pres_test(test_p17_asset_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p17_asset_bounds.c")

# ---------------------------------------------------------------------------
# Phase-17 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_pres_test(test_p17_asset_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p17_asset_feature.c")

# ---------------------------------------------------------------------------
# Phase-18 Bound tests (Rule 22 Phase A — BOUND RED)
# Covers asset accessor NULL/OOB guards, screen renderer NULL guards, HP bar
# overflow clamping, and non-zero pixel proof in name text region.
# Uses add_vm_test to link game + presentation (incl. screens + asset_data.c).
# ---------------------------------------------------------------------------
add_vm_test(test_p18_screen_text_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p18_screen_text_bounds.c")

# ---------------------------------------------------------------------------
# Phase-18 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_vm_test(test_p18_screen_text_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p18_screen_text_feature.c")

# ---------------------------------------------------------------------------
# Phase-17 patch: Title Screen Fix (font swap + BTN_B on TITLE + prompt text)
#
# Bound tests: BTN_B in TITLE must now advance to HOME; ghost events must
# leave state unchanged; PRNG must be untouched.
# Feature tests: both buttons advance TITLE; font-title returns FONT_SCRIPT_36;
# two-line layout width invariants.
# ---------------------------------------------------------------------------
add_app_test(test_p17_title_fix_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p17_title_fix_bounds.c")

add_app_test(test_p17_title_fix_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p17_title_fix_feature.c")

# ---------------------------------------------------------------------------
# Kerning fix: fq_draw_text_2x + corrected advance widths
#
# Bound tests: NULL guards on draw_text_2x, cursor overflow safety, 2x block
# pixel address arithmetic, empty-string idempotency.
# Feature tests: 2x pixel rendering, corrected FONT_SCRIPT_36 advance widths,
# two-line layout width invariants.
# Uses add_pres_test to link sprite_util.c and asset_data.c.
# ---------------------------------------------------------------------------
add_pres_test(test_kerning_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_kerning_bounds.c")

add_pres_test(test_kerning_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_kerning_feature.c")

# ---------------------------------------------------------------------------
# Phase-19 Bound tests (Rule 22 Phase A — BOUND RED)
# Home screen navigation menu: menu_index overflow/wrap, NULL guards,
# OOB menu_index clamping in renderer.
# Uses add_app_test to link game + presentation (screens/) + event_bus + app_fsm.
# ---------------------------------------------------------------------------
add_app_test(test_p19_home_menu_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p19_home_menu_bounds.c")

# ---------------------------------------------------------------------------
# Phase-19 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_app_test(test_p19_home_menu_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p19_home_menu_feature.c")


# ---------------------------------------------------------------------------
# Phase-19.5: Idle Screen, Walk Animation & Partial E-Paper Refresh
#
# Bound tests (Rule 22 Phase A — BOUND RED):
#   test_p19_5_epaper_partial_bounds — HAL partial refresh guard/counter bounds
#   test_p19_5_home_anim_bounds     — anim_frame clamping, struct size assert
#   test_p19_5_idle_bounds          — idle suppression, blit_3x NULL guards
#
# Feature tests (Rule 22 Phase B — FEATURE RED):
#   test_p19_5_epaper_partial_feature — partial LUT, interval full-refresh
#   test_p19_5_home_anim_feature     — frame alternation, interval constant
#   test_p19_5_idle_feature          — idle render, vm_build_idle, blit_3x
#
# Epaper partial tests use add_hal_test (HAL layer only — no game/ or pres/).
# Animation and idle tests use add_app_test (game + pres + screens + vm_builder
# + event_bus + app_fsm) because they test FSM state transitions.
# ---------------------------------------------------------------------------

# Phase-19.5 Bound tests
add_hal_test(test_p19_5_epaper_partial_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p19_5_epaper_partial_bounds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_epaper.c")

add_app_test(test_p19_5_home_anim_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p19_5_home_anim_bounds.c")

add_app_test(test_p19_5_idle_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p19_5_idle_bounds.c")

# Phase-19.5 Feature tests
add_hal_test(test_p19_5_epaper_partial_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p19_5_epaper_partial_feature.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_epaper.c")

add_app_test(test_p19_5_home_anim_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p19_5_home_anim_feature.c")

add_app_test(test_p19_5_idle_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p19_5_idle_feature.c")

# ---------------------------------------------------------------------------
# Phase-19 Interactive Gameplay — Onboarding, Training, Inventory Equip
#
# Bound tests (Rule 22 Phase A — BOUND RED):
#   17 bound tests: class wrap, name length/null-term, save-failure re-entry,
#   FSM state value, sizeof, score clamp, XP at L99, PRNG isolation,
#   zero targets zero XP, hit outside active, full slots rejected,
#   item ID 0 rejected, duplicate unequip, count clamp, cursor bounds,
#   empty inventory no-crash.
#
# Feature tests (Rule 22 Phase B — FEATURE RED):
#   18 feature tests: all three items — onboarding flow, training session
#   state machine, inventory equip/unequip with VM builder integration.
#
# Both use add_app_test() to link game + presentation (screens/) + vm_builder
# + event_bus + app_fsm. The new game sources (name_gen.c, training_session.c,
# equip.c) are picked up automatically by file(GLOB GAME_SOURCES ...).
# ---------------------------------------------------------------------------

# Phase-19 Interactive Bound tests
add_app_test(test_p19_interactive_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p19_interactive_bounds.c")

# Phase-19 Interactive Feature tests
add_app_test(test_p19_interactive_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p19_interactive_feature.c")

# ---------------------------------------------------------------------------
# Phase-19 Audit Fix: Training BTN_B partial XP award
# ---------------------------------------------------------------------------
add_app_test(test_p19_training_b_exit
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p19_training_b_exit.c")

# ---------------------------------------------------------------------------
# Phase-20: BLE Combat, Battle Result, Rebirth
#
# add_p20_test(<target> <source>)
#
# Links game + presentation (screens/) + vm_builder + event_bus + app_fsm
# + connectivity protocol/sync (for fq_team_sync_validate and
# fq_team_sync_clamp_equipped_count used in bounds tests).
# Includes GAME_INCLUDE, PRES_INCLUDE, MAIN_INCLUDE, CONN_INCLUDE.
# ---------------------------------------------------------------------------
function(add_p20_test target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE
        "${GAME_INCLUDE}"
        "${PRES_INCLUDE}"
        "${MAIN_INCLUDE}"
        "${CONN_INCLUDE}"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    if(GAME_SOURCES)
        target_sources(${target} PRIVATE ${GAME_SOURCES})
    endif()
    if(PRES_SOURCES)
        target_sources(${target} PRIVATE ${PRES_SOURCES})
    endif()
    if(PRES_SCREEN_SOURCES)
        target_sources(${target} PRIVATE ${PRES_SCREEN_SOURCES})
    endif()
    if(EXISTS "${PROJECT_ROOT}/main/vm_builder.c")
        target_sources(${target} PRIVATE "${PROJECT_ROOT}/main/vm_builder.c")
    endif()
    if(EXISTS "${EVENT_BUS_SOURCE}")
        target_sources(${target} PRIVATE "${EVENT_BUS_SOURCE}")
    endif()
    if(EXISTS "${APP_FSM_SOURCE}")
        target_sources(${target} PRIVATE "${APP_FSM_SOURCE}")
    endif()
    if(CONN_SOURCES)
        target_sources(${target} PRIVATE ${CONN_SOURCES})
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# ---------------------------------------------------------------------------
# Phase-20 Bound tests (Rule 22 Phase A — BOUND RED)
# Covers: combat hash determinism, PRNG isolation, XP saturation, rebirth
# underflow guards, legacy saturation, wins/losses saturation, team sync
# validation, FSM disconnect handling, BATTLE_RESULT routing, BATTLE_SETUP
# cancel/timeout, and legacy tree full no-op.
# ---------------------------------------------------------------------------
add_p20_test(test_p20_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p20_bounds.c")

# ---------------------------------------------------------------------------
# Phase-20 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_p20_test(test_p20_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p20_feature.c")

# ---------------------------------------------------------------------------
# Phase-21: I2S Audio Engine + sfxr-c Sound Effects
#
# add_p21_test(<target> <source>)
#
# Links HAL mock (mock_hal_audio.c) + game sources (which include sfxr.c and
# sfx.c once they exist) + game/lib sources.
# Includes HAL_INCLUDE (for hal_audio.h) and GAME_INCLUDE (for sfx.h, types.h).
#
# Architecture: sfx.h lives in game/include; sfxr.h/sfxr.c live in game/lib.
# sfxr.c is picked up by GAME_SOURCES glob (or added explicitly below).
# ---------------------------------------------------------------------------
# GAME_LIB_DIR is defined at the top of this file (alongside GAME_SOURCES glob).
# sfxr.c and sfx.c are included in GAME_SOURCES via the lib glob.

function(add_p21_test target source)
    add_executable(${target} ${source}
        "${CMAKE_CURRENT_SOURCE_DIR}/mock_hal_audio.c")
    target_include_directories(${target} PRIVATE
        "${HAL_INCLUDE}"
        "${GAME_INCLUDE}"
        "${GAME_LIB_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    if(GAME_SOURCES)
        target_sources(${target} PRIVATE ${GAME_SOURCES})
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# ---------------------------------------------------------------------------
# Phase-21 Bound tests (Rule 22 Phase A — BOUND RED)
# Covers: ABI pinning (ERR_NULL=3, ERR_OVERFLOW=4), write-before-init,
# null-buffer, freq-zero, duration-clamp, ring-overflow, invalid sfx id,
# sample-count bound, sfx return-type contract.
# ---------------------------------------------------------------------------
add_p21_test(test_p21_audio_sfx_bounds
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p21_audio_sfx_bounds.c")

# ---------------------------------------------------------------------------
# Phase-21 Feature tests (Rule 22 Phase B — FEATURE RED)
# ---------------------------------------------------------------------------
add_p21_test(test_p21_audio_sfx_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/test_p21_audio_sfx_feature.c")
