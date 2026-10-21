EventScript_FusionTerminal::
	lockall
	goto_if_set FLAG_FUSION_TERMINAL_INTRO, EventScript_FusionTerminal_Menu
	setflag FLAG_FUSION_TERMINAL_INTRO
	msgbox Fusion_Text_TerminalIntro, MSGBOX_DEFAULT
EventScript_FusionTerminal_Menu:
	callnative Fusion_ScriptTowerBufferStatus
	goto_if_eq VAR_RESULT, 1, EventScript_FusionTerminal_MenuWithTower
	goto_if_eq VAR_RESULT, 2, EventScript_FusionTerminal_MenuWithShop
	dynmultichoice 0, 0, FALSE, 5, 0, DYN_MULTICHOICE_CB_NONE, Fusion_Text_Fuse, Fusion_Text_Appraise, Fusion_Text_Storage, Fusion_Text_Settings, Fusion_Text_Exit
EventScript_FusionTerminal_HandleMenu:
	switch VAR_RESULT
	case 0, EventScript_FusionTerminal_Fuse
	case 1, EventScript_FusionTerminal_Appraise
	case 2, EventScript_AccessPokemonBoxLink
	case 3, EventScript_FusionTerminal_Settings
	case 5, EventScript_FusionTower
	case 6, EventScript_FusionShop
	releaseall
	end

@ After the Hall of Fame the menu also offers the Fusion Tower (VAR_RESULT 5).
EventScript_FusionTerminal_MenuWithTower:
	dynmultipush Fusion_Text_Fuse, 0
	dynmultipush Fusion_Text_Appraise, 1
	dynmultipush Fusion_Text_Tower, 5
	dynmultipush Fusion_Text_Storage, 2
	dynmultipush Fusion_Text_Settings, 3
	dynmultipush Fusion_Text_Exit, 4
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto EventScript_FusionTerminal_HandleMenu

@ In a New Game+ before the Hall of Fame, carried-over Shards can still be spent.
EventScript_FusionTerminal_MenuWithShop:
	dynmultipush Fusion_Text_Fuse, 0
	dynmultipush Fusion_Text_Appraise, 1
	dynmultipush Fusion_Text_ShardShop, 6
	dynmultipush Fusion_Text_Storage, 2
	dynmultipush Fusion_Text_Settings, 3
	dynmultipush Fusion_Text_Exit, 4
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto EventScript_FusionTerminal_HandleMenu

EventScript_FusionTower:
	callnative Fusion_ScriptTowerBufferStatus
	message Fusion_Text_TowerStatus
	waitmessage
	dynmultichoice 0, 0, FALSE, 5, 0, DYN_MULTICHOICE_CB_NONE, Fusion_Text_TowerChallenge, Fusion_Text_ShardShop, Fusion_Text_Reincarnate, Fusion_Text_TowerRetire, Fusion_Text_Back
	closemessage
	switch VAR_RESULT
	case 0, EventScript_FusionTower_Floor
	case 1, EventScript_FusionShop
	case 2, EventScript_FusionReincarnate
	case 3, EventScript_FusionTower_Retire
	goto EventScript_FusionTerminal_Menu

EventScript_FusionShop:
	setvar VAR_0x8004, 255
	callnative Fusion_ScriptShopBufferEntry
	message Fusion_Text_ShopWelcome
	waitmessage
	dynmultichoice 0, 0, FALSE, 6, 0, DYN_MULTICHOICE_CB_NONE, Fusion_Text_ShopPassiveCrystal, Fusion_Text_ShopTypePrism, Fusion_Text_ShopRareCandy, Fusion_Text_ShopBottleCap, Fusion_Text_ShopGoldBottleCap, Fusion_Text_ShopLeftovers, Fusion_Text_ShopLifeOrb, Fusion_Text_ShopChoiceScarf, Fusion_Text_ShopFocusSash, Fusion_Text_ShopMasterBall, Fusion_Text_Back
	closemessage
	copyvar VAR_0x8004, VAR_RESULT
	callnative Fusion_ScriptShopBufferEntry
	goto_if_eq VAR_RESULT, FALSE, EventScript_FusionTerminal_Menu
	msgbox Fusion_Text_ShopConfirm, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, EventScript_FusionShop
	callnative Fusion_ScriptShopBuy
	goto_if_eq VAR_RESULT, 1, EventScript_FusionShop_NotEnough
	goto_if_eq VAR_RESULT, 2, EventScript_FusionShop_BagFull
	playse SE_SHOP
	msgbox Fusion_Text_ShopBought, MSGBOX_DEFAULT
	goto EventScript_FusionShop

