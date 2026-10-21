#include "global.h"
#include "event_data.h"
#include "overworld.h"
#include "malloc.h"
#include "pokedex.h"
#include "pokemon.h"
#include "random.h"
#include "save.h"
#include "string_util.h"
#include "fusion/fusion.h"

// Reincarnation (New Game+). After the Hall of Fame the player can mark the save for
// reincarnation; the next NEW GAME starts a fresh, re-rolled world but the saved party comes
// along, reset to Lv. 5 with its IVs, moves and fusion genetics intact. Fusion Shards and the
// best tower floor are kept. Each cycle makes trainers stronger and wild genes more common.

struct FusionReincarnation
{
    struct FusionTowerSave tower;
    struct Pokemon party[PARTY_SIZE];
};

u32 Fusion_GetNewGamePlusCount(void)
{
    return gSaveBlock3Ptr->fusionTower.ngPlusCount;
}

// Called by NewGameInitData before the save data is cleared. The save file loaded at boot
// is still in RAM, so the party it holds can be copied out.
void *Fusion_StashReincarnation(void)
{
    struct FusionReincarnation *stash;

    if (gSaveFileStatus == SAVE_STATUS_EMPTY || gSaveFileStatus == SAVE_STATUS_CORRUPT
     || !gSaveBlock3Ptr->fusionTower.reincarnationPending)
        return NULL;

    stash = AllocZeroed(sizeof(*stash));
    if (stash == NULL)
        return NULL;
    stash->tower = gSaveBlock3Ptr->fusionTower;
    for (u32 i = 0; i < PARTY_SIZE; i++)
        stash->party[i] = *GetSavedPlayerPartyMon(i);
    return stash;
}

static void ReincarnateMon(struct Pokemon *mon)
{
    enum Species species = GetMonData(mon, MON_DATA_SPECIES);
    bool32 isShiny = GetMonData(mon, MON_DATA_IS_SHINY);
    u32 exp = gExperienceTables[gSpeciesInfo[species].growthRate][FUSION_REINCARNATION_LEVEL];
    u32 zero = 0;

    SetMonData(mon, MON_DATA_EXP, &exp);
    for (u32 i = 0; i < NUM_STATS; i++)
        SetMonData(mon, MON_DATA_HP_EV + i, &zero);
    // The player is a new trainer: make them the OT so their Pokémon obey without badges.
    SetMonData(mon, MON_DATA_OT_NAME, gSaveBlock2Ptr->playerName);
    SetMonData(mon, MON_DATA_OT_GENDER, &gSaveBlock2Ptr->playerGender);
    SetBoxMonOtIdReencrypt(&mon->box, READ_OTID_FROM_SAVE);
    SetMonData(mon, MON_DATA_IS_SHINY, &isShiny);
    CalculateMonStats(mon);
    HealPokemon(mon);
}

// Called at the end of NewGameInitData with the result of Fusion_StashReincarnation.
void Fusion_RestoreReincarnation(void *data)
{
    struct FusionReincarnation *stash = data;
    struct FusionTowerSave *tower = &gSaveBlock3Ptr->fusionTower;
    u32 count = 0;

    if (stash == NULL)
        return;

    tower->shards = stash->tower.shards;
    tower->bestFloor = stash->tower.bestFloor;
    tower->ngPlusCount = min(stash->tower.ngPlusCount + 1, 255);

    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &stash->party[i];
        enum Species species = GetMonData(mon, MON_DATA_SPECIES);

        if (species == SPECIES_NONE || GetMonData(mon, MON_DATA_IS_EGG))
            continue;
        ReincarnateMon(mon);
        gParties[B_TRAINER_PLAYER][count++] = *mon;
        GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_SET_SEEN);
        GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_SET_CAUGHT);
    }
    gPartiesCount[B_TRAINER_PLAYER] = count;
    Free(stash);
}

// NG+ trainers: higher levels and random passives, more of both every cycle.
void Fusion_ApplyNewGamePlusToTrainerParty(struct Pokemon *party)
{
    u32 cycle = min(Fusion_GetNewGamePlusCount(), FUSION_NG_PLUS_MAX_SCALING);

    if (cycle == 0)
        return;

    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &party[i];
        struct FusionGenetics genetics;
        enum Species species = GetMonData(mon, MON_DATA_SPECIES);
        u32 level, exp;

        if (species == SPECIES_NONE)
            continue;
        level = GetMonData(mon, MON_DATA_LEVEL);
        level = min(MAX_LEVEL, level + level * cycle * FUSION_NG_PLUS_LEVEL_PERCENT / 100);
        exp = gExperienceTables[gSpeciesInfo[species].growthRate][level];
        SetMonData(mon, MON_DATA_EXP, &exp);

        Fusion_GetMonGenetics(mon, &genetics);
        for (u32 slot = 0; slot < min(cycle, FUSION_MAX_PASSIVES); slot++)
        {
            if (genetics.passives[slot] == ABILITY_NONE)
                genetics.passives[slot] = Fusion_RollRandomPassive(mon, &genetics);
        }
        Fusion_SetMonGenetics(mon, &genetics);
        CalculateMonStats(mon);
        HealPokemon(mon);
    }
}

//
// Script interface
//

// VAR_RESULT: 0 locked, 1 available, 2 already requested. gStringVar1: the next cycle.
void Fusion_ScriptReincarnationStatus(void)
{
    ConvertIntToDecimalStringN(gStringVar1, Fusion_GetNewGamePlusCount() + 1, STR_CONV_MODE_LEFT_ALIGN, 3);
    if (!FlagGet(FLAG_SYS_GAME_CLEAR))
        gSpecialVar_Result = 0;
    else
        gSpecialVar_Result = gSaveBlock3Ptr->fusionTower.reincarnationPending ? 2 : 1;
}

// Marks (VAR_0x8004 TRUE) or unmarks the save for reincarnation and saves the game.
// VAR_RESULT: TRUE if the game was saved.
void Fusion_ScriptSetReincarnation(void)
{
    gSaveBlock3Ptr->fusionTower.reincarnationPending = gSpecialVar_0x8004 != FALSE;
    IncrementGameStat(GAME_STAT_SAVED_GAME);
    gSpecialVar_Result = TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK;
}
