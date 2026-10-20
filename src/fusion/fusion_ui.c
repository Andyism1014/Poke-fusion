#include "global.h"
#include "battle_main.h"
#include "event_data.h"
#include "list_menu.h"
#include "malloc.h"
#include "map_name_popup.h"
#include "menu.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "script.h"
#include "sound.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "fusion/fusion.h"
#include "constants/songs.h"

// Material selector: a two level list (source, then Pokémon) covering the
// party and every PC box, so parents never have to be moved into the party.

#define SELECTOR_ROWS       7
#define SELECTOR_NAME_LEN   24
#define SELECTOR_SOURCES    (1 + TOTAL_BOXES_COUNT) // Party + boxes.
#define SELECTOR_MAX_ITEMS  max(SELECTOR_SOURCES, IN_BOX_COUNT)
#define SOURCE_PARTY        0

enum
{
    LEVEL_SOURCES,
    LEVEL_MONS,
};

struct FusionSelector
{
    FusionSelectorCallback callback;
    struct FusionMonRef exclude;
    u8 level;
    u8 windowId;
    u8 titleWindowId;
    u8 listTaskId;
    u16 sourceScroll;
    u16 sourceCursor;
    struct ListMenuItem items[SELECTOR_MAX_ITEMS];
    u8 names[SELECTOR_MAX_ITEMS][SELECTOR_NAME_LEN];
    struct FusionMonRef refs[IN_BOX_COUNT];
};

static EWRAM_DATA struct FusionSelector *sSelector = NULL;

static void Task_FusionSelector(u8 taskId);
static void PrintMonRowDetails(u8 windowId, u32 id, u8 y);

static const struct WindowTemplate sSelectorWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = 1,
    .tilemapTop = 1,
    .width = 28,
    .height = 2 * SELECTOR_ROWS,
    .paletteNum = 15,
    .baseBlock = 1,
};

static const struct WindowTemplate sSelectorTitleWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = 1,
    .tilemapTop = 17,
    .width = 28,
    .height = 2,
    .paletteNum = 15,
    .baseBlock = 1 + 28 * 2 * SELECTOR_ROWS,
};

static const u8 sText_Party[] = _("Party");
static const u8 sText_Lv[] = _("Lv");
static const u8 sText_IV[] = _("IV");
static const u8 sText_P[] = _("P");
static const u8 sText_ThirdType[] = _("+T");

u32 Fusion_GetBoxMonIvTotal(struct BoxPokemon *boxMon)
{
    u32 stat, total = 0;

    for (stat = 0; stat < NUM_STATS; stat++)
        total += GetBoxMonData(boxMon, MON_DATA_HP_IV + stat);
    return total;
}

static bool32 IsSelectable(const struct FusionMonRef *ref)
{
    return Fusion_IsMaterialRef(ref) && !Fusion_RefsEqual(ref, &sSelector->exclude);
}

static struct FusionMonRef GetSourceRef(u32 source, u32 slot)
{
    if (source == SOURCE_PARTY)
        return Fusion_PartyRef(slot);
    return Fusion_BoxRef(source - 1, slot);
}

static u32 GetSourceSize(u32 source)
{
    return source == SOURCE_PARTY ? PARTY_SIZE : IN_BOX_COUNT;
}

static u32 CountSelectableInSource(u32 source)
{
    u32 slot, count = 0;

    for (slot = 0; slot < GetSourceSize(source); slot++)
    {
        struct FusionMonRef ref = GetSourceRef(source, slot);

        if (IsSelectable(&ref))
            count++;
    }
    return count;
}

