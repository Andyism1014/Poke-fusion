#include "global.h"
#include "item.h"
#include "mail.h"
#include "pokemon.h"
#include "random.h"
#include "fusion/fusion.h"

// Child generation. Every random choice draws from the rng passed in, so a
// fusion is fully determined by its parents and seed (tests rely on this).

static u32 RandomBelow(rng_value_t *rng, u32 n)
{
    return LocalRandom32(rng) % n;
}

static bool32 RollPercent(rng_value_t *rng, u32 percent)
{
    return RandomBelow(rng, 100) < percent;
}

static bool32 ContainsAbility(const enum Ability *list, u32 count, enum Ability ability)
{
    for (u32 i = 0; i < count; i++)
    {
        if (list[i] == ability)
            return TRUE;
    }
    return FALSE;
}

static bool32 ContainsType(const enum Type *list, u32 count, enum Type type)
{
    for (u32 i = 0; i < count; i++)
    {
        if (list[i] == type)
            return TRUE;
    }
    return FALSE;
}

// GetMonAbility already honors the cross-species override.
enum Ability Fusion_GetMonMainAbility(struct Pokemon *mon)
{
    return GetMonAbility(mon);
}

// The third type the UI should show: TYPE_NONE when there is no extra type, or
// when it duplicates a species type (possible after evolving or changing form).
enum Type Fusion_GetBoxMonThirdType(struct BoxPokemon *boxMon)
{
    enum Species species = GetBoxMonData(boxMon, MON_DATA_SPECIES);
    struct FusionGenetics genetics;

    Fusion_GetBoxMonGenetics(boxMon, &genetics);
    if (genetics.extraType == GetSpeciesType(species, 0) || genetics.extraType == GetSpeciesType(species, 1))
        return TYPE_NONE;
    return genetics.extraType;
}

// Abilities that rewrite the Pokémon's ability or form, or can't be copied in battle,
// only work as a main ability. They are never inherited as passives.
bool32 Fusion_CanBePassive(enum Ability ability)
{
    if (ability == ABILITY_NONE || ability >= ABILITIES_COUNT)
        return FALSE;
    if (gAbilitiesInfo[ability].cantBeCopied)
        return FALSE;
    switch (ability)
    {
    case ABILITY_GULP_MISSILE:
    // Placeholder slots and abilities the engine doesn't implement yet.
    case ABILITY_314:
    case ABILITY_317:
    case ABILITY_AURA_GUARD:
        return FALSE;
    default:
        return TRUE;
    }
}

// Every ability a parent can pass on: its main ability followed by its passives.
static u32 GetInheritableAbilities(struct Pokemon *mon, enum Ability *out)
{
    struct FusionGenetics genetics;
    u32 count = 0;

    Fusion_GetMonGenetics(mon, &genetics);
    out[count++] = Fusion_GetMonMainAbility(mon);
    for (u32 i = 0; i < FUSION_MAX_PASSIVES; i++)
    {
        if (genetics.passives[i] != ABILITY_NONE && !ContainsAbility(out, count, genetics.passives[i]))
            out[count++] = genetics.passives[i];
    }
    return count;
}

// Every type a parent can pass on: its species types followed by its extra type.
static u32 GetInheritableTypes(struct Pokemon *mon, enum Type *out)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[GetMonData(mon, MON_DATA_SPECIES)];
    struct FusionGenetics genetics;
    u32 count = 0;

    Fusion_GetMonGenetics(mon, &genetics);
    for (u32 i = 0; i < ARRAY_COUNT(info->types); i++)
    {
        if (!ContainsType(out, count, info->types[i]))
            out[count++] = info->types[i];
    }
    if (genetics.extraType != TYPE_NONE && !ContainsType(out, count, genetics.extraType))
        out[count++] = genetics.extraType;
    return count;
}

static void ShuffleU16(u16 *list, u32 count, rng_value_t *rng)
{
    for (u32 i = count; i > 1; i--)
    {
        u32 j = RandomBelow(rng, i);
        u16 tmp;
        SWAP(list[i - 1], list[j], tmp);
    }
}

static u32 BuildPassivePool(struct Pokemon *parentA, struct Pokemon *parentB, enum Ability childMain, enum Ability *pool)
{
    enum Ability abilities[2][1 + FUSION_MAX_PASSIVES];
    u32 counts[2], count = 0;

    counts[0] = GetInheritableAbilities(parentA, abilities[0]);
    counts[1] = GetInheritableAbilities(parentB, abilities[1]);
    for (u32 parent = 0; parent < 2; parent++)
    {
        for (u32 i = 0; i < counts[parent]; i++)
        {
            enum Ability ability = abilities[parent][i];

            if (Fusion_CanBePassive(ability) && ability != childMain && !ContainsAbility(pool, count, ability))
                pool[count++] = ability;
        }
    }
    return count;
}

