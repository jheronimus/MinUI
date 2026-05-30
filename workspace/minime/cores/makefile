CORES = fceumm gambatte gpsp pcsx_rearmed picodrive snes9x2005_plus
CORES+= mednafen_pce_fast mednafen_vb fake-08 mednafen_supafaust mgba pokemini race

###############################

# optional core vars
mednafen_pce_fast_REPO = https://github.com/libretro/beetle-pce-fast-libretro
mednafen_vb_REPO = https://github.com/libretro/beetle-vb-libretro

fake-08_REPO = https://github.com/jtothebell/fake-08
fake-08_CORE = fake08_libretro.so
fake-08_BUILD_PATH = fake-08/platform/libretro

fceumm_REPO = https://github.com/libretro/libretro-fceumm

gambatte_REPO = https://github.com/libretro/gambatte-libretro

mednafen_supafaust_REPO = https://github.com/libretro/supafaust

pcsx_rearmed_MAKEFILE = Makefile.libretro

picodrive_REPO = https://github.com/irixxxx/picodrive
picodrive_MAKEFILE = Makefile.libretro

pokemini_REPO = https://github.com/libretro/PokeMini
pokemini_MAKEFILE = Makefile.libretro

snes9x2005_plus_REPO = https://github.com/libretro/snes9x2005

###############################

# Override make commands to compile with platform=unix for 64-bit Linux compatibility
fceumm_MAKE = make platform=unix
gambatte_MAKE = make platform=unix
gpsp_MAKE = make platform=unix
pcsx_rearmed_MAKE = make -f Makefile.libretro platform=unix
picodrive_MAKE = make -f Makefile.libretro platform=unix GIT_REVISION=""
snes9x2005_plus_MAKE = make platform=unix USE_BLARGG_APU=1
mednafen_pce_fast_MAKE = make platform=unix
mednafen_vb_MAKE = make platform=unix
fake-08_MAKE = make platform=unix
mednafen_supafaust_MAKE = make platform=unix
mgba_MAKE = make platform=unix
pokemini_MAKE = make -f Makefile.libretro platform=unix
race_MAKE = make platform=unix

###############################

include ../../all/cores/makefile