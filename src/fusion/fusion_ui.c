#include "global.h"
#include "battle_main.h"
#include "event_data.h"
#include "list_menu.h"
#include "main.h"
#include "malloc.h"
#include "map_name_popup.h"
#include "menu.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "random.h"
#include "script.h"
#include "sound.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "fusion/fusion.h"
#include "constants/songs.h"

// Fusion Terminal UI: the material selector (party + every PC box) and the
// info card used for appraisals, fusion previews and fusion results.

#define SELECTOR_ROWS       5
#define SELECTOR_NAME_LEN   24
#define SELECTOR_SOURCES    (1 + TOTAL_BOXES_COUNT) // Party + boxes.
#define SELECTOR_MAX_ITEMS  max(SELECTOR_SOURCES, IN_BOX_COUNT)
#define SOURCE_PARTY        0

#define WINDOW_WIDTH        28
#define LIST_HEIGHT         (2 * SELECTOR_ROWS)
#define INFO_HEIGHT         6
#define CARD_HEIGHT         12

#define CARD_LINE_X         4
#define CARD_HEADER_HEIGHT  18
#define CARD_LINE_HEIGHT    12
#define CARD_COLUMN_WIDTH   130 // Appraisal card text left of the IV radar.

enum
{
    LEVEL_SOURCES,
    LEVEL_MONS,
};

struct FusionSelector
{
    FusionSelectorCallback callback;
    const u8 *title;
    struct FusionMonRef exclude;
    u8 level;
    u8 source;
    bool8 sortByIvs;
    u8 windowId;
    u8 infoWindowId;
    u8 listTaskId;
    struct ListMenuItem items[SELECTOR_MAX_ITEMS];
    u8 names[SELECTOR_MAX_ITEMS][SELECTOR_NAME_LEN];
    struct FusionMonRef refs[IN_BOX_COUNT];
};

static EWRAM_DATA struct FusionSelector *sSelector = NULL;
static EWRAM_DATA u8 sCardWindowId = 0;
static EWRAM_DATA bool8 sCardOpen = FALSE; // EWRAM starts zeroed, so this can't be a WINDOW_NONE sentinel.
// Remembered between selector openings so the player returns to the same box.
static EWRAM_DATA u16 sLastSourceScroll = 0;
static EWRAM_DATA u16 sLastSourceCursor = 0;

static void Task_FusionSelector(u8 taskId);
static void PrintMonRowDetails(u8 windowId, u32 id, u8 y);
static void MoveCursor_Mons(s32 id, bool8 onInit, struct ListMenu *list);

static const struct WindowTemplate sSelectorWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = 1,
    .tilemapTop = 1,
    .width = WINDOW_WIDTH,
    .height = LIST_HEIGHT,
    .paletteNum = 15,
    .baseBlock = 1,
};

static const struct WindowTemplate sSelectorInfoWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = 1,
    .tilemapTop = 1 + LIST_HEIGHT + 2,
    .width = WINDOW_WIDTH,
    .height = INFO_HEIGHT,
    .paletteNum = 15,
    .baseBlock = 1 + WINDOW_WIDTH * LIST_HEIGHT,
};

static const struct WindowTemplate sCardWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = 1,
    .tilemapTop = 1,
    .width = WINDOW_WIDTH,
    .height = CARD_HEIGHT,
    .paletteNum = 15,
    .baseBlock = 1,
};

// YES/NO at the right end of the message box, so it never covers the card.
// Its tiles start after the card's.
static const struct WindowTemplate sCardYesNoWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = 24,
    .tilemapTop = 15,
    .width = 5,
    .height = 4,
    .paletteNum = 15,
    .baseBlock = 1 + WINDOW_WIDTH * CARD_HEIGHT,
};

static const u8 sText_Party[] = _("Party");
static const u8 sText_Lv[] = _("Lv");
static const u8 sText_IV[] = _("IV");
static const u8 sText_P[] = _("P");
static const u8 sText_ThirdType[] = _("+T");
static const u8 sText_Dash[] = _("-");
static const u8 sText_Slash[] = _(" / ");
static const u8 sText_Separator[] = _(", ");
static const u8 sText_Or[] = _(" or ");
static const u8 sText_Ability[] = _("Ability: ");
static const u8 sText_Passive[] = _("Passive: ");
static const u8 sText_Passives[] = _("Passives: ");
static const u8 sText_None[] = _("none");
static const u8 sText_SourcesHint[] = _("{A_BUTTON} Open  {B_BUTTON} Back");
static const u8 sText_MonsHint[] = _("{A_BUTTON} Pick  {L_BUTTON}{R_BUTTON} Box  {SELECT_BUTTON} Sort");

u32 Fusion_GetBoxMonIvTotal(struct BoxPokemon *boxMon)
{
    u32 stat, total = 0;

    for (stat = 0; stat < NUM_STATS; stat++)
        total += GetBoxMonData(boxMon, MON_DATA_HP_IV + stat);
    return total;
}

