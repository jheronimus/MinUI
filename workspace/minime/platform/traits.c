#include <ctype.h>
#include <dirent.h>
#include <errno.h>
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

typedef enum {
    TYPE_STRING,
    TYPE_INT,
    TYPE_ASPECT,
} TraitType;

typedef struct {
    const char *key;
    TraitType type;
    size_t offset;
} TraitField;

#define FIELD(type, name) {#name, type, offsetof(MinimeTraits, name)}
#define STR_FIELD(name) FIELD(TYPE_STRING, name)
#define INT_FIELD(name) FIELD(TYPE_INT, name)
#define ASPECT_FIELD(name) FIELD(TYPE_ASPECT, name)
// Entry where the file key differs from the struct field name. The traits
// file is the source of truth for key names; struct fields may be shorter.
#define KEYED_STR(key, name) {key, TYPE_STRING, offsetof(MinimeTraits, name)}
#define KEYED_INT(key, name) {key, TYPE_INT, offsetof(MinimeTraits, name)}

// Schema table: the single list of keys the parser understands. Anything not
// in this table is an error, so schema drift fails loudly instead of being
// silently ignored. Strings that are not present or are "na" remain empty.
static const TraitField TRAIT_FIELDS[] = {
    STR_FIELD(device_id),
    STR_FIELD(device_model),
    INT_FIELD(screen_width),
    INT_FIELD(screen_height),
    INT_FIELD(screen_rotation),
    INT_FIELD(screen_rotation_kernel),
    ASPECT_FIELD(screen_aspect),
    INT_FIELD(screen_refresh_rate),
    STR_FIELD(screen_backlight_path),
    INT_FIELD(screen_backlight_max),
    STR_FIELD(screen_blank_path),
    INT_FIELD(screen2_width),
    INT_FIELD(screen2_height),
    INT_FIELD(screen2_rotation),
    ASPECT_FIELD(screen2_aspect),
    INT_FIELD(screen2_refresh_rate),
    STR_FIELD(screen2_backlight_path),
    STR_FIELD(screen2_blank_path),
    INT_FIELD(screen2_touch),
    STR_FIELD(screen2_touch_device_name),
    STR_FIELD(cpu_governor_path),
    STR_FIELD(cpu_clock_path),
    INT_FIELD(cpu_clock_menu),
    INT_FIELD(cpu_clock_powersave),
    INT_FIELD(cpu_clock_normal),
    INT_FIELD(cpu_clock_performance),
    INT_FIELD(cpu_undervolt_supported),
    STR_FIELD(cpu_thermal_path),
    STR_FIELD(gpu_device),
    STR_FIELD(gpu_device2),
    STR_FIELD(gpu_hdmi_connector),
    STR_FIELD(gpu_driver),
    INT_FIELD(gpu_clock_min),
    INT_FIELD(gpu_clock_max),
    STR_FIELD(audio_card),
    STR_FIELD(audio_mixer),
    STR_FIELD(audio_jack_device_name),
    INT_FIELD(audio_mic),
    KEYED_STR("input_gamepad_device_name", input_gamepad),
    KEYED_STR("input_stick_device_name", input_stick),
    KEYED_STR("input_power_device_name", input_power),
    KEYED_STR("input_volume_device_name", input_volume),
    KEYED_STR("input_menu_device_name", input_menu),
    KEYED_STR("input_lid_device_name", input_lid),
    STR_FIELD(input_rumble_device_name),
    INT_FIELD(input_touch),
    STR_FIELD(input_touch_device_name),
    INT_FIELD(key_up),
    INT_FIELD(key_down),
    INT_FIELD(key_left),
    INT_FIELD(key_right),
    INT_FIELD(key_a),
    INT_FIELD(key_b),
    INT_FIELD(key_c),
    INT_FIELD(key_x),
    INT_FIELD(key_y),
    INT_FIELD(key_z),
    INT_FIELD(key_l1),
    INT_FIELD(key_r1),
    INT_FIELD(key_l2),
    INT_FIELD(key_r2),
    INT_FIELD(key_l3),
    INT_FIELD(key_r3),
    INT_FIELD(key_start),
    INT_FIELD(key_select),
    INT_FIELD(key_menu),
    INT_FIELD(key_power),
    INT_FIELD(key_vol_up),
    INT_FIELD(key_vol_down),
    KEYED_INT("input_axis_lx", axis_lx),
    KEYED_INT("input_axis_ly", axis_ly),
    KEYED_INT("input_axis_rx", axis_rx),
    KEYED_INT("input_axis_ry", axis_ry),
    KEYED_INT("input_axis_min", axis_min),
    KEYED_INT("input_axis_center", axis_center),
    KEYED_INT("input_axis_max", axis_max),
    KEYED_INT("input_axis_lx_invert", axis_lx_invert),
    KEYED_INT("input_axis_ly_invert", axis_ly_invert),
    KEYED_INT("input_axis_rx_invert", axis_rx_invert),
    KEYED_INT("input_axis_ry_invert", axis_ry_invert),
    STR_FIELD(wifi_interface),
    STR_FIELD(bluetooth_interface),
    STR_FIELD(power_battery_sysfs),
    STR_FIELD(power_charger_online_path),
    STR_FIELD(power_led_path),
    INT_FIELD(usb_otg),
    INT_FIELD(usb_host_ports),
    INT_FIELD(usb_device_mode),
    INT_FIELD(usb_controller_mode),
    STR_FIELD(storage_sd_node),
    STR_FIELD(storage_sd2_node),
    STR_FIELD(storage_emmc_node),
};

