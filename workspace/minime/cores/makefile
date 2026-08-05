CORES = fceumm gambatte gpsp pcsx_rearmed picodrive snes9x2005_plus
CORES+= mednafen_pce_fast mednafen_vb mednafen_supafaust mgba pokemini race
CORES+= fake-08

###############################

# Every core gets a `minime` platform branch via patches/<core>.patch
# (derived from upstream's rgb30/magicmini branches, minus the per-SoC
# -march=armv8.2-a/-mtune=cortex-a55, using the shared armv8-a+crc baseline).
# The branches carry -ffast-math, EXTERNAL_ZLIB, per-core defines, and the
# aarch64 dynarec config. Set explicitly with := (the ?= default to PLATFORM
# does not survive $(eval) expansion reliably).
fceumm_PLATFORM := minime
gambatte_PLATFORM := minime
gpsp_PLATFORM := minime
pcsx_rearmed_PLATFORM := minime
picodrive_PLATFORM := minime
snes9x2005_plus_PLATFORM := minime
mednafen_pce_fast_PLATFORM := minime
mednafen_vb_PLATFORM := minime
mednafen_supafaust_PLATFORM := minime
mgba_PLATFORM := minime
pokemini_PLATFORM := minime
race_PLATFORM := minime
fake-08_PLATFORM := minime

pcsx_rearmed_MAKEFILE = Makefile.libretro
pcsx_rearmed_FLAGS = USE_MINIZ=0
picodrive_MAKEFILE = Makefile.libretro
pokemini_MAKEFILE = Makefile.libretro
snes9x2005_plus_FLAGS = USE_BLARGG_APU=1

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

snes9x2005_plus_REPO = https://github.com/libretro/snes9x2005

###############################

export CC = aarch64-linux-gnu-gcc
export CXX = aarch64-linux-gnu-g++
export AR = aarch64-linux-gnu-ar

include ../../all/cores/makefile
