"""Shared helpers for the Simplified Chinese build (see docs/fusion/CHINESE.md).

Everything here is pure Python 3 with no third-party dependencies, so the
build keeps working on a bare toolchain install.

Main pieces:
  * PO reading/writing (the translation files under text_zh/).
  * Scanners that find translatable text in the sources, mirroring exactly
    what tools/preproc does in the Chinese build:
      - C:   string literals inside _("..."), __("...") and COMPOUND_STRING("...")
      - asm: a label followed by `.string` lines (one block per label)
  * The CJK two-byte encoding shared with include/text_cjk.h.
"""

import os
import re
import sys
import unicodedata

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
TEXT_DIR = os.path.join(ROOT, "text_zh")
FONT_HEX = os.path.join(ROOT, "graphics", "fonts", "zh", "cjk_12px.hex")
FONT_HEX_EXTRA = os.path.join(ROOT, "graphics", "fonts", "zh", "cjk_12px_extra.hex")

# ---------------------------------------------------------------------------
# Encoding. Keep in sync with include/text_cjk.h.
#
# A CJK character is two bytes: LEAD, TRAIL.
#   LEAD  is one of the bytes in CJK_LEADS. They are glyph slots that are blank
#         in every Latin font, so English text never uses them.
#   TRAIL is 0x01..0xF6. It never collides with EOS or the control codes
#         (0xF7..0xFF), and never with CHAR_SPACE (0x00), which some string code
#         rewrites (e.g. battle messages turn spaces into NBSP).
# The glyph index is LEAD_INDEX * CJK_TRAILS_PER_LEAD + (TRAIL - CJK_TRAIL_MIN).
# ---------------------------------------------------------------------------
CJK_LEADS = list(range(0x3B, 0x50)) + list(range(0x87, 0xA0))
CJK_TRAIL_MIN = 0x01
CJK_TRAIL_MAX = 0xF6
CJK_TRAILS_PER_LEAD = CJK_TRAIL_MAX - CJK_TRAIL_MIN + 1
CJK_MAX_GLYPHS = len(CJK_LEADS) * CJK_TRAILS_PER_LEAD


