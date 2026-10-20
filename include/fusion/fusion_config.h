#ifndef GUARD_FUSION_CONFIG_H
#define GUARD_FUSION_CONFIG_H

// Fusion (炼妖) feature configuration. See docs/fusion/ROADMAP.md.

// Capture assist
#define FUSION_VAR_CAPTURE_MODE             VAR_FUSION_CAPTURE_MODE // Runtime capture mode, one of FUSION_CAPTURE_MODE_*. 0 keeps vanilla odds.
#define FUSION_CAPTURE_MODE_NEW_GAME        FUSION_CAPTURE_MODE_CONVENIENT // Mode set when starting a new game.
#define FUSION_CAPTURE_CONVENIENT_MULTIPLIER 2 // Catch odds multiplier in FUSION_CAPTURE_MODE_CONVENIENT.

// Fusion inheritance (percent chances)
#define FUSION_SPECIES_FROM_A_CHANCE        50 // Child species comes from parent A, otherwise parent B.
#define FUSION_NATURE_FROM_A_CHANCE         50
#define FUSION_IV_FROM_A_CHANCE             50 // Rolled per stat.
#define FUSION_IV_MUTATION_CHANCE           10 // Per stat: the inherited IV gains 1..FUSION_IV_MUTATION_MAX (capped at 31).
#define FUSION_IV_MUTATION_MAX              3
#define FUSION_MOVE_INHERIT_CHANCE          50 // Per move in the deduplicated pool of both parents' moves; at most 4 are kept.
#define FUSION_MAIN_ABILITY_FROM_A_CHANCE   50
#define FUSION_PASSIVE_INHERIT_CHANCE       45 // Per ability in the pool of both parents' main abilities and passives; at most 3 are kept.
#define FUSION_THIRD_TYPE_CHANCE            50 // Chance to gain a third type when the parents carry a type the child species lacks.

// Items given when starting a new game
#define FUSION_NEW_GAME_GIVE_BOX_LINK       TRUE // Gives the Pokémon Box Link so the PC can be opened anywhere.
#define FUSION_NEW_GAME_GIVE_TERMINAL       TRUE // Gives the Fusion Terminal key item (appraise, storage and, from phase 03, fusion).
#define FUSION_NEW_GAME_GIVE_DEXNAV         TRUE // Adds DexNav to the start menu from the beginning. Requires DEXNAV_ENABLED.

#endif // GUARD_FUSION_CONFIG_H
