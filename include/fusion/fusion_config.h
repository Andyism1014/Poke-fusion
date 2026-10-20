#ifndef GUARD_FUSION_CONFIG_H
#define GUARD_FUSION_CONFIG_H

// Fusion (炼妖) feature configuration. See docs/fusion/ROADMAP.md.

// Capture assist
#define FUSION_VAR_CAPTURE_MODE             VAR_FUSION_CAPTURE_MODE // Runtime capture mode, one of FUSION_CAPTURE_MODE_*. 0 keeps vanilla odds.
#define FUSION_CAPTURE_MODE_NEW_GAME        FUSION_CAPTURE_MODE_CONVENIENT // Mode set when starting a new game.
#define FUSION_CAPTURE_CONVENIENT_MULTIPLIER 2 // Catch odds multiplier in FUSION_CAPTURE_MODE_CONVENIENT.

// Items given when starting a new game
#define FUSION_NEW_GAME_GIVE_BOX_LINK       TRUE // Gives the Pokémon Box Link so the PC can be opened anywhere.
#define FUSION_NEW_GAME_GIVE_TERMINAL       TRUE // Gives the Fusion Terminal key item (appraise, storage and, from phase 03, fusion).
#define FUSION_NEW_GAME_GIVE_DEXNAV         TRUE // Adds DexNav to the start menu from the beginning. Requires DEXNAV_ENABLED.

#endif // GUARD_FUSION_CONFIG_H