def cjk_encode_index(index):
    lead = CJK_LEADS[index // CJK_TRAILS_PER_LEAD]
    trail = CJK_TRAIL_MIN + index % CJK_TRAILS_PER_LEAD
    return lead, trail


def is_wide_char(ch):
    """True for characters that get a 12px CJK glyph in translated text."""
    cp = ord(ch)
    if 0x2E80 <= cp <= 0x9FFF or 0xF900 <= cp <= 0xFAFF or 0x20000 <= cp:
        return True
    if 0xFF01 <= cp <= 0xFF60 or 0xFFE0 <= cp <= 0xFFE6:
        return True
    return ch in WIDE_PUNCTUATION


# Punctuation outside the CJK blocks that should still be full-width in Chinese
# text. In untranslated (English) text they keep their Latin glyphs.
WIDE_PUNCTUATION = set("“”‘’…—–·•×÷№§※→←↑↓★☆○●◎◇◆□■△▲※〃々〆〇")

# ---------------------------------------------------------------------------
# PO files
#
# The files follow PO syntax, but strings are copied verbatim from the game
# sources: \n, \l, \p, {PLAYER}, \" etc. are the game's escapes and are never
# unescaped. (So don't run msgfmt on them.)
# ---------------------------------------------------------------------------


class PoEntry:
    __slots__ = ("msgctxt", "msgid", "msgstr", "comments", "auto", "refs", "flags", "obsolete")

    def __init__(self, msgid="", msgstr="", msgctxt=None):
        self.msgctxt = msgctxt
        self.msgid = msgid
        self.msgstr = msgstr
        self.comments = []   # "# " translator comments
        self.auto = []       # "#." extracted comments
        self.refs = []       # "#:" references
        self.flags = []      # "#," flags (fuzzy, ...)
        self.obsolete = False

    @property
    def fuzzy(self):
        return "fuzzy" in self.flags

    def key(self):
        return (self.msgctxt, self.msgid)


_PO_STR = re.compile(r'^"(.*)"\s*$')


def _po_unquote(line, path, lineno):
    m = _PO_STR.match(line)
    if not m:
        raise SyntaxError("%s:%d: expected a quoted string" % (path, lineno))
    return m.group(1)


def read_po(path):
    """Returns (header_entry_or_None, [PoEntry])."""
    entries = []
    cur = PoEntry()
    field = None
    have_content = False

    def flush():
        nonlocal cur, field, have_content
        if have_content:
            entries.append(cur)
        cur = PoEntry()
        field = None
        have_content = False

    with open(path, encoding="utf-8") as f:
        lines = f.read().split("\n")
    for lineno, raw in enumerate(lines, 1):
        line = raw.strip()
        if not line:
            if have_content and field is not None:
                flush()
            continue
        obsolete = False
        if line.startswith("#~"):
            obsolete = True
            line = line[2:].strip()
            if not line:
                continue
        if line.startswith("#"):
            if field is not None:
                flush()
            if line.startswith("#."):
                cur.auto.append(line[2:].strip())
            elif line.startswith("#:"):
                cur.refs.extend(line[2:].split())
            elif line.startswith("#,"):
                cur.flags.extend(x.strip() for x in line[2:].split(",") if x.strip())
            elif line.startswith("#|"):
                pass  # previous msgid: informational only
            else:
                cur.comments.append(line[1:].strip())
            have_content = True
            continue
        if obsolete:
            cur.obsolete = True
        for kw in ("msgctxt", "msgid_plural", "msgid", "msgstr[0]", "msgstr"):
            if line.startswith(kw + " ") or line.startswith(kw + '"'):
                rest = line[len(kw):].strip()
                if kw == "msgctxt" and field is not None and field != "comment":
                    flush()
                    if obsolete:
                        cur.obsolete = True
                elif kw == "msgid" and field in ("msgid", "msgstr"):
                    flush()
                    if obsolete:
                        cur.obsolete = True
                field = {"msgstr[0]": "msgstr", "msgid_plural": "skip"}.get(kw, kw)
                value = _po_unquote(rest, path, lineno)
                if field == "msgctxt":
                    cur.msgctxt = value
                elif field == "msgid":
                    cur.msgid = value
                elif field == "msgstr":
                    cur.msgstr = value
                have_content = True
                break
        else:
            if line.startswith('"'):
                value = _po_unquote(line, path, lineno)
                if field == "msgctxt":
                    cur.msgctxt += value
                elif field == "msgid":
                    cur.msgid += value
                elif field == "msgstr":
                    cur.msgstr += value
                elif field is None:
                    raise SyntaxError("%s:%d: string outside of an entry" % (path, lineno))
            else:
                raise SyntaxError("%s:%d: cannot parse line: %s" % (path, lineno, raw))
    flush()

    header = None
    if entries and entries[0].msgid == "" and entries[0].msgctxt is None:
        header = entries.pop(0)
    return header, entries


_BREAK_AFTER = re.compile(r'(\\n|\\l|\\p)')


def _po_split(value, width=76):
    """Splits a raw string into PO continuation lines after \\n, \\l and \\p."""
    parts = []
    start = 0
    for m in _BREAK_AFTER.finditer(value):
        # don't split an escaped backslash like "\\n"
        i = m.start()
        nback = 0
        while i - 1 - nback >= 0 and value[i - 1 - nback] == "\\":
            nback += 1
        if nback % 2:
            continue
        parts.append(value[start:m.end()])
        start = m.end()
    if start < len(value):
        parts.append(value[start:])
    return parts


def _po_format_field(name, value):
    if "\t" in value or "\n" in value:
        raise ValueError("tab/newline inside a PO string: %r" % value)
    parts = _po_split(value)
    if len(parts) <= 1:
        return ['%s "%s"' % (name, value)]
    return ['%s ""' % name] + ['"%s"' % p for p in parts]


def format_po(header_text, entries):
    out = []
    if header_text:
        for line in header_text.rstrip("\n").split("\n"):
            out.append("# " + line if line else "#")
        out.append('msgid ""')
        out.append('msgstr ""')
        out.append('"Content-Type: text/plain; charset=UTF-8\\n"')
        out.append("")
    for e in entries:
        prefix = "#~ " if e.obsolete else ""
        for c in e.comments:
            out.append("# " + c if c else "#")
        for c in e.auto:
            out.append("#. " + c)
        if e.refs:
            out.append("#: " + " ".join(e.refs))
        if e.flags:
            out.append("#, " + ", ".join(e.flags))
        if e.msgctxt is not None:
            out.extend(prefix + l for l in _po_format_field("msgctxt", e.msgctxt))
        out.extend(prefix + l for l in _po_format_field("msgid", e.msgid))
        out.extend(prefix + l for l in _po_format_field("msgstr", e.msgstr))
        out.append("")
    return "\n".join(out)


def write_if_changed(path, data, binary=False):
    mode = "rb" if binary else "r"
    try:
        with open(path, mode, **({} if binary else {"encoding": "utf-8"})) as f:
            if f.read() == data:
                return False
    except FileNotFoundError:
        pass
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "wb" if binary else "w", **({} if binary else {"encoding": "utf-8", "newline": "\n"})) as f:
        f.write(data)
    os.replace(tmp, path)
    return True


