/**
 * music_table.h — FiestaQuest Music Track Table
 *
 * Maps FSM states to .mod file categories and paths. Track selection uses
 * tick_count entropy — NEVER the combat PRNG (Constitution Priority 0).
 *
 * Categories:
 *   FQ_MUSIC_CAT_CHILL   — Mellow tracks for home / idle states.
 *   FQ_MUSIC_CAT_INTENSE — High-energy tracks for combat.
 *   FQ_MUSIC_CAT_UPBEAT  — Energetic tracks for menus / onboarding.
 *
 * Placeholder paths: these are LittleFS paths where the .mod files reside
 * at runtime. In a factory build the files are bundled in the LittleFS
 * image. The placeholder strings must match the filenames embedded in the
 * partition image.
 *
 * Architecture boundary:
 *   - Lives in components/game/include/ — game layer.
 *   - MUST NOT include hal_*.h headers.
 *   - Referenced from app_main.c (application layer) for state-based selection.
 */

#ifndef FIESTAQUEST_GAME_MUSIC_TABLE_H
#define FIESTAQUEST_GAME_MUSIC_TABLE_H

#include <stdint.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Music categories
 * -------------------------------------------------------------------------
 */
typedef enum {
    FQ_MUSIC_CAT_CHILL   = 0, /**< Chill / ambient — home screen, idle. */
    FQ_MUSIC_CAT_INTENSE = 1, /**< Intense — combat. */
    FQ_MUSIC_CAT_UPBEAT  = 2, /**< Upbeat — menus, onboarding. */
    FQ_MUSIC_CAT_COUNT   = 3  /**< Sentinel. */
} fq_music_cat_t;

/* -------------------------------------------------------------------------
 * Track entry
 * -------------------------------------------------------------------------
 */
typedef struct {
    const char *path; /**< LittleFS path, e.g. "/littlefs/music/track1.mod" */
} fq_music_track_t;

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------
 */

/**
 * fq_music_table_count — Returns the number of tracks in a given category.
 *
 * @param cat Category index [0, FQ_MUSIC_CAT_COUNT-1].
 * @return    Number of tracks, or 0 if category is out of range.
 */
size_t fq_music_table_count(fq_music_cat_t cat);

/**
 * fq_music_table_get — Returns the track entry at a given index in a category.
 *
 * @param cat   Category index.
 * @param index Track index [0, fq_music_table_count(cat)-1].
 * @return      Pointer to track entry, or NULL if out of range.
 */
const fq_music_track_t *fq_music_table_get(fq_music_cat_t cat, size_t index);

/**
 * fq_music_table_pick — Pick a random track from a category using tick_count
 * entropy. Does NOT touch the combat PRNG.
 *
 * @param cat        Category to pick from.
 * @param tick_count Monotonic tick counter from fq_app_ctx_t.
 * @return           Pointer to selected track, or NULL if category is empty.
 */
const fq_music_track_t *fq_music_table_pick(fq_music_cat_t cat,
                                             uint32_t        tick_count);

#endif /* FIESTAQUEST_GAME_MUSIC_TABLE_H */
