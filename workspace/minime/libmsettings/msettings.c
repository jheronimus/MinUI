/*
 * Minime libmsettings.
 * Shared-memory settings bridge (same contract as upstream).
 * Hardware access is traits-driven and lives in platform.c/traits.c.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "msettings.h"
#include "settings.h"
#include "traits.h"

typedef struct {
    int brightness;
    int volume;
    int jack;
    int hdmi;
    int mute;
    int charging;
    int battery;
    int bt;
    int size;
} SharedSettings;

static SharedSettings *shared = NULL;
static int shm_fd = -1;
static int is_host = 0;

void InitSettings(void) {
    shm_fd = shm_open("/SharedSettings", O_RDWR | O_CREAT, 0666);
    if (shm_fd < 0)
        return;

    SharedSettings proto = {.size = sizeof(SharedSettings)};
    ftruncate(shm_fd, sizeof(SharedSettings));
    shared = mmap(NULL, sizeof(SharedSettings), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared == MAP_FAILED) {
        shared = NULL;
        return;
    }

    if (shared->size == 0) {
        is_host = 1;
        memcpy(shared, &proto, sizeof(SharedSettings));
        SetBrightness(5);
        SetVolume(10);
    } else {
        is_host = 0;
    }
    SetJack(MINIME_audioJackConnected());
}

void QuitSettings(void) {
    if (shared) {
        munmap(shared, sizeof(SharedSettings));
        shared = NULL;
    }
    if (shm_fd >= 0) {
        close(shm_fd);
        shm_fd = -1;
    }
}

int GetBrightness(void) {
    return shared ? shared->brightness : 5;
}
int GetVolume(void) {
    return shared ? shared->volume : 10;
}
int GetJack(void) {
    return shared ? shared->jack : 0;
}
int GetHDMI(void) {
    return shared ? shared->hdmi : 0;
}
int GetMute(void) {
    return shared ? shared->mute : 0;
}

void SetBrightness(int value) {
    if (!shared)
        return;
    if (value < BRIGHTNESS_MIN)
        value = BRIGHTNESS_MIN;
    if (value > BRIGHTNESS_MAX)
        value = BRIGHTNESS_MAX;
    shared->brightness = value;

    const MinimeTraits *traits = MINIME_traits();
    int max = (traits && traits->screen_backlight_max > 0) ? traits->screen_backlight_max : 255;
    int raw = (value * max) / BRIGHTNESS_MAX;
    if (value > 0 && raw == 0)
        raw = 1;
    MINIME_videoSetBacklight(raw);
}

void SetRawBrightness(int value) {
    MINIME_videoSetBacklight(value);
}

void SetVolume(int value) {
    if (!shared)
        return;
    if (value < VOLUME_MIN)
        value = VOLUME_MIN;
    if (value > VOLUME_MAX)
        value = VOLUME_MAX;
    shared->volume = value;

    int raw = (value == 0) ? 0 : 60 + ((value - 1) * 40) / (VOLUME_MAX - 1);
    MINIME_audioSetRawVolume(raw);
}

void SetRawVolume(int value) {
    MINIME_audioSetRawVolume(value);
}

void SetJack(int value) {
    if (shared)
        shared->jack = value;
}
void SetHDMI(int value) {
    if (shared)
        shared->hdmi = value;
}
void SetMute(int value) {
    if (shared)
        shared->mute = value;
}
int GetCharging(void) {
    return shared ? shared->charging : 0;
}
void SetCharging(int value) {
    if (shared)
        shared->charging = value;
}
int GetBattery(void) {
    return shared ? shared->battery : 0;
}
void SetBattery(int value) {
    if (shared)
        shared->battery = value;
}
int GetBT(void) {
    return shared ? shared->bt : 0;
}
void SetBT(int value) {
    if (shared)
        shared->bt = value;
}
