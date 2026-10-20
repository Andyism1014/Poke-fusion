#ifndef GUARD_FUSION_H
#define GUARD_FUSION_H

#include "fusion/fusion_config.h"

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

// Called from NewGameInitData.
void Fusion_NewGameInit(void);

#endif // GUARD_FUSION_H