#define TRAIT_FIELD_COUNT (sizeof(TRAIT_FIELDS) / sizeof(TRAIT_FIELDS[0]))

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

static const TraitField *findField(const char *key) {
    for (size_t i = 0; i < TRAIT_FIELD_COUNT; i++) {
        if (!strcmp(key, TRAIT_FIELDS[i].key))
            return &TRAIT_FIELDS[i];
    }
    return NULL;
}

// Apply a single key=value line to the struct. Returns 0 on success, -1 on
// unknown key, malformed int, or out-of-range value.
static int setValue(const char *key, const char *value) {
    const TraitField *field = findField(key);
    if (!field) {
        fprintf(stderr, "Minime traits: unknown key '%s' in %s\n", key, TRAITS_PATH);
        return -1;
    }

    if (field->type == TYPE_STRING) {
        copyText((char *)&traits + field->offset, MINIME_TRAIT_NAME_MAX, value);
        return 0;
    }

    if (field->type == TYPE_ASPECT) {
        // Value is "W:H" (e.g. "4:3"); maps to the aspect enum. Missing/na
        // stays UNKNOWN and the init derive step fills it from dimensions.
        MinimeScreenAspect aspect = MINIME_ASPECT_UNKNOWN;
        int w = 0, h = 0;
        if (sscanf(value, "%d:%d", &w, &h) == 2 && w > 0 && h > 0) {
            if (w * 3 == h * 4)
                aspect = MINIME_ASPECT_4x3;
            else if (w * 2 == h * 3)
                aspect = MINIME_ASPECT_3x2;
            else if (w * 9 == h * 16)
                aspect = MINIME_ASPECT_16x9;
            else if (w == h)
                aspect = MINIME_ASPECT_1x1;
        }
        *(MinimeScreenAspect *)((char *)&traits + field->offset) = aspect;
        return 0;
    }

    int parsed = parseInt(value);
    if (parsed < 0 && strcmp(value, NA)) {
        fprintf(stderr, "Minime traits: invalid integer '%s' for '%s'\n", value, key);
        return -1;
    }
    *(int *)((char *)&traits + field->offset) = parsed;
    return 0;
}

int MINIME_traitAvailable(const char *value) {
    return value && value[0] && strcmp(value, NA);
}

// Resolve the stable gpu_hdmi_connector (e.g. "HDMI-A-1") to the actual DRM
// sysfs status path. The card number prefix ("card0", "card1", ...) is the
// DRM primary-minor index, allocated first-come-first-serve at probe time,
// so it is NOT part of the trait file — we glob /sys/class/drm/card* and
// pick the connector whose suffix matches. Empty connector means no HDMI.
static void resolveHdmiConnector(void) {
    const char *connector = traits.gpu_hdmi_connector;

    if (!MINIME_traitAvailable(connector))
        return;

    DIR *dir = opendir("/sys/class/drm");
    if (!dir)
        return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        const char *dash = strchr(name, '-');
        if (!dash)
            continue;
        // name = "cardN-<connector>"; match the suffix after the card prefix.
        if (strcmp(dash + 1, connector) == 0) {
            snprintf(traits.gpu_hdmi_state_path, sizeof(traits.gpu_hdmi_state_path),
                     "/sys/class/drm/%s/status", name);
            break;
        }
    }
    closedir(dir);
}

static int validate(void) {
    if (!traits.device_id[0] || !traits.device_model[0] ||
        !MINIME_traitAvailable(traits.gpu_device) || traits.screen_width <= 0 ||
        traits.screen_height <= 0 || traits.screen_rotation < 0 ||
        !MINIME_traitAvailable(traits.screen_backlight_path) || traits.screen_backlight_max <= 0 ||
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

        key = trimWhitespace(line);
        if (!key[0] || key[0] == '#' || key[0] == '[')
            continue;
        value = strchr(key, '=');
        if (!value)
            continue;
        *value++ = '\0';
        if (setValue(trimWhitespace(key), trimWhitespace(value)) != 0) {
            fclose(file);
            return -1;
        }
    }
    fclose(file);

    // screen_aspect is parsed from the file when present; derive it from the
    // panel dimensions only as a fallback so the emitted value is authoritative.
    if (traits.screen_aspect == MINIME_ASPECT_UNKNOWN && traits.screen_width > 0 &&
        traits.screen_height > 0) {
        if (traits.screen_width * 3 == traits.screen_height * 4)
            traits.screen_aspect = MINIME_ASPECT_4x3;
        else if (traits.screen_width * 2 == traits.screen_height * 3)
            traits.screen_aspect = MINIME_ASPECT_3x2;
        else if (traits.screen_width * 9 == traits.screen_height * 16)
            traits.screen_aspect = MINIME_ASPECT_16x9;
        else if (traits.screen_width == traits.screen_height)
            traits.screen_aspect = MINIME_ASPECT_1x1;
    }

    resolveHdmiConnector();

    valid = validate() == 0;
    return valid ? 0 : -1;
}

