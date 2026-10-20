EventScript_FusionTerminal::
	lockall
EventScript_FusionTerminal_Menu:
	dynmultichoice 0, 0, FALSE, 4, 0, DYN_MULTICHOICE_CB_NONE, Fusion_Text_Fuse, Fusion_Text_Appraise, Fusion_Text_Storage, Fusion_Text_Exit
	switch VAR_RESULT
	case 0, EventScript_FusionTerminal_Fuse
	case 1, EventScript_FusionTerminal_Appraise
	case 2, EventScript_AccessPokemonBoxLink
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
	callnative Fusion_ScriptBufferPreview
	msgbox gStringVar4, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, EventScript_FusionTerminal_Menu
	callnative Fusion_ScriptDoFusion
	goto_if_eq VAR_RESULT, FALSE, EventScript_FusionTerminal_ShowMessage
	playfanfare MUS_EVOLVED
	waitfanfare
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
	callnative Fusion_ScriptBufferAppraisal
	msgbox gStringVar4, MSGBOX_DEFAULT
	closemessage
	goto EventScript_FusionTerminal_Appraise

EventScript_FusionTerminal_NoMaterials:
	msgbox Fusion_Text_NoMaterials, MSGBOX_DEFAULT
	closemessage
	goto EventScript_FusionTerminal_Menu

Fusion_Text_NoMaterials:
	.string "There are no Pokémon in your party\n"
	.string "or Boxes.$"

Fusion_Text_NotEnoughMaterials:
	.string "You need at least two Pokémon\n"
	.string "to fuse.$"

Fusion_Text_Fuse:
	.string "Fuse$"

Fusion_Text_Appraise:
	.string "Appraise$"

Fusion_Text_Storage:
	.string "Storage$"

Fusion_Text_Exit:
	.string "Exit$"

@ Debug menu: a wild battle against a fixed opponent using the player's own party.
Debug_EventScript_FusionWildBattle::
	lockall
	setwildbattle SPECIES_GEODUDE, 20
	dowildbattle
	releaseall
	end
