#include "global.h"
#include "event_data.h"
#include "item.h"
#include "load_save.h"
#include "save.h"
#include "string_util.h"
#include "fusion/fusion.h"
#include "test/test.h"

static void CreateTestMon(struct Pokemon *mon, enum Species species, u32 level)
{
    CreateMon(mon, species, level, 0, OTID_STRUCT_PLAYER_ID);
}

TEST("Shard Shop: buying spends Shards and fails without enough")
{
    ClearBag();
    gSaveBlock3Ptr->fusionTower.shards = 50;
    gSpecialVar_0x8004 = 0; // Passive Crystal
    Fusion_ScriptShopBuy();
    EXPECT_EQ(gSpecialVar_Result, 0);
    EXPECT_EQ(gSaveBlock3Ptr->fusionTower.shards, 50 - Fusion_GetShardShopCost(0));
    EXPECT(CheckBagHasItem(ITEM_PASSIVE_CRYSTAL, 1));
    Fusion_ScriptShopBuy();
    EXPECT_EQ(gSpecialVar_Result, 1);
    EXPECT_EQ(gSaveBlock3Ptr->fusionTower.shards, 50 - Fusion_GetShardShopCost(0));
    gSaveBlock3Ptr->fusionTower.shards = 0;
}

TEST("Passive Crystal adds a passive, or rerolls the last one when full")
{
    struct Pokemon mon;
    struct FusionGenetics genetics = {0};
    u8 text[128];

    CreateTestMon(&mon, SPECIES_WOBBUFFET, 20);
    EXPECT(Fusion_UseGeneItem(&mon, ITEM_PASSIVE_CRYSTAL, text));
    Fusion_GetMonGenetics(&mon, &genetics);
    EXPECT_EQ(Fusion_CountPassives(&genetics), 1);

    genetics.passives[0] = ABILITY_LEVITATE;
    genetics.passives[1] = ABILITY_INTIMIDATE;
    genetics.passives[2] = ABILITY_DROUGHT;
    Fusion_SetMonGenetics(&mon, &genetics);
    EXPECT(Fusion_UseGeneItem(&mon, ITEM_PASSIVE_CRYSTAL, text));
    Fusion_GetMonGenetics(&mon, &genetics);
    EXPECT_EQ(genetics.passives[0], ABILITY_LEVITATE);
    EXPECT_EQ(genetics.passives[1], ABILITY_INTIMIDATE);
    EXPECT_NE(genetics.passives[2], ABILITY_DROUGHT);
    EXPECT_NE(genetics.passives[2], ABILITY_NONE);
}

TEST("Type Prism gives a third type the species doesn't have")
{
    struct Pokemon mon;
    struct FusionGenetics genetics;
    u8 text[128];

    CreateTestMon(&mon, SPECIES_WOBBUFFET, 20);
    EXPECT(Fusion_UseGeneItem(&mon, ITEM_TYPE_PRISM, text));
    Fusion_GetMonGenetics(&mon, &genetics);
    EXPECT_NE(genetics.extraType, TYPE_NONE);
    EXPECT_NE(genetics.extraType, TYPE_PSYCHIC);
}

TEST("Reincarnation carries the saved party into a new game at Lv. 5")
{
    struct Pokemon mon;
    struct FusionGenetics genetics = {0}, result;
    u32 iv = 31, ev = 100;
    void *stash;
    u16 savedStatus = gSaveFileStatus;

    CreateTestMon(&mon, SPECIES_GARCHOMP, 60);
    SetMonData(&mon, MON_DATA_ATK_IV, &iv);
    SetMonData(&mon, MON_DATA_ATK_EV, &ev);
    genetics.passives[0] = ABILITY_LEVITATE;
    genetics.extraType = TYPE_FIRE;
    Fusion_SetMonGenetics(&mon, &genetics);
    ZeroPlayerPartyMons();
    gParties[B_TRAINER_PLAYER][0] = mon;
    CalculatePlayerPartyCount();
    SavePlayerParty();

    memset(&gSaveBlock3Ptr->fusionTower, 0, sizeof(gSaveBlock3Ptr->fusionTower));
    gSaveFileStatus = SAVE_STATUS_OK;
    EXPECT_EQ(Fusion_StashReincarnation(), NULL); // Not requested.

    gSaveBlock3Ptr->fusionTower.reincarnationPending = TRUE;
    gSaveBlock3Ptr->fusionTower.shards = 77;
    gSaveBlock3Ptr->fusionTower.bestFloor = 12;
    stash = Fusion_StashReincarnation();
    EXPECT_NE(stash, NULL);

    // What NewGameInitData does in between.
    ZeroPlayerPartyMons();
    memset(&gSaveBlock3Ptr->fusionTower, 0, sizeof(gSaveBlock3Ptr->fusionTower));
    gSaveBlock2Ptr->playerTrainerId[0] ^= 0x5A;
    Fusion_RestoreReincarnation(stash);

    EXPECT_EQ(gPartiesCount[B_TRAINER_PLAYER], 1);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_GARCHOMP);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_LEVEL), FUSION_REINCARNATION_LEVEL);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_ATK_IV), 31);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_ATK_EV), 0);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_OT_ID), READ_OTID_FROM_SAVE);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MET_LEVEL), FUSION_REINCARNATION_LEVEL);
    Fusion_GetMonGenetics(&gParties[B_TRAINER_PLAYER][0], &result);
    EXPECT_EQ(result.passives[0], ABILITY_LEVITATE);
    EXPECT_EQ(result.extraType, TYPE_FIRE);
    EXPECT_EQ(gSaveBlock3Ptr->fusionTower.shards, 77);
    EXPECT_EQ(gSaveBlock3Ptr->fusionTower.bestFloor, 12);
    EXPECT_EQ(gSaveBlock3Ptr->fusionTower.ngPlusCount, 1);
    EXPECT(!gSaveBlock3Ptr->fusionTower.reincarnationPending);

    memset(&gSaveBlock3Ptr->fusionTower, 0, sizeof(gSaveBlock3Ptr->fusionTower));
    gSaveFileStatus = savedStatus;
}

TEST("New Game+ trainers are stronger and have passives")
{
    struct Pokemon party[PARTY_SIZE] = {0};
    struct FusionGenetics genetics;

    CreateTestMon(&party[0], SPECIES_ZIGZAGOON, 20);
    Fusion_ApplyNewGamePlusToTrainerParty(party);
    EXPECT_EQ(GetMonData(&party[0], MON_DATA_LEVEL), 20); // No effect outside NG+.

    gSaveBlock3Ptr->fusionTower.ngPlusCount = 2;
    Fusion_ApplyNewGamePlusToTrainerParty(party);
    EXPECT_EQ(GetMonData(&party[0], MON_DATA_LEVEL), 20 + 20 * 2 * FUSION_NG_PLUS_LEVEL_PERCENT / 100);
    Fusion_GetMonGenetics(&party[0], &genetics);
    EXPECT_EQ(Fusion_CountPassives(&genetics), 2);
    EXPECT_EQ(GetMonData(&party[0], MON_DATA_HP), GetMonData(&party[0], MON_DATA_MAX_HP));
    gSaveBlock3Ptr->fusionTower.ngPlusCount = 0;
}


TEST("Random passives are always real, named abilities")
{
    for (enum Ability ability = 1; ability < ABILITIES_COUNT; ability++)
    {
        if (Fusion_CanBePassive(ability))
            EXPECT_NE(StringCompare(gAbilitiesInfo[ability].name, gAbilitiesInfo[ABILITY_NONE].name), 0);
    }
}
