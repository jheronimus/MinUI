CORES = fceumm gambatte gpsp pcsx_rearmed picodrive snes9x2005_plus
CORES+= mednafen_pce_fast mednafen_vb mednafen_supafaust mgba pokemini race
CORES+= fake-08

###############################

# Libretro cores don't recognize "minime" as a platform.
# Set platform per-core using := for reliable expansion through $(eval $(call)).
# "unix" produces .so for most cores.
# "arm64" enables aarch64 dynarec for gpsp (detects x86_64 via uname with "unix").
fceumm_PLATFORM := unix
# WANT_32BPP=0 keeps fceumm at RGB565 (minarch is RGB565-only). The unix
# platform branch in Makefile.libretro defaults WANT_32BPP to 1 (RGB888),
# which minarch rejects -> wrong palette + overscan.
fceumm_FLAGS = WANT_32BPP=0
gambatte_PLATFORM := unix
gpsp_PLATFORM := arm64
pcsx_rearmed_PLATFORM := unix
pcsx_rearmed_MAKEFILE = Makefile.libretro
pcsx_rearmed_FLAGS = USE_MINIZ=0 ARCH=aarch64 DYNAREC=ari64
picodrive_PLATFORM := aarch64
picodrive_MAKEFILE = Makefile.libretro
snes9x2005_plus_PLATFORM := unix
mednafen_pce_fast_PLATFORM := unix
mednafen_vb_PLATFORM := unix
mednafen_supafaust_PLATFORM := unix
mgba_PLATFORM := unix
pokemini_PLATFORM := unix
race_PLATFORM := unix
fake-08_PLATFORM := unix

###############################

fake-08_REPO = https://github.com/jtothebell/fake-08
fake-08_CORE = fake08_libretro.so
fake-08_BUILD_PATH = fake-08/platform/libretro

mednafen_pce_fast_REPO = https://github.com/libretro/beetle-pce-fast-libretro
mednafen_vb_REPO = https://github.com/libretro/beetle-vb-libretro

fceumm_REPO = https://github.com/libretro/libretro-fceumm

gambatte_REPO = https://github.com/libretro/gambatte-libretro

mednafen_supafaust_REPO = https://github.com/libretro/supafaust

# irixxxx/picodrive is the actively maintained fork; libretro/picodrive (the
# default) lags behind and drifts. Pin the exact commit so CI builds are
# deterministic.
picodrive_REPO = https://github.com/irixxxx/picodrive
picodrive_HASH = 5f9e79f2177443939f21ab965b1c294edbef514c

pcsx_rearmed_HASH = 99d2ce02c6a5836096ade6ced9f8ca23a1fbb7bd

# Upstream libretro/mgba removed its Makefile/Makefile.libretro in
# 89404771 (2026-08-04, migrated to CMake). Pin to the last commit that
# still ships the Makefile our build expects.
mgba_HASH = 517e518c48e351def04042dc159064dd098830a9

pokemini_REPO = https://github.com/libretro/PokeMini
pokemini_MAKEFILE = Makefile.libretro

snes9x2005_plus_REPO = https://github.com/libretro/snes9x2005
snes9x2005_plus_FLAGS = USE_BLARGG_APU=1

###############################

export CC = aarch64-linux-gnu-gcc
export CXX = aarch64-linux-gnu-g++
export AR = aarch64-linux-gnu-ar

include ../../all/cores/makefile
