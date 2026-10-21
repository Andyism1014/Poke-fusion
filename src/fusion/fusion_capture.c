#include "global.h"
#include "battle.h"
#include "battle_main.h"
#include "string_util.h"
#include "constants/characters.h"
#include "event_data.h"
#include "item.h"
#include "pokemon.h"
#include "random.h"
#include "fusion/fusion.h"
#include "constants/items.h"

// Any odds above this value always result in a capture (see SetBallThrowShakes).
#define FUSION_CAPTURE_ALWAYS 255

enum FusionCaptureMode Fusion_GetCaptureMode(void)
{
    u32 mode = VarGet(FUSION_VAR_CAPTURE_MODE);

    if (mode >= FUSION_CAPTURE_MODE_COUNT)
        return FUSION_CAPTURE_MODE_CLASSIC;
    return mode;
}

void Fusion_SetCaptureMode(enum FusionCaptureMode mode)
{
    if (mode >= FUSION_CAPTURE_MODE_COUNT)
        mode = FUSION_CAPTURE_MODE_CLASSIC;
    VarSet(FUSION_VAR_CAPTURE_MODE, mode);
}

// Rare Pokémon keep their vanilla catch odds so they stay worth collecting.
bool32 Fusion_IsCaptureAssistExempt(enum Species species)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];

    return info->isRestrictedLegendary
        || info->isSubLegendary
        || info->isMythical
        || info->isUltraBeast;
}

u32 Fusion_ApplyCaptureMode(enum Species species, u32 odds)
{
    if (Fusion_IsCaptureAssistExempt(species))
        return odds;

    switch (Fusion_GetCaptureMode())
    {
    case FUSION_CAPTURE_MODE_CONVENIENT:
        // Odds past FUSION_CAPTURE_ALWAYS already guarantee a capture, so cap the product there.
        return max(odds, min(odds * FUSION_CAPTURE_CONVENIENT_MULTIPLIER, FUSION_CAPTURE_ALWAYS));
    case FUSION_CAPTURE_MODE_MATERIAL:
        return max(odds, FUSION_CAPTURE_ALWAYS);
    case FUSION_CAPTURE_MODE_CLASSIC:
    default:
        return odds;
    }
}

// Passives that would make a single wild find game-breaking.
static bool32 IsWildPassiveCandidate(enum Ability ability)
{
    switch (ability)
    {
    case ABILITY_HUGE_POWER:
    case ABILITY_PURE_POWER:
    case ABILITY_SHADOW_TAG:
    case ABILITY_ARENA_TRAP:
    case ABILITY_MOODY:
    case ABILITY_PARENTAL_BOND:
        return FALSE;
    default:
        return Fusion_CanBePassive(ability);
    }
}

static enum Ability RollWildPassive(struct Pokemon *mon, const struct FusionGenetics *genetics)
{
    // Random draws until a valid one; nearly all abilities qualify, so this ends fast.
    for (u32 tries = 0; tries < 32; tries++)
    {
        enum Ability ability = 1 + Random() % (ABILITIES_COUNT - 1);
        bool32 duplicate = ability == GetMonAbility(mon);

        for (u32 i = 0; i < FUSION_MAX_PASSIVES; i++)
            duplicate |= genetics->passives[i] == ability;
        if (!duplicate && IsWildPassiveCandidate(ability))
            return ability;
    }
    return ABILITY_NONE;
}

static enum Type RollWildThirdType(enum Species species)
{
    for (u32 tries = 0; tries < 32; tries++)
    {
        enum Type type = TYPE_NORMAL + Random() % (TYPE_FAIRY + 1 - TYPE_NORMAL);

        if (type != TYPE_MYSTERY && type != gSpeciesInfo[species].types[0] && type != gSpeciesInfo[species].types[1])
            return type;
    }
    return TYPE_NONE;
}

