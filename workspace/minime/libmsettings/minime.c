/*
 * Minime hardware HAL.
 * Traits-driven hardware access shared by libmsettings (via -lmsettings),
 * the UI platform layer and keymon. Lives in libmsettings.so so a single
 * copy serves every consumer; upstream inlines equivalent hardware access
 * per-device in its own files.
 */
#include <fcntl.h>
#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "msettings.h"
#include "traits.h"
#include "utils.h"

int MINIME_audioJackConnected(void) {
    const MinimeTraits *traits = MINIME_traits();

    return (traits && MINIME_traitAvailable(traits->jack_state_path))
               ? getInt((char *)traits->jack_state_path)
               : 0;
}

void MINIME_audioSetRawVolume(int value) {
    const MinimeTraits *traits = MINIME_traits();
    char command[512];

    if (!traits)
        return;
    if (strcmp(traits->sound_card, "default") == 0)
        snprintf(command, sizeof(command), "amixer -q sset '%s' %d%% >/dev/null 2>&1",
                 traits->sound_mixer, value);
    else
        snprintf(command, sizeof(command), "amixer -q -c '%s' sset '%s' %d%% >/dev/null 2>&1",
                 traits->sound_card, traits->sound_mixer, value);
    (void)system(command);
}

int MINIME_videoHDMIConnected(void) {
    const MinimeTraits *traits = MINIME_traits();

    return (traits && MINIME_traitAvailable(traits->hdmi_state_path))
               ? getInt((char *)traits->hdmi_state_path)
               : 0;
}

void MINIME_videoSetBacklight(int value) {
    const MinimeTraits *traits = MINIME_traits();

    if (traits && MINIME_traitAvailable(traits->backlight_path))
        putInt((char *)traits->backlight_path, value);
}

void MINIME_videoBlank(int blank) {
    const MinimeTraits *traits = MINIME_traits();

    if (traits && MINIME_traitAvailable(traits->framebuffer_blank_path))
        putInt((char *)traits->framebuffer_blank_path, blank ? 4 : 0);
}

#define INPUT_EVENT_LIMIT 32

int MINIME_inputOpenByName(const char *expected) {
    char name[256];
    char path[64];

    if (!MINIME_traitAvailable(expected))
        return -1;
    for (int i = 0; i < INPUT_EVENT_LIMIT; i++) {
        int fd;

        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;
        memset(name, 0, sizeof(name));
        if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0 && !strcmp(name, expected))
            return fd;
        close(fd);
    }
    return -1;
}

int MINIME_inputOpenShortcutDevices(int *fds, size_t max_fds) {
    const MinimeTraits *traits = MINIME_traits();
    const char *names[3];
    int count = 0;

    if (!traits || !fds)
        return 0;
    names[0] = traits->input_gamepad;
    names[1] = traits->input_power;
    names[2] = traits->input_volume;
    for (size_t i = 0; i < 3 && (size_t)count < max_fds; i++) {
        int fd = MINIME_inputOpenByName(names[i]);

        if (fd >= 0)
            fds[count++] = fd;
    }
    return count;
}

int MINIME_inputHasCZ(void) {
    const MinimeTraits *traits = MINIME_traits();

    return traits && traits->key_c >= 0 && traits->key_z >= 0;
}

int MINIME_inputNormalizeAxis(int value, int invert) {
    const MinimeTraits *traits = MINIME_traits();
    int normalized;

    if (!traits || traits->axis_min >= traits->axis_center ||
        traits->axis_center >= traits->axis_max)
        return 0;
    if (value < traits->axis_center) {
        normalized =
            -((traits->axis_center - value) * 32767) / (traits->axis_center - traits->axis_min);
    } else {
        normalized =
            ((value - traits->axis_center) * 32767) / (traits->axis_max - traits->axis_center);
    }
    return invert ? -normalized : normalized;
}

static int readCapacity(const char *root, int *capacity) {
    char path[MINIME_TRAIT_PATH_MAX + 32];

    snprintf(path, sizeof(path), "%s/capacity", root);
    if (!MINIME_traitAvailable(path) || !capacity)
        return -1;
    *capacity = getInt(path);
    return 0;
}

static int readCharging(const MinimeTraits *traits, int *charging) {
    char path[MINIME_TRAIT_PATH_MAX + 32];
    char status[32];
    FILE *file;

    if (MINIME_traitAvailable(traits->charger_online_path)) {
        *charging = getInt((char *)traits->charger_online_path);
        return 0;
    }
    if (!MINIME_traitAvailable(traits->battery_capacity_path))
        return -1;
    snprintf(path, sizeof(path), "%s/status", traits->battery_capacity_path);
    file = fopen(path, "r");
    if (!file)
        return -1;
    status[0] = '\0';
    (void)fgets(status, sizeof(status), file);
    fclose(file);
    *charging = !strncmp(status, "Charging", 8) || !strncmp(status, "Full", 4);
    return 0;
}

int MINIME_powerGetBattery(int *charging, int *capacity) {
    const MinimeTraits *traits = MINIME_traits();

    if (!traits || !charging || !capacity || !MINIME_traitAvailable(traits->battery_capacity_path))
        return -1;
    if (readCapacity(traits->battery_capacity_path, capacity) != 0)
        return -1;
    if (readCharging(traits, charging) != 0)
        *charging = 0;
    return 0;
}

void MINIME_powerSetLED(int enabled) {
    const MinimeTraits *traits = MINIME_traits();

    if (traits && MINIME_traitAvailable(traits->power_led_path))
        putInt((char *)traits->power_led_path, enabled);
}

void MINIME_powerSetRumble(int enabled) {
    const MinimeTraits *traits = MINIME_traits();

    if (traits && MINIME_traitAvailable(traits->rumble_path))
        putInt((char *)traits->rumble_path, enabled);
}

void MINIME_powerSetCPUSpeed(int speed) {
    const MinimeTraits *traits = MINIME_traits();
    const char *governor;
    int clock = -1;

    if (!traits)
        return;

    if (speed <= 0) {
        governor = "schedutil";
        clock = traits->cpu_clock_menu;
    } else if (speed == 1) {
        governor = "schedutil";
        clock = traits->cpu_clock_powersave;
    } else if (speed == 2) {
        governor = "schedutil";
        clock = traits->cpu_clock_normal;
    } else {
        governor = "performance";
        clock = traits->cpu_clock_performance;
    }

    if (MINIME_traitAvailable(traits->cpu_governor_path))
        putFile((char *)traits->cpu_governor_path, (char *)governor);

    if (MINIME_traitAvailable(traits->cpu_clock_path) && clock > 0)
        putInt((char *)traits->cpu_clock_path, clock);
}
