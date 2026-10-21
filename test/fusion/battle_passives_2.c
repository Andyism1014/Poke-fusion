#include "global.h"
#include "fusion/fusion.h"
#include "test/battle.h"

// Phase 04c: commonly used abilities work as passives.

ASSUMPTIONS
{
    ASSUME(GetSpeciesAbility(SPECIES_WOBBUFFET, 0) == ABILITY_SHADOW_TAG);
    ASSUME(GetSpeciesType(SPECIES_WOBBUFFET, 0) == TYPE_PSYCHIC);
    ASSUME(GetMoveCategory(MOVE_SCRATCH) == DAMAGE_CATEGORY_PHYSICAL);
    ASSUME(GetMoveType(MOVE_SCRATCH) == TYPE_NORMAL);
    ASSUME(GetMoveType(MOVE_EMBER) == TYPE_FIRE);
}

// ---- Damage formula ----

SINGLE_BATTLE_TEST("Fusion passive: Huge Power doubles physical damage", s16 damage)
{
    bool32 passive;
    PARAMETRIZE { passive = FALSE; }
    PARAMETRIZE { passive = TRUE; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { if (passive) Passives(ABILITY_HUGE_POWER); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(2.0), results[1].damage);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: attack modifiers of main ability and passives stack", s16 damage)
{
    u32 setup;
    PARAMETRIZE { setup = 0; }
    PARAMETRIZE { setup = 1; } // Huge Power main, Hustle passive
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { if (setup) { Ability(ABILITY_HUGE_POWER); Passives(ABILITY_HUSTLE); } }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH, hit: TRUE); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(3.0), results[1].damage);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Guts boosts Attack and ignores the burn drop", s16 damage)
{
    bool32 passive;
    PARAMETRIZE { passive = FALSE; }
    PARAMETRIZE { passive = TRUE; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Status1(STATUS1_BURN); if (passive) Passives(ABILITY_GUTS); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(3.0), results[1].damage);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Thick Fat halves Fire damage", s16 damage)
{
    bool32 passive;
    PARAMETRIZE { passive = FALSE; }
    PARAMETRIZE { passive = TRUE; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { if (passive) Passives(ABILITY_THICK_FAT); }
    } WHEN {
        TURN { MOVE(player, MOVE_EMBER); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(0.5), results[1].damage);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Fur Coat doubles Defense", s16 damage)
{
    bool32 passive;
    PARAMETRIZE { passive = FALSE; }
    PARAMETRIZE { passive = TRUE; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { if (passive) Passives(ABILITY_FUR_COAT); }
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(0.5), results[1].damage);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Multiscale halves damage at full HP and stacks with Ice Scales", s16 damage)
{
    u32 setup;
    PARAMETRIZE { setup = 0; }
    PARAMETRIZE { setup = 1; } // Multiscale passive
    PARAMETRIZE { setup = 2; } // Multiscale + Ice Scales passives
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) {
            if (setup == 1) Passives(ABILITY_MULTISCALE);
            if (setup == 2) Passives(ABILITY_MULTISCALE, ABILITY_ICE_SCALES);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_EMBER); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(0.5), results[1].damage);
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(0.25), results[2].damage);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Filter reduces super effective damage", s16 damage)
{
    bool32 passive;
    PARAMETRIZE { passive = FALSE; }
    PARAMETRIZE { passive = TRUE; }
    GIVEN {
        ASSUME(GetMoveType(MOVE_BITE) == TYPE_DARK);
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { if (passive) Passives(ABILITY_FILTER); }
    } WHEN {
        TURN { MOVE(player, MOVE_BITE); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(0.75), results[1].damage);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Tinted Lens doubles not very effective damage", s16 damage)
{
    bool32 passive;
    PARAMETRIZE { passive = FALSE; }
    PARAMETRIZE { passive = TRUE; }
    GIVEN {
        ASSUME(GetMoveType(MOVE_CONFUSION) == TYPE_PSYCHIC);
        PLAYER(SPECIES_WOBBUFFET) { if (passive) Passives(ABILITY_TINTED_LENS); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_CONFUSION); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(2.0), results[1].damage);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Adaptability raises STAB to 2x", s16 damage)
{
    bool32 passive;
    PARAMETRIZE { passive = FALSE; }
    PARAMETRIZE { passive = TRUE; }
    GIVEN {
        ASSUME(GetMoveType(MOVE_CONFUSION) == TYPE_PSYCHIC);
        PLAYER(SPECIES_WOBBUFFET) { if (passive) Passives(ABILITY_ADAPTABILITY); }
        OPPONENT(SPECIES_RATTATA);
    } WHEN {
        TURN { MOVE(player, MOVE_CONFUSION); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(1.33), results[1].damage);
    }
}

DOUBLE_BATTLE_TEST("Fusion passive: Friend Guard reduces damage to allies", s16 damage)
{
    bool32 passive;
    PARAMETRIZE { passive = FALSE; }
    PARAMETRIZE { passive = TRUE; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        PLAYER(SPECIES_WYNAUT);
        OPPONENT(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WYNAUT) { if (passive) Passives(ABILITY_FRIEND_GUARD); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_SCRATCH, target: opponentLeft); }
    } SCENE {
        HP_BAR(opponentLeft, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(0.75), results[1].damage);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Super Luck raises the critical hit ratio")
{
    PASSES_RANDOMLY(1, 8, RNG_CRITICAL_HIT);
    GIVEN {
        WITH_CONFIG(B_CRIT_CHANCE, GEN_7);
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_SUPER_LUCK); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_SCRATCH, player);
        MESSAGE("A critical hit!");
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Shell Armor prevents critical hits")
{
    GIVEN {
        ASSUME(MoveAlwaysCrits(MOVE_FROST_BREATH));
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { Passives(ABILITY_SHELL_ARMOR); }
    } WHEN {
        TURN { MOVE(player, MOVE_FROST_BREATH); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_FROST_BREATH, player);
        NOT MESSAGE("A critical hit!");
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Compound Eyes raises accuracy")
{
    PASSES_RANDOMLY(91, 100, RNG_ACCURACY);
    GIVEN {
        ASSUME(GetMoveAccuracy(MOVE_THUNDER) == 70);
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_COMPOUND_EYES); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_THUNDER); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_THUNDER, player);
        HP_BAR(opponent);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Sand Veil lowers accuracy in a sandstorm")
{
    PASSES_RANDOMLY(4, 5, RNG_ACCURACY);
    GIVEN {
        ASSUME(GetMoveAccuracy(MOVE_POUND) == 100);
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_SAND_VEIL); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_SANDSTORM); }
        TURN { MOVE(opponent, MOVE_POUND); }
    } SCENE {
        HP_BAR(player);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: No Guard makes moves always hit")
{
    GIVEN {
        ASSUME(GetMoveAccuracy(MOVE_FOCUS_BLAST) == 70);
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_NO_GUARD); }
        OPPONENT(SPECIES_RATTATA);
    } WHEN {
        TURN { MOVE(player, MOVE_FOCUS_BLAST, hit: FALSE); }
    } SCENE {
        HP_BAR(opponent);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Unaware ignores the attacker's stat stages", s16 damage)
{
    bool32 passive;
    PARAMETRIZE { passive = FALSE; }
    PARAMETRIZE { passive = TRUE; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Speed(2); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); if (passive) Passives(ABILITY_UNAWARE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SWORDS_DANCE); }
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[1].damage, Q_4_12(2.0), results[0].damage);
    }
}

// ---- Speed and priority ----

SINGLE_BATTLE_TEST("Fusion passive: Swift Swim doubles Speed in rain")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Speed(100); Passives(ABILITY_SWIFT_SWIM); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(150); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_RAIN_DANCE); }
        TURN { MOVE(player, MOVE_SCRATCH); MOVE(opponent, MOVE_SCRATCH); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_RAIN_DANCE, opponent);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_SCRATCH, player);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_SCRATCH, opponent);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Quick Feet boosts Speed when statused")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Speed(100); Status1(STATUS1_POISON); Passives(ABILITY_QUICK_FEET); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(140); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, player);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, opponent);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Prankster gives status moves priority")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Speed(1); Passives(ABILITY_PRANKSTER); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(100); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, player);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, opponent);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Gale Wings gives Flying moves priority at full HP")
{
    GIVEN {
        ASSUME(GetMoveType(MOVE_PECK) == TYPE_FLYING);
        PLAYER(SPECIES_WOBBUFFET) { Speed(1); Passives(ABILITY_GALE_WINGS); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(100); }
    } WHEN {
        TURN { MOVE(player, MOVE_PECK); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_PECK, player);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, opponent);
    }
}

