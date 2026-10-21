# Simplified Chinese build (`make chinese`). See docs/fusion/CHINESE.md.
#
# tools/zh/zh.py turns the translations in text_zh/ into build/zh/:
#   translations.tsv  the dictionary preproc uses (-t) to replace English text
#   charmap.txt       full-width glyph codes for translated text (-z)
#   cjk_font.h, cjk_glyphs.bin, cjk_widths.bin   the font, used by src/text_cjk.c
#   deps/*.hash       one per object, changes only when that object's translations do
# The script only rewrites files whose contents change, so editing a
# translation only rebuilds the objects that use it.

ifeq ($(CHINESE),1)

ZH_TOOL      := tools/zh/zh.py
ZH_STAMP     := $(ZH_DIR)/.stamp
ZH_PO_FILES  := $(shell find text_zh -name '*.po' 2>/dev/null)
# Sources are scanned to know which object uses which text.
ZH_SCANNED   := $(shell find src include data -name '*.[ch]' -o -name '*.inc' -o -name '*.s' 2>/dev/null)
ZH_GENERATED := $(ZH_DIR)/translations.tsv $(ZH_DIR)/charmap.txt $(ZH_DIR)/cjk_font.h $(ZH_DIR)/cjk_glyphs.bin $(ZH_DIR)/cjk_widths.bin

$(ZH_STAMP): $(ZH_PO_FILES) $(wildcard tools/zh/*.py) $(wildcard graphics/fonts/zh/*.hex) charmap.txt $(ZH_SCANNED)
	python3 $(ZH_TOOL) build --out $(ZH_DIR)
	@touch $@

$(ZH_GENERATED): $(ZH_STAMP) ;
$(ZH_DIR)/deps/%.hash: $(ZH_STAMP) ;

$(C_OBJS): $(C_BUILDDIR)/%.o: $(ZH_DIR)/deps/$(C_SUBDIR)/%.c.hash | $(ZH_GENERATED)
$(DATA_ASM_OBJS): $(DATA_ASM_BUILDDIR)/%.o: $(ZH_DIR)/deps/$(DATA_ASM_SUBDIR)/%.s.hash | $(ZH_GENERATED)
$(C_ASM_OBJS) $(TEST_OBJS): | $(ZH_GENERATED)
$(C_BUILDDIR)/text_cjk.o: $(ZH_DIR)/cjk_font.h $(ZH_DIR)/cjk_glyphs.bin $(ZH_DIR)/cjk_widths.bin

endif
