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
	shared->brightness = value;
	// TODO: implement actual brightness via traits (backlight_path)
}

void SetRawBrightness(int value) {
	// TODO: implement via traits backlight_path
	(void)value;
}

void SetVolume(int value) {
	if (!shared) return;
	shared->volume = value;
	// TODO: implement actual volume via traits (sound_card, sound_mixer)
}

void SetRawVolume(int value) {
	// TODO: implement via traits sound_card/sound_mixer
	(void)value;
}

void SetJack(int value)  { if (shared) shared->jack = value; }
void SetHDMI(int value)  { if (shared) shared->hdmi = value; }
void SetMute(int value)  { if (shared) shared->mute = value; }