static u32 BuildSourceItems(void)
{
    u32 source, numItems = 0;

    for (source = 0; source < SELECTOR_SOURCES; source++)
    {
        u32 count = CountSelectableInSource(source);
        u8 *name = sSelector->names[numItems];

        if (count == 0)
            continue;

        name = StringCopy(name, source == SOURCE_PARTY ? sText_Party : GetBoxNamePtr(source - 1));
        *name++ = CHAR_SPACE;
        *name++ = CHAR_LEFT_PAREN;
        name = ConvertIntToDecimalStringN(name, count, STR_CONV_MODE_LEFT_ALIGN, 2);
        *name++ = CHAR_RIGHT_PAREN;
        *name = EOS;

        sSelector->items[numItems].name = sSelector->names[numItems];
        sSelector->items[numItems].id = source;
        numItems++;
    }
    return numItems;
}

static u32 BuildMonItems(u32 source)
{
    u32 slot, numItems = 0;

    for (slot = 0; slot < GetSourceSize(source); slot++)
    {
        struct FusionMonRef ref = GetSourceRef(source, slot);
        enum Species species;

        if (!IsSelectable(&ref))
            continue;

        species = GetBoxMonData(Fusion_GetBoxMonFromRef(&ref), MON_DATA_SPECIES);
        StringCopyN(sSelector->names[numItems], GetSpeciesName(species), SELECTOR_NAME_LEN - 1);
        sSelector->names[numItems][SELECTOR_NAME_LEN - 1] = EOS;
        sSelector->refs[numItems] = ref;
        sSelector->items[numItems].name = sSelector->names[numItems];
        sSelector->items[numItems].id = numItems;
        numItems++;
    }
    return numItems;
}

static void ShowList(u32 numItems, u16 scroll, u16 cursor)
{
    struct ListMenuTemplate template = {0};

    template.items = sSelector->items;
    template.moveCursorFunc = ListMenuDefaultCursorMoveFunc;
    template.itemPrintFunc = sSelector->level == LEVEL_MONS ? PrintMonRowDetails : NULL;
    template.totalItems = numItems;
    template.maxShowed = min(numItems, SELECTOR_ROWS);
    template.windowId = sSelector->windowId;
    template.item_X = 8;
    template.upText_Y = 1;
    template.cursorPal = 2;
    template.fillValue = 1;
    template.cursorShadowPal = 3;
    template.lettersSpacing = 1;
    template.scrollMultiple = LIST_MULTIPLE_SCROLL_DPAD;
    template.fontId = FONT_NORMAL;

    FillWindowPixelBuffer(sSelector->windowId, PIXEL_FILL(1));
    sSelector->listTaskId = ListMenuInit(&template, scroll, cursor);
    CopyWindowToVram(sSelector->windowId, COPYWIN_FULL);
}

static void PrintMonRowDetails(u8 windowId, u32 id, u8 y)
{
    struct BoxPokemon *boxMon;
    struct FusionGenetics genetics;
    u8 *text = gStringVar4;
    u8 level;

    if (id >= IN_BOX_COUNT)
        return;

    boxMon = Fusion_GetBoxMonFromRef(&sSelector->refs[id]);
    if (sSelector->refs[id].location == FUSION_LOC_PARTY)
        level = GetMonData(&gParties[B_TRAINER_PLAYER][sSelector->refs[id].slot], MON_DATA_LEVEL);
    else
        level = GetLevelFromBoxMonExp(boxMon);
    Fusion_GetBoxMonGenetics(boxMon, &genetics);

    text = StringCopy(gStringVar4, sText_Lv);
    ConvertIntToDecimalStringN(text, level, STR_CONV_MODE_LEFT_ALIGN, 3);
    AddTextPrinterParameterized(windowId, FONT_NORMAL, gStringVar4, 96, y, TEXT_SKIP_DRAW, NULL);

    text = StringCopy(gStringVar4, sText_IV);
    ConvertIntToDecimalStringN(text, Fusion_GetBoxMonIvTotal(boxMon), STR_CONV_MODE_LEFT_ALIGN, 3);
    AddTextPrinterParameterized(windowId, FONT_NORMAL, gStringVar4, 136, y, TEXT_SKIP_DRAW, NULL);

    text = StringCopy(gStringVar4, sText_P);
    ConvertIntToDecimalStringN(text, Fusion_CountPassives(&genetics), STR_CONV_MODE_LEFT_ALIGN, 1);
    AddTextPrinterParameterized(windowId, FONT_NORMAL, gStringVar4, 180, y, TEXT_SKIP_DRAW, NULL);

    if (genetics.extraType != TYPE_NONE)
        AddTextPrinterParameterized(windowId, FONT_NORMAL, sText_ThirdType, 200, y, TEXT_SKIP_DRAW, NULL);
}

