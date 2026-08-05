#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "traits.h"
#include "utils.h"
#include <fcntl.h>
#include <linux/input.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define TRAITS_PATH "/mnt/sdcard/.minime/traits"
#define NA "na"

static char *trim(char *text) {
    char *end;
    while (*text && isspace((unsigned char)*text))
        text++;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
    return text;
}

static MinimeTraits traits;
static int initialized;
static int valid;

static void copyText(char *dst, size_t size, const char *src) {
    if (!dst || !size)
        return;
    snprintf(dst, size, "%s", src ? src : "");
}

static int parseInt(const char *value) {
    char *end;
    long parsed;

    if (!value || !strcmp(value, NA))
        return -1;
    parsed = strtol(value, &end, 10);
    return *end ? -1 : (int)parsed;
}

static void setValue(const char *key, const char *value) {
#define STRING_TRAIT(name)                                                                         \
    if (!strcmp(key, #name))                                                                       \
    copyText(traits.name, sizeof(traits.name), value)
#define INT_TRAIT(name)                                                                            \
    if (!strcmp(key, #name))                                                                       \
    traits.name = parseInt(value)

    STRING_TRAIT(device_id);
    else STRING_TRAIT(device_model);
    else STRING_TRAIT(video_device);
    else INT_TRAIT(screen_width);
    else INT_TRAIT(screen_height);
    else INT_TRAIT(screen_rotation);
    else STRING_TRAIT(backlight_path);
    else INT_TRAIT(backlight_max);
    else STRING_TRAIT(framebuffer_blank_path);
    else STRING_TRAIT(hdmi_state_path);
    else STRING_TRAIT(battery_capacity_path);
    else STRING_TRAIT(charger_online_path);
    else STRING_TRAIT(rumble_path);
    else STRING_TRAIT(power_led_path);
    else STRING_TRAIT(cpu_governor_path);
    else STRING_TRAIT(cpu_clock_path);
    else INT_TRAIT(cpu_clock_menu);
    else INT_TRAIT(cpu_clock_powersave);
    else INT_TRAIT(cpu_clock_normal);
    else INT_TRAIT(cpu_clock_performance);
    else STRING_TRAIT(sound_card);
    else STRING_TRAIT(sound_mixer);
    else STRING_TRAIT(jack_state_path);
    else STRING_TRAIT(wifi_interface);
    else if (!strcmp(key, "input_gamepad_device_name"))
        copyText(traits.input_gamepad, sizeof(traits.input_gamepad), value);
    else if (!strcmp(key, "input_power_device_name"))
        copyText(traits.input_power, sizeof(traits.input_power), value);
    else if (!strcmp(key, "input_volume_device_name"))
        copyText(traits.input_volume, sizeof(traits.input_volume), value);
    else if (!strcmp(key, "input_lid_device_name"))
        copyText(traits.input_lid, sizeof(traits.input_lid), value);
    else INT_TRAIT(key_up);
    else INT_TRAIT(key_down);
    else INT_TRAIT(key_left);
    else INT_TRAIT(key_right);
    else INT_TRAIT(key_a);
    else INT_TRAIT(key_b);
    else INT_TRAIT(key_c);
    else INT_TRAIT(key_x);
    else INT_TRAIT(key_y);
    else INT_TRAIT(key_z);
    else INT_TRAIT(key_l1);
    else INT_TRAIT(key_r1);
    else INT_TRAIT(key_l2);
    else INT_TRAIT(key_r2);
    else INT_TRAIT(key_l3);
    else INT_TRAIT(key_r3);
    else INT_TRAIT(key_start);
    else INT_TRAIT(key_select);
    else INT_TRAIT(key_menu);
    else INT_TRAIT(key_power);
    else INT_TRAIT(key_vol_up);
    else INT_TRAIT(key_vol_down);
    else INT_TRAIT(axis_lx);
    else INT_TRAIT(axis_ly);
    else INT_TRAIT(axis_rx);
    else INT_TRAIT(axis_ry);
    else INT_TRAIT(axis_min);
    else INT_TRAIT(axis_center);
    else INT_TRAIT(axis_max);
    else INT_TRAIT(axis_lx_invert);
    else INT_TRAIT(axis_ly_invert);
    else INT_TRAIT(axis_rx_invert);
    else INT_TRAIT(axis_ry_invert);
    else INT_TRAIT(undervolt_supported);

#undef STRING_TRAIT
#undef INT_TRAIT
}

int MINIME_traitAvailable(const char *value) {
    return value && value[0] && strcmp(value, NA);
}

static int validate(void) {
    if (!traits.device_id[0] || !traits.device_model[0] ||
        !MINIME_traitAvailable(traits.video_device) || traits.screen_width <= 0 ||
        traits.screen_height <= 0 || traits.screen_rotation < 0 ||
        !MINIME_traitAvailable(traits.backlight_path) || traits.backlight_max <= 0 ||
        !MINIME_traitAvailable(traits.input_gamepad) ||
        !MINIME_traitAvailable(traits.input_power) || !MINIME_traitAvailable(traits.input_volume) ||
        traits.key_up < 0 || traits.key_down < 0 || traits.key_left < 0 || traits.key_right < 0 ||
        traits.key_a < 0 || traits.key_b < 0 || traits.key_x < 0 || traits.key_y < 0 ||
        traits.key_start < 0 || traits.key_select < 0 || traits.key_menu < 0 ||
        traits.key_power < 0 || traits.key_vol_up < 0 || traits.key_vol_down < 0) {
        fprintf(stderr, "Invalid required Minime traits in %s\n", TRAITS_PATH);
        return -1;
    }
    return 0;
}

int MINIME_traitsInit(void) {
    FILE *file;
    char line[512];

    if (initialized)
        return valid ? 0 : -1;
    initialized = 1;
    memset(&traits, 0, sizeof(traits));
    traits.key_c = traits.key_z = -1;
    traits.key_l1 = traits.key_r1 = -1;
    traits.key_l2 = traits.key_r2 = -1;
    traits.key_l3 = traits.key_r3 = -1;
    traits.axis_lx = traits.axis_ly = -1;
    traits.axis_rx = traits.axis_ry = -1;
    traits.axis_min = traits.axis_center = traits.axis_max = -1;

    file = fopen(TRAITS_PATH, "r");
    if (!file) {
        fprintf(stderr, "Missing Minime traits: %s\n", TRAITS_PATH);
        return -1;
    }
    while (fgets(line, sizeof(line), file)) {
        char *key;
        char *value;

        key = trim(line);
        if (!key[0] || key[0] == '#' || key[0] == '[')
            continue;
        value = strchr(key, '=');
        if (!value)
            continue;
        *value++ = '\0';
        setValue(trim(key), trim(value));
    }
    fclose(file);
    valid = validate() == 0;
    return valid ? 0 : -1;
}

const MinimeTraits *MINIME_traits(void) {
    return MINIME_traitsInit() == 0 ? &traits : NULL;
}

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
