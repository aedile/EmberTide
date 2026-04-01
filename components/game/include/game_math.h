/**
 * game_math.h — FiestaQuest Shared Integer Math Utilities.
 *
 * Provides saturating arithmetic helpers shared across game modules.
 * All functions are pure (no side effects, no global state).
 *
 * Constitution Priority 0: No floating point, no external entropy.
 *
 * Usage: include this header in any game component that requires saturating
 * addition. Placing the helpers here eliminates the duplicate `sat8_add`
 * static functions that previously existed independently in legacy.c and
 * progression.c.
 */

#ifndef FIESTAQUEST_GAME_MATH_H
#define FIESTAQUEST_GAME_MATH_H

#include <stdint.h>

/**
 * fq_sat8_add() — Saturating uint8_t addition.
 *
 * Returns min(a + b, 255). Promotes to uint16_t for the addition to
 * guarantee no overflow before the clamp.
 *
 * @param a  First operand.
 * @param b  Second operand.
 * @return   Clamped sum in [0, 255].
 */
static inline uint8_t fq_sat8_add(uint8_t a, uint8_t b)
{
    uint16_t sum = (uint16_t)a + (uint16_t)b;
    return (sum > 255u) ? 255u : (uint8_t)sum;
}

#endif /* FIESTAQUEST_GAME_MATH_H */
