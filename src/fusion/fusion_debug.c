#include "global.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "random.h"
#include "fusion/fusion.h"

// Helpers for testing fusion, used by the overworld debug menu (src/debug.c) and by tests.
// Nothing here is reachable in normal gameplay.

#define DEBUG_MATERIAL_MIN_LEVEL        5
#define DEBUG_MATERIAL_MAX_LEVEL        50
#define DEBUG_RANDOM_MAX_GENERATION     5  // Random genetics stay within the first few generations.
#define DEBUG_RANDOM_RARE_CHANCE        8  // 1 in N fused results are marked rare.
#define DEBUG_RANDOM_MAX_ATTEMPTS       64 // Rejection sampling cap, the candidate pools are large.

static const u8 *const sCaptureModeNames[FUSION_CAPTURE_MODE_COUNT] =
{
    [FUSION_CAPTURE_MODE_CLASSIC]    = COMPOUND_STRING("CLASSIC"),
    [FUSION_CAPTURE_MODE_CONVENIENT] = COMPOUND_STRING("CONVENIENT"),
    [FUSION_CAPTURE_MODE_MATERIAL]   = COMPOUND_STRING("MATERIAL"),
};

const u8 *Fusion_Debug_GetCaptureModeName(enum FusionCaptureMode mode)
{
    if (mode >= FUSION_CAPTURE_MODE_COUNT)
        mode = FUSION_CAPTURE_MODE_CLASSIC;
    return sCaptureModeNames[mode];
}

// CLASSIC -> CONVENIENT -> MATERIAL -> CLASSIC.
enum FusionCaptureMode Fusion_Debug_CycleCaptureMode(void)
{
    enum FusionCaptureMode mode = (Fusion_GetCaptureMode() + 1) % FUSION_CAPTURE_MODE_COUNT;

    Fusion_SetCaptureMode(mode);
    return Fusion_GetCaptureMode();
}

static bool32 SpeciesHasAbility(enum Species species, enum Ability ability)
{
    u32 i;

    for (i = 0; i < NUM_ABILITY_SLOTS; i++)
    {
        if (gSpeciesInfo[species].abilities[i] == ability)
            return TRUE;
    }
    return FALSE;
}

// Signature and form-changing abilities (Multitype, Stance Change, Wonder Guard...) can't be
// copied in battle either, so they are kept out of random passives until the phase 04 conflict
// table decides how they should behave.
bool32 Fusion_Debug_IsRandomPassiveCandidate(enum Ability ability)
{
    return ability != ABILITY_NONE
        && ability < ABILITIES_COUNT
        && !gAbilitiesInfo[ability].cantBeCopied;
}

static bool32 IsRandomExtraTypeCandidate(enum Type type, enum Species species)
{
    return type != TYPE_NONE
        && type < NUMBER_OF_MON_TYPES
        && type != TYPE_MYSTERY
        && type != TYPE_STELLAR
        && type != gSpeciesInfo[species].types[0]
        && type != gSpeciesInfo[species].types[1];
}

// Random but valid genetics for the given species: keeps the species ability, 0-3 distinct
// passives the species can't already have, an optional third type it doesn't already have
// and a low generation. FUSION_FLAG_FUSED is set exactly when generation > 0.
void Fusion_Debug_RandomizeGenetics(struct FusionGenetics *genetics, enum Species species)
{
    u32 i, j, attempts, passiveCount;

    memset(genetics, 0, sizeof(*genetics));
    genetics->mainAbility = ABILITY_NONE;

    passiveCount = Random() % (FUSION_MAX_PASSIVES + 1);
    for (i = 0; i < passiveCount; i++)
    {
        for (attempts = 0; attempts < DEBUG_RANDOM_MAX_ATTEMPTS; attempts++)
        {
            enum Ability ability = 1 + Random() % (ABILITIES_COUNT - 1);

            if (!Fusion_Debug_IsRandomPassiveCandidate(ability) || SpeciesHasAbility(species, ability))
                continue;
            for (j = 0; j < i; j++)
            {
                if (genetics->passives[j] == ability)
                    break;
            }
            if (j == i)
            {
                genetics->passives[i] = ability;
                break;
            }
        }
        if (genetics->passives[i] == ABILITY_NONE)
            break; // Keep the filled passives contiguous.
    }

    genetics->extraType = TYPE_NONE;
    if (Random() % 2)
    {
        for (attempts = 0; attempts < DEBUG_RANDOM_MAX_ATTEMPTS; attempts++)
        {
            enum Type type = 1 + Random() % (NUMBER_OF_MON_TYPES - 1);

            if (IsRandomExtraTypeCandidate(type, species))
            {
                genetics->extraType = type;
                break;
            }
        }
    }

    genetics->generation = Random() % (DEBUG_RANDOM_MAX_GENERATION + 1);
    if (genetics->generation > 0)
    {
        genetics->flags |= FUSION_FLAG_FUSED;
        if (Random() % DEBUG_RANDOM_RARE_CHANCE == 0)
            genetics->flags |= FUSION_FLAG_RARE;
    }
}

// Any enabled base form, so boxes don't fill up with Megas, Gigantamax forms or eggs.
enum Species Fusion_Debug_GetRandomMaterialSpecies(void)
{
    u32 attempts;

    for (attempts = 0; attempts < DEBUG_RANDOM_MAX_ATTEMPTS; attempts++)
    {
        enum Species species = 1 + Random() % (NUM_SPECIES - 1);

        if (species != SPECIES_EGG && IsSpeciesEnabled(species) && GET_BASE_SPECIES_ID(species) == species)
            return species;
    }
    return SPECIES_BULBASAUR;
}

// Fills every empty slot of the box with a random material: random species, level,
// personality and IVs, and random genetics. Returns how many Pokémon were added.
u32 Fusion_Debug_FillBoxWithMaterials(u32 boxId)
{
    u32 boxPosition, count = 0;
    struct BoxPokemon boxMon;
    struct FusionGenetics genetics;

    if (boxId >= TOTAL_BOXES_COUNT)
        return 0;

    for (boxPosition = 0; boxPosition < IN_BOX_COUNT; boxPosition++)
    {
        if (GetBoxMonDataAt(boxId, boxPosition, MON_DATA_SANITY_HAS_SPECIES))
            continue;

        enum Species species = Fusion_Debug_GetRandomMaterialSpecies();
        u32 level = DEBUG_MATERIAL_MIN_LEVEL + Random() % (DEBUG_MATERIAL_MAX_LEVEL - DEBUG_MATERIAL_MIN_LEVEL + 1);

        CreateBoxMon(&boxMon, species, level, Random32(), OTID_STRUCT_PLAYER_ID);
        SetBoxMonIVs(&boxMon, USE_RANDOM_IVS);
        GiveBoxMonInitialMoveset(&boxMon);
        Fusion_Debug_RandomizeGenetics(&genetics, species);
        Fusion_SetBoxMonGenetics(&boxMon, &genetics);
        SetBoxMonAt(boxId, boxPosition, &boxMon);
        count++;
    }
    return count;
}
