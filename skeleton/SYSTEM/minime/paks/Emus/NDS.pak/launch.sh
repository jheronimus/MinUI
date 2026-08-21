#!/bin/sh

EMU_EXE=drastic

###############################

EMU_TAG=$(basename "$(dirname "$0")" .pak)
ROM="$1"
mkdir -p "$BIOS_PATH/$EMU_TAG"
mkdir -p "$SAVES_PATH/$EMU_TAG"
HOME="$USERDATA_PATH"
cd "$HOME"
export LD_PRELOAD="$CORES_PATH/libashmem.so:$CORES_PATH/libandroid.so:$CORES_PATH/liblog.so"
minarch.elf "$CORES_PATH/${EMU_EXE}_libretro.so" "$ROM" >"$LOGS_PATH/$EMU_TAG.txt" 2>&1
