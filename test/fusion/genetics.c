#include "global.h"
#include "item.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "fusion/fusion.h"
#include "test/test.h"

#define SEEDS 200

static void MakeParent(struct Pokemon *mon, enum Species species, u32 level, u32 ivs, const struct FusionGenetics *genetics)
{
    CreateMon(mon, species, level, 0, OTID_STRUCT_PLAYER_ID);
    for (u32 i = 0; i < NUM_STATS; i++)
        SetMonData(mon, MON_DATA_HP_IV + i, &ivs);
    if (genetics != NULL)
        Fusion_SetMonGenetics(mon, genetics);
}

static const struct FusionGenetics sGengarGenetics =
{
    .passives = {ABILITY_INTIMIDATE, ABILITY_SPEED_BOOST},
    .extraType = TYPE_STEEL,
    .generation = 4,
};

static void MakeCharizardAndGengar(struct Pokemon *a, struct Pokemon *b)
{
    MakeParent(a, SPECIES_CHARIZARD, 50, 31, NULL);
    MakeParent(b, SPECIES_GENGAR, 40, 0, &sGengarGenetics);
    SetMonMoveSlot(a, MOVE_FLAMETHROWER, 0);
    SetMonMoveSlot(a, MOVE_AIR_SLASH, 1);
    SetMonMoveSlot(a, MOVE_NONE, 2);
    SetMonMoveSlot(a, MOVE_NONE, 3);
    SetMonMoveSlot(b, MOVE_SHADOW_BALL, 0);
    SetMonMoveSlot(b, MOVE_SLUDGE_BOMB, 1);
    SetMonMoveSlot(b, MOVE_FLAMETHROWER, 2); // Shared with parent A.
    SetMonMoveSlot(b, MOVE_NONE, 3);
}

static bool32 IsSpeciesType(enum Species species, enum Type type)
{
    return gSpeciesInfo[species].types[0] == type || gSpeciesInfo[species].types[1] == type;
}

TEST("Fusion genetics: child species comes from either parent at about the configured rate")
{
    struct Pokemon a, b, child;
    u32 fromA = 0;

    MakeCharizardAndGengar(&a, &b);
    for (u32 seed = 0; seed < SEEDS; seed++)
    {
        enum Species species;

        Fusion_GenerateChild(&a, &b, seed, &child);
        species = GetMonData(&child, MON_DATA_SPECIES);
        EXPECT(species == SPECIES_CHARIZARD || species == SPECIES_GENGAR);
        fromA += (species == SPECIES_CHARIZARD);
    }
    EXPECT_GT(fromA, SEEDS * (FUSION_SPECIES_FROM_A_CHANCE - 15) / 100);
    EXPECT_LT(fromA, SEEDS * (FUSION_SPECIES_FROM_A_CHANCE + 15) / 100);
}

TEST("Fusion genetics: same parents and seed give the same child")
{
    struct Pokemon a, b, child1, child2;
    struct FusionGenetics g1, g2;

    MakeCharizardAndGengar(&a, &b);
    Fusion_GenerateChild(&a, &b, 1234, &child1);
    Fusion_GenerateChild(&a, &b, 1234, &child2);
    Fusion_GetMonGenetics(&child1, &g1);
    Fusion_GetMonGenetics(&child2, &g2);

    EXPECT_EQ(GetMonData(&child1, MON_DATA_PERSONALITY), GetMonData(&child2, MON_DATA_PERSONALITY));
    EXPECT_EQ(GetMonData(&child1, MON_DATA_SPECIES), GetMonData(&child2, MON_DATA_SPECIES));
    EXPECT_EQ(GetMonData(&child1, MON_DATA_MOVE1), GetMonData(&child2, MON_DATA_MOVE1));
    EXPECT_EQ(g1.passives[0], g2.passives[0]);
    EXPECT_EQ(g1.extraType, g2.extraType);
}

TEST("Fusion genetics: level, IVs and nature come from the parents")
{
    struct Pokemon a, b, child;
    u32 nature;

    MakeCharizardAndGengar(&a, &b);
    nature = NATURE_MODEST;
    SetMonData(&a, MON_DATA_HIDDEN_NATURE, &nature);
    nature = NATURE_TIMID;
    SetMonData(&b, MON_DATA_HIDDEN_NATURE, &nature);

    for (u32 seed = 0; seed < SEEDS; seed++)
    {
        Fusion_GenerateChild(&a, &b, seed, &child);
        EXPECT_EQ(GetMonData(&child, MON_DATA_LEVEL), 40);
        nature = GetMonData(&child, MON_DATA_HIDDEN_NATURE);
        EXPECT(nature == NATURE_MODEST || nature == NATURE_TIMID);
        for (u32 i = 0; i < NUM_STATS; i++)
        {
            u32 iv = GetMonData(&child, MON_DATA_HP_IV + i);
            // Parent A has 31 everywhere, parent B has 0 (+ up to FUSION_IV_MUTATION_MAX).
            EXPECT(iv == 31 || iv <= FUSION_IV_MUTATION_MAX);
        }
    }
}

