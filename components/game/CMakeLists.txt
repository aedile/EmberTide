idf_component_register(
    SRCS
        "src/types.c"
        "src/prng.c"
        "src/crc32.c"
        "src/progression.c"
        "src/save_format.c"
        "src/combat.c"
        "src/item_engine.c"
        "src/training.c"
        "src/legacy.c"
        "src/combat_hash.c"
        "src/character.c"
        "src/equip.c"
        "src/training_session.c"
        "src/name_gen.c"
        "src/sfx.c"
        # ADR: sfxr.c uses float for audio synthesis (envelope, frequency, phase).
        # This is EXEMPT from Constitution Priority 0 float ban because:
        # 1. sfxr is audio signal processing, not deterministic game logic
        # 2. ESP32-S3 has hardware single-precision FPU (no performance penalty)
        # 3. sfxr output (PCM samples) does not feed back into combat/PRNG state
        # See: docs/fiestaquest-architecture.md v2.20 Phase 21 amendment
        "lib/sfxr.c"
        # Phase 22: MOD music playback.
        # micromod.c is integer-only (no float ADR needed).
        # music.c wraps micromod with play/stop/render API.
        # music_table.c provides state→track category mapping.
        "lib/micromod.c"
        "lib/micromod_real.c"
        "src/music.c"
        "src/music_table.c"
    INCLUDE_DIRS
        "include"
        "lib"
    # game/ MUST NOT depend on hal/, presentation/, or connectivity/.
    # This component declares no REQUIRES or PRIV_REQUIRES to those layers.
    # Enforced by CMake include path isolation and try_compile boundary checks.
)
