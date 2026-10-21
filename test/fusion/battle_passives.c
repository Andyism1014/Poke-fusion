#include "global.h"
#include "fusion/fusion.h"
#include "test/battle.h"

// Phase 04b: passive abilities work alongside the main ability.

ASSUMPTIONS
{
    ASSUME(GetSpeciesAbility(SPECIES_WOBBUFFET, 0) == ABILITY_SHADOW_TAG);
    ASSUME(GetMoveType(MOVE_EARTHQUAKE) == TYPE_GROUND);
    ASSUME(GetMoveType(MOVE_WATER_GUN) == TYPE_WATER);
    ASSUME(MoveMakesContact(MOVE_SCRATCH));
}

SINGLE_BATTLE_TEST("Fusion passive: Intimidate activates on switch-in")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_INTIMIDATE); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_INTIMIDATE);
        ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_STATS_CHANGE, opponent);
    } THEN {
        EXPECT_EQ(opponent->statStages[STAT_ATK], DEFAULT_STAT_STAGE - 1);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: main ability and passives all activate on switch-in")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Ability(ABILITY_INTIMIDATE); Passives(ABILITY_DROUGHT, ABILITY_INTIMIDATE, ABILITY_ELECTRIC_SURGE); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_INTIMIDATE);
        ABILITY_POPUP(player, ABILITY_DROUGHT);
        ABILITY_POPUP(player, ABILITY_ELECTRIC_SURGE);
    } THEN {
        // A passive that duplicates the main ability doesn't activate twice.
        EXPECT_EQ(opponent->statStages[STAT_ATK], DEFAULT_STAT_STAGE - 1);
        EXPECT(gBattleWeather & B_WEATHER_SUN);
        EXPECT_EQ(gFieldTimers.terrain, B_TERRAIN_ELECTRIC);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Speed Boost raises Speed at the end of the turn")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_SPEED_BOOST); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { }
        TURN { }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_SPEED_BOOST);
    } THEN {
        EXPECT_EQ(player->statStages[STAT_SPEED], DEFAULT_STAT_STAGE + 2);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Levitate grants Ground immunity")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_LEVITATE); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_EARTHQUAKE); }
    } SCENE {
        NOT HP_BAR(player);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Mold Breaker ignores a Levitate passive")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_LEVITATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Ability(ABILITY_MOLD_BREAKER); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_EARTHQUAKE); }
    } SCENE {
        HP_BAR(player);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: a Mold Breaker passive ignores Levitate")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Ability(ABILITY_LEVITATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Passives(ABILITY_MOLD_BREAKER); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_EARTHQUAKE); }
    } SCENE {
        HP_BAR(player);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Gastro Acid suppresses passives")
{
    GIVEN {
        ASSUME(GetMoveEffect(MOVE_GASTRO_ACID) == EFFECT_GASTRO_ACID);
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_LEVITATE); Speed(1); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(100); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_GASTRO_ACID); }
        TURN { MOVE(opponent, MOVE_EARTHQUAKE); }
    } SCENE {
        MESSAGE("The opposing Wobbuffet used Earthquake!");
        HP_BAR(player);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Water Absorb absorbs Water moves")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_WATER_ABSORB); HP(100); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_WATER_GUN); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_WATER_ABSORB);
    } THEN {
        EXPECT_GT(player->hp, 100);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Rough Skin damages contact attackers")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_ROUGH_SKIN); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_SCRATCH); }
    } SCENE {
        HP_BAR(player);
        ABILITY_POPUP(player, ABILITY_ROUGH_SKIN);
        HP_BAR(opponent);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Technician boosts weak moves once, even if duplicated", s16 damage)
{
    u32 setup;
    PARAMETRIZE { setup = 0; } // None
    PARAMETRIZE { setup = 1; } // Passive only
    PARAMETRIZE { setup = 2; } // Main and passive
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) {
            if (setup == 2) Ability(ABILITY_TECHNICIAN);
            if (setup >= 1) Passives(ABILITY_TECHNICIAN);
        }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(1.5), results[1].damage);
        EXPECT_EQ(results[1].damage, results[2].damage);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Sturdy prevents OHKO moves")
{
    GIVEN {
        ASSUME(GetMoveEffect(MOVE_FISSURE) == EFFECT_OHKO);
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_STURDY); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_FISSURE); }
    } SCENE {
        MESSAGE("The opposing Wobbuffet used Fissure!");
        ABILITY_POPUP(player, ABILITY_STURDY);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Regenerator heals on switch-out, together with Natural Cure")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Ability(ABILITY_NATURAL_CURE); Passives(ABILITY_REGENERATOR); HP(1); Status1(STATUS1_POISON); }
        PLAYER(SPECIES_WYNAUT);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { SWITCH(player, 1); }
        TURN { SWITCH(player, 0); }
    } THEN {
        EXPECT_EQ(player->hp, player->maxHP / 3 + 1);
        EXPECT_EQ(player->status1, 0);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Sheer Force boosts moves with secondary effects", s16 damage)
{
    bool32 sheerForce;
    PARAMETRIZE { sheerForce = FALSE; }
    PARAMETRIZE { sheerForce = TRUE; }
    GIVEN {
        ASSUME(MoveIsAffectedBySheerForce(MOVE_EMBER));
        PLAYER(SPECIES_WOBBUFFET) { if (sheerForce) Passives(ABILITY_SHEER_FORCE); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_EMBER); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(1.3), results[1].damage);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: an opponent's passives work too")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { Passives(ABILITY_LEVITATE, ABILITY_INTIMIDATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_EARTHQUAKE); }
    } SCENE {
        ABILITY_POPUP(opponent, ABILITY_INTIMIDATE);
        NOT HP_BAR(opponent);
    }
}
