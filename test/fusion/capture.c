#include "global.h"
#include "event_data.h"
#include "item.h"
#include "fusion/fusion.h"
#include "test/battle.h"

ASSUMPTIONS
{
    ASSUME(gSpeciesInfo[SPECIES_CLEFFA].catchRate == 150);
    ASSUME(gSpeciesInfo[SPECIES_MEWTWO].isRestrictedLegendary);
}

WILD_BATTLE_TEST("Fusion capture mode: catch odds scale with the selected mode")
{
    u32 expectedOdds;
    u32 recordedOdds;
    u32 mode;

    PARAMETRIZE(expectedOdds = 50, mode = FUSION_CAPTURE_MODE_CLASSIC);
    PARAMETRIZE(expectedOdds = 50 * FUSION_CAPTURE_CONVENIENT_MULTIPLIER, mode = FUSION_CAPTURE_MODE_CONVENIENT);
    PARAMETRIZE(expectedOdds = 255, mode = FUSION_CAPTURE_MODE_MATERIAL);

    GIVEN {
        Fusion_SetCaptureMode(mode);
        WITH_CONFIG(B_MISSING_BADGE_CATCH_MALUS, GEN_7);
        WITH_CONFIG(B_LOW_LEVEL_CATCH_BONUS, GEN_7);
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_CLEFFA) { Level(30); }
    } WHEN {
        TURN { USE_ITEM(player, ITEM_POKE_BALL); }
    } SCENE {
        CATCHING_CHANCE(&recordedOdds);
    } THEN {
        EXPECT_EQ(expectedOdds, recordedOdds);
    }
}

WILD_BATTLE_TEST("Fusion capture mode: Material catches legendary Pokémon too", u32 odds)
{
    u32 mode;

    PARAMETRIZE(mode = FUSION_CAPTURE_MODE_CLASSIC);
    PARAMETRIZE(mode = FUSION_CAPTURE_MODE_MATERIAL);

    GIVEN {
        Fusion_SetCaptureMode(mode);
        WITH_CONFIG(B_MISSING_BADGE_CATCH_MALUS, GEN_7);
        WITH_CONFIG(B_LOW_LEVEL_CATCH_BONUS, GEN_7);
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_MEWTWO) { Level(30); }
    } WHEN {
        TURN { USE_ITEM(player, ITEM_POKE_BALL); }
    } SCENE {
        CATCHING_CHANCE(&results[i].odds);
    } FINALLY {
        EXPECT_LT(results[0].odds, 255);
        EXPECT_GE(results[1].odds, 255);
    }
}

TEST("Fusion capture mode: out-of-range values fall back to classic")
{
    VarSet(FUSION_VAR_CAPTURE_MODE, FUSION_CAPTURE_MODE_COUNT);
    EXPECT_EQ(Fusion_GetCaptureMode(), FUSION_CAPTURE_MODE_CLASSIC);
}

TEST("Fusion new game init: gives Box Link, DexNav and the default capture mode")
{
    Fusion_NewGameInit();
    EXPECT(CheckBagHasItem(ITEM_POKEMON_BOX_LINK, 1) == FUSION_NEW_GAME_GIVE_BOX_LINK);
    EXPECT(CheckBagHasItem(ITEM_FUSION_TERMINAL, 1) == FUSION_NEW_GAME_GIVE_TERMINAL);
    EXPECT(FlagGet(DN_FLAG_DEXNAV_GET) == FUSION_NEW_GAME_GIVE_DEXNAV);
    EXPECT_EQ(Fusion_GetCaptureMode(), FUSION_CAPTURE_MODE_NEW_GAME);
}

TEST("Fusion wild genetics: some wild Pokemon carry valid passives or a third type")
{
    struct Pokemon mon;
    u32 withPassive = 0, withType = 0;

    for (u32 i = 0; i < 1000; i++)
    {
        struct FusionGenetics g;

        CreateMon(&mon, SPECIES_ZIGZAGOON, 5, 0, OTID_STRUCT_PLAYER_ID);
        Fusion_RollWildGenetics(&mon);
        Fusion_GetMonGenetics(&mon, &g);
        if (g.passives[0] != ABILITY_NONE)
        {
            withPassive++;
            EXPECT(Fusion_CanBePassive(g.passives[0]));
            EXPECT_NE(g.passives[0], GetMonAbility(&mon));
            EXPECT_NE(g.passives[0], g.passives[1]);
        }
        if (g.extraType != TYPE_NONE)
        {
            withType++;
            EXPECT_NE(g.extraType, TYPE_NORMAL); // Zigzagoon is Normal.
            EXPECT_NE(g.extraType, TYPE_MYSTERY);
        }
    }
    EXPECT_GT(withPassive, 1000 * FUSION_WILD_PASSIVE_CHANCE / 200);
    EXPECT_LT(withPassive, 1000 * FUSION_WILD_PASSIVE_CHANCE * 2 / 100);
    EXPECT_LT(withType, 1000 * FUSION_WILD_THIRD_TYPE_CHANCE * 3 / 100);
}

WILD_BATTLE_TEST("Fusion capture: a one-line appraisal follows Gotcha!")
{
    GIVEN {
        Fusion_SetCaptureMode(FUSION_CAPTURE_MODE_MATERIAL);
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_ZIGZAGOON) { HPIV(31); AttackIV(31); DefenseIV(31); SpAttackIV(31); SpDefenseIV(31); SpeedIV(31); Passives(ABILITY_LEVITATE); }
    } WHEN {
        TURN { USE_ITEM(player, ITEM_POKE_BALL); }
    } SCENE {
        MESSAGE("IVs 186/186\nPassive: Levitate");
    }
}
