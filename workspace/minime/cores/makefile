# Minime cores — pre-built aarch64 libretro cores.
# CI populates this directory before the zip is assembled.
# Placeholder files so the assembly step doesn't fail.

CORES = fceumm gambatte gpsp picodrive snes9x2005_plus pcsx_rearmed mgba mednafen_pce_fast pokemini race mednafen_supafaust mednafen_vb

all: output
	@echo "cores: $(CORES)"

output: Makefile
	mkdir -p output
	@for core in $(CORES); do touch output/$${core}_libretro.so; done

clean:
	rm -rf output
