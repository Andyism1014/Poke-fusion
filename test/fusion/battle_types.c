#include "global.h"
#include "fusion/fusion.h"
#include "test/battle.h"

// Phase 05: a persistent fusion third type works in battle.

ASSUMPTIONS
{
    ASSUME(GetSpeciesType(SPECIES_WOBBUFFET, 0) == TYPE_PSYCHIC);
    ASSUME(GetSpeciesType(SPECIES_WOBBUFFET, 1) == TYPE_PSYCHIC);
    ASSUME(GetMoveType(MOVE_SCRATCH) == TYPE_NORMAL);
    ASSUME(GetMoveType(MOVE_EMBER) == TYPE_FIRE);
    ASSUME(GetMoveType(MOVE_ROCK_THROW) == TYPE_ROCK);
    ASSUME(GetMoveType(MOVE_EARTHQUAKE) == TYPE_GROUND);
}

SINGLE_BATTLE_TEST("Fusion type: the third type is set when the Pokémon enters battle")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { FusionType(TYPE_GHOST); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { }
    } THEN {
        EXPECT_EQ(player->types[2], TYPE_GHOST);
        EXPECT_EQ(opponent->types[2], TYPE_MYSTERY);
    }
}

SINGLE_BATTLE_TEST("Fusion type: a Ghost third type grants Normal immunity")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { FusionType(TYPE_GHOST); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_SCRATCH); }
    } SCENE {
        MESSAGE("It doesn't affect Wobbuffet…");
        NOT HP_BAR(player);
    }
}

SINGLE_BATTLE_TEST("Fusion type: defensive multipliers stack up to 8x", s16 damage)
{
    enum Type fusionType;
    PARAMETRIZE { fusionType = TYPE_NONE; }
    PARAMETRIZE { fusionType = TYPE_BUG; }
    GIVEN {
        ASSUME(GetSpeciesType(SPECIES_CHARIZARD, 0) == TYPE_FIRE);
        ASSUME(GetSpeciesType(SPECIES_CHARIZARD, 1) == TYPE_FLYING);
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_CHARIZARD) { FusionType(fusionType); HP(999); MaxHP(999); }
    } WHEN {
        TURN { MOVE(player, MOVE_ROCK_THROW); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(2.0), results[1].damage);
    }
}

SINGLE_BATTLE_TEST("Fusion type: a third type matching a species type is not counted twice", s16 damage)
{
    enum Type fusionType;
    PARAMETRIZE { fusionType = TYPE_NONE; }
    PARAMETRIZE { fusionType = TYPE_FLYING; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_CHARIZARD) { FusionType(fusionType); HP(999); MaxHP(999); }
    } WHEN {
        TURN { MOVE(player, MOVE_ROCK_THROW); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_EQ(results[0].damage, results[1].damage);
    }
}

SINGLE_BATTLE_TEST("Fusion type: moves of the third type get STAB", s16 damage)
{
    enum Type fusionType;
    PARAMETRIZE { fusionType = TYPE_NONE; }
    PARAMETRIZE { fusionType = TYPE_FIRE; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { FusionType(fusionType); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_EMBER); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(1.5), results[1].damage);
    }
}

SINGLE_BATTLE_TEST("Fusion type: Roost removes a Flying third type for the turn")
{
    bool32 roost;
    PARAMETRIZE { roost = FALSE; }
    PARAMETRIZE { roost = TRUE; }
    GIVEN {
        ASSUME(GetMoveEffect(MOVE_ROOST) == EFFECT_ROOST);
        PLAYER(SPECIES_WOBBUFFET) { FusionType(TYPE_FLYING); HP(1); MaxHP(500); Speed(100); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); }
    } WHEN {
        TURN { MOVE(player, roost ? MOVE_ROOST : MOVE_CELEBRATE); MOVE(opponent, MOVE_EARTHQUAKE); }
    } SCENE {
        if (roost)
            HP_BAR(player);
        else
            MESSAGE("It doesn't affect Wobbuffet…");
    }
}

