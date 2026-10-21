#ifndef GUARD_TEXT_CJK_H
#define GUARD_TEXT_CJK_H

// Two-byte CJK text, used by the Simplified Chinese build (`make chinese`).
// See docs/fusion/CHINESE.md. The English build never defines FUSION_CHINESE,
// so none of this is compiled into it.
//
// A CJK character is a lead byte followed by a trail byte:
//   lead:  0x3B-0x4F or 0x87-0x9F. These glyph slots are blank in every Latin
//          font, so English text never contains them.
//   trail: 0x01-0xF6. Never EOS or a control code (0xF7-0xFF), and never
//          CHAR_SPACE, which some string code rewrites.
// Keep in sync with CJK_LEADS etc. in tools/zh/zhlib.py.

#ifndef FUSION_CHINESE
#define FUSION_CHINESE 0
#endif

#define CJK_LEAD_A_FIRST    0x3B
#define CJK_LEAD_A_LAST     0x4F
#define CJK_LEAD_B_FIRST    0x87
#define CJK_LEAD_B_LAST     0x9F
#define CJK_TRAIL_FIRST     0x01
#define CJK_TRAIL_LAST      0xF6
#define CJK_TRAILS_PER_LEAD (CJK_TRAIL_LAST - CJK_TRAIL_FIRST + 1)

// Width of a full-width glyph in FONT_NORMAL, for layout code.
#define CJK_GLYPH_WIDTH     12

static inline bool32 IsCJKLeadByte(u32 c)
{
#if FUSION_CHINESE
    return (c >= CJK_LEAD_A_FIRST && c <= CJK_LEAD_A_LAST)
        || (c >= CJK_LEAD_B_FIRST && c <= CJK_LEAD_B_LAST);
#else
    return FALSE;
#endif
}

static inline bool32 IsCJKTrailByte(u32 c)
{
    return c >= CJK_TRAIL_FIRST && c <= CJK_TRAIL_LAST;
}

// TRUE if str points to a complete two-byte character.
static inline bool32 IsCJKChar(const u8 *str)
{
    return IsCJKLeadByte(str[0]) && IsCJKTrailByte(str[1]);
}

static inline u32 GetCJKGlyphIndex(u32 lead, u32 trail)
{
    u32 row = (lead <= CJK_LEAD_A_LAST) ? lead - CJK_LEAD_A_FIRST
                                        : lead - CJK_LEAD_B_FIRST + (CJK_LEAD_A_LAST - CJK_LEAD_A_FIRST + 1);
    return row * CJK_TRAILS_PER_LEAD + trail - CJK_TRAIL_FIRST;
}

#if FUSION_CHINESE
void DecompressGlyph_CJK(u32 glyphIndex, u32 fontId);
u32 GetCJKGlyphWidth(u32 glyphIndex, u32 fontId);
bool32 IsCJKNoLineStartChar(const u8 *str);
#endif

#endif // GUARD_TEXT_CJK_H