EventScript_FusionShop_NotEnough:
	msgbox Fusion_Text_ShopNotEnough, MSGBOX_DEFAULT
	goto EventScript_FusionShop

EventScript_FusionShop_BagFull:
	msgbox Fusion_Text_ShopBagFull, MSGBOX_DEFAULT
	goto EventScript_FusionShop

EventScript_FusionReincarnate:
	callnative Fusion_ScriptReincarnationStatus
	goto_if_eq VAR_RESULT, 2, EventScript_FusionReincarnate_Cancel
	msgbox Fusion_Text_ReincarnateInfo, MSGBOX_DEFAULT
	msgbox Fusion_Text_ReincarnateConfirm, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, EventScript_FusionTower
	setvar VAR_0x8004, TRUE
	callnative Fusion_ScriptSetReincarnation
	goto_if_eq VAR_RESULT, FALSE, EventScript_FusionReincarnate_SaveFailed
	playfanfare MUS_OBTAIN_ITEM
	message Fusion_Text_ReincarnateSaved
	waitfanfare
	waitmessage
	waitbuttonpress
	msgbox Fusion_Text_ReincarnateResetNow, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, EventScript_FusionTerminal_Menu
	special DoSoftReset
	waitstate
	end

EventScript_FusionReincarnate_Cancel:
	msgbox Fusion_Text_ReincarnateCancelConfirm, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, EventScript_FusionTower
	setvar VAR_0x8004, FALSE
	callnative Fusion_ScriptSetReincarnation
	goto_if_eq VAR_RESULT, FALSE, EventScript_FusionReincarnate_SaveFailed
	msgbox Fusion_Text_ReincarnateCanceled, MSGBOX_DEFAULT
	goto EventScript_FusionTower

EventScript_FusionReincarnate_SaveFailed:
	msgbox Fusion_Text_ReincarnateSaveFailed, MSGBOX_DEFAULT
	goto EventScript_FusionTower

EventScript_FusionTower_Floor:
	callnative Fusion_ScriptTowerPrepareFloor
	goto_if_eq VAR_RESULT, FALSE, EventScript_FusionTower_NoFighters
	msgbox Fusion_Text_TowerFloorIntro, MSGBOX_DEFAULT
	closemessage
	callnative Fusion_ScriptTowerStartBattle
	callnative Fusion_ScriptTowerAfterBattle
	goto_if_eq VAR_RESULT, FALSE, EventScript_FusionTower_Lost
	msgbox Fusion_Text_TowerCleared, MSGBOX_DEFAULT
	callnative Fusion_ScriptTowerRollRewards
	message Fusion_Text_TowerChooseReward
	waitmessage
	dynmultichoice 0, 0, TRUE, 3, 0, DYN_MULTICHOICE_CB_NONE, gStringVar1, gStringVar2, gStringVar3
	closemessage
	callnative Fusion_ScriptTowerGrantReward
	goto EventScript_FusionTower

EventScript_FusionTower_Lost:
	msgbox Fusion_Text_TowerLost, MSGBOX_DEFAULT
	closemessage
	goto EventScript_FusionTerminal_Menu

EventScript_FusionTower_NoFighters:
	msgbox Fusion_Text_TowerNoFighters, MSGBOX_DEFAULT
	closemessage
	goto EventScript_FusionTower