def po_files():
    result = []
    for dirpath, dirnames, filenames in os.walk(TEXT_DIR):
        dirnames.sort()
        for fn in sorted(filenames):
            if fn.endswith(".po"):
                result.append(os.path.join(dirpath, fn))
    return result


def rel(path):
    return os.path.relpath(path, ROOT).replace(os.sep, "/")


# Kinds of entries:
#   KIND_C   -- C string literal. msgid = English literal (raw), msgctxt = None
#               or a source path ("src/data/moves_info.h") to translate the
#               literal differently in that one file.
#   KIND_ASM -- script text block. msgctxt = label (or "path:label" when the
#               label is ambiguous), msgid = the English text of the block.
KIND_C = "c"
KIND_ASM = "s"


def po_kind(path):
    """Script text lives under text_zh/scripts/, everything else is C strings."""
    r = os.path.relpath(path, TEXT_DIR).replace(os.sep, "/")
    return KIND_ASM if r.startswith("scripts/") else KIND_C


def load_translations(paths=None, warn=None):
    """Loads every PO file. Returns dict kind -> {(ctx, key): (msgstr, path, entry)}."""
    if paths is None:
        paths = po_files()
    result = {KIND_C: {}, KIND_ASM: {}}
    for path in paths:
        kind = po_kind(path)
        _, entries = read_po(path)
        table = result[kind]
        for e in entries:
            if e.obsolete:
                continue
            if kind == KIND_ASM:
                if not e.msgctxt:
                    raise SyntaxError("%s: script entry without msgctxt (label): %r" % (rel(path), e.msgid[:40]))
                k = (None, e.msgctxt)
            else:
                k = (e.msgctxt, e.msgid)
            prev = table.get(k)
            if prev is not None and prev[0] and e.msgstr and prev[0] != e.msgstr and warn:
                warn("conflicting translations for %r in %s and %s (using the first)" % (k[1][:50], rel(prev[1]), rel(path)))
            if prev is None or (not prev[0] and e.msgstr):
                table[k] = (e.msgstr, path, e)
    return result


# ---------------------------------------------------------------------------
# C scanning
# ---------------------------------------------------------------------------

# Macros whose string literal argument is converted by preproc (directly or
# after expansion, e.g. ITEM_NAME -> COMPOUND_STRING_SIZE_LIMIT -> COMPOUND_STRING).
C_STRING_MACROS = ("COMPOUND_STRING_SIZE_LIMIT", "COMPOUND_STRING", "ITEM_PLURAL_NAME", "ITEM_NAME", "__", "_")
C_SCAN_DIRS = ("src", "include")


def c_source_files():
    """Every .c/.h that can contribute string literals to the ROM."""
    files = []
    for top in C_SCAN_DIRS:
        for dirpath, dirnames, filenames in os.walk(os.path.join(ROOT, top)):
            dirnames.sort()
            for fn in sorted(filenames):
                if fn.endswith((".c", ".h")):
                    files.append(os.path.join(dirpath, fn))
    return files


_IDENT = re.compile(r"[A-Za-z0-9_]")


def _skip_ws_comments(s, i):
    n = len(s)
    while i < n:
        c = s[i]
        if c in " \t\r\n":
            i += 1
        elif s.startswith("//", i):
            j = s.find("\n", i)
            i = n if j < 0 else j
        elif s.startswith("/*", i):
            j = s.find("*/", i + 2)
            i = n if j < 0 else j + 2
        elif c == "\\" and i + 1 < n and s[i + 1] == "\n":
            i += 2
        else:
            break
    return i


def _read_c_literal(s, i):
    """s[i] == '"'. Returns (raw_contents, index_after_closing_quote)."""
    j = i + 1
    n = len(s)
    while j < n:
        c = s[j]
        if c == "\\":
            j += 2
            continue
        if c == '"':
            return s[i + 1:j], j + 1
        if c == "\n":
            return None, j
        j += 1
    return None, n


