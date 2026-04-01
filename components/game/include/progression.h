/**
 * progression.h — FiestaQuest Effective Stat Curve Lookup.
 *
 * Maps a raw uint8_t stat value (0-255) to an effective stat using a frozen
 * 256-entry logarithmic lookup table. The table embodies:
 *
 *   effective = round(10 * ln(raw + 1) / ln(11))
 *
 * This formula was evaluated offline in Python; the results are embedded as
 * literal integers. No floating-point math ever executes at runtime.
 *
 * Domain:  raw in [0, 255]
 * Range:   effective in [0, 23]
 * Properties: monotonically non-decreasing, pure function (no side effects).
 *
 * Pinned values (from frozen table):
 *   raw=0   → 0
 *   raw=1   → 3
 *   raw=10  → 10
 *   raw=50  → 16
 *   raw=100 → 19
 *   raw=200 → 22
 *   raw=255 → 23
 */

#ifndef FIESTAQUEST_PROGRESSION_H
#define FIESTAQUEST_PROGRESSION_H

#include <stdint.h>

/**
 * fq_effective_stat() — Look up the effective stat for a raw stat value.
 *
 * Pure function: result depends only on `raw`. No global mutable state.
 * Safe for all uint8_t inputs including 0 and 255.
 *
 * @param raw  Raw stat value in [0, 255].
 * @return     Effective stat in [0, 23].
 */
uint8_t fq_effective_stat(uint8_t raw);

#endif /* FIESTAQUEST_PROGRESSION_H */
