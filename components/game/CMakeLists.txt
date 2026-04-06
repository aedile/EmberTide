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
        # Phase 22: MOD music playback (game-layer wrapper).
        # micromod.c exports: micromod_init(), micromod_render() — used by music.c.
        # micromod.c is integer-only (no float ADR needed).
        # music.c wraps micromod with play/stop/render API.
        # music_table.c provides state→track category mapping.
        "lib/micromod.c"
        # Phase 23: Real micromod player (Martin Cameron, public domain).
        #
        # ADR (ADV-A4): Why both micromod.c and micromod_real.c coexist:
        #   - micromod.c exports the game-layer API: micromod_init(), micromod_render(),
        #     micromod_ctx_t. These are used by music.c (game/ layer wrapper).
        #   - micromod_real.c exports the hardware-player API: micromod_initialise(),
        #     micromod_get_audio(), micromod_set_gain(). These are called directly from
        #     app_main.c for real-time MOD playback on the ESP32-S3.
        #   - The two files export DIFFERENT public symbols — there is NO linker conflict.
        #   - micromod_real.c holds all global player state (module_data, channels[],
        #     song_length, etc.) as translation-unit statics. This is intentional:
        #     the Martin Cameron player is a single-instance design and its state is
        #     not exposed in any header. This is consistent with its original public-
        #     domain design and acceptable given the single-producer audio model in
        #     app_main.c (do_music_tick() is the sole caller, no mutex needed).
        #   - Future refactor path: migrate music.c to call micromod_real directly,
        #     then remove the old micromod.c stub. Blocked until Phase 24 audio ADR.
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
