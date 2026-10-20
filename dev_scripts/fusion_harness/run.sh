#!/usr/bin/env bash
# Headless scripted GBA runner (mGBA core via libmgba 0.10, no display needed).
#
# Usage: ./run.sh [--state FILE.ss0] [--elf FILE.elf] [--sav FILE.sav] [--rtc-epoch MS] [--verbose] ROM INPUT_SCRIPT OUT_DIR
#   INPUT_SCRIPT may be "-" for stdin.
#   --state   start from this savestate instead of power-on
#   --elf     symbol source for read/write (default: ROM's sibling .elf, used only if the script reads/writes memory)
#   --sav     load battery save (read-only; never written back)
#   --rtc-epoch  fixed RTC start in ms since epoch (default 2024-01-01 12:00 UTC; RTC then advances with emulated time)
#
# Script syntax (one command per line, '#' starts a comment; keys: A B L R START SELECT UP DOWN LEFT RIGHT, combine with '+'):
#   wait N                 advance N frames (only keys set via 'keys' are held)
#   press KEYS [N] [AFTER] hold KEYS for N frames (default 2), then release and wait AFTER frames (default 0)
#   hold KEYS N [AFTER]    same as press (default N=1); e.g. 'hold START+R 10'
#   tap KEYS [COUNT] [GAP] press KEYS 2 frames then release GAP frames (default 10), COUNT times (mash text)
#   keys KEYS|NONE         keys held down during subsequent waits (e.g. 'keys B' to run)
#   shot NAME              save OUT_DIR/NAME.png (240x160)
#   savestate NAME         save OUT_DIR/NAME.ss0 (absolute/./ paths allowed)
#   loadstate NAME|PATH    load a state (existing path, else OUT_DIR/NAME.ss0)
#   reset                  power-cycle
#   read SYM|ADDR [8|16|32]          print memory (SYM may be SYM+OFFSET; symbols from the .elf)
#   write SYM|ADDR VALUE [8|16|32]   poke memory
#   echo TEXT...           print a marker line;  quit  stop early
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$HERE/gbarun"
if [[ ! -x "$BIN" || "$HERE/gbarun.c" -nt "$BIN" ]]; then
  if [[ ! -f /usr/include/mgba/core/core.h ]]; then
    echo "installing libmgba-dev libpng-dev..." >&2
    DEBIAN_FRONTEND=noninteractive apt-get install -y libmgba-dev libpng-dev >&2
  fi
  gcc -O2 -Wall -o "$BIN" "$HERE/gbarun.c" -lmgba -lpng
fi

args=(); elf=""
while [[ $# -gt 0 && "$1" == --* ]]; do
  case "$1" in
    --elf) elf="$2"; shift 2 ;;
    --state|--sav|--rtc-epoch) args+=("$1" "$2"); shift 2 ;;
    --verbose) args+=("$1"); shift ;;
    -h|--help) sed -n '2,26p' "$0"; exit 0 ;;
    *) echo "unknown option $1" >&2; exit 2 ;;
  esac
done
if [[ $# -ne 3 ]]; then sed -n '2,26p' "$0" >&2; exit 2; fi
rom="$1"; script="$2"; out="$3"

# Symbol map only when the script touches memory (nm on a big .elf costs ~1s; cached by mtime).
if [[ "$script" != "-" ]] && grep -qiE '^[[:space:]]*(read|peek|write|poke)[[:space:]]' "$script"; then
  [[ -z "$elf" ]] && elf="${rom%.*}.elf"
  if [[ -f "$elf" ]]; then
    mkdir -p "$HERE/.cache"
    syms="$HERE/.cache/$(basename "$elf").$(stat -c %Y "$elf").syms"
    [[ -f "$syms" ]] || nm "$elf" > "$syms"
    args+=(--syms "$syms")
  fi
fi
exec "$BIN" "${args[@]}" "$rom" "$script" "$out"