_C_TOKEN = re.compile(
    r'//[^\n]*'
    r'|/\*.*?\*/'
    r'|"(?:\\.|[^"\\\n])*"'
    r"|'(?:\\.|[^'\\\n])*'"
    r'|(?<![A-Za-z0-9_])(?:COMPOUND_STRING_SIZE_LIMIT|COMPOUND_STRING|ITEM_PLURAL_NAME|ITEM_NAME|__|_)(?![A-Za-z0-9_])',
    re.S)
_C_ARGS = re.compile(r'(?:\s|/\*.*?\*/|//[^\n]*)*', re.S)
_C_LIT = re.compile(r'"((?:\\.|[^"\\\n])*)"')


def scan_c_text(text):
    """Yields (line, key) for every translatable literal in C source text."""
    import bisect
    line_starts = None
    pos = 0
    n = len(text)
    while True:
        m = _C_TOKEN.search(text, pos)
        if not m:
            return
        tok = m.group(0)
        pos = m.end()
        if tok[0] in "/\"'":
            continue
        j = _C_ARGS.match(text, pos).end()
        if j >= n or text[j] != "(":
            continue
        j = _C_ARGS.match(text, j + 1).end()
        parts = []
        while True:
            lm = _C_LIT.match(text, j)
            if not lm:
                break
            parts.append(lm.group(1))
            j = _C_ARGS.match(text, lm.end()).end()
        if parts and j < n and (text[j] == ")" or (text[j] == "," and tok[0] != "_")):
            if line_starts is None:
                line_starts = [0] + [x.end() for x in re.finditer("\n", text)]
            yield bisect.bisect_right(line_starts, m.start()), "".join(parts)
            pos = j + 1


def scan_c_files(files=None):
    """Returns list of (relpath, line, key)."""
    if files is None:
        files = c_source_files()
    result = []
    for path in files:
        try:
            with open(path, encoding="utf-8") as f:
                text = f.read()
        except UnicodeDecodeError:
            with open(path, encoding="latin-1") as f:
                text = f.read()
        if "_(" not in text and "COMPOUND_STRING" not in text and "ITEM_" not in text:
            continue
        r = rel(path)
        for line, key in scan_c_text(text):
            result.append((r, line, key))
    return result


_C_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)


def c_include_closure(src_files):
    """Maps each .c file to the set of project files it (transitively) includes.

    Over-approximates: #if is ignored. Used only to decide which objects must be
    rebuilt when a translation changes."""
    cache = {}

    def direct(path):
        if path in cache:
            return cache[path]
        deps = []
        try:
            with open(os.path.join(ROOT, path), encoding="utf-8", errors="replace") as f:
                text = f.read()
        except OSError:
            cache[path] = deps
            return deps
        base = os.path.dirname(path)
        for inc in _C_INCLUDE.findall(text):
            for cand in (os.path.join(base, inc), os.path.join("include", inc), inc):
                cand = os.path.normpath(cand).replace(os.sep, "/")
                if os.path.isfile(os.path.join(ROOT, cand)):
                    deps.append(cand)
                    break
        cache[path] = deps
        return deps

    closures = {}
    for src in src_files:
        seen = set()
        stack = [src]
        while stack:
            p = stack.pop()
            if p in seen:
                continue
            seen.add(p)
            stack.extend(direct(p))
        closures[src] = seen
    return closures


# ---------------------------------------------------------------------------
# asm scanning (mirrors tools/preproc/asm_file.cpp + preproc.cpp)
# ---------------------------------------------------------------------------

ASM_ROOT_DIRS = ("data",)
_ASM_LABEL = re.compile(r"^([A-Za-z_.][A-Za-z0-9_.]*)(::?)")
_ASM_STRING = re.compile(r'^\.string\s+"((?:[^"\\]|\\.)*)"\s*(,\s*\S+)?\s*$')
_ASM_INCLUDE = re.compile(r'^\.include\s+"([^"]+)"')


def asm_root_files():
    files = []
    for d in ASM_ROOT_DIRS:
        p = os.path.join(ROOT, d)
        for fn in sorted(os.listdir(p)):
            if fn.endswith(".s"):
                files.append(os.path.join(p, fn))
    return files


def _strip_asm_comments(text):
    """Same rules as AsmFile::RemoveComments, keeps line structure."""
    out = []
    i = 0
    n = len(text)
    q = None
    while i < n:
        c = text[i]
        if q:
            if c == "\\" and i + 1 < n and text[i + 1] == q:
                out.append(text[i:i + 2])
                i += 2
                continue
            if c == q:
                q = None
            out.append(c)
            i += 1
        elif c == "@" and (i == 0 or text[i - 1] != "\\"):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c == "/" and text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join("\n" if ch == "\n" else " " for ch in text[i:j]))
            i = j
        else:
            if c in "\"'":
                q = c
            out.append(c)
            i += 1
    return "".join(out)


