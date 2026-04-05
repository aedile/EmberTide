/**
 * music_table.c — FiestaQuest Music Track Table Implementation
 *
 * Maps FSM state categories to LittleFS .mod file paths.
 * Track selection uses tick_count entropy — never the combat PRNG.
 *
 * Placeholder tracks: these paths correspond to .mod files bundled in the
 * LittleFS partition image. In a factory build the real module files are
 * at these paths. For development/testing the paths are symbolic.
 *
 * Constitution Priority 0: No floating point. No combat PRNG.
 */

#include "music_table.h"
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Static track tables by category.
 *
 * Each entry is a LittleFS path. Paths are null-terminated string literals
 * stored in flash (const — read-only).
 * -------------------------------------------------------------------------
 */

static const fq_music_track_t s_chill_tracks[] = {
    { "/littlefs/music/chill_01.mod" },
    { "/littlefs/music/chill_02.mod" },
    { "/littlefs/music/chill_03.mod" },
    { "/littlefs/music/chill_04.mod" },
    { "/littlefs/music/chill_05.mod" },
};

static const fq_music_track_t s_intense_tracks[] = {
    { "/littlefs/music/intense_01.mod" },
    { "/littlefs/music/intense_02.mod" },
    { "/littlefs/music/intense_03.mod" },
    { "/littlefs/music/intense_04.mod" },
};

static const fq_music_track_t s_upbeat_tracks[] = {
    { "/littlefs/music/upbeat_01.mod" },
    { "/littlefs/music/upbeat_02.mod" },
    { "/littlefs/music/upbeat_03.mod" },
};

/* -------------------------------------------------------------------------
 * Category dispatch table (indexed by fq_music_cat_t).
 * -------------------------------------------------------------------------
 */
typedef struct {
    const fq_music_track_t *tracks;
    size_t                   count;
} cat_entry_t;

static const cat_entry_t s_categories[FQ_MUSIC_CAT_COUNT] = {
    { s_chill_tracks,   sizeof(s_chill_tracks)   / sizeof(s_chill_tracks[0])   },
    { s_intense_tracks, sizeof(s_intense_tracks) / sizeof(s_intense_tracks[0]) },
    { s_upbeat_tracks,  sizeof(s_upbeat_tracks)  / sizeof(s_upbeat_tracks[0])  },
};

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------
 */

size_t fq_music_table_count(fq_music_cat_t cat)
{
    if ((unsigned)cat >= (unsigned)FQ_MUSIC_CAT_COUNT) {
        return 0u;
    }
    return s_categories[cat].count;
}

const fq_music_track_t *fq_music_table_get(fq_music_cat_t cat, size_t index)
{
    if ((unsigned)cat >= (unsigned)FQ_MUSIC_CAT_COUNT) {
        return NULL;
    }
    if (index >= s_categories[cat].count) {
        return NULL;
    }
    return &s_categories[cat].tracks[index];
}

const fq_music_track_t *fq_music_table_pick(fq_music_cat_t cat,
                                              uint32_t        tick_count)
{
    if ((unsigned)cat >= (unsigned)FQ_MUSIC_CAT_COUNT) {
        return NULL;
    }
    size_t count = s_categories[cat].count;
    if (count == 0u) {
        return NULL;
    }
    /* Simple deterministic selection using tick_count modulo count.
     * Does NOT touch any PRNG — Constitution Priority 0 preserved. */
    size_t index = (size_t)(tick_count % (uint32_t)count);
    return &s_categories[cat].tracks[index];
}
