/**
 * test_assert.h — FiestaQuest host test assertion macros.
 *
 * Typed assertion macros that print diagnostic output and exit on failure.
 * Each macro prints [PASS] to stdout on success and [FAIL] with expected/actual
 * values to stderr before exit(1) on failure.
 *
 * Usage:
 *   TEST_ASSERT_EQUAL_UINT32(300u, result);
 *   TEST_ASSERT_NOT_NULL(ptr);
 *   TEST_ASSERT_NULL(ptr);
 *
 * Design: macros capture __FILE__ and __LINE__ at the call site so failure
 * messages identify the exact test source location.
 */

#ifndef FIESTAQUEST_TEST_ASSERT_H
#define FIESTAQUEST_TEST_ASSERT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Internal helper: print a pass message. Used by typed macros below.
 * ---------------------------------------------------------------------------*/
#define _TA_PASS(expr_str) \
    printf("[PASS] %s:%d  %s\n", __FILE__, __LINE__, (expr_str))

/* ---------------------------------------------------------------------------
 * TEST_ASSERT_EQUAL_UINT8(expected, actual)
 * ---------------------------------------------------------------------------*/
#define TEST_ASSERT_EQUAL_UINT8(expected, actual)                         \
    do {                                                                   \
        uint8_t _exp = (uint8_t)(expected);                                \
        uint8_t _act = (uint8_t)(actual);                                  \
        if (_exp != _act) {                                                 \
            fprintf(stderr,                                                 \
                "[FAIL] %s:%d  expected=%" PRIu8 " actual=%" PRIu8 "\n",  \
                __FILE__, __LINE__, _exp, _act);                           \
            exit(1);                                                       \
        }                                                                  \
        _TA_PASS(#actual);                                                 \
    } while (0)

/* ---------------------------------------------------------------------------
 * TEST_ASSERT_EQUAL_UINT16(expected, actual)
 * ---------------------------------------------------------------------------*/
#define TEST_ASSERT_EQUAL_UINT16(expected, actual)                          \
    do {                                                                     \
        uint16_t _exp = (uint16_t)(expected);                                \
        uint16_t _act = (uint16_t)(actual);                                  \
        if (_exp != _act) {                                                   \
            fprintf(stderr,                                                   \
                "[FAIL] %s:%d  expected=%" PRIu16 " actual=%" PRIu16 "\n",  \
                __FILE__, __LINE__, _exp, _act);                             \
            exit(1);                                                         \
        }                                                                    \
        _TA_PASS(#actual);                                                   \
    } while (0)

/* ---------------------------------------------------------------------------
 * TEST_ASSERT_EQUAL_UINT32(expected, actual)
 * ---------------------------------------------------------------------------*/
#define TEST_ASSERT_EQUAL_UINT32(expected, actual)                          \
    do {                                                                     \
        uint32_t _exp = (uint32_t)(expected);                                \
        uint32_t _act = (uint32_t)(actual);                                  \
        if (_exp != _act) {                                                   \
            fprintf(stderr,                                                   \
                "[FAIL] %s:%d  expected=%" PRIu32 " actual=%" PRIu32 "\n",  \
                __FILE__, __LINE__, _exp, _act);                             \
            exit(1);                                                         \
        }                                                                    \
        _TA_PASS(#actual);                                                   \
    } while (0)

/* ---------------------------------------------------------------------------
 * TEST_ASSERT_EQUAL_INT(expected, actual)
 * ---------------------------------------------------------------------------*/
#define TEST_ASSERT_EQUAL_INT(expected, actual)                    \
    do {                                                            \
        int _exp = (int)(expected);                                 \
        int _act = (int)(actual);                                   \
        if (_exp != _act) {                                         \
            fprintf(stderr,                                         \
                "[FAIL] %s:%d  expected=%d actual=%d\n",           \
                __FILE__, __LINE__, _exp, _act);                   \
            exit(1);                                               \
        }                                                          \
        _TA_PASS(#actual);                                         \
    } while (0)

/* ---------------------------------------------------------------------------
 * TEST_ASSERT_NOT_NULL(ptr)
 * ---------------------------------------------------------------------------*/
#define TEST_ASSERT_NOT_NULL(ptr)                                  \
    do {                                                            \
        if ((ptr) == NULL) {                                        \
            fprintf(stderr,                                         \
                "[FAIL] %s:%d  expected non-NULL pointer: %s\n",   \
                __FILE__, __LINE__, #ptr);                         \
            exit(1);                                               \
        }                                                          \
        _TA_PASS(#ptr " != NULL");                                 \
    } while (0)

/* ---------------------------------------------------------------------------
 * TEST_ASSERT_NULL(ptr)
 * ---------------------------------------------------------------------------*/
#define TEST_ASSERT_NULL(ptr)                                      \
    do {                                                            \
        if ((ptr) != NULL) {                                        \
            fprintf(stderr,                                         \
                "[FAIL] %s:%d  expected NULL, got %p\n",           \
                __FILE__, __LINE__, (const void *)(ptr));          \
            exit(1);                                               \
        }                                                          \
        _TA_PASS(#ptr " == NULL");                                 \
    } while (0)

/* ---------------------------------------------------------------------------
 * TEST_ASSERT_TRUE(condition)
 * ---------------------------------------------------------------------------*/
#define TEST_ASSERT_TRUE(condition)                                \
    do {                                                            \
        if (!(condition)) {                                         \
            fprintf(stderr,                                         \
                "[FAIL] %s:%d  expected TRUE: %s\n",               \
                __FILE__, __LINE__, #condition);                   \
            exit(1);                                               \
        }                                                          \
        _TA_PASS(#condition);                                      \
    } while (0)

/* ---------------------------------------------------------------------------
 * TEST_ASSERT_FAIL(msg)
 *
 * Unconditional failure — use to mark code paths that must not be reached.
 * ---------------------------------------------------------------------------*/
#define TEST_ASSERT_FAIL(msg)                                      \
    do {                                                            \
        fprintf(stderr,                                             \
            "[FAIL] %s:%d  %s\n",                                  \
            __FILE__, __LINE__, (msg));                            \
        exit(1);                                                   \
    } while (0)

#endif /* FIESTAQUEST_TEST_ASSERT_H */