static u8 *AppendTypes(u8 *dst, enum Species species, enum Type extraType)
{
    enum Type type1 = gSpeciesInfo[species].types[0], type2 = gSpeciesInfo[species].types[1];

    dst = StringCopy(dst, gTypesInfo[type1].name);
    if (type2 != type1)
        dst = StringCopy(StringCopy(dst, sText_Slash), gTypesInfo[type2].name);
    if (extraType != TYPE_NONE && extraType != type1 && extraType != type2)
        dst = StringCopy(StringCopy(dst, sText_Slash), gTypesInfo[extraType].name);
    return dst;
}

static u8 *AppendPassiveList(u8 *dst, const struct FusionGenetics *genetics)
{
    u32 count = 0;

    for (u32 i = 0; i < FUSION_MAX_PASSIVES; i++)
    {
        if (genetics->passives[i] == ABILITY_NONE)
            continue;
        if (count++ > 0)
            dst = StringCopy(dst, sText_Separator);
        dst = StringCopy(dst, gAbilitiesInfo[genetics->passives[i]].name);
    }
    if (count == 0)
        dst = StringCopy(dst, sText_None);
    return dst;
}

//
// Material selector
//

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

static u32 GetRefIvTotal(const struct FusionMonRef *ref)
{
    return Fusion_GetBoxMonIvTotal(Fusion_GetBoxMonFromRef(ref));
}

static void SortRefsByIvs(u32 count)
{
    u16 ivs[IN_BOX_COUNT];

    for (u32 i = 0; i < count; i++)
        ivs[i] = GetRefIvTotal(&sSelector->refs[i]);
    // Insertion sort, highest IV total first.
    for (u32 i = 1; i < count; i++)
    {
        struct FusionMonRef ref = sSelector->refs[i];
        u16 iv = ivs[i];
        s32 j = i - 1;

        while (j >= 0 && ivs[j] < iv)
        {
            sSelector->refs[j + 1] = sSelector->refs[j];
            ivs[j + 1] = ivs[j];
            j--;
        }
        sSelector->refs[j + 1] = ref;
        ivs[j + 1] = iv;
    }
}

static u32 BuildMonItems(u32 source)
{
    u32 slot, numItems = 0;

    for (slot = 0; slot < GetSourceSize(source); slot++)
    {
        struct FusionMonRef ref = GetSourceRef(source, slot);

        if (IsSelectable(&ref))
            sSelector->refs[numItems++] = ref;
    }
    if (sSelector->sortByIvs)
        SortRefsByIvs(numItems);

    for (u32 i = 0; i < numItems; i++)
    {
        enum Species species = GetBoxMonData(Fusion_GetBoxMonFromRef(&sSelector->refs[i]), MON_DATA_SPECIES);

        StringCopyN(sSelector->names[i], GetSpeciesName(species), SELECTOR_NAME_LEN - 1);
        sSelector->names[i][SELECTOR_NAME_LEN - 1] = EOS;
        sSelector->items[i].name = sSelector->names[i];
        sSelector->items[i].id = i;
    }
    return numItems;
}

