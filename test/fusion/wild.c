#include "global.h"
#include "wild_encounter.h"
#include "fusion/fusion.h"
#include "test/test.h"
#include "constants/maps.h"

// All-generation wild encounters (fusion_wild.c). The randomizer is off in tests,
// so these call the pure mapping with explicit seeds.

#define KEYS 200

static bool32 IsEvolvedSpecies(enum Species species)
{
    for (enum Species s = 1; s < NUM_SPECIES; s++)
    {
        const struct Evolution *evolutions;
        if (!IsSpeciesEnabled(s))
            continue;
        evolutions = GetSpeciesEvolutions(s);
        if (evolutions == NULL)
            continue;
        for (u32 i = 0; evolutions[i].method != EVOLUTIONS_END; i++)
        {
            if (evolutions[i].targetSpecies == species)
                return TRUE;
        }
    }
    return FALSE;
}

TEST("Fusion wild: the mapping is fixed for a seed and changes with the seed")
{
    u32 same = 0;

    for (u32 key = 0; key < KEYS; key++)
    {
        enum Species a = Fusion_RandomizeWildSpeciesWithSeed(SPECIES_ZIGZAGOON, key, 1234);
        EXPECT_EQ(a, Fusion_RandomizeWildSpeciesWithSeed(SPECIES_ZIGZAGOON, key, 1234));
        same += (a == Fusion_RandomizeWildSpeciesWithSeed(SPECIES_ZIGZAGOON, key, 98765));
    }
    EXPECT_LT(same, KEYS / 4);
}

TEST("Fusion wild: any species can replace any slot, legendaries included")
{
    u32 legendary = 0, evolved = 0, basic = 0;

    for (u32 key = 0; key < KEYS * 4; key++)
    {
        enum Species species = Fusion_RandomizeWildSpeciesWithSeed(SPECIES_ZIGZAGOON, key, 42);
        const struct SpeciesInfo *info = &gSpeciesInfo[species];

        EXPECT(Fusion_CanAppearAsRandomWild(species));
        legendary += (info->isRestrictedLegendary || info->isSubLegendary || info->isMythical || info->isUltraBeast);
        if (IsEvolvedSpecies(species))
            evolved++;
        else
            basic++;
    }
    EXPECT_GT(legendary, 0);
    EXPECT_GT(evolved, 0);
    EXPECT_GT(basic, 0);
}

TEST("Fusion wild: legendaries can appear, Megas and cosmetic forms never do")
{
    EXPECT(Fusion_CanAppearAsRandomWild(SPECIES_MEWTWO));
    EXPECT(Fusion_CanAppearAsRandomWild(SPECIES_RAYQUAZA));
    EXPECT(Fusion_CanAppearAsRandomWild(SPECIES_MEW));
    EXPECT(!Fusion_CanAppearAsRandomWild(SPECIES_GARDEVOIR_MEGA));
    EXPECT(!Fusion_CanAppearAsRandomWild(SPECIES_PIKACHU_COSPLAY));
    EXPECT(Fusion_CanAppearAsRandomWild(SPECIES_RATTATA_ALOLA));
    EXPECT(Fusion_CanAppearAsRandomWild(SPECIES_SPRIGATITO));
}

TEST("Fusion wild: the three starters are different random species, fixed for a seed")
{
    for (u32 seed = 0; seed < 50; seed++)
    {
        enum Species a = Fusion_GetRandomStarterWithSeed(0, seed);
        enum Species b = Fusion_GetRandomStarterWithSeed(1, seed);
        enum Species c = Fusion_GetRandomStarterWithSeed(2, seed);

        EXPECT(Fusion_CanAppearAsRandomWild(a));
        EXPECT(Fusion_CanAppearAsRandomWild(b));
        EXPECT(Fusion_CanAppearAsRandomWild(c));
        EXPECT_NE(a, b);
        EXPECT_NE(a, c);
        EXPECT_NE(b, c);
        EXPECT_EQ(a, Fusion_GetRandomStarterWithSeed(0, seed));
    }
}

TEST("Fusion wild: Hoenn's encounter tables reach a large part of the Pokedex")
{
    static u8 seen[(NUM_SPECIES + 7) / 8];
    u32 distinct = 0;

    memset(seen, 0, sizeof(seen));
    for (u32 h = 0; gWildMonHeaders[h].mapGroup != MAP_GROUP(MAP_UNDEFINED); h++)
    {
        for (u32 t = 0; t < TIMES_OF_DAY_COUNT; t++)
        {
            const struct WildEncounterTypes *types = &gWildMonHeaders[h].encounterTypes[t];
            const struct WildPokemonInfo *infos[] = {types->landMonsInfo, types->waterMonsInfo, types->rockSmashMonsInfo, types->fishingMonsInfo};
            const u32 counts[] = {NUM_LAND_MONS_ENCOUNTER_SLOTS, NUM_WATER_MONS_ENCOUNTER_SLOTS, NUM_ROCK_SMASH_MONS_ENCOUNTER_SLOTS, NUM_FISHING_MONS_ENCOUNTER_SLOTS};

            for (u32 k = 0; k < ARRAY_COUNT(infos); k++)
            {
                if (infos[k] == NULL)
                    continue;
                for (u32 i = 0; i < counts[k]; i++)
                {
                    const struct WildPokemon *slot = &infos[k]->wildPokemon[i];
                    enum Species species = Fusion_RandomizeWildSpeciesWithSeed(slot->species, (u32)slot, 2024);

                    if (!(seen[species / 8] & (1 << (species % 8))))
                    {
                        seen[species / 8] |= 1 << (species % 8);
                        distinct++;
                    }
                }
            }
        }
    }
    EXPECT_GT(distinct, 500);
}