const MinimeTraits *MINIME_traits(void) {
    return MINIME_traitsInit() == 0 ? &traits : NULL;
}

int MINIME_audioJackConnected(void) {
    const MinimeTraits *traits = MINIME_traits();

    return (traits && MINIME_traitAvailable(traits->audio_jack_device_name)) ? 1 : 0;
}

void MINIME_audioSetRawVolume(int value) {
    const MinimeTraits *traits = MINIME_traits();
    char card_flag[64] = "";
    char command[512];

    if (!traits)
        return;
    // audio_mixer is the ALSA simple-mixer control (e.g. "Line Out").
    // Codec init quirks (DAC pswitch unmute, Playback Mux / jack routing)
    // are owned by the firmware ui init.d, not the UI.
    if (strcmp(traits->audio_card, "default") != 0) {
        snprintf(card_flag, sizeof(card_flag), "-c '%s' ", traits->audio_card);
    }
    snprintf(command, sizeof(command), "amixer -q %ssset '%s' %d%% unmute >/dev/null 2>&1",
             card_flag, traits->audio_mixer, value);
    (void)system(command);
}

int MINIME_videoHDMIConnected(void) {
    const MinimeTraits *traits = MINIME_traits();

    if (!traits || !MINIME_traitAvailable(traits->gpu_hdmi_state_path))
        return 0;

    // DRM connector status files ("/sys/class/drm/cardN-<connector>/status")
    // contain text ("connected"/"disconnected"), not an integer. Parse the
    // text first, then fall back to numeric paths for non-DRM backends.
    char status[16] = "";
    getFile((char *)traits->gpu_hdmi_state_path, status, sizeof(status));
    if (status[0] != '\0') {
        if (prefixMatch("connected", status))
            return 1;
        if (prefixMatch("disconnected", status) || prefixMatch("unknown", status))
            return 0;
    }
    return getInt((char *)traits->gpu_hdmi_state_path);
}

void MINIME_videoSetBacklight(int value) {
    const MinimeTraits *traits = MINIME_traits();

    if (traits && MINIME_traitAvailable(traits->screen_backlight_path))
        putInt((char *)traits->screen_backlight_path, value);
}

void MINIME_videoBlank(int blank) {
    const MinimeTraits *traits = MINIME_traits();

    if (traits && MINIME_traitAvailable(traits->screen_blank_path))
        putInt((char *)traits->screen_blank_path, blank ? 4 : 0);
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
    if (!traits || !fds)
        return 0;
    // Data-driven device list: add a device by adding a trait field + one
    // element here — sizeof keeps the loop in sync (no separate count to
    // forget to bump). Empty/unset names are skipped by MINIME_inputOpenByName.
    const char *names[] = {
        traits->input_gamepad, traits->input_stick, traits->input_power,
        traits->input_volume,  traits->input_menu,
    };
    int count = 0;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]) && (size_t)count < max_fds; i++) {
        int fd = MINIME_inputOpenByName(names[i]);

        if (fd >= 0)
            fds[count++] = fd;
    }
    return count;
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

    if (MINIME_traitAvailable(traits->power_charger_online_path)) {
        *charging = getInt((char *)traits->power_charger_online_path);
        return 0;
    }
    if (!MINIME_traitAvailable(traits->power_battery_sysfs))
        return -1;
    snprintf(path, sizeof(path), "%s/status", traits->power_battery_sysfs);
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

    if (!traits || !charging || !capacity || !MINIME_traitAvailable(traits->power_battery_sysfs))
        return -1;
    if (readCapacity(traits->power_battery_sysfs, capacity) != 0)
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
    int fd;
    struct ff_effect effect;

    if (!traits || !MINIME_traitAvailable(traits->input_rumble_device_name))
        return;

    // The rumble motor is exposed as an input device with FF_RUMBLE
    // (e.g. "pwm-vibrator"). Upload an effect with the desired magnitude;
    // the memless FF layer replays it until it is removed or replaced.
    fd = MINIME_inputOpenByName(traits->input_rumble_device_name);
    if (fd < 0)
        return;

    memset(&effect, 0, sizeof(effect));
    effect.type = FF_RUMBLE;
    effect.id = -1;
    if (enabled) {
        effect.u.rumble.strong_magnitude = 0xffff;
        effect.u.rumble.weak_magnitude = 0xffff;
    }
    if (ioctl(fd, EVIOCSFF, &effect) < 0 && enabled) {
        // No effect was ever uploaded; an explicit stop has nothing to remove.
        if (errno != ENODEV)
            close(fd);
        return;
    }
    close(fd);
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