static void PrintInfo(const u8 *line2, const u8 *line3)
{
    u32 windowId = sSelector->infoWindowId;

    FillWindowPixelBuffer(windowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(windowId, FONT_NORMAL, sSelector->title, 0, 1, TEXT_SKIP_DRAW, NULL);
    if (line2 != NULL)
        AddTextPrinterParameterized(windowId, FONT_SMALL, line2, 0, 18, TEXT_SKIP_DRAW, NULL);
    if (line3 != NULL)
        AddTextPrinterParameterized(windowId, FONT_SMALL, line3, 0, 32, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(windowId, COPYWIN_GFX);
}

static void ShowList(u32 numItems, u16 scroll, u16 cursor)
{
    struct ListMenuTemplate template = {0};

    template.items = sSelector->items;
    template.moveCursorFunc = sSelector->level == LEVEL_MONS ? MoveCursor_Mons : ListMenuDefaultCursorMoveFunc;
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

// Live details of the highlighted Pokémon: ability, types and passives.
static void MoveCursor_Mons(s32 id, bool8 onInit, struct ListMenu *list)
{
    struct Pokemon mon;
    struct FusionGenetics genetics;
    u8 *text;

    if (!onInit)
        PlaySE(SE_SELECT);
    if (id < 0 || id >= IN_BOX_COUNT || !Fusion_ReadMonFromRef(&sSelector->refs[id], &mon))
        return;

    Fusion_GetMonGenetics(&mon, &genetics);
    text = StringCopy(gStringVar1, sText_Ability);
    text = StringCopy(text, gAbilitiesInfo[GetMonAbility(&mon)].name);
    text = StringCopy(text, sText_Slash);
    AppendTypes(text, GetMonData(&mon, MON_DATA_SPECIES), genetics.extraType);
    AppendPassiveList(StringCopy(gStringVar2, sText_Passives), &genetics);
    PrintInfo(gStringVar1, gStringVar2);
}

static void OpenLevel(u32 level, u32 source, u16 scroll, u16 cursor)
{
    u32 numItems;

    sSelector->level = level;
    sSelector->source = source;
    if (level == LEVEL_SOURCES)
    {
        numItems = BuildSourceItems();
        PrintInfo(sText_SourcesHint, NULL);
    }
    else
    {
        numItems = BuildMonItems(source);
    }
    ShowList(numItems, scroll, cursor);
    if (level == LEVEL_MONS)
        CopyWindowToVram(sSelector->infoWindowId, COPYWIN_FULL);
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
    ClearStdWindowAndFrame(sSelector->infoWindowId, TRUE);
    RemoveWindow(sSelector->infoWindowId);
    FREE_AND_SET_NULL(sSelector);
    DestroyTask(taskId);

    callback(selected, &result);
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

// Callers must check Fusion_CountMaterials first: an empty list can't be shown.
void Fusion_OpenMaterialSelector(const u8 *title, const struct FusionMonRef *exclude, FusionSelectorCallback callback)
{
    sSelector = AllocZeroed(sizeof(*sSelector));
    sSelector->callback = callback;
    sSelector->title = title;
    if (exclude != NULL)
        sSelector->exclude = *exclude;

    HideMapNamePopUpWindow();
    LoadMessageBoxAndBorderGfx();
    sSelector->windowId = AddWindow(&sSelectorWindowTemplate);
    DrawStdWindowFrame(sSelector->windowId, FALSE);
    sSelector->infoWindowId = AddWindow(&sSelectorInfoWindowTemplate);
    DrawStdWindowFrame(sSelector->infoWindowId, FALSE);

    // The remembered cursor may point past the end if a box was emptied.
    if (sLastSourceScroll + sLastSourceCursor >= BuildSourceItems())
        sLastSourceScroll = sLastSourceCursor = 0;
    OpenLevel(LEVEL_SOURCES, 0, sLastSourceScroll, sLastSourceCursor);
    CopyWindowToVram(sSelector->infoWindowId, COPYWIN_FULL);
    CreateTask(Task_FusionSelector, 3);
}

// L/R: the previous / next source that has selectable Pokémon.
static bool32 TrySwitchSource(s32 direction)
{
    s32 source = sSelector->source;

    for (u32 i = 0; i < SELECTOR_SOURCES; i++)
    {
        source = (source + direction + SELECTOR_SOURCES) % SELECTOR_SOURCES;
        if (CountSelectableInSource(source) > 0)
        {
            if (source == sSelector->source)
                return FALSE;
            DestroyListMenuTask(sSelector->listTaskId, NULL, NULL);
            OpenLevel(LEVEL_MONS, source, 0, 0);
            return TRUE;
        }
    }
    return FALSE;
}

static void Task_FusionSelector(u8 taskId)
{
    s32 input;

    if (sSelector->level == LEVEL_MONS)
    {
        if (JOY_NEW(L_BUTTON) || JOY_NEW(R_BUTTON))
        {
            if (TrySwitchSource(JOY_NEW(R_BUTTON) ? 1 : -1))
                PlaySE(SE_SELECT);
            return;
        }
        if (JOY_NEW(SELECT_BUTTON))
        {
            PlaySE(SE_SELECT);
            sSelector->sortByIvs ^= TRUE;
            DestroyListMenuTask(sSelector->listTaskId, NULL, NULL);
            OpenLevel(LEVEL_MONS, sSelector->source, 0, 0);
            return;
        }
    }

    input = ListMenu_ProcessInput(sSelector->listTaskId);
    if (input == LIST_NOTHING_CHOSEN)
        return;

    PlaySE(SE_SELECT);
    if (sSelector->level == LEVEL_SOURCES)
    {
        if (input == LIST_CANCEL)
        {
            ListMenuGetScrollAndRow(sSelector->listTaskId, &sLastSourceScroll, &sLastSourceCursor);
            CloseSelector(taskId, FALSE, NULL);
            return;
        }
        DestroyListMenuTask(sSelector->listTaskId, &sLastSourceScroll, &sLastSourceCursor);
        OpenLevel(LEVEL_MONS, input, 0, 0);
    }
    else
    {
        if (input == LIST_CANCEL)
        {
            DestroyListMenuTask(sSelector->listTaskId, NULL, NULL);
            OpenLevel(LEVEL_SOURCES, 0, sLastSourceScroll, sLastSourceCursor);
            CopyWindowToVram(sSelector->infoWindowId, COPYWIN_FULL);
            return;
        }
        CloseSelector(taskId, TRUE, &sSelector->refs[input]);
    }
}

//
// Info card
//

static void OpenCard(void)
{
    if (sCardOpen)
        Fusion_ScriptHideCard();
    LoadMessageBoxAndBorderGfx();
    sCardOpen = TRUE;
    sCardWindowId = AddWindow(&sCardWindowTemplate);
    DrawStdWindowFrame(sCardWindowId, FALSE);
    FillWindowPixelBuffer(sCardWindowId, PIXEL_FILL(1));
}

static void PrintCardHeader(const u8 *left, const u8 *right)
{
    AddTextPrinterParameterized(sCardWindowId, FONT_NORMAL, left, CARD_LINE_X, 1, TEXT_SKIP_DRAW, NULL);
    if (right != NULL)
    {
        u32 x = WINDOW_WIDTH * 8 - CARD_LINE_X - GetStringWidth(FONT_SMALL, right, 0);
        AddTextPrinterParameterized(sCardWindowId, FONT_SMALL, right, x, 4, TEXT_SKIP_DRAW, NULL);
    }
}

static void PrintCardLine(u32 line, const u8 *text)
{
    u32 y = CARD_HEADER_HEIGHT + line * CARD_LINE_HEIGHT;
    AddTextPrinterParameterized(sCardWindowId, FONT_SMALL, text, CARD_LINE_X, y, TEXT_SKIP_DRAW, NULL);
}

// A line of the text column beside the appraisal card's IV radar, narrowed to fit.
static void PrintCardColumnLine(u32 line, const u8 *text)
{
    u32 y = CARD_HEADER_HEIGHT + line * CARD_LINE_HEIGHT;
    AddTextPrinterParameterized(sCardWindowId, GetFontIdToFit(text, FONT_SMALL, 0, CARD_COLUMN_WIDTH), text, CARD_LINE_X, y, TEXT_SKIP_DRAW, NULL);
}

static void ShowCard(void)
{
    CopyWindowToVram(sCardWindowId, COPYWIN_FULL);
}

static void Task_CardYesNo(u8 taskId)
{
    s8 input = Menu_ProcessInputNoWrapClearOnChoose();

    if (input == MENU_NOTHING_CHOSEN)
        return;
    gSpecialVar_Result = (input == 0); // YES = 1, NO = 0 in scripts
    DestroyTask(taskId);
    ScriptContext_Enable();
}

// Use with waitstate. VAR_RESULT is YES or NO (B counts as NO).
void Fusion_ScriptCardYesNo(void)
{
    CreateYesNoMenu(&sCardYesNoWindowTemplate, STD_WINDOW_BASE_TILE_NUM, STD_WINDOW_PALETTE_NUM, 0);
    CreateTask(Task_CardYesNo, 80);
}

void Fusion_ScriptHideCard(void)
{
    if (!sCardOpen)
        return;
    ClearStdWindowAndFrame(sCardWindowId, TRUE);
    RemoveWindow(sCardWindowId);
    sCardOpen = FALSE;
}

static const u8 sText_CardHeader[] = _("{STR_VAR_1}  Lv{STR_VAR_2}");
static const u8 sText_CardGeneration[] = _("Fusion gen {STR_VAR_1}");
static const u8 sText_CardNature[] = _("{STR_VAR_1} nature");
static const u8 sText_CardPerfectIvs[] = _("{STR_VAR_1}V");
static const u8 sText_CardIvTotal[] = _("{STR_VAR_1}/186");
static const u8 sText_Types[] = _("Types: ");

// IV radar on the right of the appraisal card, laid out like the in-game IV judge.
#define RADAR_CENTER_X      178
#define RADAR_CENTER_Y      57
#define RADAR_RADIUS        25
#define RADAR_DIR_ONE       64 // Fixed-point 1.0 of the axis directions.
#define RADAR_LABEL_GAP     3
#define RADAR_LABEL_HEIGHT  11

struct RadarPoint
{
    s16 x, y;
};

struct RadarAxis
{
    u8 stat;
    s8 dx, dy;
};

// HP on top, then clockwise. 55/64 ~ cos 30°.
static const struct RadarAxis sRadarAxes[NUM_STATS] =
{
    {STAT_HP,      0, -RADAR_DIR_ONE},
    {STAT_ATK,    55, -RADAR_DIR_ONE / 2},
    {STAT_DEF,    55,  RADAR_DIR_ONE / 2},
    {STAT_SPEED,   0,  RADAR_DIR_ONE},
    {STAT_SPDEF, -55,  RADAR_DIR_ONE / 2},
    {STAT_SPATK, -55, -RADAR_DIR_ONE / 2},
};

static const u8 *const sRadarStatNames[NUM_STATS] =
{
    [STAT_HP] = COMPOUND_STRING("HP"),
    [STAT_ATK] = COMPOUND_STRING("Atk"),
    [STAT_DEF] = COMPOUND_STRING("Def"),
    [STAT_SPEED] = COMPOUND_STRING("Spe"),
    [STAT_SPATK] = COMPOUND_STRING("SpA"),
    [STAT_SPDEF] = COMPOUND_STRING("SpD"),
};

static const u8 sCardTextColors[] = {TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};
static const u8 sCardPerfectColors[] = {TEXT_COLOR_WHITE, TEXT_COLOR_RED, TEXT_COLOR_LIGHT_RED};

static s32 DivRound(s32 n, s32 d)
{
    return (n + (n < 0 ? -d : d) / 2) / d;
}

// The point at value/max along an axis.
static struct RadarPoint GetRadarPoint(const struct RadarAxis *axis, u32 value, u32 max)
{
    s32 scale = RADAR_RADIUS * value, denom = RADAR_DIR_ONE * max;
    struct RadarPoint point = {RADAR_CENTER_X + DivRound(axis->dx * scale, denom), RADAR_CENTER_Y + DivRound(axis->dy * scale, denom)};

    return point;
}

static void FillCardSpan(s32 left, s32 right, s32 y, u8 color)
{
    FillWindowPixelRect(sCardWindowId, PIXEL_FILL(color), left, y, right - left + 1, 1);
}

static void DrawCardLine(struct RadarPoint a, struct RadarPoint b, u8 color)
{
    s32 dx = abs(b.x - a.x), dy = -abs(b.y - a.y);
    s32 stepX = a.x < b.x ? 1 : -1, stepY = a.y < b.y ? 1 : -1;
    s32 error = dx + dy;

    for (;;)
    {
        s32 error2 = 2 * error;

        FillCardSpan(a.x, a.x, a.y, color);
        if (a.x == b.x && a.y == b.y)
            break;
        if (error2 >= dy)
        {
            error += dy;
            a.x += stepX;
        }
        if (error2 <= dx)
        {
            error += dx;
            a.y += stepY;
        }
    }
}

// Scanline fill. Horizontal edges can be skipped: the other two edges reach their ends.
static void FillCardTriangle(const struct RadarPoint *points, u8 color)
{
    s32 top = min(points[0].y, min(points[1].y, points[2].y));
    s32 bottom = max(points[0].y, max(points[1].y, points[2].y));

    for (s32 y = top; y <= bottom; y++)
    {
        s32 left = WINDOW_WIDTH * 8, right = -1;

        for (u32 i = 0; i < 3; i++)
        {
            struct RadarPoint a = points[i], b = points[(i + 1) % 3];
            s32 x;

            if (a.y == b.y || y < min(a.y, b.y) || y > max(a.y, b.y))
                continue;
            x = a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y);
            left = min(left, x);
            right = max(right, x);
        }
        if (left <= right)
            FillCardSpan(left, right, y, color);
    }
}

// "HP 31" above/below the tip for the vertical axes, name over value beside it for the others.
static void PrintRadarLabel(const struct RadarAxis *axis, u32 iv)
{
    struct RadarPoint tip = GetRadarPoint(axis, 1, 1);
    const u8 *name = sRadarStatNames[axis->stat];
    const u8 *valueColors = iv == MAX_PER_STAT_IVS ? sCardPerfectColors : sCardTextColors;
    s32 nameWidth, valueWidth, nameX, valueX, nameY, valueY;

    ConvertIntToDecimalStringN(gStringVar1, iv, STR_CONV_MODE_LEFT_ALIGN, 2);
    nameWidth = GetStringWidth(FONT_SMALL, name, 0);
    valueWidth = GetStringWidth(FONT_SMALL, gStringVar1, 0);
    if (axis->dx == 0)
    {
        nameX = tip.x - (nameWidth + RADAR_LABEL_GAP + valueWidth) / 2;
        valueX = nameX + nameWidth + RADAR_LABEL_GAP;
        nameY = valueY = axis->dy < 0 ? tip.y - RADAR_LABEL_HEIGHT - RADAR_LABEL_GAP : tip.y;
    }
    else
    {
        nameX = axis->dx > 0 ? tip.x + RADAR_LABEL_GAP : tip.x - RADAR_LABEL_GAP - nameWidth;
        valueX = axis->dx > 0 ? tip.x + RADAR_LABEL_GAP : tip.x - RADAR_LABEL_GAP - valueWidth;
        nameY = tip.y - RADAR_LABEL_HEIGHT;
        valueY = tip.y;
    }
    AddTextPrinterParameterized3(sCardWindowId, FONT_SMALL, nameX, nameY, sCardTextColors, TEXT_SKIP_DRAW, name);
    AddTextPrinterParameterized3(sCardWindowId, FONT_SMALL, valueX, valueY, valueColors, TEXT_SKIP_DRAW, gStringVar1);
}

// Outer ring = 31, inner ring = half. Perfect IVs get a red dot and label, and the
// header shows how many there are ("5V") next to the total.
static void DrawIvRadar(struct Pokemon *mon)
{
    struct RadarPoint center = {RADAR_CENTER_X, RADAR_CENTER_Y};
    struct RadarPoint outer[NUM_STATS], inner[NUM_STATS], ivPoints[NUM_STATS];
    u32 ivs[NUM_STATS];
    u32 perfect = 0, totalWidth, perfectWidth;

    for (u32 i = 0; i < NUM_STATS; i++)
    {
        ivs[i] = GetMonData(mon, MON_DATA_HP_IV + sRadarAxes[i].stat);
        perfect += (ivs[i] == MAX_PER_STAT_IVS);
        outer[i] = GetRadarPoint(&sRadarAxes[i], 1, 1);
        inner[i] = GetRadarPoint(&sRadarAxes[i], 1, 2);
        ivPoints[i] = GetRadarPoint(&sRadarAxes[i], ivs[i], MAX_PER_STAT_IVS);
        PrintRadarLabel(&sRadarAxes[i], ivs[i]);
    }

    ConvertIntToDecimalStringN(gStringVar1, perfect, STR_CONV_MODE_LEFT_ALIGN, 1);
    StringExpandPlaceholders(gStringVar3, sText_CardPerfectIvs);
    ConvertIntToDecimalStringN(gStringVar1, Fusion_GetBoxMonIvTotal(&mon->box), STR_CONV_MODE_LEFT_ALIGN, 3);
    StringExpandPlaceholders(gStringVar4, sText_CardIvTotal);
    totalWidth = GetStringWidth(FONT_SMALL, gStringVar4, 0);
    perfectWidth = GetStringWidth(FONT_NORMAL, gStringVar3, 0);
    AddTextPrinterParameterized3(sCardWindowId, FONT_SMALL, WINDOW_WIDTH * 8 - CARD_LINE_X - totalWidth, 3,
                                 sCardTextColors, TEXT_SKIP_DRAW, gStringVar4);
    AddTextPrinterParameterized3(sCardWindowId, FONT_NORMAL, WINDOW_WIDTH * 8 - CARD_LINE_X - totalWidth - RADAR_LABEL_GAP - perfectWidth, 1,
                                 perfect > 0 ? sCardPerfectColors : sCardTextColors, TEXT_SKIP_DRAW, gStringVar3);

    for (u32 i = 0; i < NUM_STATS; i++)
    {
        u32 next = (i + 1) % NUM_STATS;

        DrawCardLine(center, outer[i], TEXT_COLOR_LIGHT_GRAY);
        DrawCardLine(outer[i], outer[next], TEXT_COLOR_LIGHT_GRAY);
        DrawCardLine(inner[i], inner[next], TEXT_COLOR_LIGHT_GRAY);
    }
    for (u32 i = 0; i < NUM_STATS; i++)
    {
        struct RadarPoint triangle[] = {center, ivPoints[i], ivPoints[(i + 1) % NUM_STATS]};

        FillCardTriangle(triangle, TEXT_COLOR_LIGHT_BLUE);
    }
    for (u32 i = 0; i < NUM_STATS; i++)
        DrawCardLine(ivPoints[i], ivPoints[(i + 1) % NUM_STATS], TEXT_COLOR_BLUE);
    for (u32 i = 0; i < NUM_STATS; i++)
    {
        if (ivs[i] == MAX_PER_STAT_IVS)
            FillWindowPixelRect(sCardWindowId, PIXEL_FILL(TEXT_COLOR_RED), ivPoints[i].x - 1, ivPoints[i].y - 1, 3, 3);
    }
}

static void DrawAppraisalCard(struct Pokemon *mon)
{
    struct FusionGenetics genetics;
    u32 line = 0;

    Fusion_GetMonGenetics(mon, &genetics);
    OpenCard();

    StringCopy(gStringVar1, GetSpeciesName(GetMonData(mon, MON_DATA_SPECIES)));
    ConvertIntToDecimalStringN(gStringVar2, GetMonData(mon, MON_DATA_LEVEL), STR_CONV_MODE_LEFT_ALIGN, 3);
    StringExpandPlaceholders(gStringVar4, sText_CardHeader);
    PrintCardHeader(gStringVar4, NULL);

    if (genetics.generation > 0)
    {
        ConvertIntToDecimalStringN(gStringVar1, genetics.generation, STR_CONV_MODE_LEFT_ALIGN, 2);
        StringExpandPlaceholders(gStringVar4, sText_CardGeneration);
        AddTextPrinterParameterized(sCardWindowId, FONT_SMALL, gStringVar4,
                                    CARD_LINE_X + CARD_COLUMN_WIDTH - GetStringWidth(FONT_SMALL, gStringVar4, 0),
                                    CARD_HEADER_HEIGHT, TEXT_SKIP_DRAW, NULL);
    }
    StringCopy(gStringVar1, gNaturesInfo[GetMonData(mon, MON_DATA_HIDDEN_NATURE)].name);
    StringExpandPlaceholders(gStringVar4, sText_CardNature);
    PrintCardColumnLine(line++, gStringVar4);

    AppendTypes(StringCopy(gStringVar4, sText_Types), GetMonData(mon, MON_DATA_SPECIES), genetics.extraType);
    PrintCardColumnLine(line++, gStringVar4);

    StringCopy(StringCopy(gStringVar4, sText_Ability), gAbilitiesInfo[GetMonAbility(mon)].name);
    PrintCardColumnLine(line++, gStringVar4);

    if (Fusion_CountPassives(&genetics) == 0)
    {
        StringCopy(StringCopy(gStringVar4, sText_Passives), sText_None);
        PrintCardColumnLine(line++, gStringVar4);
    }
    for (u32 i = 0; i < FUSION_MAX_PASSIVES; i++)
    {
        if (genetics.passives[i] == ABILITY_NONE)
            continue;
        StringCopy(StringCopy(gStringVar4, sText_Passive), gAbilitiesInfo[genetics.passives[i]].name);
        PrintCardColumnLine(line++, gStringVar4);
    }
    // Last, so the labels' text boxes never paint over the lines.
    DrawIvRadar(mon);
    ShowCard();
}

//
// Script interface. The selected Pokémon is returned in VAR_0x8004 (location),
// VAR_0x8005 (box) and VAR_0x8006 (slot); VAR_RESULT is TRUE if one was chosen.
//

static EWRAM_DATA struct FusionMonRef sParentA = {0};
static EWRAM_DATA struct FusionMonRef sParentB = {0};

static const u8 sText_ChooseToAppraise[] = _("Choose a Pokémon to appraise.");
static const u8 sText_ChooseParentA[] = _("Parent 1: keeps its slot.");
static const u8 sText_ChooseParentB[] = _("Parent 2: will be consumed.");
static const u8 sText_PreviewHeader[] = _("{STR_VAR_1} + {STR_VAR_2}");
static const u8 sText_PreviewLevel[] = _("Child: Lv{STR_VAR_3}, species of either parent");
static const u8 sText_PreviewIvs[] = _("IVs, nature: from either parent, IVs may rise");
static const u8 sText_PreviewPassives[] = _("Passive pool: {STR_VAR_1} ({STR_VAR_3}% each, up to 3)");
static const u8 sText_PreviewMoves[] = _("Move pool: {STR_VAR_1} ({STR_VAR_3}% each, up to 4)");
static const u8 sText_PreviewThirdType[] = _("Third type: {STR_VAR_1}% chance");
static const u8 sText_PreviewNoThirdType[] = _("Third type: not possible");
static const u8 sText_CheckInvalid[] = _("These Pokémon can't be fused.");
static const u8 sText_CheckLastPartyMon[] = _("You can't fuse away the last\nPokémon in your party.");
static const u8 sText_CheckHoldsMail[] = _("Please remove the Mail first.");
static const u8 sText_CheckBagFull[] = _("There's no room in the Bag for\nthe item Parent 2 is holding.");

static void ScriptSelectorCallback(bool32 selected, const struct FusionMonRef *ref)
{
    gSpecialVar_Result = selected;
    gSpecialVar_0x8004 = ref->location;
    gSpecialVar_0x8005 = ref->box;
    gSpecialVar_0x8006 = ref->slot;
    ScriptContext_Enable();
}

static void ParentASelectorCallback(bool32 selected, const struct FusionMonRef *ref)
{
    sParentA = *ref;
    ScriptSelectorCallback(selected, ref);
}

static void ParentBSelectorCallback(bool32 selected, const struct FusionMonRef *ref)
{
    sParentB = *ref;
    ScriptSelectorCallback(selected, ref);
}

void Fusion_ScriptCountMaterials(void)
{
    gSpecialVar_Result = Fusion_CountMaterials(NULL);
}

void Fusion_ScriptOpenAppraiseSelector(void)
{
    Fusion_OpenMaterialSelector(sText_ChooseToAppraise, NULL, ScriptSelectorCallback);
}

void Fusion_ScriptSelectParentA(void)
{
    Fusion_OpenMaterialSelector(sText_ChooseParentA, NULL, ParentASelectorCallback);
}

void Fusion_ScriptSelectParentB(void)
{
    Fusion_OpenMaterialSelector(sText_ChooseParentB, &sParentA, ParentBSelectorCallback);
}

// Shows the selected Pokémon's card; gStringVar1 holds its species name for the message.
void Fusion_ScriptShowAppraisalCard(void)
{
    struct FusionMonRef ref = {.location = gSpecialVar_0x8004, .box = gSpecialVar_0x8005, .slot = gSpecialVar_0x8006};
    struct Pokemon mon;

    if (!Fusion_ReadMonFromRef(&ref, &mon))
        return;
    DrawAppraisalCard(&mon);
    StringCopy(gStringVar1, GetSpeciesName(GetMonData(&mon, MON_DATA_SPECIES)));
}

// VAR_RESULT is the FusionCheckResult; on failure gStringVar4 holds the reason.
void Fusion_ScriptCheckParents(void)
{
    static const u8 *const sCheckTexts[] =
    {
        [FUSION_CHECK_INVALID] = sText_CheckInvalid,
        [FUSION_CHECK_LAST_PARTY_MON] = sText_CheckLastPartyMon,
        [FUSION_CHECK_HOLDS_MAIL] = sText_CheckHoldsMail,
        [FUSION_CHECK_BAG_FULL] = sText_CheckBagFull,
    };
    enum FusionCheckResult result = Fusion_CheckParents(&sParentA, &sParentB);

    gSpecialVar_Result = result;
    if (result != FUSION_CHECK_OK)
        StringCopy(gStringVar4, sCheckTexts[result]);
}

// Shows what can happen, never the actual result: the child is only rolled on confirmation.
// gStringVar2 holds parent 2's species name for the confirmation message.
void Fusion_ScriptShowPreviewCard(void)
{
    struct FusionPreview preview;
    struct Pokemon parentA, parentB;
    u32 line = 0;
    u8 *text;

    Fusion_ReadMonFromRef(&sParentA, &parentA);
    Fusion_ReadMonFromRef(&sParentB, &parentB);
    Fusion_BuildPreview(&parentA, &parentB, &preview);
    OpenCard();

    StringCopy(gStringVar1, GetSpeciesName(GetMonData(&parentA, MON_DATA_SPECIES)));
    StringCopy(gStringVar2, GetSpeciesName(GetMonData(&parentB, MON_DATA_SPECIES)));
    ConvertIntToDecimalStringN(gStringVar3, preview.level, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringExpandPlaceholders(gStringVar4, sText_PreviewHeader);
    PrintCardHeader(gStringVar4, NULL);
    StringExpandPlaceholders(gStringVar4, sText_PreviewLevel);
    PrintCardLine(line++, gStringVar4);
    PrintCardLine(line++, sText_PreviewIvs);

    text = StringCopy(gStringVar4, sText_Ability);
    text = StringCopy(text, gAbilitiesInfo[preview.mainAbilities[0]].name);
    if (preview.mainAbilities[1] != preview.mainAbilities[0])
        StringCopy(StringCopy(text, sText_Or), gAbilitiesInfo[preview.mainAbilities[1]].name);
    PrintCardLine(line++, gStringVar4);

    // gStringVar1/3 are reused below; parent 2's name stays in gStringVar2 for the script.
    ConvertIntToDecimalStringN(gStringVar1, preview.passivePoolSize, STR_CONV_MODE_LEFT_ALIGN, 2);
    ConvertIntToDecimalStringN(gStringVar3, FUSION_PASSIVE_INHERIT_CHANCE, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringExpandPlaceholders(gStringVar4, sText_PreviewPassives);
    PrintCardLine(line++, gStringVar4);
    ConvertIntToDecimalStringN(gStringVar1, preview.movePoolSize, STR_CONV_MODE_LEFT_ALIGN, 2);
    ConvertIntToDecimalStringN(gStringVar3, FUSION_MOVE_INHERIT_CHANCE, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringExpandPlaceholders(gStringVar4, sText_PreviewMoves);
    PrintCardLine(line++, gStringVar4);

    if (max(preview.thirdTypePoolSize[0], preview.thirdTypePoolSize[1]) > 0)
    {
        ConvertIntToDecimalStringN(gStringVar1, FUSION_THIRD_TYPE_CHANCE, STR_CONV_MODE_LEFT_ALIGN, 3);
        StringExpandPlaceholders(gStringVar4, sText_PreviewThirdType);
        PrintCardLine(line++, gStringVar4);
    }
    else
    {
        PrintCardLine(line++, sText_PreviewNoThirdType);
    }
    ShowCard();
}

// VAR_RESULT is TRUE on success; the child's card is then shown and gStringVar1 holds its name.
void Fusion_ScriptDoFusion(void)
{
    struct Pokemon child;

    gSpecialVar_Result = Fusion_Fuse(&sParentA, &sParentB, Random32(), &child);
    if (!gSpecialVar_Result)
    {
        StringCopy(gStringVar4, sText_CheckInvalid);
        return;
    }
    DrawAppraisalCard(&child);
    StringCopy(gStringVar1, GetSpeciesName(GetMonData(&child, MON_DATA_SPECIES)));
}

//
// Settings
//

static const u8 sText_ModeClassic[] = _("Classic");
static const u8 sText_ModeConvenient[] = _("Convenient");
static const u8 sText_ModeMaterial[] = _("Material");
static const u8 sText_AllGens[] = _("All gens");
static const u8 sText_Original[] = _("Original");
static const u8 sText_On[] = _("ON");
static const u8 sText_Off[] = _("OFF");

// gStringVar1: capture mode name, gStringVar2: Exp. Share state, gStringVar3: wild Pokémon.
void Fusion_ScriptBufferSettings(void)
{
    static const u8 *const sModeNames[FUSION_CAPTURE_MODE_COUNT] =
    {
        [FUSION_CAPTURE_MODE_CLASSIC] = sText_ModeClassic,
        [FUSION_CAPTURE_MODE_CONVENIENT] = sText_ModeConvenient,
        [FUSION_CAPTURE_MODE_MATERIAL] = sText_ModeMaterial,
    };

    StringCopy(gStringVar1, sModeNames[Fusion_GetCaptureMode()]);
    StringCopy(gStringVar2, FlagGet(FLAG_FUSION_EXP_SHARE) ? sText_On : sText_Off);
    StringCopy(gStringVar3, Fusion_IsWildRandomizerEnabled() ? sText_AllGens : sText_Original);
}

void Fusion_ScriptToggleWildRandomizer(void)
{
    if (FlagGet(FLAG_FUSION_WILD_ORIGINAL))
        FlagClear(FLAG_FUSION_WILD_ORIGINAL);
    else
        FlagSet(FLAG_FUSION_WILD_ORIGINAL);
}

// VAR_RESULT: the chosen FusionCaptureMode.
void Fusion_ScriptSetCaptureMode(void)
{
    if (gSpecialVar_Result < FUSION_CAPTURE_MODE_COUNT)
        Fusion_SetCaptureMode(gSpecialVar_Result);
}

void Fusion_ScriptToggleExpShare(void)
{
    if (FlagGet(FLAG_FUSION_EXP_SHARE))
        FlagClear(FLAG_FUSION_EXP_SHARE);
    else
        FlagSet(FLAG_FUSION_EXP_SHARE);
}