class AsmBlock:
    __slots__ = ("label", "file", "line", "text", "padded", "frlg")

    def __init__(self, label, file, line, frlg=False):
        self.label = label
        self.file = file
        self.line = line
        self.text = ""
        self.padded = False
        self.frlg = frlg


_ASM_COND = re.compile(r"^(?:\.|#)\s*(if|ifdef|ifndef|else|elif|endif)\b(.*)$")


def scan_asm_file(relpath, blocks, visited, frlg=False, stack=()):
    """Appends AsmBlocks found in relpath (following .include) to blocks.

    Blocks inside `.if IS_FRLG` / `#if IS_FRLG` are flagged (they don't end up
    in the Emerald ROM)."""
    if relpath in stack:
        return
    path = os.path.join(ROOT, relpath)
    if not os.path.isfile(path):
        return
    first = relpath not in visited
    visited.add(relpath)
    with open(path, encoding="utf-8", errors="replace") as f:
        text = _strip_asm_comments(f.read())
    cur = None   # block being collected
    pending = None  # label waiting for its first .string
    cond = []   # stack of (is_frlg_test, frlg_state_before)
    in_frlg = frlg
    for lineno, line in enumerate(text.split("\n"), 1):
        s = line.strip()
        if not s:
            continue
        cm = _ASM_COND.match(s)
        if cm:
            kw, arg = cm.group(1), cm.group(2).strip()
            if kw.startswith("if"):
                test = "IS_FRLG" in arg and kw == "if"
                negated = test and arg.lstrip().startswith("!")
                cond.append((test, in_frlg, negated))
                if test:
                    in_frlg = not negated
            elif kw in ("else", "elif") and cond:
                test, before, negated = cond[-1]
                if test:
                    in_frlg = negated if kw == "else" else before
            elif kw == "endif" and cond:
                in_frlg = cond.pop()[1]
            cur = None
            pending = None
            continue
        m = _ASM_LABEL.match(s)
        if m and not s.startswith("."):
            label = m.group(1)
            rest = s[m.end():].strip()
            cur = None
            pending = AsmBlock(label, relpath, lineno, in_frlg)
            if not rest:
                continue
            s = rest
        if s.startswith(".string"):
            sm = _ASM_STRING.match(s)
            if cur is None and pending is not None:
                cur = pending
                pending = None
                if first:
                    blocks.append(cur)
            if cur is not None and sm:
                cur.text += sm.group(1)
                if sm.group(2):
                    cur.padded = True
                if sm.group(1).endswith("$"):
                    cur = None
            elif cur is not None:
                cur.padded = True  # something we don't understand: never translate
                cur = None
            continue
        cur = None
        pending = None
        im = _ASM_INCLUDE.match(s)
        if im:
            scan_asm_file(im.group(1), blocks, visited, in_frlg, stack + (relpath,))


def scan_asm():
    """Returns [AsmBlock] reachable from data/*.s, in include order."""
    blocks = []
    visited = set()
    for root in asm_root_files():
        scan_asm_file(rel(root), blocks, visited)
    return blocks


def asm_include_closure(root_relpath):
    files = set()

    def walk(relpath):
        if relpath in files:
            return
        path = os.path.join(ROOT, relpath)
        if not os.path.isfile(path):
            return
        files.add(relpath)
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                m = _ASM_INCLUDE.match(line.strip())
                if m:
                    walk(m.group(1))
    walk(root_relpath)
    return files


# ---------------------------------------------------------------------------
# Width estimation (for the lint/stats tools)
# ---------------------------------------------------------------------------

_LATIN_WIDTHS = None


def latin_widths():
    """Per-character widths of FONT_NORMAL, keyed by unicode char (from charmap + fonts.c)."""
    global _LATIN_WIDTHS
    if _LATIN_WIDTHS is not None:
        return _LATIN_WIDTHS
    with open(os.path.join(ROOT, "src", "fonts.c"), encoding="utf-8") as f:
        src = f.read()
    m = re.search(r"gFontNormalLatinGlyphWidths\[\]\s*=\s*\{([^}]*)\}", src)
    widths = [int(x) for x in re.findall(r"\d+", m.group(1))]
    table = {}
    with open(os.path.join(ROOT, "charmap.txt"), encoding="utf-8") as f:
        for line in f:
            line = line.split("@")[0].strip()
            cm = re.match(r"^'(\\?.)'\s*=\s*([0-9A-Fa-f]{2})$", line)
            if cm:
                ch = cm.group(1)[-1]
                code = int(cm.group(2), 16)
                if ch not in table and code < len(widths):
                    table[ch] = widths[code]
            if line.startswith("@ Hiragana"):
                break
    _LATIN_WIDTHS = table
    return table


