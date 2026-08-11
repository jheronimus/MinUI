# MinUI (Minime Fork)

PLATFORMS = minime

###########################################################

HOST_WORKSPACE=$(shell pwd)/workspace
GUEST_WORKSPACE=/root/workspace

BUILD_HASH:=$(shell git rev-parse --short HEAD)
RELEASE_TIME:=$(shell TZ=GMT date +%Y%m%d)
RELEASE_BETA=
RELEASE_BASE=MinUI-$(RELEASE_TIME)$(RELEASE_BETA)
RELEASE_DOT:=$(shell find ./releases/ -regextype posix-extended -regex ".*/${RELEASE_BASE}-[0-9]+-base\.zip" 2>/dev/null | wc -l | sed 's/ //g')
RELEASE_NAME=$(RELEASE_BASE)-$(RELEASE_DOT)

###########################################################

.PHONY: build

export MAKEFLAGS=--no-print-directory

all: setup build system special package

shell:
	docker build -t $(PLATFORM)-toolchain toolchains/$(PLATFORM)-toolchain
	docker run -it --rm -v $(HOST_WORKSPACE):$(GUEST_WORKSPACE) $(PLATFORM)-toolchain /bin/bash

name:
	@echo $(RELEASE_NAME)

build:
	# ----------------------------------------------------
	docker build -t $(PLATFORM)-toolchain toolchains/$(PLATFORM)-toolchain
	docker run --rm -v $(HOST_WORKSPACE):$(GUEST_WORKSPACE) $(PLATFORM)-toolchain /bin/bash -c '. ~/.bashrc && cd /root/workspace && make'
	# ----------------------------------------------------

system:
	make -f ./workspace/$(PLATFORM)/platform/makefile.copy PLATFORM=$(PLATFORM)

	# populate system
	cp ./workspace/$(PLATFORM)/keymon/keymon.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/$(PLATFORM)/show/show.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/$(PLATFORM)/libmsettings/libmsettings.so ./build/SYSTEM/$(PLATFORM)/lib
	cp ./workspace/all/minui/build/$(PLATFORM)/minui.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/minarch/build/$(PLATFORM)/minarch.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/syncsettings/build/$(PLATFORM)/syncsettings.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/say/build/$(PLATFORM)/say.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/clock/build/$(PLATFORM)/clock.elf ./build/EXTRAS/Tools/$(PLATFORM)/Clock.pak/
	cp ./workspace/all/minput/build/$(PLATFORM)/minput.elf ./build/EXTRAS/Tools/$(PLATFORM)/Input.pak/

cores:
	@test -n "$(CORES_DIR)" || (echo "CORES_DIR not set — run the shared build-cores step first" && exit 1)
	# All emulator cores ship flat in SYSTEM/$(PLATFORM)/cores (CORES_PATH);
	# every pak loads <EMU_EXE>_libretro.so from there. CORES_DIR is the flat
	# output of minime/build/cores/buildcores.sh.
	mkdir -p ./build/SYSTEM/$(PLATFORM)/cores
	cp $(CORES_DIR)/*.so ./build/SYSTEM/$(PLATFORM)/cores/

common: build system

clean:
	rm -rf ./build

setup: name
	# ----------------------------------------------------
	# ready fresh build
	rm -rf ./build
	mkdir -p ./releases
	cp -R ./skeleton ./build

	# remove authoring detritus
	cd ./build && find . -type f -name '.keep' -delete
	cd ./build && find . -type f -name '*.meta' -delete
	echo $(BUILD_HASH) > ./workspace/hash.txt

	# copy readmes to workspace so we can use Linux fmt instead of host's
	mkdir -p ./workspace/readmes
	cp ./skeleton/BASE/README.txt ./workspace/readmes/BASE-in.txt
	cp ./skeleton/EXTRAS/README.txt ./workspace/readmes/EXTRAS-in.txt

	# format readmes for release
	$(MAKE) -C ./workspace/all/readmes readmes

done:
	say "done" 2>/dev/null || true

special:

tidy:

package:
	# ----------------------------------------------------
	# zip up build

	# move formatted readmes from workspace to build
	cp ./workspace/readmes/BASE-out.txt ./build/BASE/README.txt
	cp ./workspace/readmes/EXTRAS-out.txt ./build/EXTRAS/README.txt
	rm -rf ./workspace/readmes

	cd ./build/SYSTEM && echo "$(RELEASE_NAME)\n$(BUILD_HASH)" > version.txt
	./commits.sh > ./build/SYSTEM/commits.txt
	cd ./build && find . -type f -name '.DS_Store' -delete
	mkdir -p ./build/PAYLOAD
	mv ./build/SYSTEM ./build/PAYLOAD/.system

	cd ./build/PAYLOAD && zip -r MinUI.zip .system
	mv ./build/PAYLOAD/MinUI.zip ./build/BASE

	cd ./build/BASE && zip -r ../../releases/$(RELEASE_NAME)-base.zip Bios Roms Saves .minime MinUI.zip README.txt
	cd ./build/EXTRAS && zip -r ../../releases/$(RELEASE_NAME)-extras.zip Bios Emus Roms Saves Tools README.txt
	echo "$(RELEASE_NAME)" > ./build/latest.txt

###########################################################

.DEFAULT:
	# ----------------------------------------------------
	# $@
	@echo "$(PLATFORMS)" | grep -q "\b$@\b" && (make common PLATFORM=$@) || (exit 1)