EventScript_FusionTower_Retire:
	msgbox Fusion_Text_TowerRetireConfirm, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, EventScript_FusionTower
	callnative Fusion_ScriptTowerEndRun
	msgbox Fusion_Text_TowerRetired, MSGBOX_DEFAULT
	closemessage
	goto EventScript_FusionTerminal_Menu

EventScript_FusionTerminal_Fuse:
	callnative Fusion_ScriptCountMaterials
	goto_if_lt VAR_RESULT, 2, EventScript_FusionTerminal_NotEnoughMaterials
EventScript_FusionTerminal_ChooseParents:
	callnative Fusion_ScriptSelectParentA
	waitstate
	goto_if_eq VAR_RESULT, FALSE, EventScript_FusionTerminal_Menu
	callnative Fusion_ScriptSelectParentB
	waitstate
	goto_if_eq VAR_RESULT, FALSE, EventScript_FusionTerminal_ChooseParents
	callnative Fusion_ScriptCheckParents
	goto_if_ne VAR_RESULT, 0, EventScript_FusionTerminal_ShowMessage
	callnative Fusion_ScriptShowPreviewCard
	message Fusion_Text_ConfirmFusion
	waitmessage
	callnative Fusion_ScriptCardYesNo
	waitstate
	closemessage
	callnative Fusion_ScriptHideCard
	goto_if_eq VAR_RESULT, NO, EventScript_FusionTerminal_ChooseParents
	callnative Fusion_ScriptDoFusion
	goto_if_eq VAR_RESULT, FALSE, EventScript_FusionTerminal_ShowMessage
	playfanfare MUS_EVOLVED
	message Fusion_Text_FusionComplete
	waitmessage
	waitfanfare
	waitbuttonpress
	closemessage
	callnative Fusion_ScriptHideCard
	goto EventScript_FusionTerminal_Menu

EventScript_FusionTerminal_ShowMessage:
	msgbox gStringVar4, MSGBOX_DEFAULT
	closemessage
	goto EventScript_FusionTerminal_Menu

EventScript_FusionTerminal_NotEnoughMaterials:
	msgbox Fusion_Text_NotEnoughMaterials, MSGBOX_DEFAULT
	closemessage
	goto EventScript_FusionTerminal_Menu

EventScript_FusionTerminal_Appraise:
	callnative Fusion_ScriptCountMaterials
	goto_if_eq VAR_RESULT, 0, EventScript_FusionTerminal_NoMaterials
	callnative Fusion_ScriptOpenAppraiseSelector
	waitstate
	goto_if_eq VAR_RESULT, FALSE, EventScript_FusionTerminal_Menu
	callnative Fusion_ScriptShowAppraisalCard
	msgbox Fusion_Text_AppraisalDone, MSGBOX_DEFAULT
	closemessage
	callnative Fusion_ScriptHideCard
	goto EventScript_FusionTerminal_Appraise

EventScript_FusionTerminal_NoMaterials:
	msgbox Fusion_Text_NoMaterials, MSGBOX_DEFAULT
	closemessage
	goto EventScript_FusionTerminal_Menu

EventScript_FusionTerminal_Settings:
	callnative Fusion_ScriptBufferSettings
	message Fusion_Text_SettingsSummary
	waitmessage
	dynmultichoice 0, 0, FALSE, 4, 0, DYN_MULTICHOICE_CB_NONE, Fusion_Text_CaptureMode, Fusion_Text_ExpShare, Fusion_Text_WildPokemon, Fusion_Text_Back
	closemessage
	switch VAR_RESULT
	case 0, EventScript_FusionTerminal_CaptureMode
	case 1, EventScript_FusionTerminal_ExpShare
	case 2, EventScript_FusionTerminal_WildPokemon
	goto EventScript_FusionTerminal_Menu

EventScript_FusionTerminal_CaptureMode:
	message Fusion_Text_CaptureModeHelp
	waitmessage
	dynmultichoice 0, 0, FALSE, 3, 0, DYN_MULTICHOICE_CB_NONE, Fusion_Text_ModeClassic, Fusion_Text_ModeConvenient, Fusion_Text_ModeMaterial
	closemessage
	goto_if_eq VAR_RESULT, MULTI_B_PRESSED, EventScript_FusionTerminal_Settings
	callnative Fusion_ScriptSetCaptureMode
	goto EventScript_FusionTerminal_Settings

