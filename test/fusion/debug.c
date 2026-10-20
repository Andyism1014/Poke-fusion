#include "global.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "fusion/fusion.h"
#include "test/test.h"

static bool32 SpeciesHasType(enum Species species, enum Type type)
{
    return gSpeciesInfo[species].types[0] == type || gSpeciesInfo[species].types[1] == type;
}

static bool32 SpeciesHasAbility(enum Species species, enum Ability ability)
{
    for (u32 i = 0; i < NUM_ABILITY_SLOTS; i++)
    {
        if (gSpeciesInfo[species].abilities[i] == ability)
            return TRUE;
    }
    return FALSE;
}

static void ExpectValidRandomGenetics(const struct FusionGenetics *genetics, enum Species species)
{
    u32 count = Fusion_CountPassives(genetics);

    EXPECT_EQ(genetics->mainAbility, ABILITY_NONE);
    for (u32 i = 0; i < FUSION_MAX_PASSIVES; i++)
    {
        if (i >= count)
        {
            // Filled passives are contiguous.
            EXPECT_EQ(genetics->passives[i], ABILITY_NONE);
            continue;
        }
        EXPECT(Fusion_Debug_IsRandomPassiveCandidate(genetics->passives[i]));
        EXPECT(!SpeciesHasAbility(species, genetics->passives[i]));
        for (u32 j = 0; j < i; j++)
            EXPECT_NE(genetics->passives[i], genetics->passives[j]);
    }

    if (genetics->extraType != TYPE_NONE)
    {
        EXPECT_LT(genetics->extraType, NUMBER_OF_MON_TYPES);
        EXPECT_NE(genetics->extraType, TYPE_MYSTERY);
        EXPECT_NE(genetics->extraType, TYPE_STELLAR);
        EXPECT(!SpeciesHasType(species, genetics->extraType));
    }

    EXPECT_LE(genetics->generation, FUSION_MAX_GENERATION);
    EXPECT_EQ(!!(genetics->flags & FUSION_FLAG_FUSED), genetics->generation > 0);
}

TEST("Fusion debug: random genetics are valid and survive packing")
{
    enum Species species = SPECIES_NONE;

    PARAMETRIZE { species = SPECIES_GASTLY; }
    PARAMETRIZE { species = SPECIES_PIKACHU; }
    PARAMETRIZE { species = SPECIES_SHEDINJA; }

    for (u32 i = 0; i < 64; i++)
    {
        u8 packed[FUSION_GENETICS_SIZE];
        struct FusionGenetics genetics, out;

        Fusion_Debug_RandomizeGenetics(&genetics, species);
        ExpectValidRandomGenetics(&genetics, species);

        Fusion_PackGenetics(&genetics, packed);
        Fusion_UnpackGenetics(packed, &out);
        EXPECT_EQ(out.mainAbility, genetics.mainAbility);
        for (u32 j = 0; j < FUSION_MAX_PASSIVES; j++)
            EXPECT_EQ(out.passives[j], genetics.passives[j]);
        EXPECT_EQ(out.extraType, genetics.extraType);
        EXPECT_EQ(out.generation, genetics.generation);
        EXPECT_EQ(out.flags, genetics.flags);
    }
}

TEST("Fusion debug: random material species are enabled base forms")
{
    for (u32 i = 0; i < 64; i++)
    {
        enum Species species = Fusion_Debug_GetRandomMaterialSpecies();

        EXPECT_NE(species, SPECIES_NONE);
        EXPECT_NE(species, SPECIES_EGG);
        EXPECT(IsSpeciesEnabled(species));
        EXPECT_EQ(GET_BASE_SPECIES_ID(species), species);
    }
}

TEST("Fusion debug: filling a box only uses its empty slots")
{
    struct Pokemon mon;
    struct FusionGenetics genetics;
    const struct FusionGenetics kept = {.passives = {ABILITY_INTIMIDATE}, .generation = 2, .flags = FUSION_FLAG_FUSED};

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_GASTLY, 18, 0, OTID_STRUCT_PLAYER_ID);
    Fusion_SetMonGenetics(&mon, &kept);
    SetBoxMonAt(2, 5, &mon.box);

    EXPECT_EQ(Fusion_Debug_FillBoxWithMaterials(2), IN_BOX_COUNT - 1);
    for (u32 i = 0; i < IN_BOX_COUNT; i++)
    {
        struct BoxPokemon *boxMon = GetBoxedMonPtr(2, i);
        enum Species species = GetBoxMonData(boxMon, MON_DATA_SPECIES);

        EXPECT(GetBoxMonData(boxMon, MON_DATA_SANITY_HAS_SPECIES));
        EXPECT(!GetBoxMonData(boxMon, MON_DATA_SANITY_IS_BAD_EGG));
        EXPECT(!GetBoxMonData(boxMon, MON_DATA_IS_EGG));
        Fusion_GetBoxMonGenetics(boxMon, &genetics);
        if (i == 5)
        {
            EXPECT_EQ(species, SPECIES_GASTLY);
            EXPECT_EQ(genetics.passives[0], ABILITY_INTIMIDATE);
            EXPECT_EQ(genetics.generation, 2);
        }
        else
        {
            ExpectValidRandomGenetics(&genetics, species);
        }
    }

    // A full box is left alone, other boxes are untouched.
    EXPECT_EQ(Fusion_Debug_FillBoxWithMaterials(2), 0);
    EXPECT(!GetBoxMonDataAt(1, 0, MON_DATA_SANITY_HAS_SPECIES));
    EXPECT(!GetBoxMonDataAt(3, 0, MON_DATA_SANITY_HAS_SPECIES));
    EXPECT_EQ(Fusion_Debug_FillBoxWithMaterials(TOTAL_BOXES_COUNT), 0);
}

TEST("Fusion debug: capture mode cycles through every mode")
{
    Fusion_SetCaptureMode(FUSION_CAPTURE_MODE_CLASSIC);
    EXPECT_EQ(Fusion_Debug_CycleCaptureMode(), FUSION_CAPTURE_MODE_CONVENIENT);
    EXPECT_EQ(Fusion_Debug_CycleCaptureMode(), FUSION_CAPTURE_MODE_MATERIAL);
    EXPECT_EQ(Fusion_Debug_CycleCaptureMode(), FUSION_CAPTURE_MODE_CLASSIC);
    EXPECT_EQ(Fusion_GetCaptureMode(), FUSION_CAPTURE_MODE_CLASSIC);
}