// Called for every wild Pokémon right after it is created.
void Fusion_RollWildGenetics(struct Pokemon *mon)
{
    enum Species species = GetMonData(mon, MON_DATA_SPECIES);
    struct FusionGenetics genetics = {0};

    if (Fusion_IsCaptureAssistExempt(species))
        return;

    if (Random() % 100 < FUSION_WILD_PASSIVE_CHANCE)
    {
        genetics.passives[0] = RollWildPassive(mon, &genetics);
        if (Random() % 100 < FUSION_WILD_SECOND_PASSIVE_CHANCE)
            genetics.passives[1] = RollWildPassive(mon, &genetics);
    }
    if (Random() % 100 < FUSION_WILD_THIRD_TYPE_CHANCE)
        genetics.extraType = RollWildThirdType(species);

    if (genetics.passives[1] != ABILITY_NONE || genetics.extraType != TYPE_NONE)
        genetics.flags |= FUSION_FLAG_RARE;
    if (genetics.passives[0] != ABILITY_NONE || genetics.extraType != TYPE_NONE)
        Fusion_SetMonGenetics(mon, &genetics);
}

EWRAM_DATA u8 gFusionBattleMessage[FUSION_BATTLE_MESSAGE_SIZE] = {0};

static const u8 sText_CaughtIvs[] = _("IVs {STR_VAR_1}/186");
static const u8 sText_CaughtRare[] = _("  A rare find!");
static const u8 sText_CaughtPassive[] = _("Passive: ");
static const u8 sText_CaughtThirdType[] = _("Third type: ");
static const u8 sText_CaughtSeparator[] = _("  ");
static const u8 sText_Separator[] = _(", ");

// Battle script native, run right after "Gotcha!": a one-message appraisal of the
// caught Pokémon so the player knows immediately if it's a good material.
void BS_FusionCaughtAppraisal(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_OPPONENT_A][gBattlerPartyIndexes[gBattlerTarget]];
    struct FusionGenetics genetics;
    u8 *text;

    Fusion_GetMonGenetics(mon, &genetics);
    ConvertIntToDecimalStringN(gStringVar1, Fusion_GetBoxMonIvTotal(&mon->box), STR_CONV_MODE_LEFT_ALIGN, 3);
    text = StringExpandPlaceholders(gFusionBattleMessage, sText_CaughtIvs);
    if (genetics.flags & FUSION_FLAG_RARE)
        text = StringCopy(text, sText_CaughtRare);

    if (genetics.passives[0] != ABILITY_NONE || genetics.extraType != TYPE_NONE)
        *text++ = CHAR_NEWLINE;
    if (genetics.passives[0] != ABILITY_NONE)
    {
        text = StringCopy(StringCopy(text, sText_CaughtPassive), gAbilitiesInfo[genetics.passives[0]].name);
        if (genetics.passives[1] != ABILITY_NONE)
            text = StringCopy(StringCopy(text, sText_Separator), gAbilitiesInfo[genetics.passives[1]].name);
        if (genetics.extraType != TYPE_NONE)
            text = StringCopy(text, sText_CaughtSeparator);
    }
    if (genetics.extraType != TYPE_NONE)
        text = StringCopy(StringCopy(text, sText_CaughtThirdType), gTypesInfo[genetics.extraType].name);
    *text++ = CHAR_PROMPT_CLEAR;
    *text = EOS;

    // callnative: opcode + function pointer
    gBattlescriptCurrInstr += 1 + sizeof(void (*)(void));
}

void Fusion_NewGameInit(void)
{
    Fusion_SetCaptureMode(FUSION_CAPTURE_MODE_NEW_GAME);

    if (FUSION_NEW_GAME_GIVE_BOX_LINK)
        AddBagItem(ITEM_POKEMON_BOX_LINK, 1);
    if (FUSION_NEW_GAME_GIVE_TERMINAL)
        AddBagItem(ITEM_FUSION_TERMINAL, 1);

    if (FUSION_NEW_GAME_EXP_SHARE)
        FlagSet(FLAG_FUSION_EXP_SHARE);
    if (FUSION_NEW_GAME_RUNNING_SHOES)
        FlagSet(FLAG_SYS_B_DASH);

#if DEXNAV_ENABLED
    if (FUSION_NEW_GAME_GIVE_DEXNAV)
        FlagSet(DN_FLAG_DEXNAV_GET);
#endif
}