TEST("Fusion genetics: moves come from the deduplicated parent pool")
{
    struct Pokemon a, b, child;

    MakeCharizardAndGengar(&a, &b);
    for (u32 seed = 0; seed < SEEDS; seed++)
    {
        u16 moves[MAX_MON_MOVES];
        bool32 fromPool = FALSE;

        Fusion_GenerateChild(&a, &b, seed, &child);
        for (u32 i = 0; i < MAX_MON_MOVES; i++)
            moves[i] = GetMonData(&child, MON_DATA_MOVE1 + i);
        for (u32 i = 0; i < MAX_MON_MOVES; i++)
        {
            for (u32 j = i + 1; j < MAX_MON_MOVES; j++)
                EXPECT(moves[i] == MOVE_NONE || moves[i] != moves[j]);
            if (moves[i] == MOVE_FLAMETHROWER || moves[i] == MOVE_AIR_SLASH
             || moves[i] == MOVE_SHADOW_BALL || moves[i] == MOVE_SLUDGE_BOMB)
                fromPool = TRUE;
        }
        // Either inherited moves, or the species' default level-up moves when nothing was inherited.
        EXPECT(fromPool || moves[0] != MOVE_NONE);
    }
}

TEST("Fusion genetics: passives come from both parents' abilities without duplicates")
{
    struct Pokemon a, b, child;
    u32 withPassives = 0;

    MakeCharizardAndGengar(&a, &b);
    for (u32 seed = 0; seed < SEEDS; seed++)
    {
        struct FusionGenetics g;
        enum Ability main;

        Fusion_GenerateChild(&a, &b, seed, &child);
        Fusion_GetMonGenetics(&child, &g);
        main = Fusion_GetMonMainAbility(&child);
        EXPECT(main == Fusion_GetMonMainAbility(&a) || main == Fusion_GetMonMainAbility(&b));

        for (u32 i = 0; i < FUSION_MAX_PASSIVES; i++)
        {
            enum Ability passive = g.passives[i];

            if (passive == ABILITY_NONE)
                continue;
            EXPECT_NE(passive, main);
            EXPECT(passive == ABILITY_INTIMIDATE || passive == ABILITY_SPEED_BOOST
                || passive == Fusion_GetMonMainAbility(&a) || passive == Fusion_GetMonMainAbility(&b));
            for (u32 j = i + 1; j < FUSION_MAX_PASSIVES; j++)
                EXPECT_NE(passive, g.passives[j]);
        }
        withPassives += (Fusion_CountPassives(&g) > 0);
    }
    EXPECT_GT(withPassives, SEEDS / 2);
}

TEST("Fusion genetics: a cross-species main ability is stored as an override")
{
    struct Pokemon a, b, child;
    bool32 sawOverride = FALSE;

    MakeCharizardAndGengar(&a, &b);
    for (u32 seed = 0; seed < SEEDS && !sawOverride; seed++)
    {
        struct FusionGenetics g;

        Fusion_GenerateChild(&a, &b, seed, &child);
        Fusion_GetMonGenetics(&child, &g);
        if (g.mainAbility != ABILITY_NONE)
        {
            sawOverride = TRUE;
            EXPECT_NE(g.mainAbility, GetAbilityBySpecies(GetMonData(&child, MON_DATA_SPECIES), GetMonData(&child, MON_DATA_ABILITY_NUM)));
            EXPECT_EQ(g.mainAbility, GetMonAbility(&child));
        }
    }
    EXPECT(sawOverride);
}

TEST("Fusion genetics: third type is a parent type the child species lacks")
{
    struct Pokemon a, b, child;
    u32 withThirdType = 0;

    MakeCharizardAndGengar(&a, &b);
    for (u32 seed = 0; seed < SEEDS; seed++)
    {
        struct FusionGenetics g;
        enum Species species;

        Fusion_GenerateChild(&a, &b, seed, &child);
        Fusion_GetMonGenetics(&child, &g);
        species = GetMonData(&child, MON_DATA_SPECIES);
        if (g.extraType == TYPE_NONE)
            continue;
        withThirdType++;
        EXPECT(!IsSpeciesType(species, g.extraType));
        EXPECT(IsSpeciesType(SPECIES_CHARIZARD, g.extraType) || IsSpeciesType(SPECIES_GENGAR, g.extraType)
            || g.extraType == TYPE_STEEL);
    }
    EXPECT_GT(withThirdType, SEEDS * (FUSION_THIRD_TYPE_CHANCE - 15) / 100);
    EXPECT_LT(withThirdType, SEEDS * (FUSION_THIRD_TYPE_CHANCE + 15) / 100);
}

