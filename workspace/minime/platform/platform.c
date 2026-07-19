/*
 * Minime platform — minimal stub.
 * Implements the platform API that minui and minarch call.
 * Feature imports will replace hardcoded values with traits-based detection.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "platform.h"
#include "msettings.h"

// Runtime screen state — will be populated from traits
int plat_screen_width  = 640;
int plat_screen_height = 480;
int plat_screen_rotation = 0;

void PLAT_init(void) {
	// TODO: read traits file, populate plat_screen_* and button/keycode defines
	// See IMPORT src/platform/minime/traits.c for target implementation
}

void PLAT_quit(void) {
}

void PLAT_initInput(void) {
	// TODO: open input devices by name from traits
}

void PLAT_quitInput(void) {
}

int PLAT_pollInput(void) {
	return 0;
}

void PLAT_initVideo(void) {
	// TODO: SDL2 init with traits-based screen dimensions
}

void PLAT_quitVideo(void) {
}

void PLAT_flip(void) {
}

void PLAT_enableBacklight(int enable) {
	(void)enable;
	// TODO: write to backlight_path from traits
}

int PLAT_getBatteryStatus(void) {
	// TODO: read battery_capacity_path from traits
	return 100;
}

int PLAT_isCharging(void) {
	// TODO: read charger_online_path from traits
	return 0;
}

void PLAT_powerOff(void) {
	unlink("/tmp/minui_exec");
	exit(0);
}

void PLAT_setRumble(int level) {
	(void)level;
	// TODO: write to rumble_path from traits
}

char* PLAT_getModel(void) {
	return "minime";
}

void PLAT_initLid(void) {
	// TODO: open lid_switch_path from traits
}

int PLAT_lidChanged(void) {
	return 0;
}

void PLAT_enableOverlay(void) {
}

void PLAT_disableOverlay(void) {
}