// ---- Move type changes ----

SINGLE_BATTLE_TEST("Fusion passive: Pixilate turns Normal moves into boosted Fairy moves")
{
    GIVEN {
        ASSUME(GetSpeciesType(SPECIES_GASTLY, 0) == TYPE_GHOST);
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_PIXILATE); }
        OPPONENT(SPECIES_GASTLY);
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_SCRATCH, player);
        HP_BAR(opponent);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: -ate passives get the power boost", s16 damage)
{
    bool32 passive;
    PARAMETRIZE { passive = FALSE; }
    PARAMETRIZE { passive = TRUE; }
    GIVEN {
        WITH_CONFIG(B_ATE_MULTIPLIER, GEN_7);
        PLAYER(SPECIES_WOBBUFFET) { if (passive) Passives(ABILITY_NORMALIZE); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(1.2), results[1].damage);
    }
}

// ---- Status and stat drop immunities ----

SINGLE_BATTLE_TEST("Fusion passive: status immunities block their status")
{
    enum Ability ability;
    enum Move move;
    PARAMETRIZE { ability = ABILITY_INSOMNIA;     move = MOVE_HYPNOSIS; }
    PARAMETRIZE { ability = ABILITY_VITAL_SPIRIT; move = MOVE_HYPNOSIS; }
    PARAMETRIZE { ability = ABILITY_LIMBER;       move = MOVE_THUNDER_WAVE; }
    PARAMETRIZE { ability = ABILITY_IMMUNITY;     move = MOVE_TOXIC; }
    PARAMETRIZE { ability = ABILITY_WATER_VEIL;   move = MOVE_WILL_O_WISP; }
    PARAMETRIZE { ability = ABILITY_WATER_BUBBLE; move = MOVE_WILL_O_WISP; }
    PARAMETRIZE { ability = ABILITY_PURIFYING_SALT; move = MOVE_TOXIC; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ability); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, move, hit: TRUE); }
    } SCENE {
        ABILITY_POPUP(player, ability);
    } THEN {
        EXPECT_EQ(player->status1, 0);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Own Tempo prevents confusion")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_OWN_TEMPO); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_CONFUSE_RAY); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_OWN_TEMPO);
    } THEN {
        EXPECT_EQ((u32)player->volatiles.confusionTimer, 0);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Inner Focus prevents flinching")
{
    GIVEN {
        ASSUME(GetMoveEffect(MOVE_FAKE_OUT) == EFFECT_FIRST_TURN_ONLY);
        PLAYER(SPECIES_WOBBUFFET) { Speed(1); Passives(ABILITY_INNER_FOCUS); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(100); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_FAKE_OUT); MOVE(player, MOVE_CELEBRATE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_FAKE_OUT, opponent);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, player);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Oblivious prevents Taunt")
{
    GIVEN {
        WITH_CONFIG(B_OBLIVIOUS_TAUNT, GEN_6);
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_OBLIVIOUS); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_TAUNT); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_OBLIVIOUS);
    } THEN {
        EXPECT_EQ((u32)player->volatiles.tauntTimer, 0);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: stat drop blockers")
{
    enum Ability ability;
    enum Move move;
    enum Stat stat;
    PARAMETRIZE { ability = ABILITY_CLEAR_BODY;      move = MOVE_GROWL;        stat = STAT_ATK; }
    PARAMETRIZE { ability = ABILITY_WHITE_SMOKE;     move = MOVE_TAIL_WHIP;    stat = STAT_DEF; }
    PARAMETRIZE { ability = ABILITY_FULL_METAL_BODY; move = MOVE_SCARY_FACE;   stat = STAT_SPEED; }
    PARAMETRIZE { ability = ABILITY_HYPER_CUTTER;    move = MOVE_GROWL;        stat = STAT_ATK; }
    PARAMETRIZE { ability = ABILITY_BIG_PECKS;       move = MOVE_TAIL_WHIP;    stat = STAT_DEF; }
    PARAMETRIZE { ability = ABILITY_KEEN_EYE;        move = MOVE_SAND_ATTACK;  stat = STAT_ACC; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ability); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, move); }
    } SCENE {
        ABILITY_POPUP(player, ability);
    } THEN {
        EXPECT_EQ(player->statStages[stat], DEFAULT_STAT_STAGE);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Clear Body blocks Intimidate")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_CLEAR_BODY); }
        OPPONENT(SPECIES_WOBBUFFET) { Ability(ABILITY_INTIMIDATE); }
    } WHEN {
        TURN { }
    } SCENE {
        ABILITY_POPUP(opponent, ABILITY_INTIMIDATE);
        ABILITY_POPUP(player, ABILITY_CLEAR_BODY);
    } THEN {
        EXPECT_EQ(player->statStages[STAT_ATK], DEFAULT_STAT_STAGE);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Shield Dust blocks secondary effects")
{
    GIVEN {
        ASSUME(MoveHasAdditionalEffectWithChance(MOVE_NUZZLE, MOVE_EFFECT_PARALYSIS, 100) == TRUE);
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_SHIELD_DUST); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_NUZZLE); }
    } SCENE {
        HP_BAR(player);
    } THEN {
        EXPECT_EQ(player->status1, 0);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Magic Guard prevents indirect damage")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_MAGIC_GUARD); Status1(STATUS1_POISON); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_SANDSTORM); }
    } THEN {
        EXPECT_EQ(player->hp, player->maxHP);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Rock Head prevents recoil")
{
    GIVEN {
        ASSUME(GetMoveEffect(MOVE_DOUBLE_EDGE) == EFFECT_RECOIL);
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_ROCK_HEAD); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_DOUBLE_EDGE); }
    } SCENE {
        HP_BAR(opponent);
    } THEN {
        EXPECT_EQ(player->hp, player->maxHP);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Soundproof and Overcoat block moves")
{
    enum Ability ability;
    enum Move move;
    PARAMETRIZE { ability = ABILITY_SOUNDPROOF; move = MOVE_GROWL; }
    PARAMETRIZE { ability = ABILITY_OVERCOAT; move = MOVE_SPORE; }
    PARAMETRIZE { ability = ABILITY_BULLETPROOF; move = MOVE_SHADOW_BALL; }
    GIVEN {
        ASSUME(IsSoundMove(MOVE_GROWL));
        ASSUME(IsPowderMove(MOVE_SPORE));
        ASSUME(IsBallisticMove(MOVE_SHADOW_BALL));
        PLAYER(SPECIES_WOBBUFFET) { Passives(ability); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, move); }
    } SCENE {
        NOT ANIMATION(ANIM_TYPE_MOVE, move, opponent);
    } THEN {
        EXPECT_EQ(player->statStages[STAT_ATK], DEFAULT_STAT_STAGE);
        EXPECT_EQ(player->status1, 0);
        EXPECT_EQ(player->hp, player->maxHP);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Queenly Majesty blocks priority moves")
{
    GIVEN {
        ASSUME(GetMovePriority(MOVE_QUICK_ATTACK) > 0);
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_QUEENLY_MAJESTY); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_QUICK_ATTACK); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_QUEENLY_MAJESTY);
        NOT HP_BAR(player);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Leaf Guard prevents status in sun")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_LEAF_GUARD); }
        OPPONENT(SPECIES_WOBBUFFET) { Ability(ABILITY_DROUGHT); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_THUNDER_WAVE); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_LEAF_GUARD);
    } THEN {
        EXPECT_EQ(player->status1, 0);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Damp prevents explosions")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_DAMP); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_EXPLOSION); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_DAMP);
        NOT HP_BAR(player);
    }
}

