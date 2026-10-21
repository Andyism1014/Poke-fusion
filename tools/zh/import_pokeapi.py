#!/usr/bin/env python3
"""Fills in the official Simplified Chinese names from PokeAPI's CSV data.

    python3 tools/zh/import_pokeapi.py CSV_DIR [--report FILE]

CSV_DIR holds files from
https://github.com/PokeAPI/pokeapi/tree/master/data/v2/csv (languages,
pokemon_species(_names), moves, move_names, abilities, ability_names, items,
item_names, types, type_names, natures, nature_names).

Each name becomes an entry in the PO file of the source file that defines it,
with that file as msgctxt, so the translation only applies there (the move
"Psychic" and the type "Psychic" have different Chinese names). Entries made
by this script carry the comment "#. PokeAPI"; re-running it only updates
those, so names that were fixed by hand stay fixed (remove the comment to make
an entry manual).

Names that don't fit the game's buffers are shortened with SHORT_NAMES below
and listed in the report.
"""

import argparse
import collections
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import zhlib  # noqa: E402
from zhlib import PoEntry, ROOT, TEXT_DIR, rel  # noqa: E402

ZH = "12"
EN = "9"
TAG = "PokeAPI"

# Byte limits (without the terminator). One Chinese character is two bytes.
LIMITS = {
    "species": 12,      # POKEMON_NAME_LENGTH (speciesName[], nicknames)
    "category": 12,     # categoryName[13]
    "ability": 16,      # ABILITY_NAME_LENGTH
    "type": 8,          # TYPE_NAME_LENGTH
    "item": 19,         # ITEM_NAME_LENGTH (20, sizeof includes the terminator)
    "item_plural": 21,  # ITEM_NAME_PLURAL_LENGTH
    "move": 16,         # MOVE_NAME_LENGTH (buffers, battle menus)
    "nature": 16,
}

# Official names that are too long for the game, and what to use instead.
SHORT_NAMES = {
}

# Names PokeAPI doesn't have. G-Max moves: the official names from Sword/Shield.
# The rest never had an official Simplified Chinese name (Gen 3 mail, FRLG key
# items, this hack's items) and are our own translations.
EXTRA_NAMES = {
    "move": {
        "G-Max Vine Lash": "超极巨灰飞鞭灭", "G-Max Wildfire": "超极巨地狱灭焰",
        "G-Max Cannonade": "超极巨水炮轰灭", "G-Max Befuddle": "超极巨幻影幻觉",
        "G-Max Volt Crash": "超极巨万雷轰顶", "G-Max Gold Rush": "超极巨特大金币",
        "G-Max Chi Strike": "超极巨会心一击", "G-Max Terror": "超极巨幻影幽魂",
        "G-Max Foam Burst": "超极巨激漩泡涡", "G-Max Resonance": "超极巨极光旋律",
        "G-Max Cuddle": "超极巨热情拥抱", "G-Max Replenish": "超极巨资源再生",
        "G-Max Malodor": "超极巨臭气冲天", "G-Max Meltdown": "超极巨液金熔击",
        "G-Max Drum Solo": "超极巨狂擂乱打", "G-Max Fireball": "超极巨破阵火球",
        "G-Max Hydrosnipe": "超极巨狙击神射", "G-Max Wind Rage": "超极巨旋风袭卷",
        "G-Max Gravitas": "超极巨天道七星", "G-Max Stonesurge": "超极巨岩阵以待",
        "G-Max Volcalith": "超极巨炎石喷发", "G-Max Tartness": "超极巨酸不溜丢",
        "G-Max Sweetness": "超极巨琼浆玉液", "G-Max Sandblast": "超极巨沙尘漫天",
        "G-Max Stun Shock": "超极巨晶光转转", "G-Max Centiferno": "超极巨百火焚野",
        "G-Max Smite": "超极巨天谴雷诛", "G-Max Snooze": "超极巨睡魔降临",
        "G-Max Finale": "超极巨幸福圆满", "G-Max Steelsurge": "超极巨钢铁阵法",
        "G-Max Depletion": "超极巨劣化衰变", "G-Max One Blow": "超极巨夺命一击",
        "G-Max Rapid Flow": "超极巨流水连击",
    },
    "item": {
        "Orange Mail": "橙色邮件", "Harbor Mail": "港口邮件", "Glitter Mail": "闪亮邮件",
        "Mech Mail": "机械邮件", "Wood Mail": "木纹邮件", "Wave Mail": "波浪邮件",
        "Bead Mail": "珠子邮件", "Shadow Mail": "影子邮件", "Tropic Mail": "热带邮件",
        "Dream Mail": "梦境邮件", "Fab Mail": "华丽邮件", "Retro Mail": "复古邮件",
        "HM08": "秘传学习器08", "Berry Pouch": "树果袋", "Powder Jar": "粉末瓶",
        "{POKEBLOCK} Case": "宝可方块盒", "Fame Checker": "名人查询器", "Teachy TV": "教学电视",
        "Mystic Ticket": "神秘船票", "Aurora Ticket": "极光船票", "Old Sea Map": "古老海图",
        "Magma Emblem": "熔岩队徽章", "Bike Voucher": "自行车兑换券", "Tri-Pass": "三岛通行证",
        "Rainbow Pass": "彩虹通行证", "Ruby": "红宝石", "Sapphire": "蓝宝石",
        "Fusion Terminal": "炼妖终端", "Passive Crystal": "被动水晶", "Type Prism": "属性棱镜",
    },
}
EXTRA_TAG = "not an official name"