SINGLE_BATTLE_TEST("Fusion type: Soak removes the third type until the Pokémon switches out")
{
    GIVEN {
        ASSUME(GetMoveEffect(MOVE_SOAK) == EFFECT_SOAK);
        PLAYER(SPECIES_WOBBUFFET) { FusionType(TYPE_GHOST); }
        PLAYER(SPECIES_WYNAUT);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_SOAK); }
        TURN { MOVE(opponent, MOVE_SCRATCH); }
        TURN { SWITCH(player, 1); }
        TURN { SWITCH(player, 0); }
        TURN { MOVE(opponent, MOVE_SCRATCH); }
    } SCENE {
        MESSAGE("Wobbuffet transformed into the Water type!");
        MESSAGE("The opposing Wobbuffet used Scratch!");
        HP_BAR(player);
        MESSAGE("The opposing Wobbuffet used Scratch!");
        MESSAGE("It doesn't affect Wobbuffet…");
    }
}

SINGLE_BATTLE_TEST("Fusion type: Soak works on a pure Water type that has a third type")
{
    GIVEN {
        ASSUME(GetMoveEffect(MOVE_SOAK) == EFFECT_SOAK);
        ASSUME(GetSpeciesType(SPECIES_SQUIRTLE, 0) == TYPE_WATER);
        ASSUME(GetSpeciesType(SPECIES_SQUIRTLE, 1) == TYPE_WATER);
        PLAYER(SPECIES_SQUIRTLE) { FusionType(TYPE_GHOST); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_SOAK); }
    } SCENE {
        MESSAGE("Squirtle transformed into the Water type!");
    } THEN {
        EXPECT_EQ(player->types[2], TYPE_MYSTERY);
    }
}

SINGLE_BATTLE_TEST("Fusion type: Forest's Curse replaces the third type until switching out")
{
    GIVEN {
        ASSUME(GetMoveEffect(MOVE_FORESTS_CURSE) == EFFECT_THIRD_TYPE);
        PLAYER(SPECIES_WOBBUFFET) { FusionType(TYPE_GHOST); }
        PLAYER(SPECIES_WYNAUT);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_FORESTS_CURSE); }
        TURN { SWITCH(player, 1); }
        TURN { SWITCH(player, 0); }
    } SCENE {
        MESSAGE("The opposing Wobbuffet used Forest's Curse!");
    } THEN {
        EXPECT_EQ(player->types[2], TYPE_GHOST);
    }
}

SINGLE_BATTLE_TEST("Fusion type: Transform copies the third type")
{
    GIVEN {
        ASSUME(GetMoveEffect(MOVE_TRANSFORM) == EFFECT_TRANSFORM);
        PLAYER(SPECIES_WOBBUFFET) { FusionType(TYPE_GHOST); Speed(1); }
        OPPONENT(SPECIES_DITTO) { Speed(100); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_TRANSFORM); }
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        MESSAGE("Wobbuffet used Scratch!");
        MESSAGE("It doesn't affect the opposing Ditto…");
    }
}

SINGLE_BATTLE_TEST("Fusion type: Terastallization overrides the third type")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { FusionType(TYPE_GHOST); TeraType(TYPE_NORMAL); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE, gimmick: GIMMICK_TERA); MOVE(opponent, MOVE_SCRATCH); }
    } SCENE {
        MESSAGE("The opposing Wobbuffet used Scratch!");
        HP_BAR(player);
    }
}

AI_SINGLE_BATTLE_TEST("Fusion type: AI avoids Normal moves against a Ghost third type")
{
    GIVEN {
        AI_FLAGS(AI_FLAG_CHECK_BAD_MOVE | AI_FLAG_CHECK_VIABILITY | AI_FLAG_TRY_TO_FAINT);
        PLAYER(SPECIES_WOBBUFFET) { FusionType(TYPE_GHOST); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SCRATCH, MOVE_EMBER); }
    } WHEN {
        TURN { EXPECT_MOVE(opponent, MOVE_EMBER); }
    }
}
