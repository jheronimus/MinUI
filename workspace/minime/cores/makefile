CORES = fceumm gambatte gpsp pcsx_rearmed picodrive snes9x2005_plus
CORES+= mednafen_pce_fast mednafen_vb mednafen_supafaust mgba pokemini race
CORES+= fake-08

###############################

# Libretro cores don't recognize "minime" as a platform.
# Set platform per-core using := for reliable expansion through $(eval $(call)).
# "unix" produces .so for most cores.
# "arm64" enables aarch64 dynarec for gpsp (detects x86_64 via uname with "unix").
fceumm_PLATFORM := unix
gambatte_PLATFORM := unix
gpsp_PLATFORM := arm64
pcsx_rearmed_PLATFORM := unix
picodrive_PLATFORM := unix
snes9x2005_plus_PLATFORM := unix
mednafen_pce_fast_PLATFORM := unix
mednafen_vb_PLATFORM := unix
mednafen_supafaust_PLATFORM := unix
mgba_PLATFORM := unix
pokemini_PLATFORM := unix
race_PLATFORM := unix
fake-08_PLATFORM := unix

###############################

# optional core vars
# *_REPO=
# *_HASH=
# *_CORE=
# *_FLAGS=
# *_MAKEFILE=
# *_BUILD_PATH=

fake-08_REPO = https://github.com/jtothebell/fake-08
fake-08_CORE = fake08_libretro.so
fake-08_BUILD_PATH = fake-08/platform/libretro

mednafen_pce_fast_REPO = https://github.com/libretro/beetle-pce-fast-libretro
mednafen_vb_REPO = https://github.com/libretro/beetle-vb-libretro

fceumm_REPO = https://github.com/libretro/libretro-fceumm

gambatte_REPO = https://github.com/libretro/gambatte-libretro

mednafen_supafaust_REPO = https://github.com/libretro/supafaust

pcsx_rearmed_MAKEFILE = Makefile.libretro
pcsx_rearmed_FLAGS = PLATFORM=libretro

picodrive_REPO = https://github.com/irixxxx/picodrive
picodrive_MAKEFILE = Makefile.libretro

pokemini_REPO = https://github.com/libretro/PokeMini
pokemini_MAKEFILE = Makefile.libretro

snes9x2005_plus_REPO = https://github.com/libretro/snes9x2005
snes9x2005_plus_FLAGS = USE_BLARGG_APU=1

###############################

export CC = aarch64-linux-gnu-gcc
export CXX = aarch64-linux-gnu-g++
export AR = aarch64-linux-gnu-ar

include ../../all/cores/makefile
