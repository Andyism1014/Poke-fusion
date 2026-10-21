#include "global.h"
#include "data.h"
#include "event_data.h"
#include "constants/characters.h"
#include "fusion/fusion.h"
#include "test/test.h"

static void GivePlayerMon(u32 level)
{
    ZeroPlayerPartyMons();
    CreateMon(&gParties[B_TRAINER_PLAYER][0], SPECIES_WOBBUFFET, level, 0, OTID_STRUCT_PLAYER_ID);
    CalculatePlayerPartyCount();
}

static u32 CountEnemyPassives(void)
{
    u32 total = 0;

    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        struct FusionGenetics g;
        if (!GetMonData(&gParties[B_TRAINER_OPPONENT_A][i], MON_DATA_SANITY_HAS_SPECIES))
            continue;
        Fusion_GetMonGenetics(&gParties[B_TRAINER_OPPONENT_A][i], &g);
        total += Fusion_CountPassives(&g);
    }
    return total;
}

TEST("Fusion Tower: team size and level grow with the floor")
{
    GivePlayerMon(50);
    EXPECT_EQ(Fusion_GetTowerPartySize(1), 3);
    EXPECT_EQ(Fusion_GetTowerPartySize(9), 4);
    EXPECT_EQ(Fusion_GetTowerPartySize(10), PARTY_SIZE); // Boss.
    EXPECT_LT(Fusion_GetTowerOpponentLevel(1, 50), Fusion_GetTowerOpponentLevel(30, 50));
    EXPECT_LE(Fusion_GetTowerOpponentLevel(999, 100), MAX_LEVEL);
}

TEST("Fusion Tower: floors are deterministic for a run seed")
{
    enum Species first;

    GivePlayerMon(50);
    Fusion_BuildTowerFloor(7, 1234);
    first = GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES);
    Fusion_BuildTowerFloor(7, 1234);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES), first);
    EXPECT_EQ(gPartiesCount[B_TRAINER_OPPONENT_A], Fusion_GetTowerPartySize(7));
}

TEST("Fusion Tower: later floors have more passives and a trainer to battle")
{
    u32 early, late;

    GivePlayerMon(60);
    Fusion_BuildTowerFloor(2, 99);
    early = CountEnemyPassives();
    Fusion_BuildTowerFloor(25, 99);
    late = CountEnemyPassives();
    EXPECT_EQ(early, 0);
    EXPECT_GT(late, early);

    gFusionTowerBattle = TRUE;
    EXPECT_EQ(GetTrainerStructFromId(1), Fusion_GetTowerTrainer());
    EXPECT_NE(GetTrainerStructFromId(1)->trainerName[0], EOS);
    gFusionTowerBattle = FALSE;
}

TEST("Fusion Tower: opponents never Dynamax or Terastallize")
{
    GivePlayerMon(50);
    Fusion_BuildTowerFloor(12, 5);
    for (u32 i = 0; i < Fusion_GetTowerPartySize(12); i++)
    {
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_OPPONENT_A][i], MON_DATA_DYNAMAX_LEVEL), BLOCK_AI_DYNAMAX);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_OPPONENT_A][i], MON_DATA_TERA_TYPE), TYPE_MYSTERY);
    }
}

TEST("Fusion Tower: high-level opponents are fully evolved")
{
    GivePlayerMon(80);
    Fusion_BuildTowerFloor(1, 77);
    for (u32 i = 0; i < Fusion_GetTowerPartySize(1); i++)
    {
        const struct Evolution *evolutions = GetSpeciesEvolutions(GetMonData(&gParties[B_TRAINER_OPPONENT_A][i], MON_DATA_SPECIES));
        EXPECT(evolutions == NULL || evolutions[0].method == EVOLUTIONS_END);
    }
}
