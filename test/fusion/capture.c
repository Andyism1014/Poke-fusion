#include "global.h"
#include "event_data.h"
#include "item.h"
#include "fusion/fusion.h"
#include "test/battle.h"

ASSUMPTIONS
{
    ASSUME(gSpeciesInfo[SPECIES_CLEFFA].catchRate == 150);
    ASSUME(!Fusion_IsCaptureAssistExempt(SPECIES_CLEFFA));
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

WILD_BATTLE_TEST("Fusion capture mode: legendary Pokémon keep vanilla catch odds", u32 odds)
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
        EXPECT_EQ(results[0].odds, results[1].odds);
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
    EXPECT(FlagGet(DN_FLAG_DEXNAV_GET) == FUSION_NEW_GAME_GIVE_DEXNAV);
    EXPECT_EQ(Fusion_GetCaptureMode(), FUSION_CAPTURE_MODE_NEW_GAME);
}