TEST("Fusion genetics: no third type when the parents share the child's types")
{
    struct Pokemon a, b, child;
    struct FusionGenetics g;

    MakeParent(&a, SPECIES_WOBBUFFET, 30, 10, NULL);
    MakeParent(&b, SPECIES_WOBBUFFET, 30, 10, NULL);
    for (u32 seed = 0; seed < SEEDS; seed++)
    {
        Fusion_GenerateChild(&a, &b, seed, &child);
        Fusion_GetMonGenetics(&child, &g);
        EXPECT_EQ(g.extraType, TYPE_NONE);
    }
}

TEST("Fusion genetics: generation increases and the child is marked as fused")
{
    struct Pokemon a, b, child;
    struct FusionGenetics g;

    MakeCharizardAndGengar(&a, &b);
    Fusion_GenerateChild(&a, &b, 7, &child);
    Fusion_GetMonGenetics(&child, &g);
    EXPECT_EQ(g.generation, sGengarGenetics.generation + 1);
    EXPECT(g.flags & FUSION_FLAG_FUSED);
}

TEST("Fusion: fusing a party mon with a boxed mon returns the boxed parent's item")
{
    struct FusionMonRef refA = Fusion_PartyRef(0), refB = Fusion_BoxRef(4, 10);
    struct Pokemon b, child;
    u16 item = ITEM_LEFTOVERS;

    ZeroPlayerPartyMons();
    ResetPokemonStorageSystem();
    ClearBag();
    MakeParent(&gParties[B_TRAINER_PLAYER][0], SPECIES_CHARIZARD, 50, 31, NULL);
    CalculatePlayerPartyCount();
    MakeParent(&b, SPECIES_GENGAR, 40, 0, &sGengarGenetics);
    SetMonData(&b, MON_DATA_HELD_ITEM, &item);
    SetBoxMonAt(4, 10, &b.box);

    EXPECT_EQ(Fusion_CheckParents(&refA, &refB), FUSION_CHECK_OK);
    EXPECT(Fusion_Fuse(&refA, &refB, 99, &child));
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_PERSONALITY), GetMonData(&child, MON_DATA_PERSONALITY));
    EXPECT_EQ(GetBoxMonDataAt(4, 10, MON_DATA_SPECIES), SPECIES_NONE);
    EXPECT(CheckBagHasItem(ITEM_LEFTOVERS, 1));
}

TEST("Fusion: parents holding mail are rejected")
{
    struct FusionMonRef refA = Fusion_PartyRef(0), refB = Fusion_PartyRef(1);
    u16 mail = ITEM_ORANGE_MAIL;

    ZeroPlayerPartyMons();
    MakeParent(&gParties[B_TRAINER_PLAYER][0], SPECIES_CHARIZARD, 50, 31, NULL);
    MakeParent(&gParties[B_TRAINER_PLAYER][1], SPECIES_GENGAR, 40, 0, NULL);
    CalculatePlayerPartyCount();
    SetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_HELD_ITEM, &mail);

    EXPECT_EQ(Fusion_CheckParents(&refA, &refB), FUSION_CHECK_HOLDS_MAIL);
}

TEST("Fusion genetics: form and copy abilities are never inherited as passives")
{
    struct Pokemon a, b, child;

    EXPECT(!Fusion_CanBePassive(ABILITY_TRACE));
    EXPECT(!Fusion_CanBePassive(ABILITY_WONDER_GUARD));
    EXPECT(!Fusion_CanBePassive(ABILITY_STANCE_CHANGE));
    EXPECT(Fusion_CanBePassive(ABILITY_INTIMIDATE));

    MakeParent(&a, SPECIES_WOBBUFFET, 30, 10, NULL);
    MakeParent(&b, SPECIES_GARDEVOIR, 30, 10, NULL);
    SetMonData(&b, MON_DATA_ABILITY_NUM, &(u32){1});
    EXPECT_EQ(GetMonAbility(&b), ABILITY_TRACE);
    for (u32 seed = 0; seed < SEEDS; seed++)
    {
        struct FusionGenetics g;
        Fusion_GenerateChild(&a, &b, seed, &child);
        Fusion_GetMonGenetics(&child, &g);
        for (u32 i = 0; i < FUSION_MAX_PASSIVES; i++)
            EXPECT_NE(g.passives[i], ABILITY_TRACE);
    }
}

TEST("Fusion child counts as met at the lower of its parents' met levels")
{
    struct Pokemon parentA, parentB, child;
    u32 metA = 8, metB = 15;

    CreateMon(&parentA, SPECIES_WOBBUFFET, 40, 0, OTID_STRUCT_PLAYER_ID);
    CreateMon(&parentB, SPECIES_ZIGZAGOON, 45, 0, OTID_STRUCT_PLAYER_ID);
    SetMonData(&parentA, MON_DATA_MET_LEVEL, &metA);
    SetMonData(&parentB, MON_DATA_MET_LEVEL, &metB);
    Fusion_GenerateChild(&parentA, &parentB, 1234, &child);
    EXPECT_EQ(GetMonData(&child, MON_DATA_LEVEL), 40);
    EXPECT_EQ(GetMonData(&child, MON_DATA_MET_LEVEL), 8);
}