// ---- Stat change abilities ----

SINGLE_BATTLE_TEST("Fusion passive: Contrary and Simple change stat changes")
{
    enum Ability ability;
    s32 expected;
    PARAMETRIZE { ability = ABILITY_CONTRARY; expected = DEFAULT_STAT_STAGE - 2; }
    PARAMETRIZE { ability = ABILITY_SIMPLE;   expected = DEFAULT_STAT_STAGE + 4; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ability); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_SWORDS_DANCE); }
    } THEN {
        EXPECT_EQ(player->statStages[STAT_ATK], expected);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Defiant and Competitive react to stat drops")
{
    enum Ability ability;
    enum Stat stat;
    PARAMETRIZE { ability = ABILITY_DEFIANT;     stat = STAT_ATK; }
    PARAMETRIZE { ability = ABILITY_COMPETITIVE; stat = STAT_SPATK; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ability); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_TAIL_WHIP); }
    } SCENE {
        ABILITY_POPUP(player, ability);
    } THEN {
        EXPECT_EQ(player->statStages[STAT_DEF], DEFAULT_STAT_STAGE - 1);
        EXPECT_EQ(player->statStages[stat], DEFAULT_STAT_STAGE + 2);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Moxie raises Attack after a KO")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_MOXIE); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); }
        OPPONENT(SPECIES_WYNAUT);
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); SEND_OUT(opponent, 1); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_MOXIE);
    } THEN {
        EXPECT_EQ(player->statStages[STAT_ATK], DEFAULT_STAT_STAGE + 1);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: on-hit stat abilities (Justified, Stamina, Weak Armor)")
{
    enum Ability ability;
    enum Move move;
    PARAMETRIZE { ability = ABILITY_JUSTIFIED;  move = MOVE_BITE; }
    PARAMETRIZE { ability = ABILITY_STAMINA;    move = MOVE_SCRATCH; }
    PARAMETRIZE { ability = ABILITY_WEAK_ARMOR; move = MOVE_SCRATCH; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ability); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, move); }
    } SCENE {
        ABILITY_POPUP(player, ability);
    } THEN {
        if (ability == ABILITY_JUSTIFIED)
            EXPECT_EQ(player->statStages[STAT_ATK], DEFAULT_STAT_STAGE + 1);
        else if (ability == ABILITY_STAMINA)
            EXPECT_EQ(player->statStages[STAT_DEF], DEFAULT_STAT_STAGE + 1);
        else
            EXPECT_EQ(player->statStages[STAT_DEF], DEFAULT_STAT_STAGE - 1);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Anger Point maxes Attack after a critical hit")
{
    GIVEN {
        ASSUME(MoveAlwaysCrits(MOVE_FROST_BREATH));
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_ANGER_POINT); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_FROST_BREATH); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_ANGER_POINT);
    } THEN {
        EXPECT_EQ(player->statStages[STAT_ATK], MAX_STAT_STAGE);
    }
}

