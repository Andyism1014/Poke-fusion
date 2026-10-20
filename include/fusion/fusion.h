#ifndef GUARD_FUSION_H
#define GUARD_FUSION_H

#include "fusion/fusion_config.h"
#include "constants/abilities.h"
#include "constants/pokemon.h"

#define FUSION_MAX_PASSIVES     3
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
bool32 Fusion_IsCaptureAssistExempt(enum Species species);
u32 Fusion_ApplyCaptureMode(enum Species species, u32 odds);

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
enum Type Fusion_GetBoxMonThirdType(struct BoxPokemon *boxMon);
void Fusion_BuildPreview(struct Pokemon *parentA, struct Pokemon *parentB, struct FusionPreview *preview);
void Fusion_GenerateChild(struct Pokemon *parentA, struct Pokemon *parentB, u32 seed, struct Pokemon *child);
enum FusionCheckResult Fusion_CheckParents(const struct FusionMonRef *refA, const struct FusionMonRef *refB);
bool32 Fusion_Fuse(const struct FusionMonRef *refA, const struct FusionMonRef *refB, u32 seed, struct Pokemon *child);

// fusion_save.c
void Fusion_PackGenetics(const struct FusionGenetics *genetics, u8 *packed);
void Fusion_UnpackGenetics(const u8 *packed, struct FusionGenetics *genetics);
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
void Fusion_ScriptBufferAppraisal(void);
void Fusion_ScriptSelectParentA(void);
void Fusion_ScriptSelectParentB(void);
void Fusion_ScriptCheckParents(void);
void Fusion_ScriptBufferPreview(void);
void Fusion_ScriptDoFusion(void);

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