static u32 BuildThirdTypePool(struct Pokemon *parentA, struct Pokemon *parentB, enum Species childSpecies, enum Type *pool)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[childSpecies];
    enum Type types[2][3];
    u32 counts[2], count = 0;

    counts[0] = GetInheritableTypes(parentA, types[0]);
    counts[1] = GetInheritableTypes(parentB, types[1]);
    for (u32 parent = 0; parent < 2; parent++)
    {
        for (u32 i = 0; i < counts[parent]; i++)
        {
            enum Type type = types[parent][i];

            if (type != TYPE_NONE && type != TYPE_MYSTERY && type != TYPE_STELLAR
             && !ContainsType(info->types, ARRAY_COUNT(info->types), type)
             && !ContainsType(pool, count, type))
                pool[count++] = type;
        }
    }
    return count;
}

static u32 BuildMovePool(struct Pokemon *parentA, struct Pokemon *parentB, u16 *pool)
{
    u32 count = 0;

    for (u32 parent = 0; parent < 2; parent++)
    {
        struct Pokemon *mon = parent == 0 ? parentA : parentB;

        for (u32 i = 0; i < MAX_MON_MOVES; i++)
        {
            enum Move move = GetMonData(mon, MON_DATA_MOVE1 + i);
            bool32 duplicate = FALSE;

            for (u32 j = 0; j < count; j++)
                duplicate |= (pool[j] == move);
            if (move != MOVE_NONE && !duplicate)
                pool[count++] = move;
        }
    }
    return count;
}

void Fusion_BuildPreview(struct Pokemon *parentA, struct Pokemon *parentB, struct FusionPreview *preview)
{
    u16 moves[2 * MAX_MON_MOVES];
    enum Ability passives[2 * (1 + FUSION_MAX_PASSIVES)];
    enum Type types[6];

    preview->level = min(GetMonData(parentA, MON_DATA_LEVEL), GetMonData(parentB, MON_DATA_LEVEL));
    preview->mainAbilities[0] = Fusion_GetMonMainAbility(parentA);
    preview->mainAbilities[1] = Fusion_GetMonMainAbility(parentB);
    preview->movePoolSize = BuildMovePool(parentA, parentB, moves);
    // The child's main ability is excluded from its passive pool, so the
    // size depends on which main ability is rolled; report the larger pool.
    preview->passivePoolSize = max(BuildPassivePool(parentA, parentB, preview->mainAbilities[0], passives),
                                   BuildPassivePool(parentA, parentB, preview->mainAbilities[1], passives));
    preview->thirdTypePoolSize[0] = BuildThirdTypePool(parentA, parentB, GetMonData(parentA, MON_DATA_SPECIES), types);
    preview->thirdTypePoolSize[1] = BuildThirdTypePool(parentA, parentB, GetMonData(parentB, MON_DATA_SPECIES), types);
}

static void SetChildMainAbility(struct Pokemon *child, enum Ability ability, struct FusionGenetics *genetics)
{
    enum Species species = GetMonData(child, MON_DATA_SPECIES);

    for (u32 slot = 0; slot < NUM_ABILITY_SLOTS; slot++)
    {
        if (gSpeciesInfo[species].abilities[slot] == ability)
        {
            SetMonData(child, MON_DATA_ABILITY_NUM, &slot);
            genetics->mainAbility = ABILITY_NONE;
            return;
        }
    }
    // Not one of the species' own abilities: store it as an override.
    genetics->mainAbility = ability;
}

