#ifndef GUARD_FUSION_H
#define GUARD_FUSION_H

#include "fusion/fusion_config.h"
#include "constants/abilities.h"
#include "constants/pokemon.h"
#include "constants/fusion.h"

#define FUSION_GENETICS_SIZE    6 // Bytes stored per BoxPokemon, see PokemonSubstruct2.fusionGenetics.
#define FUSION_MAX_GENERATION   31

enum FusionGeneticsFlags
{
    FUSION_FLAG_FUSED = 1 << 0, // Created by fusion.
    FUSION_FLAG_RARE  = 1 << 1, // Marked as a rare result, shown in the fusion log.
};

// Unpacked view of the fusion data stored in every BoxPokemon.
// All-zero means "no fusion data": the mon behaves like a vanilla Pokémon.
struct FusionGenetics
{
    enum Ability mainAbility;                   // ABILITY_NONE = use the species ability.
    enum Ability passives[FUSION_MAX_PASSIVES]; // ABILITY_NONE = empty slot.
    enum Type extraType;                        // TYPE_NONE = no third type.
    u8 generation;                              // 0 = never fused.
    u8 flags;                                   // FUSION_FLAG_*
};

enum FusionMonLocation
{
    FUSION_LOC_NONE,
    FUSION_LOC_PARTY,
    FUSION_LOC_BOX,
};

// Points at a Pokémon in the party or in any PC box.
struct FusionMonRef
{
    u8 location; // enum FusionMonLocation
    u8 box;      // Only for FUSION_LOC_BOX.
    u8 slot;     // Party index or box position.
};

enum FusionCaptureMode
{
    FUSION_CAPTURE_MODE_CLASSIC,    // Vanilla catch odds.
    FUSION_CAPTURE_MODE_CONVENIENT, // Regular wild Pokémon have multiplied catch odds.
    FUSION_CAPTURE_MODE_MATERIAL,   // Regular wild Pokémon are always caught.
    FUSION_CAPTURE_MODE_COUNT,
};

// fusion_capture.c
enum FusionCaptureMode Fusion_GetCaptureMode(void);
void Fusion_SetCaptureMode(enum FusionCaptureMode mode);
u32 Fusion_ApplyCaptureMode(u32 odds);
void Fusion_RollWildGenetics(struct Pokemon *mon);
enum Ability Fusion_RollRandomPassive(struct Pokemon *mon, const struct FusionGenetics *genetics);
enum Type Fusion_RollRandomThirdType(enum Species species);

// fusion_tower.c
struct Trainer;
extern bool8 gFusionTowerBattle;
extern bool8 gFusionDebugTowerAutoWin;
const struct Trainer *Fusion_GetTowerTrainer(void);
bool32 Fusion_IsTowerUnlocked(void);

// Reincarnation (New Game+), gene items
u32 Fusion_GetNewGamePlusCount(void);
void *Fusion_StashReincarnation(void);
void Fusion_RestoreReincarnation(void *data);
void Fusion_ApplyNewGamePlusToTrainerParty(struct Pokemon *party);
bool32 Fusion_UseGeneItem(struct Pokemon *mon, enum Item item, u8 *dest);
u32 Fusion_GetTowerOpponentLevel(u32 floor, u32 playerTopLevel);
u32 Fusion_GetTowerPartySize(u32 floor);
void Fusion_BuildTowerFloor(u32 floor, u32 seed);
void Fusion_ScriptTowerBufferStatus(void);
void Fusion_ScriptTowerPrepareFloor(void);
void Fusion_ScriptTowerStartBattle(void);
void Fusion_ScriptTowerAfterBattle(void);
void Fusion_ScriptTowerEndRun(void);
void Fusion_ScriptTowerRollRewards(void);
void Fusion_ScriptTowerGrantReward(void);
u32 Fusion_GetShardShopCost(u32 entry);
void Fusion_ScriptShopBufferEntry(void);
void Fusion_ScriptShopBuy(void);
void Fusion_ScriptReincarnationStatus(void);
void Fusion_ScriptSetReincarnation(void);

// fusion_wild.c
struct WildPokemon;
bool32 Fusion_IsWildRandomizerEnabled(void);
enum Species Fusion_RandomizeWildSpecies(enum Species original, u32 key);
enum Species Fusion_RandomizeWildSpeciesWithSeed(enum Species original, u32 key, u32 seed);
bool32 Fusion_CanAppearAsRandomWild(enum Species species);
#define FUSION_STARTER_COUNT 3
enum Species Fusion_GetRandomStarter(u32 index);
enum Species Fusion_GetRandomStarterWithSeed(u32 index, u32 seed);
enum Species Fusion_GetWildSlotSpecies(const struct WildPokemon *slot);
void Fusion_RerollWildSeed(void);
#define FUSION_BATTLE_MESSAGE_SIZE 160
extern u8 gFusionBattleMessage[FUSION_BATTLE_MESSAGE_SIZE];
void Fusion_BufferCaughtAppraisal(struct Pokemon *mon);
void BS_FusionCaughtAppraisal(void);

enum FusionCheckResult
{
    FUSION_CHECK_OK,
    FUSION_CHECK_INVALID,         // Missing, egg, or both parents are the same Pokémon.
    FUSION_CHECK_LAST_PARTY_MON,  // Parent B is the last party member and parent A is boxed.
    FUSION_CHECK_HOLDS_MAIL,
    FUSION_CHECK_BAG_FULL,        // Parent B's held item can't be returned to the bag.
};

