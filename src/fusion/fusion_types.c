#include "global.h"
#include "pokemon.h"
#include "fusion/fusion.h"

// The persistent third type as the battle engine stores it in
// BattlePokemon.fusionType and types[2]: TYPE_MYSTERY means none.
enum Type Fusion_GetMonBattleThirdType(struct Pokemon *mon)
{
    struct FusionGenetics genetics;

    Fusion_GetMonGenetics(mon, &genetics);
    if (genetics.extraType == TYPE_NONE)
        return TYPE_MYSTERY;
    return genetics.extraType;
}

// Cross-species main ability, ABILITY_NONE if the species ability applies.
enum Ability Fusion_GetMonAbilityOverride(struct Pokemon *mon)
{
    struct FusionGenetics genetics;

    Fusion_GetMonGenetics(mon, &genetics);
    return genetics.mainAbility;
}

// Passives as the battle engine stores them in BattlePokemon.passives.
void Fusion_GetMonPassives(struct Pokemon *mon, enum Ability *passives)
{
    struct FusionGenetics genetics;

    Fusion_GetMonGenetics(mon, &genetics);
    for (u32 i = 0; i < FUSION_MAX_PASSIVES; i++)
        passives[i] = genetics.passives[i];
}

// Everything the battle engine needs from the genetics, with a single decryption.
void Fusion_GetMonBattleData(struct Pokemon *mon, enum Type *thirdType, enum Ability *mainAbility, enum Ability *passives)
{
    struct FusionGenetics genetics;

    Fusion_GetMonGenetics(mon, &genetics);
    *thirdType = genetics.extraType == TYPE_NONE ? TYPE_MYSTERY : genetics.extraType;
    *mainAbility = genetics.mainAbility;
    for (u32 i = 0; i < FUSION_MAX_PASSIVES; i++)
        passives[i] = genetics.passives[i];
}