def read_csv(d, name):
    with open(os.path.join(d, name + ".csv"), encoding="utf-8") as f:
        return list(csv.DictReader(f))


def names_by_id(rows, id_field, lang, field="name"):
    return {r[id_field]: r[field] for r in rows if r["local_language_id"] == lang and r[field]}


def norm(s):
    s = s.replace("é", "e").replace("’", "'").lower()
    return re.sub(r"[^a-z0-9]", "", s)


# A data block is "[CONSTANT] =" or a "#define MACRO(...)" that builds one (e.g. the Silvally forms).
_BLOCK = re.compile(r"^\s*(?:\[((?:SPECIES|MOVE|ABILITY|ITEM|TYPE|NATURE)_\w+)\]\s*=|#define\s+(\w+))", re.M)
_FIELD = re.compile(r"\.(\w+)\s*=\s*(?:_|COMPOUND_STRING|ITEM_NAME|ITEM_PLURAL_NAME)\(\s*\"((?:[^\"\\]|\\.)*)\"\s*\)")
_NATDEX = re.compile(r"\.natDexNum\s*=\s*(NATIONAL_DEX_\w+)")


def parse_blocks(path):
    """Yields (CONSTANT, {field: literal}, natdex) for the [CONSTANT] = {...} blocks of a data file."""
    with open(os.path.join(ROOT, path), encoding="utf-8") as f:
        text = f.read()
    starts = list(_BLOCK.finditer(text))
    for i, m in enumerate(starts):
        end = starts[i + 1].start() if i + 1 < len(starts) else len(text)
        chunk = text[m.end():end]
        fields = {}
        for fm in _FIELD.finditer(chunk):
            fields.setdefault(fm.group(1), fm.group(2))
        nd = _NATDEX.search(chunk)
        yield m.group(1) or m.group(2), fields, nd.group(1) if nd else None


def national_dex_numbers():
    with open(os.path.join(ROOT, "include", "constants", "pokedex.h"), encoding="utf-8") as f:
        text = f.read()
    m = re.search(r"enum\s+NationalDexOrder\s*\{(.*?)\}", text, re.S) or re.search(r"enum[^{]*\{(.*?NATIONAL_DEX_BULBASAUR.*?)\}", text, re.S)
    numbers = {}
    n = 0
    for tok in re.findall(r"(NATIONAL_DEX_\w+)\s*(=\s*[^,]+)?,", m.group(1)):
        name, value = tok
        if value:
            v = value.strip("= ").strip()
            n = numbers[v] if v in numbers else int(v, 0)
        numbers[name] = n
        n += 1
    return numbers


