#include "global.h"
#include "battle.h"
#include "battle_setup.h"
#include "data.h"
#include "event_data.h"
#include "item.h"
#include "pokemon.h"
#include "random.h"
#include "script_pokemon_util.h"
#include "string_util.h"
#include "fusion/fusion.h"
#include "constants/battle_ai.h"
#include "constants/characters.h"
#include "constants/items.h"
#include "constants/songs.h"
#include "constants/trainers.h"

// Fusion Tower: an endless, roguelike post-game challenge run from the Fusion Terminal.
// Every floor is one trainer battle against a generated team that gets stronger floor by
// floor (more passives, third types, higher IVs, better held items; a boss every 10
// floors). The party is not healed between floors; after a win the player picks one of
// three rewards. A loss ends the run. Fusion Shards and the best floor persist.

EWRAM_DATA bool8 gFusionTowerBattle = FALSE;
EWRAM_DATA bool8 gFusionDebugTowerAutoWin = FALSE; // Debug menu: floors are won without a battle.
static EWRAM_DATA struct Trainer sTowerTrainer = {0};

struct TowerTrainerStyle
{
    u8 trainerClass;
    enum TrainerPicID trainerPic;
    u8 gender;
    const u8 *name;
};

static const struct TowerTrainerStyle sTowerTrainers[] =
{
    {TRAINER_CLASS_COOLTRAINER, TRAINER_PIC_COOLTRAINER_M, MALE, COMPOUND_STRING("Kai")},
    {TRAINER_CLASS_COOLTRAINER, TRAINER_PIC_COOLTRAINER_F, FEMALE, COMPOUND_STRING("Mira")},
    {TRAINER_CLASS_EXPERT, TRAINER_PIC_EXPERT_M, MALE, COMPOUND_STRING("Tobias")},
    {TRAINER_CLASS_EXPERT, TRAINER_PIC_EXPERT_F, FEMALE, COMPOUND_STRING("Sena")},
    {TRAINER_CLASS_DRAGON_TAMER, TRAINER_PIC_DRAGON_TAMER, MALE, COMPOUND_STRING("Ryuu")},
};

static const struct TowerTrainerStyle sTowerBoss =
{
    TRAINER_CLASS_CHAMPION, TRAINER_PIC_STEVEN, MALE, COMPOUND_STRING("Alchemist"),
};

// The party is built at runtime, but battle code still reads per-mon trainer data (gimmicks,
// Tera, balls...) through Trainer.party, so it must point at inert entries.
static const struct TrainerMon sTowerTrainerMons[PARTY_SIZE] = {0};

static const u16 sTowerHeldItems[] =
{
    ITEM_SITRUS_BERRY, ITEM_LEFTOVERS, ITEM_LIFE_ORB, ITEM_FOCUS_SASH, ITEM_ASSAULT_VEST, ITEM_CHOICE_SCARF,
};

const struct Trainer *Fusion_GetTowerTrainer(void)
{
    return &sTowerTrainer;
}

bool32 Fusion_IsTowerUnlocked(void)
{
    return !FUSION_TOWER_REQUIRES_GAME_CLEAR || FlagGet(FLAG_SYS_GAME_CLEAR);
}

static bool32 IsBossFloor(u32 floor)
{
    return floor % FUSION_TOWER_BOSS_INTERVAL == 0;
}

static u32 GetPlayerTopLevel(void)
{
    u32 top = 5;

    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_SANITY_HAS_SPECIES)
         && !GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_IS_EGG))
            top = max(top, GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_LEVEL));
    }
    return top;
}

u32 Fusion_GetTowerOpponentLevel(u32 floor, u32 playerTopLevel)
{
    return min(MAX_LEVEL, playerTopLevel + floor / 3 - 2 + (IsBossFloor(floor) ? 3 : 0));
}