EventScript_FusionTerminal_ExpShare:
	callnative Fusion_ScriptToggleExpShare
	goto EventScript_FusionTerminal_Settings

EventScript_FusionTerminal_WildPokemon:
	msgbox Fusion_Text_WildPokemonHelp, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, EventScript_FusionTerminal_Settings
	callnative Fusion_ScriptToggleWildRandomizer
	goto EventScript_FusionTerminal_Settings

Fusion_Text_ConfirmFusion:
	.string "{STR_VAR_2} will be consumed.\n"
	.string "Fuse them?$"

Fusion_Text_FusionComplete:
	.string "Fusion complete!\n"
	.string "{STR_VAR_1} was created.$"

Fusion_Text_AppraisalDone:
	.string "{STR_VAR_1}'s appraisal.$"

Fusion_Text_NotEnoughMaterials:
	.string "You need at least two Pokémon\n"
	.string "to fuse.$"

Fusion_Text_NoMaterials:
	.string "There are no Pokémon in your party\n"
	.string "or Boxes.$"

Fusion_Text_SettingsSummary:
	.string "Capture: {STR_VAR_1}  Exp. Share: {STR_VAR_2}\n"
	.string "Wild Pokémon: {STR_VAR_3}$"

Fusion_Text_CaptureModeHelp:
	.string "Convenient: wild Pokémon are 2x easier.\n"
	.string "Material: always caught. Not legendaries.$"

Fusion_Text_TerminalIntro:
	.string "Welcome to the Fusion Terminal!\p"
	.string "Fuse: combine two Pokémon from your\n"
	.string "party or any Box into a new one.\p"
	.string "The child takes moves, IVs, its main\n"
	.string "ability and passives from both.\l"
	.string "Both parents are used up.\p"
	.string "Appraise: see IVs, passives and types.\n"
	.string "Settings: capture mode, Exp. Share.\p"
	.string "Tip: wild Pokémon sometimes carry\n"
	.string "passives or a third type. Catch them!$"

Fusion_Text_Tower:
	.string "Tower$"

Fusion_Text_TowerStatus:
	.string "Fusion Tower - next floor {STR_VAR_1}\n"
	.string "Best: {STR_VAR_2}  Shards: {STR_VAR_3}$"

Fusion_Text_ShardShop:
	.string "Shard Shop$"

Fusion_Text_Reincarnate:
	.string "Reincarnate$"

Fusion_Text_ShopWelcome:
	.string "Shard Shop - you have {STR_VAR_3} Shards.\n"
	.string "What would you like?$"

Fusion_Text_ShopPassiveCrystal:
	.string "Passive Crystal  30$"

Fusion_Text_ShopTypePrism:
	.string "Type Prism  20$"

Fusion_Text_ShopRareCandy:
	.string "Rare Candy  3$"

Fusion_Text_ShopBottleCap:
	.string "Bottle Cap  15$"

Fusion_Text_ShopGoldBottleCap:
	.string "Gold Bottle Cap  40$"

Fusion_Text_ShopLeftovers:
	.string "Leftovers  25$"

Fusion_Text_ShopLifeOrb:
	.string "Life Orb  25$"

Fusion_Text_ShopChoiceScarf:
	.string "Choice Scarf  25$"

Fusion_Text_ShopFocusSash:
	.string "Focus Sash  20$"

Fusion_Text_ShopMasterBall:
	.string "Master Ball  100$"

Fusion_Text_ShopConfirm:
	.string "Buy a {STR_VAR_1}\n"
	.string "for {STR_VAR_2} Shards?$"

Fusion_Text_ShopBought:
	.string "You got a {STR_VAR_1}!\n"
	.string "Shards left: {STR_VAR_3}.$"

Fusion_Text_ShopNotEnough:
	.string "You don't have enough Shards.$"

Fusion_Text_ShopBagFull:
	.string "There's no room in your Bag.$"

