#!/bin/sh
# lha2adf.sh - make a bootable Amiga floppy image (ADF) from an LHA archive
#
# Usage:   utils/lha2adf.sh archive.lha image.adf ['startup-sequence line']
# Example (Amiga MINIX 1.5.10):
#   utils/lha2adf.sh emu/images/1.5.10/amboot.lha emu/work/1.5.10/minix-boot.adf \
#                    'BOOT:Minix -s 0 -e 512'
#
# Requires the Debian packages lhasa (for lha) and python3-venv.  xdftool
# (from the Python package amitools) is installed into a temporary
# directory, which is removed at the end together with everything else;
# only the ADF is kept.  Installing amitools needs network access (PyPI).
set -eu

usage="Usage: $0 archive.lha image.adf ['startup-sequence line']"
[ $# -ge 2 ] || { echo "$usage" >&2; exit 1; }

# Check that the required packages are installed
missing=0
if ! command -v lha >/dev/null 2>&1; then
	echo "Error: 'lha' not found - install it with: sudo apt install lhasa" >&2
	missing=1
fi
if ! python3 -c 'import ensurepip, venv' >/dev/null 2>&1; then
	echo "Error: python3-venv not found - install it with: sudo apt install python3-venv" >&2
	missing=1
fi
[ "$missing" -eq 0 ] || exit 1

LHA=$(realpath "$1")		# absolute paths, because we change
ADF=$(realpath -m "$2")		# the directory below
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# Install amitools (xdftool) into the temporary directory
echo "Installing amitools into $TMP ..."
python3 -m venv "$TMP/venv"
"$TMP/venv/bin/pip" install --quiet --disable-pip-version-check amitools
XDFTOOL="$TMP/venv/bin/xdftool"

# Unpack the archive
mkdir "$TMP/files"
lha xqw="$TMP/files" "$LHA"

# Optional: replace s/startup-sequence
if [ $# -ge 3 ]; then
	mkdir -p "$TMP/files/s"
	printf '%s\n' "$3" > "$TMP/files/s/startup-sequence"
fi

# Empty ADF: OFS file system, volume name BOOT, standard boot block
mkdir -p "$(dirname "$ADF")"
rm -f "$ADF"
"$XDFTOOL" "$ADF" create + format BOOT ofs + boot install

# Create the directories, then write the files
cd "$TMP/files"
find . -mindepth 1 -type d | sed 's|^\./||' | sort | while read -r d; do
	"$XDFTOOL" "$ADF" makedir "$d"
done
find . -type f | sed 's|^\./||' | sort | while read -r f; do
	"$XDFTOOL" "$ADF" write "$f" "$f"
done

"$XDFTOOL" "$ADF" info
