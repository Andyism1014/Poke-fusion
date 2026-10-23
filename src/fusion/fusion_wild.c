#include "global.h"
#include "event_data.h"
#include "pokemon.h"
#include "random.h"
#include "wild_encounter.h"
#include "fusion/fusion.h"
#include "constants/flags.h"

// All-generation wild encounters. Every wild slot (each entry of every encounter table)
// is mapped to a species drawn evenly from every Pokémon in the game, legendaries
// included. The slot keeps its original level, so early areas stay low level. The
// mapping only depends on the save's seed and the slot, so each area is fixed for the
// whole save and a new game rolls a new world. The starters come from the same pool.

#define STARTER_KEY 0x57A27E00

// Built on first use.
static EWRAM_DATA u16 sWildPool[NUM_SPECIES] = {0};
static EWRAM_DATA u16 sWildPoolCount = 0;
static EWRAM_DATA u8 sWildPoolBits[(NUM_SPECIES + 7) / 8] = {0};

static bool32 IsRandomWildCandidate(enum Species species)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];

    if (!IsSpeciesEnabled(species) || info->catchRate == 0)
        return FALSE;
    if (info->isMegaEvolution || info->isPrimalReversion || info->isGigantamax || info->isUltraBurst
     || info->isTotem || info->isTeraForm)
        return FALSE;
    // Alternate forms only appear when they're a regional variant.
    if (GET_BASE_SPECIES_ID(species) != species
     && !(info->isAlolanForm || info->isGalarianForm || info->isHisuianForm || info->isPaldeanForm))
        return FALSE;
    return TRUE;
}

static void BuildWildPool(void)
{
    sWildPoolCount = 0;
    for (enum Species species = 1; species < NUM_SPECIES; species++)
    {
        if (IsRandomWildCandidate(species))
        {
            sWildPool[sWildPoolCount++] = species;
            sWildPoolBits[species / 8] |= 1 << (species % 8);
        }
    }
}

static u32 HashSlot(u32 seed, u32 key)
{
    u32 h = seed ^ (key * 0x9E3779B1);
    h ^= h >> 16;
    h *= 0x85EBCA6B;
    h ^= h >> 13;
    h *= 0xC2B2AE35;
    h ^= h >> 16;
    return h;
}

static enum Species PickFromPool(u32 hash)
{
    if (sWildPoolCount == 0)
        BuildWildPool();
    return sWildPool[hash % sWildPoolCount];
}

bool32 Fusion_IsWildRandomizerEnabled(void)
{
    return FUSION_WILD_RANDOMIZER && !FlagGet(FLAG_FUSION_WILD_ORIGINAL);
}

// Pure mapping, independent of the save's settings.
enum Species Fusion_RandomizeWildSpeciesWithSeed(enum Species original, u32 key, u32 seed)
{
    if (original == SPECIES_NONE || original >= NUM_SPECIES)
        return original;
    return PickFromPool(HashSlot(seed, key));
}

enum Species Fusion_RandomizeWildSpecies(enum Species original, u32 key)
{
    if (!Fusion_IsWildRandomizerEnabled())
        return original;
    return Fusion_RandomizeWildSpeciesWithSeed(original, key, gSaveBlock3Ptr->fusionWildSeed);
}

bool32 Fusion_CanAppearAsRandomWild(enum Species species)
{
    if (sWildPoolCount == 0)
        BuildWildPool();
    return species < NUM_SPECIES && (sWildPoolBits[species / 8] >> (species % 8)) & 1;
}

// Three different species from the whole pool, fixed for a seed.
enum Species Fusion_GetRandomStarterWithSeed(u32 index, u32 seed)
{
    enum Species picks[FUSION_STARTER_COUNT];
    u32 key = STARTER_KEY;

    for (u32 i = 0; i <= index && i < FUSION_STARTER_COUNT; i++)
    {
        bool32 duplicate;

        do
        {
            picks[i] = PickFromPool(HashSlot(seed, key++));
            duplicate = FALSE;
            for (u32 j = 0; j < i; j++)
                duplicate |= (picks[j] == picks[i]);
        } while (duplicate);
    }
    return picks[min(index, FUSION_STARTER_COUNT - 1)];
}

enum Species Fusion_GetRandomStarter(u32 index)
{
    return Fusion_GetRandomStarterWithSeed(index, gSaveBlock3Ptr->fusionWildSeed);
}

// The species of a wild encounter slot after randomization. The slot's address identifies
// it, so the same table entry always gives the same species for a given save.
enum Species Fusion_GetWildSlotSpecies(const struct WildPokemon *slot)
{
    return Fusion_RandomizeWildSpecies(slot->species, (u32)slot);
}

void Fusion_RerollWildSeed(void)
{
    gSaveBlock3Ptr->fusionWildSeed = Random32();
}
