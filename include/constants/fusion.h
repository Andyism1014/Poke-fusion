#ifndef GUARD_CONSTANTS_FUSION_H
#define GUARD_CONSTANTS_FUSION_H

#define FUSION_MAX_PASSIVES 3 // Passive abilities per Pokémon, active alongside the main ability.

// Independent progress for each kind of passive event, so one event can't resume another's slot.
enum FusionPassiveCursor
{
    FUSION_PASSIVE_CURSOR_SWITCH_IN,
    FUSION_PASSIVE_CURSOR_END_TURN,
    FUSION_PASSIVE_CURSOR_MOVE_END,
    FUSION_PASSIVE_CURSOR_MOVE_END_ATTACKER,
    FUSION_PASSIVE_CURSOR_MOVE_END_FOES_FAINTED,
    FUSION_PASSIVE_CURSOR_COLOR_CHANGE,
    FUSION_PASSIVE_CURSOR_COUNT,
};

#endif // GUARD_CONSTANTS_FUSION_H