static void OpenLevel(u32 level, u32 source)
{
    u32 numItems;

    sSelector->level = level;
    if (level == LEVEL_SOURCES)
    {
        numItems = BuildSourceItems();
        ShowList(numItems, sSelector->sourceScroll, sSelector->sourceCursor);
    }
    else
    {
        numItems = BuildMonItems(source);
        ShowList(numItems, 0, 0);
    }
}

static void CloseSelector(u8 taskId, bool32 selected, const struct FusionMonRef *ref)
{
    FusionSelectorCallback callback = sSelector->callback;
    struct FusionMonRef result = {0};

    if (selected)
        result = *ref;

    DestroyListMenuTask(sSelector->listTaskId, NULL, NULL);
    ClearStdWindowAndFrame(sSelector->windowId, TRUE);
    RemoveWindow(sSelector->windowId);
    ClearStdWindowAndFrame(sSelector->titleWindowId, TRUE);
    RemoveWindow(sSelector->titleWindowId);
    FREE_AND_SET_NULL(sSelector);
    DestroyTask(taskId);

    callback(selected, &result);
}

// Callers must check Fusion_CountMaterials first: an empty list can't be shown.
void Fusion_OpenMaterialSelector(const u8 *title, const struct FusionMonRef *exclude, FusionSelectorCallback callback)
{
    sSelector = AllocZeroed(sizeof(*sSelector));
    sSelector->callback = callback;
    if (exclude != NULL)
        sSelector->exclude = *exclude;

    HideMapNamePopUpWindow();
    LoadMessageBoxAndBorderGfx();
    sSelector->windowId = AddWindow(&sSelectorWindowTemplate);
    DrawStdWindowFrame(sSelector->windowId, FALSE);
    sSelector->titleWindowId = AddWindow(&sSelectorTitleWindowTemplate);
    DrawStdWindowFrame(sSelector->titleWindowId, FALSE);
    FillWindowPixelBuffer(sSelector->titleWindowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(sSelector->titleWindowId, FONT_NORMAL, title, 0, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(sSelector->titleWindowId, COPYWIN_FULL);

    OpenLevel(LEVEL_SOURCES, 0);
    CreateTask(Task_FusionSelector, 3);
}

u32 Fusion_CountMaterials(const struct FusionMonRef *exclude)
{
    struct FusionMonRef ref = {.location = FUSION_LOC_NONE};
    u32 count = 0;

    while (Fusion_NextMaterialRef(&ref))
    {
        if (exclude == NULL || !Fusion_RefsEqual(&ref, exclude))
            count++;
    }
    return count;
}

static void Task_FusionSelector(u8 taskId)
{
    s32 input = ListMenu_ProcessInput(sSelector->listTaskId);

    if (input == LIST_NOTHING_CHOSEN)
        return;

    PlaySE(SE_SELECT);
    if (sSelector->level == LEVEL_SOURCES)
    {
        if (input == LIST_CANCEL)
        {
            CloseSelector(taskId, FALSE, NULL);
            return;
        }
        DestroyListMenuTask(sSelector->listTaskId, &sSelector->sourceScroll, &sSelector->sourceCursor);
        OpenLevel(LEVEL_MONS, input);
    }
    else
    {
        if (input == LIST_CANCEL)
        {
            DestroyListMenuTask(sSelector->listTaskId, NULL, NULL);
            OpenLevel(LEVEL_SOURCES, 0);
            return;
        }
        CloseSelector(taskId, TRUE, &sSelector->refs[input]);
    }
}

// Script interface. The selected Pokémon is returned in VAR_0x8004 (location),
// VAR_0x8005 (box) and VAR_0x8006 (slot); VAR_RESULT is TRUE if one was chosen.

static const u8 sText_ChooseToAppraise[] = _("Choose a Pokémon to appraise.");
static const u8 sText_Dash[] = _("-");
static const u8 sText_AppraisalHeader[] = _("{STR_VAR_1}  Lv{STR_VAR_2}  IVs {STR_VAR_3}/186\p");
static const u8 sText_Ability[] = _("Ability: ");
static const u8 sText_Passives[] = _("\nPassives: ");
static const u8 sText_None[] = _("none");
static const u8 sText_Separator[] = _(", ");
static const u8 sText_ExtraType[] = _("\pThird type: ");
static const u8 sText_Generation[] = _("\nFusion generation: ");

static void ScriptSelectorCallback(bool32 selected, const struct FusionMonRef *ref)
{
    gSpecialVar_Result = selected;
    gSpecialVar_0x8004 = ref->location;
    gSpecialVar_0x8005 = ref->box;
    gSpecialVar_0x8006 = ref->slot;
    ScriptContext_Enable();
}

void Fusion_ScriptCountMaterials(void)
{
    gSpecialVar_Result = Fusion_CountMaterials(NULL);
}

void Fusion_ScriptOpenAppraiseSelector(void)
{
    Fusion_OpenMaterialSelector(sText_ChooseToAppraise, NULL, ScriptSelectorCallback);
}

static u8 *AppendAbilityName(u8 *dst, enum Ability ability)
{
    return StringCopy(dst, gAbilitiesInfo[ability].name);
}

// Writes a multi-page summary of the selected Pokémon's genetics to gStringVar4.
void Fusion_ScriptBufferAppraisal(void)
{
    struct FusionMonRef ref = {.location = gSpecialVar_0x8004, .box = gSpecialVar_0x8005, .slot = gSpecialVar_0x8006};
    struct Pokemon mon;
    struct FusionGenetics genetics;
    u32 i, numPassives = 0;
    u8 *text;

    if (!Fusion_ReadMonFromRef(&ref, &mon))
    {
        gStringVar4[0] = EOS;
        return;
    }
    Fusion_GetMonGenetics(&mon, &genetics);

    StringCopy(gStringVar1, GetSpeciesName(GetMonData(&mon, MON_DATA_SPECIES)));
    ConvertIntToDecimalStringN(gStringVar2, GetMonData(&mon, MON_DATA_LEVEL), STR_CONV_MODE_LEFT_ALIGN, 3);
    ConvertIntToDecimalStringN(gStringVar3, Fusion_GetBoxMonIvTotal(&mon.box), STR_CONV_MODE_LEFT_ALIGN, 3);
    text = StringExpandPlaceholders(gStringVar4, sText_AppraisalHeader);

    text = StringCopy(text, sText_Ability);
    text = AppendAbilityName(text, genetics.mainAbility != ABILITY_NONE ? genetics.mainAbility : GetMonAbility(&mon));

    text = StringCopy(text, sText_Passives);
    for (i = 0; i < FUSION_MAX_PASSIVES; i++)
    {
        if (genetics.passives[i] == ABILITY_NONE)
            continue;
        if (numPassives++ > 0)
            text = StringCopy(text, sText_Separator);
        text = AppendAbilityName(text, genetics.passives[i]);
    }
    if (numPassives == 0)
        text = StringCopy(text, sText_None);

    text = StringCopy(text, sText_ExtraType);
    text = StringCopy(text, genetics.extraType != TYPE_NONE ? gTypesInfo[genetics.extraType].name : sText_Dash);
    text = StringCopy(text, sText_Generation);
    ConvertIntToDecimalStringN(text, genetics.generation, STR_CONV_MODE_LEFT_ALIGN, 2);
}