// ---- End of turn ----

SINGLE_BATTLE_TEST("Fusion passive: Rain Dish and Dry Skin both heal in rain")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { HP(100); MaxHP(400); Passives(ABILITY_RAIN_DISH, ABILITY_DRY_SKIN); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_RAIN_DANCE); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_RAIN_DISH);
        ABILITY_POPUP(player, ABILITY_DRY_SKIN);
    } THEN {
        EXPECT_EQ(player->hp, 100 + 400 / 16 + 400 / 8);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Ice Body heals in snow")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { HP(100); MaxHP(400); Passives(ABILITY_ICE_BODY); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_SNOWSCAPE); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_ICE_BODY);
    } THEN {
        EXPECT_EQ(player->hp, 100 + 400 / 16);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Solar Power hurts in sun")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { MaxHP(400); HP(400); Passives(ABILITY_SOLAR_POWER); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_SUNNY_DAY); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_SOLAR_POWER);
    } THEN {
        EXPECT_EQ(player->hp, 400 - 400 / 8);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Poison Heal heals instead of poison damage")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { HP(100); MaxHP(400); Status1(STATUS1_POISON); Passives(ABILITY_POISON_HEAL); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_POISON_HEAL);
    } THEN {
        EXPECT_EQ(player->hp, 100 + 400 / 8);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Hydration cures status in rain")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Status1(STATUS1_POISON); Passives(ABILITY_HYDRATION); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_RAIN_DANCE); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_HYDRATION);
    } THEN {
        EXPECT_EQ(player->status1, 0);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Shed Skin and Speed Boost both activate")
{
    PASSES_RANDOMLY(33, 100, RNG_SHED_SKIN);
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Status1(STATUS1_BURN); Passives(ABILITY_SHED_SKIN, ABILITY_SPEED_BOOST); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_SHED_SKIN);
    }
}

