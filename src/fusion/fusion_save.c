#include "global.h"
#include "pokemon.h"
#include "fusion/fusion.h"

// Bit layout of the 48 bits in PokemonSubstruct2.fusionGenetics (little endian):
//  0- 8 mainAbility
//  9-17 passives[0]
// 18-26 passives[1]
// 27-35 passives[2]
// 36-40 extraType
// 41-45 generation
// 46-47 flags
#define ABILITY_BITS    9
#define TYPE_BITS       5
#define GENERATION_BITS 5
#define FLAG_BITS       2

#define SHIFT_MAIN_ABILITY  0
#define SHIFT_PASSIVES      (SHIFT_MAIN_ABILITY + ABILITY_BITS)
#define SHIFT_EXTRA_TYPE    (SHIFT_PASSIVES + ABILITY_BITS * FUSION_MAX_PASSIVES)
#define SHIFT_GENERATION    (SHIFT_EXTRA_TYPE + TYPE_BITS)
#define SHIFT_FLAGS         (SHIFT_GENERATION + GENERATION_BITS)

STATIC_ASSERT(SHIFT_FLAGS + FLAG_BITS == FUSION_GENETICS_SIZE * 8, FusionGeneticsFillsStorage);
STATIC_ASSERT(sizeof(((struct PokemonSubstruct2 *)0)->fusionGenetics) == FUSION_GENETICS_SIZE, FusionGeneticsStorageSize);
STATIC_ASSERT(ABILITIES_COUNT <= (1 << ABILITY_BITS), FusionAbilitiesFit);
STATIC_ASSERT(NUMBER_OF_MON_TYPES <= (1 << TYPE_BITS), FusionTypesFit);
STATIC_ASSERT(FUSION_MAX_GENERATION < (1 << GENERATION_BITS), FusionGenerationFits);

static inline u64 GetBits(u64 raw, u32 shift, u32 bits)
{
    return (raw >> shift) & ((1ull << bits) - 1);
}

static inline u64 PutBits(u32 value, u32 shift, u32 bits)
{
    return ((u64)value & ((1ull << bits) - 1)) << shift;
}

static enum Ability SanitizeAbility(u32 ability)
{
    return ability < ABILITIES_COUNT ? ability : ABILITY_NONE;
}

void Fusion_PackGenetics(const struct FusionGenetics *genetics, u8 *packed)
{
    u64 raw = 0;
    u32 i;

    raw |= PutBits(SanitizeAbility(genetics->mainAbility), SHIFT_MAIN_ABILITY, ABILITY_BITS);
    for (i = 0; i < FUSION_MAX_PASSIVES; i++)
        raw |= PutBits(SanitizeAbility(genetics->passives[i]), SHIFT_PASSIVES + ABILITY_BITS * i, ABILITY_BITS);
    raw |= PutBits(genetics->extraType < NUMBER_OF_MON_TYPES ? genetics->extraType : TYPE_NONE, SHIFT_EXTRA_TYPE, TYPE_BITS);
    raw |= PutBits(min(genetics->generation, FUSION_MAX_GENERATION), SHIFT_GENERATION, GENERATION_BITS);
    raw |= PutBits(genetics->flags, SHIFT_FLAGS, FLAG_BITS);

    for (i = 0; i < FUSION_GENETICS_SIZE; i++)
        packed[i] = raw >> (8 * i);
}

void Fusion_UnpackGenetics(const u8 *packed, struct FusionGenetics *genetics)
{
    u64 raw = 0;
    u32 i;

    for (i = 0; i < FUSION_GENETICS_SIZE; i++)
        raw |= (u64)packed[i] << (8 * i);

    genetics->mainAbility = SanitizeAbility(GetBits(raw, SHIFT_MAIN_ABILITY, ABILITY_BITS));
    for (i = 0; i < FUSION_MAX_PASSIVES; i++)
        genetics->passives[i] = SanitizeAbility(GetBits(raw, SHIFT_PASSIVES + ABILITY_BITS * i, ABILITY_BITS));
    genetics->extraType = GetBits(raw, SHIFT_EXTRA_TYPE, TYPE_BITS);
    if (genetics->extraType >= NUMBER_OF_MON_TYPES)
        genetics->extraType = TYPE_NONE;
    genetics->generation = GetBits(raw, SHIFT_GENERATION, GENERATION_BITS);
    genetics->flags = GetBits(raw, SHIFT_FLAGS, FLAG_BITS);
}

// Main ability only, without unpacking the rest.
enum Ability Fusion_UnpackMainAbility(const u8 *packed)
{
    u32 raw = packed[0] | (packed[1] << 8);
    return SanitizeAbility(GetBits(raw, SHIFT_MAIN_ABILITY, ABILITY_BITS));
}

void Fusion_GetBoxMonGenetics(struct BoxPokemon *boxMon, struct FusionGenetics *genetics)
{
    u8 packed[FUSION_GENETICS_SIZE];

    GetBoxMonData(boxMon, MON_DATA_FUSION_GENETICS, packed);
    Fusion_UnpackGenetics(packed, genetics);
}

void Fusion_SetBoxMonGenetics(struct BoxPokemon *boxMon, const struct FusionGenetics *genetics)
{
    u8 packed[FUSION_GENETICS_SIZE];

    Fusion_PackGenetics(genetics, packed);
    SetBoxMonData(boxMon, MON_DATA_FUSION_GENETICS, packed);
}

void Fusion_GetMonGenetics(struct Pokemon *mon, struct FusionGenetics *genetics)
{
    Fusion_GetBoxMonGenetics(&mon->box, genetics);
}

void Fusion_SetMonGenetics(struct Pokemon *mon, const struct FusionGenetics *genetics)
{
    Fusion_SetBoxMonGenetics(&mon->box, genetics);
}

u32 Fusion_CountPassives(const struct FusionGenetics *genetics)
{
    u32 i, count = 0;

    for (i = 0; i < FUSION_MAX_PASSIVES; i++)
    {
        if (genetics->passives[i] != ABILITY_NONE)
            count++;
    }
    return count;
}
