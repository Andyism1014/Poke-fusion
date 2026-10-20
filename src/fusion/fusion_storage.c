#include "global.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "fusion/fusion.h"

// Unified access to fusion materials in the party and in every PC box.
// Parents are always read into temporary copies; party and boxes are only
// modified by Fusion_CommitFusion once every check has passed.

struct FusionMonRef Fusion_PartyRef(u32 slot)
{
    struct FusionMonRef ref = {.location = FUSION_LOC_PARTY, .slot = slot};
    return ref;
}

struct FusionMonRef Fusion_BoxRef(u32 box, u32 slot)
{
    struct FusionMonRef ref = {.location = FUSION_LOC_BOX, .box = box, .slot = slot};
    return ref;
}

bool32 Fusion_RefsEqual(const struct FusionMonRef *a, const struct FusionMonRef *b)
{
    if (a->location != b->location || a->slot != b->slot)
        return FALSE;
    return a->location != FUSION_LOC_BOX || a->box == b->box;
}

struct BoxPokemon *Fusion_GetBoxMonFromRef(const struct FusionMonRef *ref)
{
    switch (ref->location)
    {
    case FUSION_LOC_PARTY:
        if (ref->slot < PARTY_SIZE)
            return &gParties[B_TRAINER_PLAYER][ref->slot].box;
        break;
    case FUSION_LOC_BOX:
        if (ref->box < TOTAL_BOXES_COUNT && ref->slot < IN_BOX_COUNT)
            return GetBoxedMonPtr(ref->box, ref->slot);
        break;
    }
    return NULL;
}

static bool32 IsMaterialBoxMon(struct BoxPokemon *boxMon)
{
    return boxMon != NULL
        && GetBoxMonData(boxMon, MON_DATA_SANITY_HAS_SPECIES)
        && !GetBoxMonData(boxMon, MON_DATA_SANITY_IS_BAD_EGG)
        && !GetBoxMonData(boxMon, MON_DATA_IS_EGG);
}

bool32 Fusion_IsMaterialRef(const struct FusionMonRef *ref)
{
    return IsMaterialBoxMon(Fusion_GetBoxMonFromRef(ref));
}

bool32 Fusion_ReadMonFromRef(const struct FusionMonRef *ref, struct Pokemon *dst)
{
    if (!Fusion_IsMaterialRef(ref))
        return FALSE;

    if (ref->location == FUSION_LOC_PARTY)
        *dst = gParties[B_TRAINER_PLAYER][ref->slot];
    else
        BoxMonToMon(Fusion_GetBoxMonFromRef(ref), dst);
    return TRUE;
}

static u32 CountUsablePartyMonsExcept(const struct FusionMonRef *excluded)
{
    u32 i, count = 0;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct FusionMonRef ref = Fusion_PartyRef(i);

        if (!Fusion_RefsEqual(&ref, excluded) && Fusion_IsMaterialRef(&ref))
            count++;
    }
    return count;
}

bool32 Fusion_CanCommitFusion(const struct FusionMonRef *parentA, const struct FusionMonRef *parentB)
{
    if (!Fusion_IsMaterialRef(parentA) || !Fusion_IsMaterialRef(parentB))
        return FALSE;
    if (Fusion_RefsEqual(parentA, parentB))
        return FALSE;
    // The party must keep at least one Pokémon: the child takes parent A's slot,
    // so this only matters when A is boxed and B is the last party member.
    if (parentA->location == FUSION_LOC_BOX && parentB->location == FUSION_LOC_PARTY
     && CountUsablePartyMonsExcept(parentB) == 0)
        return FALSE;
    return TRUE;
}

// The child replaces parent A in place and parent B is removed.
bool32 Fusion_CommitFusion(const struct FusionMonRef *parentA, const struct FusionMonRef *parentB, struct Pokemon *child)
{
    if (!Fusion_CanCommitFusion(parentA, parentB) || !GetMonData(child, MON_DATA_SANITY_HAS_SPECIES))
        return FALSE;

    if (parentA->location == FUSION_LOC_PARTY)
    {
        gParties[B_TRAINER_PLAYER][parentA->slot] = *child;
        CalculateMonStats(&gParties[B_TRAINER_PLAYER][parentA->slot]);
    }
    else
    {
        SetBoxMonAt(parentA->box, parentA->slot, &child->box);
    }

    if (parentB->location == FUSION_LOC_PARTY)
    {
        ZeroMonData(&gParties[B_TRAINER_PLAYER][parentB->slot]);
        CompactPartySlots();
        CalculatePlayerPartyCount();
    }
    else
    {
        ZeroBoxMonAt(parentB->box, parentB->slot);
    }
    return TRUE;
}

// Iterates over every material: party first, then boxes in order.
// Start with ref->location == FUSION_LOC_NONE. Returns FALSE when done.
bool32 Fusion_NextMaterialRef(struct FusionMonRef *ref)
{
    struct FusionMonRef next = *ref;

    if (next.location == FUSION_LOC_NONE)
    {
        next = Fusion_PartyRef(0);
    }
    else
    {
        next.slot++;
    }

    while (TRUE)
    {
        if (next.location == FUSION_LOC_PARTY && next.slot >= PARTY_SIZE)
            next = Fusion_BoxRef(0, 0);
        if (next.location == FUSION_LOC_BOX && next.slot >= IN_BOX_COUNT)
        {
            next.box++;
            next.slot = 0;
        }
        if (next.location == FUSION_LOC_BOX && next.box >= TOTAL_BOXES_COUNT)
        {
            ref->location = FUSION_LOC_NONE;
            return FALSE;
        }
        if (Fusion_IsMaterialRef(&next))
        {
            *ref = next;
            return TRUE;
        }
        next.slot++;
    }
}