// ---- Misc ----

SINGLE_BATTLE_TEST("Fusion passive: Serene Grace doubles secondary effect chances")
{
    PASSES_RANDOMLY(20, 100, RNG_SECONDARY_EFFECT);
    GIVEN {
        ASSUME(MoveHasAdditionalEffectWithChance(MOVE_EMBER, MOVE_EFFECT_BURN, 10) == TRUE);
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_SERENE_GRACE); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_EMBER); }
    } SCENE {
        STATUS_ICON(opponent, burn: TRUE);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Skill Link makes multi-hit moves hit 5 times")
{
    GIVEN {
        ASSUME(IsMultiHitMove(MOVE_BULLET_SEED));
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_SKILL_LINK); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_BULLET_SEED); }
    } SCENE {
        MESSAGE("The Pokémon was hit 5 times!");
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Rough Skin and Iron Barbs both damage contact attackers")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_ROUGH_SKIN, ABILITY_IRON_BARBS); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_SCRATCH); }
    } SCENE {
        HP_BAR(player);
        ABILITY_POPUP(player, ABILITY_ROUGH_SKIN);
        HP_BAR(opponent);
        ABILITY_POPUP(player, ABILITY_IRON_BARBS);
        HP_BAR(opponent);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: contact status abilities")
{
    enum Ability ability;
    u32 status, tag;
    PARAMETRIZE { ability = ABILITY_STATIC;       status = STATUS1_PARALYSIS; tag = RNG_STATIC; }
    PARAMETRIZE { ability = ABILITY_FLAME_BODY;   status = STATUS1_BURN;      tag = RNG_FLAME_BODY; }
    PARAMETRIZE { ability = ABILITY_POISON_POINT; status = STATUS1_POISON;    tag = RNG_POISON_POINT; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ability); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_SCRATCH, WITH_RNG(tag, TRUE)); }
    } SCENE {
        ABILITY_POPUP(player, ability);
    } THEN {
        EXPECT_EQ(opponent->status1, status);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Pickpocket steals the attacker's item")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_PICKPOCKET); }
        OPPONENT(SPECIES_WOBBUFFET) { Item(ITEM_ORAN_BERRY); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_SCRATCH); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_PICKPOCKET);
    } THEN {
        EXPECT_EQ(player->item, ITEM_ORAN_BERRY);
        EXPECT_EQ(opponent->item, ITEM_NONE);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Mummy replaces the attacker's ability on contact")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_MUMMY); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_SCRATCH); }
    } SCENE {
        MESSAGE("The opposing Wobbuffet acquired Mummy!");
    } THEN {
        EXPECT_EQ(opponent->ability, ABILITY_MUMMY);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Pressure makes moves use 2 PP")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { Passives(ABILITY_PRESSURE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); }
    } THEN {
        EXPECT_EQ(player->pp[0], GetMovePP(MOVE_SCRATCH) - 2);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Sticky Hold prevents item theft")
{
    GIVEN {
        ASSUME(GetMoveEffect(MOVE_TRICK) == EFFECT_TRICK);
        PLAYER(SPECIES_WOBBUFFET) { Item(ITEM_ORAN_BERRY); Passives(ABILITY_STICKY_HOLD); }
        OPPONENT(SPECIES_WOBBUFFET) { Item(ITEM_LEFTOVERS); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_TRICK); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_STICKY_HOLD);
    } THEN {
        EXPECT_EQ(player->item, ITEM_ORAN_BERRY);
    }
}

SINGLE_BATTLE_TEST("Fusion passive: Suction Cups prevents Roar")
{
    GIVEN {
        ASSUME(GetMoveEffect(MOVE_ROAR) == EFFECT_ROAR);
        PLAYER(SPECIES_WOBBUFFET) { Passives(ABILITY_SUCTION_CUPS); }
        PLAYER(SPECIES_WYNAUT);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponent, MOVE_ROAR); }
    } SCENE {
        ABILITY_POPUP(player, ABILITY_SUCTION_CUPS);
    } THEN {
        EXPECT_EQ(player->species, SPECIES_WOBBUFFET);
    }
}
