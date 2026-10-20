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
