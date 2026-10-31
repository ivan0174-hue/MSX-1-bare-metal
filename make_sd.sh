#!/bin/bash
# Copies everything needed for the MSX emulator onto the SD card.
# Usage: ./make_sd.sh <sd-mount-point> [bios-file] [roms-folder]
#   e.g. ./make_sd.sh /mnt/e ~/cbios_main_msx1.rom
SRC="${SRC:-$HOME/msx-bare}"
CIRCLE="${CIRCLE:-$HOME/circle}"
DEST="$1"; BIOS="$2"; ROMS="$3"

if [ -z "$DEST" ] || [ ! -d "$DEST" ]; then
	echo "Usage: $0 <sd-mount-point> [bios-file] [roms-folder]"
	echo "SD folder not found: '$DEST' (mount it first, see instructions)"
	exit 1
fi
for f in bootcode.bin start.elf fixup.dat; do
	[ -f "$CIRCLE/boot/$f" ] || { echo "Missing $CIRCLE/boot/$f - run: (cd $CIRCLE/boot && make)"; exit 1; }
done
[ -f "$SRC/src/kernel8.img" ] || { echo "Missing $SRC/src/kernel8.img - build the emulator first"; exit 1; }

for f in bootcode.bin start.elf fixup.dat; do cp "$CIRCLE/boot/$f" "$DEST/"; done
cp "$SRC/src/kernel8.img" "$SRC/sdcard/config.txt" "$DEST/"
[ -f "$DEST/msx.cfg" ] || cp "$SRC/sdcard/msx.cfg" "$DEST/"      # keep an existing msx.cfg
[ -n "$BIOS" ] && cp "$BIOS" "$DEST/msx1.rom"
[ -n "$ROMS" ] && [ -d "$ROMS" ] && cp -r "$ROMS"/. "$DEST/"
sync

echo "Copied to $DEST:"; ls "$DEST"
[ -f "$DEST/msx1.rom" ] || echo "WARNING: msx1.rom (32 KB MSX1 BIOS) is not on the card yet - the emulator will show an orange screen."
