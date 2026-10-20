EventScript_FusionTerminal::
	lockall
EventScript_FusionTerminal_Menu:
	dynmultichoice 0, 0, FALSE, 3, 0, DYN_MULTICHOICE_CB_NONE, Fusion_Text_Appraise, Fusion_Text_Storage, Fusion_Text_Exit
	switch VAR_RESULT
	case 0, EventScript_FusionTerminal_Appraise
	case 1, EventScript_AccessPokemonBoxLink
	releaseall
	end

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

Fusion_Text_Appraise:
	.string "Appraise$"

Fusion_Text_Storage:
	.string "Storage$"

Fusion_Text_Exit:
	.string "Exit$"
