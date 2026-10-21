EventScript_FusionTerminal::
	lockall
EventScript_FusionTerminal_Menu:
	dynmultichoice 0, 0, FALSE, 5, 0, DYN_MULTICHOICE_CB_NONE, Fusion_Text_Fuse, Fusion_Text_Appraise, Fusion_Text_Storage, Fusion_Text_Settings, Fusion_Text_Exit
	switch VAR_RESULT
	case 0, EventScript_FusionTerminal_Fuse
	case 1, EventScript_FusionTerminal_Appraise
	case 2, EventScript_AccessPokemonBoxLink
	case 3, EventScript_FusionTerminal_Settings
	releaseall
	end

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
	dynmultichoice 0, 0, FALSE, 3, 0, DYN_MULTICHOICE_CB_NONE, Fusion_Text_CaptureMode, Fusion_Text_ExpShare, Fusion_Text_Back
	closemessage
	switch VAR_RESULT
	case 0, EventScript_FusionTerminal_CaptureMode
	case 1, EventScript_FusionTerminal_ExpShare
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
	.string "Capture mode: {STR_VAR_1}\n"
	.string "Exp. Share (whole party): {STR_VAR_2}$"

Fusion_Text_CaptureModeHelp:
	.string "Convenient: wild Pokémon are 2x easier.\n"
	.string "Material: always caught. Not legendaries.$"

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

Fusion_Text_Back:
	.string "Back$"

Fusion_Text_ModeClassic:
	.string "Classic$"

Fusion_Text_ModeConvenient:
	.string "Convenient$"

Fusion_Text_ModeMaterial:
	.string "Material$"

@ Debug menu: a wild battle against a fixed opponent using the player's own party.
Debug_EventScript_FusionWildBattle::
	lockall
	setwildbattle SPECIES_GEODUDE, 20
	dowildbattle
	releaseall
	end
