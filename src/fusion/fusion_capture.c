#include "global.h"
#include "event_data.h"
#include "item.h"
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

void Fusion_NewGameInit(void)
{
    Fusion_SetCaptureMode(FUSION_CAPTURE_MODE_NEW_GAME);

    if (FUSION_NEW_GAME_GIVE_BOX_LINK)
        AddBagItem(ITEM_POKEMON_BOX_LINK, 1);

#if DEXNAV_ENABLED
    if (FUSION_NEW_GAME_GIVE_DEXNAV)
        FlagSet(DN_FLAG_DEXNAV_GET);
#endif
}