u32 Fusion_GetTowerPartySize(u32 floor)
{
    if (IsBossFloor(floor))
        return PARTY_SIZE;
    return min(3 + floor / 5, PARTY_SIZE);
}

static bool32 CanEvolve(enum Species species)
{
    const struct Evolution *evolutions = GetSpeciesEvolutions(species);

    return evolutions != NULL && evolutions[0].method != EVOLUTIONS_END;
}

// Anything that can appear in the wild, stronger on later floors. High-level opponents are
// fully evolved, as a trainer's team would be.
static enum Species RollTowerSpecies(u32 floor, u32 level, rng_value_t *rng)
{
    u32 minBst = min(300 + floor * 10, IsBossFloor(floor) ? 540 : 500);

    for (u32 tries = 0; tries < 400; tries++)
    {
        enum Species species = 1 + LocalRandom32(rng) % (NUM_SPECIES - 1);

        if (Fusion_CanAppearAsRandomWild(species) && GetSpeciesBaseStatTotal(species) >= minBst
         && (level < FUSION_TOWER_FULLY_EVOLVED_LEVEL || !CanEvolve(species)))
            return species;
    }
    return SPECIES_GARCHOMP;
}

static void CreateTowerMon(struct Pokemon *mon, u32 floor, u32 level, rng_value_t *rng)
{
    struct FusionGenetics genetics = {0};
    enum Species species = RollTowerSpecies(floor, level, rng);
    u32 minIv = min(floor, MAX_PER_STAT_IVS);
    u32 passives = min(FUSION_MAX_PASSIVES, floor / FUSION_TOWER_FLOORS_PER_PASSIVE + (IsBossFloor(floor) ? 1 : 0));

    u32 blockDynamax = BLOCK_AI_DYNAMAX;
    enum Type blockTera = TYPE_MYSTERY;

    CreateMon(mon, species, level, LocalRandom32(rng), OTID_STRUCT_RANDOM_NO_SHINY);
    GiveMonInitialMoveset(mon);
    // Like trainer party data without gimmicks: the AI must not Dynamax or Terastallize.
    SetMonData(mon, MON_DATA_DYNAMAX_LEVEL, &blockDynamax);
    SetMonData(mon, MON_DATA_TERA_TYPE, &blockTera);
    for (u32 i = 0; i < NUM_STATS; i++)
    {
        u32 iv = minIv + LocalRandom32(rng) % (MAX_PER_STAT_IVS - minIv + 1);
        SetMonData(mon, MON_DATA_HP_IV + i, &iv);
    }
    for (u32 i = 0; i < passives; i++)
        genetics.passives[i] = Fusion_RollRandomPassive(mon, &genetics);
    if (LocalRandom32(rng) % 100 < min(10 + floor * 2, 60))
        genetics.extraType = Fusion_RollRandomThirdType(species);
    genetics.generation = min(floor / 5, FUSION_MAX_GENERATION);
    Fusion_SetMonGenetics(mon, &genetics);

    if (floor >= 5)
    {
        u16 item = sTowerHeldItems[LocalRandom32(rng) % ARRAY_COUNT(sTowerHeldItems)];
        SetMonData(mon, MON_DATA_HELD_ITEM, &item);
    }
    CalculateMonStats(mon);
}

static void BuildTowerTrainer(u32 floor, rng_value_t *rng)
{
    const struct TowerTrainerStyle *style = IsBossFloor(floor) ? &sTowerBoss
                                          : &sTowerTrainers[LocalRandom32(rng) % ARRAY_COUNT(sTowerTrainers)];

    memset(&sTowerTrainer, 0, sizeof(sTowerTrainer));
    sTowerTrainer.trainerClass = style->trainerClass;
    sTowerTrainer.trainerPic = style->trainerPic;
    sTowerTrainer.gender = style->gender;
    StringCopyN(sTowerTrainer.trainerName, style->name, TRAINER_NAME_LENGTH);
    sTowerTrainer.trainerName[TRAINER_NAME_LENGTH] = EOS;
    sTowerTrainer.aiFlags = floor >= 10 ? AI_FLAG_SMART_TRAINER : AI_FLAG_BASIC_TRAINER;
    sTowerTrainer.partySize = Fusion_GetTowerPartySize(floor);
    sTowerTrainer.party = sTowerTrainerMons;
}