Fusion_Text_ReincarnateInfo:
	.string "Reincarnation starts a new adventure\n"
	.string "in a freshly rolled world.\p"
	.string "Your party comes along at Lv. 5 and\n"
	.string "keeps its IVs, moves and genes.\p"
	.string "Shards and your best floor are kept.\n"
	.string "Trainers get stronger every cycle.$"

Fusion_Text_ReincarnateConfirm:
	.string "Prepare reincarnation {STR_VAR_1}?\n"
	.string "The game will be saved.$"

Fusion_Text_ReincarnateSaved:
	.string "Saved! Choose NEW GAME on the title\n"
	.string "screen to be reborn with your party.$"

Fusion_Text_ReincarnateResetNow:
	.string "Return to the title screen now?$"

Fusion_Text_ReincarnateCancelConfirm:
	.string "Reincarnation is ready. Cancel it?$"

Fusion_Text_ReincarnateCanceled:
	.string "Reincarnation canceled. Saved.$"

Fusion_Text_ReincarnateSaveFailed:
	.string "The game couldn't be saved.$"

Fusion_Text_TowerChallenge:
	.string "Challenge$"

Fusion_Text_TowerRetire:
	.string "End run$"

Fusion_Text_TowerFloorIntro:
	.string "Floor {STR_VAR_1}: {STR_VAR_2} appears\n"
	.string "with {STR_VAR_3} Pokémon!$"

Fusion_Text_TowerCleared:
	.string "Floor cleared!\n"
	.string "You earned {STR_VAR_1} Fusion Shards.$"

Fusion_Text_TowerChooseReward:
	.string "Choose a reward.\n"
	.string "Your party is not healed between floors.$"

Fusion_Text_TowerLost:
	.string "The run ended on floor {STR_VAR_1}.\n"
	.string "Your party has been healed.$"

Fusion_Text_TowerNoFighters:
	.string "None of your Pokémon can fight.$"

Fusion_Text_TowerRetireConfirm:
	.string "End this run? Your Shards and best\n"
	.string "floor are kept, and your party healed.$"

Fusion_Text_TowerRetired:
	.string "The run is over. See you again!$"

Fusion_Text_Fuse:
	.string "Fuse$"

Fusion_Text_Appraise:
	.string "Appraise$"

Fusion_Text_Storage:
	.string "Storage$"

Fusion_Text_Settings:
	.string "Settings$"

Fusion_Text_Exit:
	.string "Exit$"

Fusion_Text_CaptureMode:
	.string "Capture mode$"

Fusion_Text_ExpShare:
	.string "Exp. Share$"

Fusion_Text_WildPokemon:
	.string "Wild Pokémon$"

Fusion_Text_WildPokemonHelp:
	.string "All gens: every area has its own Pokémon\n"
	.string "from Gen 1-9, rolled for this save.\p"
	.string "Original: the classic Hoenn Pokémon.\n"
	.string "Switch to the other one?$"

Fusion_Text_Back:
	.string "Back$"

Fusion_Text_ModeClassic:
	.string "Classic$"

Fusion_Text_ModeConvenient:
	.string "Convenient$"

Fusion_Text_ModeMaterial:
	.string "Material$"

@ Debug menu: a wild battle from this map's encounter table.
Debug_EventScript_FusionLocalWildBattle::
	lockall
	callnative Fusion_DebugLocalWildBattle
	goto_if_eq VAR_RESULT, FALSE, Debug_EventScript_FusionNoLocalWild
	waitstate
	releaseall
	end

Debug_EventScript_FusionNoLocalWild:
	msgbox Fusion_Text_NoLocalWild, MSGBOX_DEFAULT
	releaseall
	end

Fusion_Text_NoLocalWild:
	.string "No wild Pokémon live here.$"

@ Debug menu: a wild battle against a fixed opponent using the player's own party.
Debug_EventScript_FusionWildBattle::
	lockall
	setwildbattle SPECIES_GEODUDE, 20
	dowildbattle
	releaseall
	end
