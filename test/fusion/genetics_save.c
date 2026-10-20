#include "global.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "fusion/fusion.h"
#include "test/test.h"

static const struct FusionGenetics sSampleGenetics =
{
    .mainAbility = ABILITY_LEVITATE,
    .passives = {ABILITY_INTIMIDATE, ABILITY_SPEED_BOOST, ABILITY_REGENERATOR},
    .extraType = TYPE_GHOST,
    .generation = 3,
    .flags = FUSION_FLAG_FUSED,
};

static void ExpectGeneticsEqual(const struct FusionGenetics *a, const struct FusionGenetics *b)
{
    EXPECT_EQ(a->mainAbility, b->mainAbility);
    for (u32 i = 0; i < FUSION_MAX_PASSIVES; i++)
        EXPECT_EQ(a->passives[i], b->passives[i]);
    EXPECT_EQ(a->extraType, b->extraType);
    EXPECT_EQ(a->generation, b->generation);
    EXPECT_EQ(a->flags, b->flags);
}

TEST("Fusion genetics: pack and unpack round-trip")
{
    u8 packed[FUSION_GENETICS_SIZE];
    struct FusionGenetics out;

    Fusion_PackGenetics(&sSampleGenetics, packed);
    Fusion_UnpackGenetics(packed, &out);
    ExpectGeneticsEqual(&sSampleGenetics, &out);
}

TEST("Fusion genetics: maximum values round-trip")
{
    u8 packed[FUSION_GENETICS_SIZE];
    struct FusionGenetics in =
    {
        .mainAbility = ABILITIES_COUNT - 1,
        .passives = {ABILITIES_COUNT - 1, ABILITIES_COUNT - 1, ABILITIES_COUNT - 1},
        .extraType = NUMBER_OF_MON_TYPES - 1,
        .generation = FUSION_MAX_GENERATION,
        .flags = FUSION_FLAG_FUSED | FUSION_FLAG_RARE,
    };
    struct FusionGenetics out;

    Fusion_PackGenetics(&in, packed);
    Fusion_UnpackGenetics(packed, &out);
    ExpectGeneticsEqual(&in, &out);
}

TEST("Fusion genetics: all-zero bytes mean no fusion data")
{
    u8 packed[FUSION_GENETICS_SIZE] = {0};
    struct FusionGenetics out;

    Fusion_UnpackGenetics(packed, &out);
    EXPECT_EQ(out.mainAbility, ABILITY_NONE);
    EXPECT_EQ(Fusion_CountPassives(&out), 0);
    EXPECT_EQ(out.extraType, TYPE_NONE);
    EXPECT_EQ(out.generation, 0);
}

TEST("Fusion genetics: stored in an encrypted mon without corrupting it")
{
    struct Pokemon mon;
    struct FusionGenetics out;

    CreateMon(&mon, SPECIES_GASTLY, 18, 0x12345678, OTID_STRUCT_PLAYER_ID);
    SetMonData(&mon, MON_DATA_HP_IV, &(u32){31});
    Fusion_SetMonGenetics(&mon, &sSampleGenetics);
    Fusion_GetMonGenetics(&mon, &out);

    ExpectGeneticsEqual(&sSampleGenetics, &out);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_SANITY_IS_BAD_EGG), FALSE);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_SPECIES), SPECIES_GASTLY);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_HP_IV), 31);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_LEVEL), 18);
}

TEST("Fusion genetics: contest conditions no longer overwrite genetics")
{
    struct Pokemon mon;
    struct FusionGenetics out;

    CreateMon(&mon, SPECIES_GASTLY, 18, 0, OTID_STRUCT_PLAYER_ID);
    Fusion_SetMonGenetics(&mon, &sSampleGenetics);
    SetMonData(&mon, MON_DATA_COOL, &(u8){255});
    SetMonData(&mon, MON_DATA_SHEEN, &(u8){255});
    Fusion_GetMonGenetics(&mon, &out);

    ExpectGeneticsEqual(&sSampleGenetics, &out);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_COOL), 0);
}

TEST("Fusion genetics: survive box storage, evolution and party copies")
{
    struct Pokemon mon, out;
    struct FusionGenetics genetics;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_GASTLY, 25, 0, OTID_STRUCT_PLAYER_ID);
    Fusion_SetMonGenetics(&mon, &sSampleGenetics);

    // Into box 7 and back out.
    SetBoxMonAt(6, 12, &mon.box);
    BoxMonAtToMon(6, 12, &out);
    Fusion_GetMonGenetics(&out, &genetics);
    ExpectGeneticsEqual(&sSampleGenetics, &genetics);

    // Evolution only rewrites the species.
    SetMonData(&out, MON_DATA_SPECIES, &(u16){SPECIES_HAUNTER});
    CalculateMonStats(&out);
    Fusion_GetMonGenetics(&out, &genetics);
    ExpectGeneticsEqual(&sSampleGenetics, &genetics);
}

TEST("Fusion genetics: displayed third type skips duplicates of the species types")
{
    struct Pokemon mon;
    struct FusionGenetics genetics = {0};

    CreateMon(&mon, SPECIES_GASTLY, 18, 0, OTID_STRUCT_PLAYER_ID); // Ghost/Poison
    EXPECT_EQ(Fusion_GetBoxMonThirdType(&mon.box), TYPE_NONE);

    genetics.extraType = TYPE_FIRE;
    Fusion_SetMonGenetics(&mon, &genetics);
    EXPECT_EQ(Fusion_GetBoxMonThirdType(&mon.box), TYPE_FIRE);

    genetics.extraType = TYPE_POISON;
    Fusion_SetMonGenetics(&mon, &genetics);
    EXPECT_EQ(Fusion_GetBoxMonThirdType(&mon.box), TYPE_NONE);
}
