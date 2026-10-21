#include "global.h"
#include "battle.h"
#include "main.h"
#include "menu.h"
#include "pokemon.h"
#include "sound.h"
#include "string_util.h"
#include "text.h"
#include "window.h"
#include "constants/songs.h"
#include "fusion/fusion.h"

// Ability overview panels: the main ability and every passive, each with its description, on
// one screen. Shown from the summary screen (SELECT on the Info page) and from the battle action
// menu (FUSION_BATTLE_ABILITY_PANEL_BUTTON) for the Pokémon on the field.

static const u8 sText_AbilityLabel[] = _("Ability: ");
static const u8 sText_PassiveLabel[] = _("Passive {STR_VAR_1}: ");

// slot 0 is the main ability, slot N is passive N. The name is fitted into nameWidth pixels
// from style->x, the description into the window width minus style->x on both sides.
void Fusion_PrintAbilityPanelEntry(u32 windowId, const struct FusionAbilityPanelStyle *style, u32 slot, enum Ability ability, u32 y, u32 nameWidth)
{
    const u8 *name = gAbilitiesInfo[ability].name;
    const u8 *desc = gAbilitiesInfo[ability].description;
    u32 labelWidth, descWidth;
    u8 label[16];

    if (slot == 0)
    {
        StringCopy(label, sText_AbilityLabel);
    }
    else
    {
        ConvertIntToDecimalStringN(gStringVar1, slot, STR_CONV_MODE_LEFT_ALIGN, 1);
        StringExpandPlaceholders(label, sText_PassiveLabel);
    }
    labelWidth = GetStringWidth(FONT_NORMAL, label, 0);
    descWidth = WindowWidthPx(windowId) - 2 * style->x - style->descX;

    AddTextPrinterParameterized4(windowId, FONT_NORMAL, style->x, y, 0, 0, style->labelColors, TEXT_SKIP_DRAW, label);
    AddTextPrinterParameterized4(windowId, GetFontIdToFit(name, FONT_NORMAL, 0, nameWidth - labelWidth), style->x + labelWidth, y,
                                 0, 0, style->textColors, TEXT_SKIP_DRAW, name);
    AddTextPrinterParameterized4(windowId, GetFontIdToFit(desc, style->descFontId, 0, descWidth), style->x + style->descX, y + style->descY,
                                 0, 0, style->textColors, TEXT_SKIP_DRAW, desc);
}

// Battle panel: covers the battle scene above the action menu (BG0 rows 20-33 are blank while
// the action menu is shown). Its tiles reuse the move menu windows, which are redrawn when the
// move menu opens, and stay clear of the message box frame tiles at DLG_WINDOW_BASE_TILE_NUM.
#define BATTLE_PANEL_MARGIN 3

static const struct WindowTemplate sBattlePanelWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = 0,
    .tilemapTop = 20,
    .width = DISPLAY_TILE_WIDTH,
    .height = 14,
    .paletteNum = DLG_WINDOW_PALETTE_NUM,
    .baseBlock = 0x0220,
};

static const u8 sBattlePanelTextColors[3]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};
static const u8 sBattlePanelLabelColors[3] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_BLUE, TEXT_COLOR_LIGHT_BLUE};
static const u8 sBattlePanelFoeColors[3]   = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_RED, TEXT_COLOR_LIGHT_RED};

static const struct FusionAbilityPanelStyle sBattlePanelStyle =
{
    .x = BATTLE_PANEL_MARGIN + 2,
    .descX = 8,
    .descY = 13,
    .descFontId = FONT_SMALL,
    .textColors = sBattlePanelTextColors,
    .labelColors = sBattlePanelLabelColors,
};

static const u8 sText_BattlePanelHeader[] = _("{LEFT_ARROW}{STR_VAR_1}{RIGHT_ARROW}");
static const u8 sText_BattlePanelFoe[] = _("Foe {STR_VAR_2}");

static EWRAM_DATA u8 sBattlePanelWindowId = 0;
static EWRAM_DATA u8 sBattlePanelBattler = 0;

// Main ability first, then the passives the battle engine would use (see GetBattlerPassive),
// ignoring suppression so the panel always lists what the Pokémon carries.
static u32 GetBattlerAbilityList(enum BattlerId battler, enum Ability *abilities)
{
    u32 i, j, count = 0;

    abilities[count++] = gBattleMons[battler].ability;
    for (i = 0; i < FUSION_MAX_PASSIVES; i++)
    {
        enum Ability passive = gBattleMons[battler].passives[i];

        if (passive == ABILITY_NONE || passive >= ABILITIES_COUNT)
            continue;
        for (j = 0; j < count && abilities[j] != passive; j++)
            ;
        if (j == count)
            abilities[count++] = passive;
    }
    return count;
}

