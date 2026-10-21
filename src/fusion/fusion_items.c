#include "global.h"
#include "battle_main.h"
#include "party_menu.h"
#include "pokemon.h"
#include "string_util.h"
#include "fusion/fusion.h"
#include "constants/items.h"

// Gene items bought with Fusion Shards, used on a party Pokémon from the bag.

static const u8 sText_GainedPassive[] = _("{STR_VAR_1} gained the passive\n{STR_VAR_2}!{PAUSE_UNTIL_PRESS}");
static const u8 sText_RerolledPassive[] = _("{STR_VAR_1}'s {STR_VAR_3}\nbecame {STR_VAR_2}!{PAUSE_UNTIL_PRESS}");
static const u8 sText_NewThirdType[] = _("{STR_VAR_1}'s third type\nis now {STR_VAR_2}!{PAUSE_UNTIL_PRESS}");

// Applies a Passive Crystal or Type Prism. Returns FALSE if it has no effect; otherwise
// the result message is written to dest.
bool32 Fusion_UseGeneItem(struct Pokemon *mon, enum Item item, u8 *dest)
{
    struct FusionGenetics genetics;
    enum Species species = GetMonData(mon, MON_DATA_SPECIES);
    const u8 *text;

    if (GetMonData(mon, MON_DATA_IS_EGG) || species == SPECIES_NONE)
        return FALSE;

    Fusion_GetMonGenetics(mon, &genetics);
    GetMonNickname(mon, gStringVar1);
    switch (item)
    {
    case ITEM_PASSIVE_CRYSTAL:
    {
        u32 slot = 0;
        enum Ability ability;

        while (slot < FUSION_MAX_PASSIVES - 1 && genetics.passives[slot] != ABILITY_NONE)
            slot++;
        if (genetics.passives[slot] != ABILITY_NONE)
            StringCopy(gStringVar3, gAbilitiesInfo[genetics.passives[slot]].name);
        text = genetics.passives[slot] != ABILITY_NONE ? sText_RerolledPassive : sText_GainedPassive;
        ability = Fusion_RollRandomPassive(mon, &genetics);
        if (ability == ABILITY_NONE)
            return FALSE;
        genetics.passives[slot] = ability;
        StringCopy(gStringVar2, gAbilitiesInfo[ability].name);
        break;
    }
    case ITEM_TYPE_PRISM:
    {
        enum Type type;
        u32 tries = 0;

        do
        {
            type = Fusion_RollRandomThirdType(species);
        } while (type == genetics.extraType && ++tries < 8);
        if (type == TYPE_NONE)
            return FALSE;
        genetics.extraType = type;
        StringCopy(gStringVar2, gTypesInfo[type].name);
        text = sText_NewThirdType;
        break;
    }
    default:
        return FALSE;
    }

    Fusion_SetMonGenetics(mon, &genetics);
    StringExpandPlaceholders(dest, text);
    return TRUE;
}