class Importer:
    def __init__(self, csv_dir):
        self.d = csv_dir
        self.entries = collections.defaultdict(dict)   # po path -> {(ctx, msgid): msgstr}
        self.report = collections.defaultdict(list)
        self.stats = collections.Counter()

    def add(self, category, src, english, chinese, const, tag=TAG):
        limit = LIMITS[category]
        key = (category, english)
        if key in SHORT_NAMES:
            self.report["shortened"].append("%s %s: %s -> %s" % (category, english, chinese, SHORT_NAMES[key]))
            chinese = SHORT_NAMES[key]
        size = sum(2 if zhlib.is_wide_char(c) or ord(c) > 0x2E7F else 1 for c in chinese)
        if size > limit:
            self.report["too long"].append("%s %s (%s): %s is %d bytes > %d" % (category, english, const, chinese, size, limit))
            self.stats[category + " too long"] += 1
            return
        if '"' in chinese or "\\" in chinese:
            chinese = chinese.replace('"', "“")
        # Full-width letters and digits (ＰＰ, Ｘ, １００) become half-width: they read
        # the same next to the 12px glyphs and take half the space.
        chinese = "".join(chr(ord(c) - 0xFEE0) if "Ａ" <= c <= "Ｚ" or "ａ" <= c <= "ｚ" or "０" <= c <= "９" else c
                          for c in chinese)
        po = os.path.join(TEXT_DIR, src + ".po")
        self.entries[po][(src, english)] = (chinese, tag)
        self.stats[category] += 1

    def missing(self, category, const, english, src=None):
        extra = EXTRA_NAMES.get(category, {}).get(english)
        if extra and src:
            self.add(category, src, english, extra, const, TAG if category == "move" else EXTRA_TAG)
            self.stats[category + " from EXTRA_NAMES"] += 1
            return
        self.report["no official name"].append("%s %s %r" % (category, const, english))
        self.stats[category + " missing"] += 1

    # -- species --------------------------------------------------------
    def species(self):
        rows = read_csv(self.d, "pokemon_species_names")
        zh = names_by_id(rows, "pokemon_species_id", ZH)
        genus = names_by_id(rows, "pokemon_species_id", ZH, "genus")
        dex = national_dex_numbers()
        folder = "src/data/pokemon/species_info"
        for fn in sorted(os.listdir(os.path.join(ROOT, folder))):
            if not fn.endswith(".h"):
                continue
            src = folder + "/" + fn
            for const, fields, natdex in parse_blocks(src):
                if not natdex or natdex not in dex:
                    continue
                sid = str(dex[natdex])
                if "speciesName" in fields:
                    if sid in zh:
                        self.add("species", src, fields["speciesName"], zh[sid], const)
                    else:
                        self.missing("species", const, fields["speciesName"])
                if "categoryName" in fields and sid in genus:
                    g = genus[sid]
                    if g.endswith("宝可梦"):
                        g = g[:-3]
                    self.add("category", src, fields["categoryName"], g, const)

    # -- generic ----------------------------------------------------------
    def simple(self, category, src, prefix, table, names_table, id_field, fields=("name",), extra=None):
        ids = {r["identifier"]: r["id"] for r in read_csv(self.d, table)}
        rows = read_csv(self.d, names_table)
        zh = names_by_id(rows, id_field, ZH)
        en = names_by_id(rows, id_field, EN)
        by_en = {}
        for i, n in en.items():
            by_en.setdefault(norm(n), i)
        for const, f, _ in parse_blocks(src):
            if not const.startswith(prefix + "_"):
                continue
            english = f.get(fields[0])
            if english is None:
                continue
            ident = const[len(prefix) + 1:].lower().replace("_", "-")
            cand = [ident, ident + "--physical"]
            if extra:
                cand = extra(ident) + cand
            iid = next((ids[c] for c in cand if c in ids and ids[c] in zh), None)
            if iid is None:
                iid = by_en.get(norm(english))
                if iid not in zh:
                    iid = None
            if iid is None:
                self.missing(category, const, english, src)
                continue
            self.add(category, src, english, zh[iid], const)
            for other in fields[1:]:
                if other in f:
                    self.add(category + "_plural", src, f[other], zh[iid], const)

    def run(self):
        self.species()
        self.simple("move", "src/data/moves_info.h", "MOVE", "moves", "move_names", "move_id",
                    extra=lambda i: [i.replace("g-max-", "g-max-")])
        self.simple("ability", "src/data/abilities.h", "ABILITY", "abilities", "ability_names", "ability_id")
        self.simple("item", "src/data/items.h", "ITEM", "items", "item_names", "item_id", fields=("name", "pluralName"))
        self.simple("type", "src/data/types_info.h", "TYPE", "types", "type_names", "type_id")
        self.simple("nature", "src/pokemon.c", "NATURE", "natures", "nature_names", "nature_id")

    def write(self):
        changed = 0
        for po, names in sorted(self.entries.items()):
            if os.path.exists(po):
                header, entries = zhlib.read_po(po)
            else:
                entries = []
            index = {(e.msgctxt, e.msgid): e for e in entries}
            for (ctx, msgid), (msgstr, tag) in names.items():
                e = index.get((ctx, msgid))
                if e is None:
                    e = PoEntry(msgid, msgstr, ctx)
                    entries.append(e)
                    index[(ctx, msgid)] = e
                elif not (TAG in e.auto or EXTRA_TAG in e.auto or not e.msgstr):
                    continue  # fixed by hand
                e.msgstr = msgstr
                e.auto = [a for a in e.auto if a not in (TAG, EXTRA_TAG)] + [tag]
                e.obsolete = False
            r = os.path.relpath(po, TEXT_DIR)[:-3]
            data = zhlib.format_po(
                "Simplified Chinese translation of %s.\n"
                "Generated by `python3 tools/zh/zh.py extract`; fill in the msgstr lines.\n"
                "See docs/fusion/CHINESE.md for the rules (line width, \\n \\l \\p, placeholders).\n" % r, entries)
            if zhlib.write_if_changed(po, data + ("\n" if not data.endswith("\n") else "")):
                changed += 1
        return changed


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv_dir")
    ap.add_argument("--report", help="write the list of missing/shortened names here")
    args = ap.parse_args()
    imp = Importer(args.csv_dir)
    imp.run()
    changed = imp.write()
    for k in sorted(imp.stats):
        print("%-24s %d" % (k, imp.stats[k]))
    print("%d PO files written" % changed)
    lines = []
    for k, v in imp.report.items():
        lines.append("== %s (%d)" % (k, len(v)))
        lines.extend("  " + x for x in v)
    if args.report:
        with open(args.report, "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
    else:
        print("\n".join(lines))


if __name__ == "__main__":
    main()