// What a fusion of two parents can produce, for the confirmation screen.
struct FusionPreview
{
    u8 level;
    enum Ability mainAbilities[2];
    u8 movePoolSize;
    u8 passivePoolSize;
    u8 thirdTypePoolSize[2]; // Indexed by which parent provides the species.
};

// fusion_genetics.c
enum Ability Fusion_GetMonMainAbility(struct Pokemon *mon);
bool32 Fusion_CanBePassive(enum Ability ability);
enum Type Fusion_GetBoxMonThirdType(struct BoxPokemon *boxMon);
void Fusion_BuildPreview(struct Pokemon *parentA, struct Pokemon *parentB, struct FusionPreview *preview);
void Fusion_GenerateChild(struct Pokemon *parentA, struct Pokemon *parentB, u32 seed, struct Pokemon *child);
enum FusionCheckResult Fusion_CheckParents(const struct FusionMonRef *refA, const struct FusionMonRef *refB);
bool32 Fusion_Fuse(const struct FusionMonRef *refA, const struct FusionMonRef *refB, u32 seed, struct Pokemon *child);

// fusion_types.c
enum Type Fusion_GetMonBattleThirdType(struct Pokemon *mon);
enum Ability Fusion_GetMonAbilityOverride(struct Pokemon *mon);
void Fusion_GetMonPassives(struct Pokemon *mon, enum Ability *passives);
void Fusion_GetMonBattleData(struct Pokemon *mon, enum Type *thirdType, enum Ability *mainAbility, enum Ability *passives);

// fusion_save.c
void Fusion_PackGenetics(const struct FusionGenetics *genetics, u8 *packed);
void Fusion_UnpackGenetics(const u8 *packed, struct FusionGenetics *genetics);
enum Ability Fusion_UnpackMainAbility(const u8 *packed);
void Fusion_GetBoxMonGenetics(struct BoxPokemon *boxMon, struct FusionGenetics *genetics);
void Fusion_SetBoxMonGenetics(struct BoxPokemon *boxMon, const struct FusionGenetics *genetics);
void Fusion_GetMonGenetics(struct Pokemon *mon, struct FusionGenetics *genetics);
void Fusion_SetMonGenetics(struct Pokemon *mon, const struct FusionGenetics *genetics);
u32 Fusion_CountPassives(const struct FusionGenetics *genetics);

// fusion_storage.c
struct FusionMonRef Fusion_PartyRef(u32 slot);
struct FusionMonRef Fusion_BoxRef(u32 box, u32 slot);
bool32 Fusion_RefsEqual(const struct FusionMonRef *a, const struct FusionMonRef *b);
struct BoxPokemon *Fusion_GetBoxMonFromRef(const struct FusionMonRef *ref);
bool32 Fusion_IsMaterialRef(const struct FusionMonRef *ref);
bool32 Fusion_ReadMonFromRef(const struct FusionMonRef *ref, struct Pokemon *dst);
bool32 Fusion_CanCommitFusion(const struct FusionMonRef *parentA, const struct FusionMonRef *parentB);
bool32 Fusion_CommitFusion(const struct FusionMonRef *parentA, const struct FusionMonRef *parentB, struct Pokemon *child);
bool32 Fusion_NextMaterialRef(struct FusionMonRef *ref);

// fusion_ui.c
typedef void (*FusionSelectorCallback)(bool32 selected, const struct FusionMonRef *ref);
u32 Fusion_GetBoxMonIvTotal(struct BoxPokemon *boxMon);
u32 Fusion_CountMaterials(const struct FusionMonRef *exclude);
void Fusion_OpenMaterialSelector(const u8 *title, const struct FusionMonRef *exclude, FusionSelectorCallback callback);
void Fusion_ScriptCountMaterials(void);
void Fusion_ScriptOpenAppraiseSelector(void);
void Fusion_ScriptShowAppraisalCard(void);
void Fusion_ScriptHideCard(void);
void Fusion_ScriptCardYesNo(void);
void Fusion_ScriptSelectParentA(void);
void Fusion_ScriptSelectParentB(void);
void Fusion_ScriptCheckParents(void);
void Fusion_ScriptShowPreviewCard(void);
void Fusion_ScriptDoFusion(void);
void Fusion_ScriptBufferSettings(void);
void Fusion_ScriptSetCaptureMode(void);
void Fusion_ScriptToggleExpShare(void);
void Fusion_ScriptToggleWildRandomizer(void);

// fusion_ability_panel.c
struct FusionAbilityPanelStyle
{
    u8 x;          // Left edge of every line.
    u8 descX;      // Description indent from x.
    u8 descY;      // Description offset below the name line.
    u8 descFontId; // Narrowed to fit if needed.
    const u8 *textColors;
    const u8 *labelColors;
};
void Fusion_PrintAbilityPanelEntry(u32 windowId, const struct FusionAbilityPanelStyle *style, u32 slot, enum Ability ability, u32 y, u32 nameWidth);
void Fusion_OpenBattleAbilityPanel(u32 battler);
bool32 Fusion_HandleBattleAbilityPanelInput(void);

// fusion_debug.c
const u8 *Fusion_Debug_GetCaptureModeName(enum FusionCaptureMode mode);
enum FusionCaptureMode Fusion_Debug_CycleCaptureMode(void);
bool32 Fusion_Debug_IsRandomPassiveCandidate(enum Ability ability);
void Fusion_Debug_RandomizeGenetics(struct FusionGenetics *genetics, enum Species species);
enum Species Fusion_Debug_GetRandomMaterialSpecies(void);
u32 Fusion_Debug_FillBoxWithMaterials(u32 boxId);

// Called from NewGameInitData.
void Fusion_NewGameInit(void);

#endif // GUARD_FUSION_H
