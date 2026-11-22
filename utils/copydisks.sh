#!/bin/sh
# copydisks.sh - copy the 720 KB PC floppy images (disk02 ... disk09) as
# working copies with the extension .img, as FS-UAE expects them
#
# Usage:   utils/copydisks.sh [-f] source-dir target-dir
# Example (Amiga MINIX 1.5.10):
#   utils/copydisks.sh emu/images/1.5.10 emu/work/1.5.10
#
# Every file in source-dir that is exactly 737280 bytes (80 tracks, 2 sides,
# 9 sectors of 512 bytes) is copied to target-dir/<name>.img.  The originals
# stay unchanged.  Existing copies are kept, because FS-UAE writes into
# them (your changes made in MINIX); -f overwrites them.
set -eu

usage="Usage: $0 [-f] source-dir target-dir"
force=0
if [ "${1:-}" = "-f" ]; then
	force=1
	shift
fi
[ $# -eq 2 ] || { echo "$usage" >&2; exit 1; }
SRC=$1
DST=$2
[ -d "$SRC" ] || { echo "Error: '$SRC' is not a directory" >&2; exit 1; }

mkdir -p "$DST"
n=0
for f in "$SRC"/*; do
	[ -f "$f" ] || continue
	[ "$(stat -c %s "$f")" -eq 737280 ] || continue
	out="$DST/$(basename "$f").img"
	if [ -e "$out" ] && [ "$force" -eq 0 ]; then
		echo "kept:   $out (exists, use -f to overwrite)"
	else
		cp "$f" "$out"
		echo "copied: $out"
	fi
	n=$((n + 1))
done
[ "$n" -gt 0 ] || { echo "Error: no 720 KB images found in '$SRC'" >&2; exit 1; }

