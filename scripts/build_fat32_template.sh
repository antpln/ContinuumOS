#!/usr/bin/env bash
set -euo pipefail

TEMPLATE="${1:-fat32_template.img}"
SIZE_MB=16
README_TEXT="Welcome to the ContinuumOS FAT32 test image."

if ! command -v mformat >/dev/null || ! command -v mcopy >/dev/null; then
    echo "Error: mtools is required (commands mformat/mcopy not found)." >&2
    exit 1
fi

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

# Create blank image
dd if=/dev/zero of="$TEMPLATE" bs=1M count=$SIZE_MB status=none

# Format the image as FAT32 and add a sample file
mformat -i "$TEMPLATE" -F ::
printf '%s\n' "$README_TEXT" > "$TMPDIR/FAT32README"
mcopy -i "$TEMPLATE" "$TMPDIR/FAT32README" ::/FAT32README

echo "Created FAT32 template image: $TEMPLATE"
