#include "global.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "fusion/fusion.h"
#include "test/test.h"

static void GiveMonToParty(u32 slot, enum Species species)
{
    CreateMon(&gParties[B_TRAINER_PLAYER][slot], species, 20, 0, OTID_STRUCT_PLAYER_ID);
    CalculatePlayerPartyCount();
}

static void GiveMonToBox(u32 box, u32 slot, enum Species species)
{
    struct Pokemon mon;

    CreateMon(&mon, species, 20, 0, OTID_STRUCT_PLAYER_ID);
    SetBoxMonAt(box, slot, &mon.box);
}

static void SetupStorage(void)
{
    ZeroPlayerPartyMons();
    ResetPokemonStorageSystem();
}

static void MakeChild(struct Pokemon *child, enum Species species)
{
    CreateMon(child, species, 20, 0, OTID_STRUCT_PLAYER_ID);
}

TEST("Fusion storage: party + box, child replaces party parent and box parent is removed")
{
    struct FusionMonRef a = Fusion_PartyRef(0), b = Fusion_BoxRef(6, 3);
    struct Pokemon child;

    SetupStorage();
    GiveMonToParty(0, SPECIES_CHARIZARD);
    GiveMonToBox(6, 3, SPECIES_GENGAR);
    MakeChild(&child, SPECIES_CHARIZARD);

    EXPECT(Fusion_CommitFusion(&a, &b, &child));
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_CHARIZARD);
    EXPECT_EQ(GetBoxMonDataAt(6, 3, MON_DATA_SPECIES), SPECIES_NONE);
}

TEST("Fusion storage: box + box in different boxes")
{
    struct FusionMonRef a = Fusion_BoxRef(1, 0), b = Fusion_BoxRef(13, 29);
    struct Pokemon child;

    SetupStorage();
    GiveMonToParty(0, SPECIES_WOBBUFFET);
    GiveMonToBox(1, 0, SPECIES_GASTLY);
    GiveMonToBox(13, 29, SPECIES_PIKACHU);
    MakeChild(&child, SPECIES_PIKACHU);

    EXPECT(Fusion_CommitFusion(&a, &b, &child));
    EXPECT_EQ(GetBoxMonDataAt(1, 0, MON_DATA_SPECIES), SPECIES_PIKACHU);
    EXPECT_EQ(GetBoxMonDataAt(13, 29, MON_DATA_SPECIES), SPECIES_NONE);
}

TEST("Fusion storage: party + party compacts the party")
{
    struct FusionMonRef a = Fusion_PartyRef(2), b = Fusion_PartyRef(0);
    struct Pokemon child;

    SetupStorage();
    GiveMonToParty(0, SPECIES_GASTLY);
    GiveMonToParty(1, SPECIES_WOBBUFFET);
    GiveMonToParty(2, SPECIES_CHARIZARD);
    MakeChild(&child, SPECIES_GENGAR);

    EXPECT(Fusion_CommitFusion(&a, &b, &child));
    EXPECT_EQ(gPartiesCount[B_TRAINER_PLAYER], 2);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_WOBBUFFET);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_SPECIES), SPECIES_GENGAR);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][2], MON_DATA_SPECIES), SPECIES_NONE);
}

TEST("Fusion storage: the last party member cannot be consumed by a boxed parent")
{
    struct FusionMonRef a = Fusion_BoxRef(0, 0), b = Fusion_PartyRef(0);
    struct Pokemon child;

    SetupStorage();
    GiveMonToParty(0, SPECIES_WOBBUFFET);
    GiveMonToBox(0, 0, SPECIES_GASTLY);
    MakeChild(&child, SPECIES_GASTLY);

    EXPECT(!Fusion_CanCommitFusion(&a, &b));
    EXPECT(!Fusion_CommitFusion(&a, &b, &child));
    // Nothing was modified.
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_WOBBUFFET);
    EXPECT_EQ(GetBoxMonDataAt(0, 0, MON_DATA_SPECIES), SPECIES_GASTLY);
}

TEST("Fusion storage: invalid parents are rejected without changes")
{
    struct FusionMonRef a = Fusion_PartyRef(0), empty = Fusion_BoxRef(3, 3), egg = Fusion_BoxRef(3, 4);
    struct Pokemon child;

    SetupStorage();
    GiveMonToParty(0, SPECIES_WOBBUFFET);
    GiveMonToParty(1, SPECIES_GASTLY);
    GiveMonToBox(3, 4, SPECIES_PICHU);
    SetBoxMonDataAt(3, 4, MON_DATA_IS_EGG, &(u8){TRUE});
    MakeChild(&child, SPECIES_WOBBUFFET);

    EXPECT(!Fusion_CommitFusion(&a, &a, &child));
    EXPECT(!Fusion_CommitFusion(&a, &empty, &child));
    EXPECT(!Fusion_CommitFusion(&a, &egg, &child));
    EXPECT_EQ(gPartiesCount[B_TRAINER_PLAYER], 2);
    EXPECT_EQ(GetBoxMonDataAt(3, 4, MON_DATA_IS_EGG), TRUE);
}

TEST("Fusion storage: iterator visits party then boxes and skips empty slots and eggs")
{
    struct FusionMonRef ref = {.location = FUSION_LOC_NONE};

    SetupStorage();
    GiveMonToParty(0, SPECIES_WOBBUFFET);
    GiveMonToBox(0, 5, SPECIES_GASTLY);
    GiveMonToBox(0, 6, SPECIES_PICHU);
    SetBoxMonDataAt(0, 6, MON_DATA_IS_EGG, &(u8){TRUE});
    GiveMonToBox(13, 29, SPECIES_PIKACHU);

    EXPECT(Fusion_NextMaterialRef(&ref));
    EXPECT_EQ(ref.location, FUSION_LOC_PARTY);
    EXPECT_EQ(ref.slot, 0);
    EXPECT(Fusion_NextMaterialRef(&ref));
    EXPECT_EQ(ref.location, FUSION_LOC_BOX);
    EXPECT_EQ(ref.box, 0);
    EXPECT_EQ(ref.slot, 5);
    EXPECT(Fusion_NextMaterialRef(&ref));
    EXPECT_EQ(ref.box, 13);
    EXPECT_EQ(ref.slot, 29);
    EXPECT(!Fusion_NextMaterialRef(&ref));
}

TEST("Fusion storage: reading a boxed parent gives a temporary copy")
{
    struct FusionMonRef ref = Fusion_BoxRef(2, 2);
    struct Pokemon copy;

    SetupStorage();
    GiveMonToBox(2, 2, SPECIES_GASTLY);
    EXPECT(Fusion_ReadMonFromRef(&ref, &copy));
    SetMonData(&copy, MON_DATA_SPECIES, &(u16){SPECIES_GENGAR});
    EXPECT_EQ(GetBoxMonDataAt(2, 2, MON_DATA_SPECIES), SPECIES_GASTLY);
}