_FONT_WIDTHS = {}
CJK_WIDTH_BY_FONT = {"Normal": 12, "Small": 12, "Short": 12, "Narrow": 11, "SmallNarrow": 11,
                     "ShortNarrow": 11, "Narrower": 10, "SmallNarrower": 10, "ShortNarrower": 10}


def font_widths(font):
    """Latin glyph widths of gFont<font>LatinGlyphWidths, keyed by unicode char."""
    if font in _FONT_WIDTHS:
        return _FONT_WIDTHS[font]
    with open(os.path.join(ROOT, "src", "fonts.c"), encoding="utf-8") as f:
        src = f.read()
    m = re.search(r"gFont%sLatinGlyphWidths\[\]\s*=\s*\{([^}]*)\}" % font, src)
    widths = [int(x) for x in re.findall(r"\d+", m.group(1))]
    table = {}
    with open(os.path.join(ROOT, "charmap.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("@ Hiragana"):
                break
            cm = re.match(r"^'(\\?.)'\s*=\s*([0-9A-Fa-f]{2})\s*(@.*)?$", line.strip())
            if cm:
                ch = cm.group(1)[-1]
                code = int(cm.group(2), 16)
                if ch not in table and code < len(widths):
                    table[ch] = widths[code]
    _FONT_WIDTHS[font] = table
    return table


def text_width(raw, font="Normal"):
    """Width in pixels of a one-line raw string in one of the fonts (placeholders count as 0)."""
    lw = font_widths(font)
    cjk = CJK_WIDTH_BY_FONT[font]
    w = 0
    i = 0
    while i < len(raw):
        c = raw[i]
        if c == "{":
            j = raw.find("}", i)
            i = len(raw) if j < 0 else j + 1
            continue
        if c == "\\":
            i += 2
            continue
        if c != "$":
            w += cjk if (is_wide_char(c) or ord(c) > 0x2E7F) else lw.get(c, 6)
        i += 1
    return w


PLACEHOLDER_WIDTHS = {
    "PLAYER": 42, "RIVAL": 42, "STR_VAR_1": 60, "STR_VAR_2": 60, "STR_VAR_3": 60,
    "KUN": 0, "VERSION": 36, "AQUA": 48, "MAGMA": 48, "ARCHIE": 36, "MAXIE": 36,
    "KYOGRE": 36, "GROUDON": 36, "REGION": 24,
}


def line_widths(raw, cjk_width=12, unknown_placeholder=48):
    """Splits raw game text at \\n \\l \\p and returns [(line_text, width_px)]."""
    lw = latin_widths()
    lines = []
    cur = []
    width = 0
    i = 0
    n = len(raw)
    while i < n:
        c = raw[i]
        if c == "\\" and i + 1 < n:
            e = raw[i + 1]
            i += 2
            if e in "nlp":
                lines.append(("".join(cur), width))
                cur = []
                width = 0
            else:
                cur.append(e)
                width += lw.get(e, 6)
            continue
        if c == "{":
            j = raw.find("}", i)
            if j < 0:
                j = n - 1
            name = raw[i + 1:j].strip().split(" ")[0]
            cur.append(raw[i:j + 1])
            if name in PLACEHOLDER_WIDTHS:
                width += PLACEHOLDER_WIDTHS[name]
            elif name.endswith("_BUTTON") or name.startswith("DPAD"):
                width += 12
            elif name.startswith("B_") and ("NAME" in name or "MOVE" in name or "ITEM" in name or "ABILITY" in name):
                width += unknown_placeholder
            elif name.startswith("B_") or name.startswith("STR_"):
                width += unknown_placeholder
            i = j + 1
            continue
        if c == "$":
            i += 1
            continue
        cur.append(c)
        width += cjk_width if is_wide_char(c) or ord(c) > 0x2E7F else lw.get(c, 6)
        i += 1
    lines.append(("".join(cur), width))
    return lines


def warn(msg):
    sys.stderr.write("zh: warning: %s\n" % msg)