// Fills the opponent party for a floor. Deterministic for a given floor and run seed.
void Fusion_BuildTowerFloor(u32 floor, u32 seed)
{
    rng_value_t rng = LocalRandomSeed(seed ^ (floor * 0x9E3779B1));
    u32 level = Fusion_GetTowerOpponentLevel(floor, GetPlayerTopLevel());

    BuildTowerTrainer(floor, &rng);
    ZeroEnemyPartyMons();
    for (u32 i = 0; i < Fusion_GetTowerPartySize(floor); i++)
        CreateTowerMon(&gParties[B_TRAINER_OPPONENT_A][i], floor, level, &rng);
    CalculateEnemyPartyCount();
}

static u32 NextFloor(void)
{
    return gSaveBlock3Ptr->fusionTower.floor + 1;
}

static void EndRun(void)
{
    gSaveBlock3Ptr->fusionTower.floor = 0;
    gSaveBlock3Ptr->fusionTower.inRun = FALSE;
    HealPlayerParty();
}

//
// Script interface
//

// gStringVar1: next floor, gStringVar2: best floor, gStringVar3: shards.
// VAR_RESULT: 1 if the tower is unlocked, 2 if only the Shard Shop is (Shards carried into
// a New Game+), 0 otherwise.
void Fusion_ScriptTowerBufferStatus(void)
{
    ConvertIntToDecimalStringN(gStringVar1, NextFloor(), STR_CONV_MODE_LEFT_ALIGN, 4);
    ConvertIntToDecimalStringN(gStringVar2, gSaveBlock3Ptr->fusionTower.bestFloor, STR_CONV_MODE_LEFT_ALIGN, 4);
    ConvertIntToDecimalStringN(gStringVar3, gSaveBlock3Ptr->fusionTower.shards, STR_CONV_MODE_LEFT_ALIGN, 5);
    if (Fusion_IsTowerUnlocked())
        gSpecialVar_Result = 1;
    else
        gSpecialVar_Result = gSaveBlock3Ptr->fusionTower.shards > 0 ? 2 : 0;
}

// VAR_RESULT: FALSE if no party member can fight.
void Fusion_ScriptTowerPrepareFloor(void)
{
    u32 floor = NextFloor();

    gSpecialVar_Result = FALSE;
    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) && !GetMonData(mon, MON_DATA_IS_EGG) && GetMonData(mon, MON_DATA_HP) > 0)
            gSpecialVar_Result = TRUE;
    }
    if (!gSpecialVar_Result)
        return;

    if (!gSaveBlock3Ptr->fusionTower.inRun)
    {
        gSaveBlock3Ptr->fusionTower.inRun = TRUE;
        gSaveBlock3Ptr->fusionTower.runSeed = Random32();
    }
    Fusion_BuildTowerFloor(floor, gSaveBlock3Ptr->fusionTower.runSeed);
    ConvertIntToDecimalStringN(gStringVar1, floor, STR_CONV_MODE_LEFT_ALIGN, 4);
    StringCopy(gStringVar2, sTowerTrainer.trainerName);
    ConvertIntToDecimalStringN(gStringVar3, Fusion_GetTowerPartySize(floor), STR_CONV_MODE_LEFT_ALIGN, 1);
}