void Fusion_GenerateChild(struct Pokemon *parentA, struct Pokemon *parentB, u32 seed, struct Pokemon *child)
{
    rng_value_t rng = LocalRandomSeed(seed);
    struct Pokemon *speciesParent = RollPercent(&rng, FUSION_SPECIES_FROM_A_CHANCE) ? parentA : parentB;
    enum Species species = GetMonData(speciesParent, MON_DATA_SPECIES);
    u32 level = min(GetMonData(parentA, MON_DATA_LEVEL), GetMonData(parentB, MON_DATA_LEVEL));
    struct FusionGenetics genetics = {0}, geneticsA, geneticsB;
    enum Ability abilityPool[2 * (1 + FUSION_MAX_PASSIVES)];
    enum Type typePool[6];
    u16 movePool[2 * MAX_MON_MOVES], passiveOrder[ARRAY_COUNT(abilityPool)];
    enum Ability childMain;
    u32 i, count, inherited, value;

    CreateMon(child, species, level, LocalRandom32(&rng), OTID_STRUCT_PLAYER_ID);

    // Nature
    value = GetMonData(RollPercent(&rng, FUSION_NATURE_FROM_A_CHANCE) ? parentA : parentB, MON_DATA_HIDDEN_NATURE);
    SetMonData(child, MON_DATA_HIDDEN_NATURE, &value);

    // IVs
    for (i = 0; i < NUM_STATS; i++)
    {
        value = GetMonData(RollPercent(&rng, FUSION_IV_FROM_A_CHANCE) ? parentA : parentB, MON_DATA_HP_IV + i);
        if (RollPercent(&rng, FUSION_IV_MUTATION_CHANCE))
            value = min(value + 1 + RandomBelow(&rng, FUSION_IV_MUTATION_MAX), MAX_PER_STAT_IVS);
        SetMonData(child, MON_DATA_HP_IV + i, &value);
    }

    // Moves: each pooled move is inherited independently, at most MAX_MON_MOVES.
    count = BuildMovePool(parentA, parentB, movePool);
    inherited = 0;
    for (i = 0; i < count; i++)
    {
        if (RollPercent(&rng, FUSION_MOVE_INHERIT_CHANCE))
            movePool[inherited++] = movePool[i];
    }
    ShuffleU16(movePool, inherited, &rng);
    if (inherited > 0)
    {
        for (i = 0; i < MAX_MON_MOVES; i++)
            SetMonMoveSlot(child, i < inherited ? movePool[i] : MOVE_NONE, i);
    }
    else
    {
        GiveMonInitialMoveset(child);
    }

    // Main ability, then passives drawn from everything both parents carry.
    SetChildMainAbility(child, RollPercent(&rng, FUSION_MAIN_ABILITY_FROM_A_CHANCE) ? Fusion_GetMonMainAbility(parentA) : Fusion_GetMonMainAbility(parentB), &genetics);
    childMain = genetics.mainAbility != ABILITY_NONE ? genetics.mainAbility : GetMonAbility(child);
    count = BuildPassivePool(parentA, parentB, childMain, abilityPool);
    inherited = 0;
    for (i = 0; i < count; i++)
    {
        if (RollPercent(&rng, FUSION_PASSIVE_INHERIT_CHANCE))
            passiveOrder[inherited++] = i;
    }
    ShuffleU16(passiveOrder, inherited, &rng);
    for (i = 0; i < min(inherited, FUSION_MAX_PASSIVES); i++)
        genetics.passives[i] = abilityPool[passiveOrder[i]];

    // Third type
    count = BuildThirdTypePool(parentA, parentB, species, typePool);
    if (count > 0 && RollPercent(&rng, FUSION_THIRD_TYPE_CHANCE))
        genetics.extraType = typePool[RandomBelow(&rng, count)];

    Fusion_GetMonGenetics(parentA, &geneticsA);
    Fusion_GetMonGenetics(parentB, &geneticsB);
    genetics.generation = min(max(geneticsA.generation, geneticsB.generation) + 1, FUSION_MAX_GENERATION);
    genetics.flags = FUSION_FLAG_FUSED;
    Fusion_SetMonGenetics(child, &genetics);

    // Keep the species parent's ball and parent A's held item.
    value = GetMonData(speciesParent, MON_DATA_POKEBALL);
    SetMonData(child, MON_DATA_POKEBALL, &value);
    value = GetMonData(parentA, MON_DATA_HELD_ITEM);
    SetMonData(child, MON_DATA_HELD_ITEM, &value);

    CalculateMonStats(child);
}

enum FusionCheckResult Fusion_CheckParents(const struct FusionMonRef *refA, const struct FusionMonRef *refB)
{
    struct Pokemon parentA, parentB;
    enum Item itemB;

    if (!Fusion_ReadMonFromRef(refA, &parentA) || !Fusion_ReadMonFromRef(refB, &parentB) || Fusion_RefsEqual(refA, refB))
        return FUSION_CHECK_INVALID;
    if (!Fusion_CanCommitFusion(refA, refB))
        return FUSION_CHECK_LAST_PARTY_MON;
    if (ItemIsMail(GetMonData(&parentA, MON_DATA_HELD_ITEM)) || ItemIsMail(GetMonData(&parentB, MON_DATA_HELD_ITEM)))
        return FUSION_CHECK_HOLDS_MAIL;
    itemB = GetMonData(&parentB, MON_DATA_HELD_ITEM);
    if (itemB != ITEM_NONE && !CheckBagHasSpace(itemB, 1))
        return FUSION_CHECK_BAG_FULL;
    return FUSION_CHECK_OK;
}

// Generates the child and commits it. Parent B's held item goes to the bag.
bool32 Fusion_Fuse(const struct FusionMonRef *refA, const struct FusionMonRef *refB, u32 seed, struct Pokemon *child)
{
    struct Pokemon parentA, parentB;
    enum Item itemB;

    if (Fusion_CheckParents(refA, refB) != FUSION_CHECK_OK)
        return FALSE;

    Fusion_ReadMonFromRef(refA, &parentA);
    Fusion_ReadMonFromRef(refB, &parentB);
    Fusion_GenerateChild(&parentA, &parentB, seed, child);
    itemB = GetMonData(&parentB, MON_DATA_HELD_ITEM);

    if (!Fusion_CommitFusion(refA, refB, child))
        return FALSE;
    if (itemB != ITEM_NONE)
        AddBagItem(itemB, 1);
    return TRUE;
}
