#include "global.h"
#include "event_data.h"
#include "pokemon.h"
#include "random.h"
#include "wild_encounter.h"
#include "fusion/fusion.h"
#include "constants/flags.h"

// All-generation wild encounters. Every wild slot (each entry of every encounter table)
// is mapped to a random species from Gen 1-9 with a similar base stat total and the same
// evolution stage, so the difficulty curve of the original game is kept. The mapping only
// depends on the save's seed and the slot, so each area is fixed for the whole save and a
// new game rolls a new world.

#define BST_UNIT        4  // sWildBstUnits stores the base stat total divided by this.
#define BST_WINDOWS     3

static const u8 sBstWindowPercents[BST_WINDOWS] = {FUSION_WILD_BST_WINDOW, FUSION_WILD_BST_WINDOW * 2, 100};

// Built on first use: 0 = species can't appear in the wild.
static EWRAM_DATA u8 sWildBstUnits[NUM_SPECIES] = {0};
static EWRAM_DATA u8 sEvolvedSpecies[(NUM_SPECIES + 7) / 8] = {0};
static EWRAM_DATA bool8 sWildTablesReady = FALSE;

static bool32 IsEvolved(enum Species species)
{
    return (sEvolvedSpecies[species / 8] >> (species % 8)) & 1;
}

static bool32 IsRandomWildCandidate(enum Species species)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];

    if (!IsSpeciesEnabled(species) || info->catchRate == 0 || Fusion_IsCaptureAssistExempt(species))
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

static void BuildWildTables(void)
{
    for (enum Species species = 1; species < NUM_SPECIES; species++)
    {
        const struct Evolution *evolutions;

        if (!IsSpeciesEnabled(species))
            continue;
        evolutions = GetSpeciesEvolutions(species);
        if (IsRandomWildCandidate(species))
            sWildBstUnits[species] = max(1u, min(255u, GetSpeciesBaseStatTotal(species) / BST_UNIT));
        if (evolutions == NULL)
            continue;
        for (u32 i = 0; evolutions[i].method != EVOLUTIONS_END; i++)
        {
            enum Species target = evolutions[i].targetSpecies;
            if (target < NUM_SPECIES)
                sEvolvedSpecies[target / 8] |= 1 << (target % 8);
        }
    }
    sWildTablesReady = TRUE;
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

static bool32 IsSimilar(enum Species candidate, u32 bstUnits, u32 windowPercent, bool32 evolved)
{
    u32 units = sWildBstUnits[candidate];
    u32 window = bstUnits * windowPercent / 100;

    if (units == 0 || IsEvolved(candidate) != evolved)
        return FALSE;
    return units + window >= bstUnits && units <= bstUnits + window;
}

bool32 Fusion_IsWildRandomizerEnabled(void)
{
    return FUSION_WILD_RANDOMIZER && !FlagGet(FLAG_FUSION_WILD_ORIGINAL);
}

// Pure mapping, independent of the save's settings.
enum Species Fusion_RandomizeWildSpeciesWithSeed(enum Species original, u32 key, u32 seed)
{
    u32 hash, bstUnits;
    bool32 evolved;

    if (original == SPECIES_NONE || original >= NUM_SPECIES)
        return original;
    if (!sWildTablesReady)
        BuildWildTables();

    hash = HashSlot(seed, key);
    bstUnits = max(1u, GetSpeciesBaseStatTotal(original) / BST_UNIT);
    evolved = IsEvolved(original);

    for (u32 w = 0; w < BST_WINDOWS; w++)
    {
        u32 count = 0, pick;

        for (enum Species species = 1; species < NUM_SPECIES; species++)
            count += IsSimilar(species, bstUnits, sBstWindowPercents[w], evolved);
        if (count == 0)
            continue;

        pick = hash % count;
        for (enum Species species = 1; species < NUM_SPECIES; species++)
        {
            if (IsSimilar(species, bstUnits, sBstWindowPercents[w], evolved) && pick-- == 0)
                return species;
        }
    }
    return original;
}

enum Species Fusion_RandomizeWildSpecies(enum Species original, u32 key)
{
    if (!Fusion_IsWildRandomizerEnabled())
        return original;
    return Fusion_RandomizeWildSpeciesWithSeed(original, key, gSaveBlock3Ptr->fusionWildSeed);
}

bool32 Fusion_CanAppearAsRandomWild(enum Species species)
{
    if (!sWildTablesReady)
        BuildWildTables();
    return species < NUM_SPECIES && sWildBstUnits[species] != 0;
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