// Starts the battle prepared by Fusion_ScriptTowerPrepareFloor. VAR_0x8004: TRUE if a battle
// was started, so the script must `waitstate` until it ends (it then resumes with VAR_RESULT
// set by CB2_EndFusionTowerBattle).
void Fusion_ScriptTowerStartBattle(void)
{
    gSpecialVar_Result = FALSE;
    gSpecialVar_0x8004 = FALSE;
    if (DEBUG_OVERWORLD_MENU && gFusionDebugTowerAutoWin)
    {
        gSpecialVar_Result = TRUE;
        return;
    }
    gSpecialVar_0x8004 = TRUE;
    gFusionTowerBattle = TRUE;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    BattleSetup_StartFusionTowerBattle(IsBossFloor(NextFloor()) ? MUS_VS_CHAMPION : MUS_VS_TRAINER);
}

// VAR_RESULT: TRUE if the floor was cleared. gStringVar1: shards earned / floor reached.
void Fusion_ScriptTowerAfterBattle(void)
{
    struct FusionTowerSave *tower = &gSaveBlock3Ptr->fusionTower;
    u32 floor = NextFloor();

    if (gSpecialVar_Result)
    {
        u32 shards = (IsBossFloor(floor) ? 5 : 1) * (1 + floor / 5);

        tower->floor = floor;
        tower->bestFloor = max(tower->bestFloor, floor);
        tower->shards = min(tower->shards + shards, FUSION_TOWER_MAX_SHARDS);
        ConvertIntToDecimalStringN(gStringVar1, shards, STR_CONV_MODE_LEFT_ALIGN, 4);
    }
    else
    {
        ConvertIntToDecimalStringN(gStringVar1, floor, STR_CONV_MODE_LEFT_ALIGN, 4);
        EndRun();
    }
}

void Fusion_ScriptTowerEndRun(void)
{
    EndRun();
}

// Rewards offered after a cleared floor.
enum TowerReward
{
    TOWER_REWARD_HEAL,
    TOWER_REWARD_RARE_CANDY,
    TOWER_REWARD_SHARDS,
    TOWER_REWARD_MAX_ELIXIR,
    TOWER_REWARD_BOTTLE_CAP,
    TOWER_REWARD_HELD_ITEM,
    TOWER_REWARD_COUNT,
};

static const u8 sText_RewardHeal[] = _("Heal party");
static const u8 sText_RewardRareCandy[] = _("Rare Candy x2");
static const u8 sText_RewardShards[] = _("+10 Shards");
static const u8 sText_RewardMaxElixir[] = _("Max Elixir");
static const u8 sText_RewardBottleCap[] = _("Bottle Cap");
static const u8 sText_RewardHeldItem[] = _("Random held item");

static const u8 *const sRewardNames[TOWER_REWARD_COUNT] =
{
    [TOWER_REWARD_HEAL] = sText_RewardHeal,
    [TOWER_REWARD_RARE_CANDY] = sText_RewardRareCandy,
    [TOWER_REWARD_SHARDS] = sText_RewardShards,
    [TOWER_REWARD_MAX_ELIXIR] = sText_RewardMaxElixir,
    [TOWER_REWARD_BOTTLE_CAP] = sText_RewardBottleCap,
    [TOWER_REWARD_HELD_ITEM] = sText_RewardHeldItem,
};

// Three distinct rewards; "Heal party" is always one of them so a run can't dead-end.
// The choices go to VAR_0x8004-0x8006, their names to gStringVar1-3.
void Fusion_ScriptTowerRollRewards(void)
{
    u16 *choices[] = {&gSpecialVar_0x8004, &gSpecialVar_0x8005, &gSpecialVar_0x8006};
    u8 *names[] = {gStringVar1, gStringVar2, gStringVar3};
    u32 used = 1 << TOWER_REWARD_HEAL;

    *choices[0] = TOWER_REWARD_HEAL;
    for (u32 i = 1; i < 3; i++)
    {
        u32 reward;
        do
        {
            reward = Random() % TOWER_REWARD_COUNT;
        } while (used & (1 << reward));
        used |= 1 << reward;
        *choices[i] = reward;
    }
    for (u32 i = 0; i < 3; i++)
        StringCopy(names[i], sRewardNames[*choices[i]]);
}

