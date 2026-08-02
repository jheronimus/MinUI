/*
 * Minime libmsettings — minimal stub.
 * Reads settings from shared memory (same contract as upstream).
 * Feature imports will add traits-based hardware access.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "msettings.h"

typedef struct {
	int brightness;
	int volume;
	int jack;
	int hdmi;
	int mute;
	int size;
} SharedSettings;

static SharedSettings* shared = NULL;
static int shm_fd = -1;
static int is_host = 0;

void InitSettings(void) {
	shm_fd = shm_open("/SharedSettings", O_RDWR | O_CREAT, 0666);
	if (shm_fd<0) return;

SharedSettings proto = {.size=sizeof(SharedSettings)};
	ftruncate(shm_fd, sizeof(SharedSettings));
	shared = mmap(NULL, sizeof(SharedSettings), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (shared==MAP_FAILED) { shared=NULL; return; }

	if (shared->size==0) {
		is_host = 1;
		memcpy(shared, &proto, sizeof(SharedSettings));
		SetBrightness(5);
		SetVolume(10);
	} else {
		is_host = 0;
	}
}

void QuitSettings(void) {
	if (shared) { munmap(shared, sizeof(SharedSettings)); shared=NULL; }
	if (shm_fd>=0) { close(shm_fd); shm_fd=-1; }
}

int GetBrightness(void) { return shared ? shared->brightness : 5; }
int GetVolume(void)     { return shared ? shared->volume : 10; }
int GetJack(void)       { return shared ? shared->jack : 0; }
int GetHDMI(void)       { return shared ? shared->hdmi : 0; }
int GetMute(void)       { return shared ? shared->mute : 0; }

void SetBrightness(int value) {
	if (!shared) return;
	if (value < 0) value = 0;
	if (value > 10) value = 10;
	shared->brightness = value;

	// Write brightness to /sys/class/backlight/*/brightness
	FILE *f = fopen("/sys/class/backlight/backlight/brightness", "w");
	if (!f) {
		// Glob/try alternate backlight paths
		f = fopen("/sys/class/backlight/backlight_lcd/brightness", "w");
	}
	if (f) {
		// Scale 0..10 to 0..255 (or 1..10)
		int raw = (value * 255) / 10;
		if (value > 0 && raw == 0) raw = 1;
		fprintf(f, "%d\n", raw);
		fclose(f);
	}
}

void SetRawBrightness(int value) {
	SetBrightness(value / 25);
}

void SetVolume(int value) {
	if (!shared) return;
	if (value < 0) value = 0;
	if (value > 20) value = 20;
	shared->volume = value;
	
	int percent = (value * 100) / 20;
	char cmd[512];
	snprintf(cmd, sizeof(cmd),
		"amixer sset 'Playback' %d%% unmute >/dev/null 2>&1 || "
		"amixer sset 'Line Out' %d%% unmute >/dev/null 2>&1 || "
		"amixer sset 'Master' %d%% unmute >/dev/null 2>&1 || "
		"amixer sset 'DAC' %d%% unmute >/dev/null 2>&1 || "
		"amixer sset 'Speaker' %d%% unmute >/dev/null 2>&1",
		percent, percent, percent, percent, percent);
	system(cmd);
}

void SetRawVolume(int value) {
	SetVolume(value / 5);
}

void SetJack(int value)  { if (shared) shared->jack = value; }
void SetHDMI(int value)  { if (shared) shared->hdmi = value; }
void SetMute(int value)  { if (shared) shared->mute = value; }
