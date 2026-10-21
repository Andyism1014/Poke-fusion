#include "global.h"
#include "fusion/fusion.h"
#include "test/battle.h"

// Phase 04a: a cross-species main ability replaces the species ability.

ASSUMPTIONS
{
    ASSUME(GetSpeciesAbility(SPECIES_WOBBUFFET, 0) != ABILITY_INTIMIDATE);
    ASSUME(GetSpeciesAbility(SPECIES_WOBBUFFET, 0) != ABILITY_LEVITATE);
    ASSUME(GetMoveType(MOVE_EARTHQUAKE) == TYPE_GROUND);
}

TEST("Fusion ability: GetMonAbility returns the cross-species main ability")
{
    struct Pokemon mon;
    struct FusionGenetics genetics = {.mainAbility = ABILITY_LEVITATE};

    CreateMon(&mon, SPECIES_WOBBUFFET, 50, 0, OTID_STRUCT_PLAYER_ID);
    EXPECT_EQ(GetMonAbility(&mon), GetSpeciesAbility(SPECIES_WOBBUFFET, 0));
    Fusion_SetMonGenetics(&mon, &genetics);
    EXPECT_EQ(GetMonAbility(&mon), ABILITY_LEVITATE);
}

SINGLE_BATTLE_TEST("Fusion ability: a cross-species Intimidate activates on switch-in")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { FusionAbility(ABILITY_INTIMIDATE); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_INTIMIDATE);
        ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_STATS_CHANGE, opponent);
    } THEN {
        EXPECT_EQ(player->ability, ABILITY_INTIMIDATE);
        EXPECT_EQ(opponent->statStages[STAT_ATK], DEFAULT_STAT_STAGE - 1);
    }
}

SINGLE_BATTLE_TEST("Fusion ability: a cross-species Levitate grants Ground immunity")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { FusionAbility(ABILITY_LEVITATE); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_EARTHQUAKE); }
    } SCENE {
        NOT HP_BAR(player);
    }
}

SINGLE_BATTLE_TEST("Fusion ability: the cross-species ability comes back after switching out")
{
    GIVEN {
        ASSUME(GetMoveEffect(MOVE_WORRY_SEED) == EFFECT_OVERWRITE_ABILITY);
        PLAYER(SPECIES_WOBBUFFET) { FusionAbility(ABILITY_LEVITATE); }
        PLAYER(SPECIES_WYNAUT);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_WORRY_SEED); }
        TURN { SWITCH(player, 1); }
        TURN { SWITCH(player, 0); }
    } THEN {
        EXPECT_EQ(player->ability, ABILITY_LEVITATE);
    }
}

SINGLE_BATTLE_TEST("Fusion ability: an opposing trainer's fused Pokémon uses its cross-species ability")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { FusionAbility(ABILITY_INTIMIDATE); }
    } WHEN {
        TURN { }
    } SCENE {
        ABILITY_POPUP(opponent, ABILITY_INTIMIDATE);
    } THEN {
        EXPECT_EQ(player->statStages[STAT_ATK], DEFAULT_STAT_STAGE - 1);
    }
}

AI_SINGLE_BATTLE_TEST("Fusion ability: AI knows a cross-species Levitate")
{
    GIVEN {
        AI_FLAGS(AI_FLAG_CHECK_BAD_MOVE | AI_FLAG_CHECK_VIABILITY | AI_FLAG_TRY_TO_FAINT);
        PLAYER(SPECIES_WOBBUFFET) { FusionAbility(ABILITY_LEVITATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_EARTHQUAKE, MOVE_TACKLE); }
    } WHEN {
        TURN { EXPECT_MOVE(opponent, MOVE_TACKLE); }
    }
}