// VAR_RESULT: index (0-2) of the chosen reward among VAR_0x8004-0x8006.
void Fusion_ScriptTowerGrantReward(void)
{
    u16 choices[] = {gSpecialVar_0x8004, gSpecialVar_0x8005, gSpecialVar_0x8006};
    u32 reward = choices[min(gSpecialVar_Result, 2)];

    switch (reward)
    {
    case TOWER_REWARD_HEAL:
        HealPlayerParty();
        break;
    case TOWER_REWARD_RARE_CANDY:
        AddBagItem(ITEM_RARE_CANDY, 2);
        break;
    case TOWER_REWARD_SHARDS:
        gSaveBlock3Ptr->fusionTower.shards = min(gSaveBlock3Ptr->fusionTower.shards + 10, FUSION_TOWER_MAX_SHARDS);
        break;
    case TOWER_REWARD_MAX_ELIXIR:
        AddBagItem(ITEM_MAX_ELIXIR, 1);
        break;
    case TOWER_REWARD_BOTTLE_CAP:
        AddBagItem(ITEM_BOTTLE_CAP, 1);
        break;
    case TOWER_REWARD_HELD_ITEM:
        AddBagItem(sTowerHeldItems[Random() % ARRAY_COUNT(sTowerHeldItems)], 1);
        break;
    }
}

// Shard Shop: the order matches the menu in data/scripts/fusion.inc.
static const struct
{
    u16 item;
    u16 cost;
} sShardShop[] =
{
    {ITEM_PASSIVE_CRYSTAL, 30},
    {ITEM_TYPE_PRISM,      20},
    {ITEM_RARE_CANDY,       3},
    {ITEM_BOTTLE_CAP,      15},
    {ITEM_GOLD_BOTTLE_CAP, 40},
    {ITEM_LEFTOVERS,       25},
    {ITEM_LIFE_ORB,        25},
    {ITEM_CHOICE_SCARF,    25},
    {ITEM_FOCUS_SASH,      20},
    {ITEM_MASTER_BALL,    100},
};

u32 Fusion_GetShardShopCost(u32 entry)
{
    return entry < ARRAY_COUNT(sShardShop) ? sShardShop[entry].cost : 0;
}

// VAR_0x8004: shop entry. VAR_RESULT: FALSE for an invalid entry.
// gStringVar1: item name, gStringVar2: cost, gStringVar3: Shards owned.
void Fusion_ScriptShopBufferEntry(void)
{
    u32 entry = gSpecialVar_0x8004;

    ConvertIntToDecimalStringN(gStringVar3, gSaveBlock3Ptr->fusionTower.shards, STR_CONV_MODE_LEFT_ALIGN, 5);
    gSpecialVar_Result = entry < ARRAY_COUNT(sShardShop);
    if (!gSpecialVar_Result)
        return;
    CopyItemName(sShardShop[entry].item, gStringVar1);
    ConvertIntToDecimalStringN(gStringVar2, sShardShop[entry].cost, STR_CONV_MODE_LEFT_ALIGN, 4);
}

// VAR_0x8004: shop entry. VAR_RESULT: 0 bought, 1 not enough Shards, 2 bag full.
void Fusion_ScriptShopBuy(void)
{
    u32 entry = min(gSpecialVar_0x8004, ARRAY_COUNT(sShardShop) - 1);
    struct FusionTowerSave *tower = &gSaveBlock3Ptr->fusionTower;

    if (tower->shards < sShardShop[entry].cost)
        gSpecialVar_Result = 1;
    else if (!AddBagItem(sShardShop[entry].item, 1))
        gSpecialVar_Result = 2;
    else
    {
        tower->shards -= sShardShop[entry].cost;
        gSpecialVar_Result = 0;
    }
    ConvertIntToDecimalStringN(gStringVar3, tower->shards, STR_CONV_MODE_LEFT_ALIGN, 5);
}