static void DrawBattlePanel(void)
{
    u32 windowId = sBattlePanelWindowId, battler = sBattlePanelBattler;
    u32 width = WindowWidthPx(windowId), height = sBattlePanelWindowTemplate.height * 8;
    u32 i, count, entryHeight, headerX;
    enum Ability abilities[1 + FUSION_MAX_PASSIVES];
    u8 *header = gStringVar4;

    FillWindowPixelBuffer(windowId, PIXEL_FILL(TEXT_COLOR_WHITE));
    FillWindowPixelRect(windowId, PIXEL_FILL(TEXT_COLOR_LIGHT_GRAY), 1, 1, width - 2, 1);
    FillWindowPixelRect(windowId, PIXEL_FILL(TEXT_COLOR_LIGHT_GRAY), 1, height - 2, width - 2, 1);
    FillWindowPixelRect(windowId, PIXEL_FILL(TEXT_COLOR_LIGHT_GRAY), 1, 1, 1, height - 2);
    FillWindowPixelRect(windowId, PIXEL_FILL(TEXT_COLOR_LIGHT_GRAY), width - 2, 1, 1, height - 2);

    StringCopy_Nickname(gStringVar2, gBattleMons[battler].nickname);
    if (IsOnPlayerSide(battler))
        StringCopy(gStringVar1, gStringVar2);
    else
        StringExpandPlaceholders(gStringVar1, sText_BattlePanelFoe);
    StringExpandPlaceholders(header, sText_BattlePanelHeader);
    headerX = width - BATTLE_PANEL_MARGIN - 2 - GetStringWidth(FONT_NORMAL, header, 0);
    AddTextPrinterParameterized4(windowId, FONT_NORMAL, headerX, BATTLE_PANEL_MARGIN, 0, 0,
                                 IsOnPlayerSide(battler) ? sBattlePanelLabelColors : sBattlePanelFoeColors, TEXT_SKIP_DRAW, header);

    count = GetBattlerAbilityList(battler, abilities);
    entryHeight = (height - 2 * BATTLE_PANEL_MARGIN) / count;
    if (entryHeight > 28)
        entryHeight = 28;
    for (i = 0; i < count; i++)
    {
        u32 nameWidth = (i == 0 ? headerX - 4 : width - BATTLE_PANEL_MARGIN) - sBattlePanelStyle.x;
        Fusion_PrintAbilityPanelEntry(windowId, &sBattlePanelStyle, i, abilities[i], BATTLE_PANEL_MARGIN + i * entryHeight, nameWidth);
    }
    CopyWindowToVram(windowId, COPYWIN_GFX);
}

void Fusion_OpenBattleAbilityPanel(u32 battler)
{
    LoadMessageBoxAndBorderGfx(); // For the palette.
    sBattlePanelWindowId = AddWindow(&sBattlePanelWindowTemplate);
    sBattlePanelBattler = battler;
    PutWindowTilemap(sBattlePanelWindowId);
    DrawBattlePanel();
    CopyWindowToVram(sBattlePanelWindowId, COPYWIN_MAP);
}

// Cycles through the Pokémon on the field, starting from the one whose action is being chosen.
static void ShowNextBattler(s32 delta)
{
    u32 i, battler = sBattlePanelBattler;

    for (i = 0; i < gBattlersCount; i++)
    {
        battler = (battler + gBattlersCount + delta) % gBattlersCount;
        if (IsBattlerAlive(battler))
            break;
    }
    if (battler == sBattlePanelBattler)
        return;
    PlaySE(SE_SELECT);
    sBattlePanelBattler = battler;
    DrawBattlePanel();
}

// Returns TRUE once the panel is closed.
bool32 Fusion_HandleBattleAbilityPanelInput(void)
{
    if (JOY_NEW(DPAD_LEFT))
    {
        ShowNextBattler(-1);
    }
    else if (JOY_NEW(DPAD_RIGHT))
    {
        ShowNextBattler(1);
    }
    else if (JOY_NEW(A_BUTTON | B_BUTTON | FUSION_BATTLE_ABILITY_PANEL_BUTTON))
    {
        PlaySE(SE_SELECT);
        ClearWindowTilemap(sBattlePanelWindowId);
        CopyWindowToVram(sBattlePanelWindowId, COPYWIN_MAP);
        RemoveWindow(sBattlePanelWindowId);
        return TRUE;
    }
    return FALSE;
}
