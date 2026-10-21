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

TEST("Fusion wild: replacements have a similar base stat total and the same stage")
{
    static const u16 sOriginals[] = {SPECIES_ZIGZAGOON, SPECIES_LINOONE, SPECIES_WINGULL, SPECIES_GARDEVOIR, SPECIES_SHEDINJA};
    u32 close = 0, total = 0;

    for (u32 o = 0; o < ARRAY_COUNT(sOriginals); o++)
    {
        enum Species original = sOriginals[o];
        u32 bst = GetSpeciesBaseStatTotal(original);
        bool32 evolved = IsEvolvedSpecies(original);

        for (u32 key = 0; key < 40; key++)
        {
            enum Species species = Fusion_RandomizeWildSpeciesWithSeed(original, key, 42);
            u32 newBst = GetSpeciesBaseStatTotal(species);

            EXPECT(Fusion_CanAppearAsRandomWild(species));
            EXPECT_EQ(IsEvolvedSpecies(species), evolved);
            total++;
            close += (newBst * 100 <= bst * (100 + FUSION_WILD_BST_WINDOW + 2) && newBst * 100 + bst * (FUSION_WILD_BST_WINDOW + 2) >= bst * 100);
        }
    }
    EXPECT_GT(close, total * 9 / 10);
}

TEST("Fusion wild: legendaries, Megas and cosmetic forms never appear")
{
    EXPECT(!Fusion_CanAppearAsRandomWild(SPECIES_MEWTWO));
    EXPECT(!Fusion_CanAppearAsRandomWild(SPECIES_RAYQUAZA));
    EXPECT(!Fusion_CanAppearAsRandomWild(SPECIES_GARDEVOIR_MEGA));
    EXPECT(!Fusion_CanAppearAsRandomWild(SPECIES_PIKACHU_COSPLAY));
    EXPECT(Fusion_CanAppearAsRandomWild(SPECIES_RATTATA_ALOLA));
    EXPECT(Fusion_CanAppearAsRandomWild(SPECIES_SPRIGATITO));

    for (u32 key = 0; key < KEYS; key++)
        EXPECT(!Fusion_IsCaptureAssistExempt(Fusion_RandomizeWildSpeciesWithSeed(SPECIES_SALAMENCE, key, 7)));
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
    EXPECT_GT(distinct, 300);
}
