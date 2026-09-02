#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "traits.h"
#include "utils.h"

#define TRAITS_PATH "/mnt/sdcard/.minime/traits"
#define NA "na"

///////////////////////////////
// Canonical Hardware Variables

char device_id[MINIME_TRAIT_NAME_MAX] = "";
char device_model[MINIME_TRAIT_PATH_MAX] = "";

int screen_width = 640;
int screen_height = 480;
int screen_rotation = 0;
int screen_padding = 10;
int screen_row_count = 6;
MinimeScreenAspect screen_aspect = MINIME_ASPECT_4x3;
int screen_refresh_rate = 60;
char screen_backlight_path[MINIME_TRAIT_PATH_MAX] = "";
int screen_backlight_max = 255;
char screen_blank_path[MINIME_TRAIT_PATH_MAX] = "";

char gpu_device[MINIME_TRAIT_PATH_MAX] = "";
char gpu_hdmi_state_path[MINIME_TRAIT_PATH_MAX] = "";
int gpu_hdmi_width = 1280;
int gpu_hdmi_height = 720;
char gpu_devfreq_path[MINIME_TRAIT_PATH_MAX] = "";
int gpu_clock_min = -1;
int gpu_clock_max = -1;

char cpu_governor_path[MINIME_TRAIT_PATH_MAX] = "";
char cpu_clock_path[MINIME_TRAIT_PATH_MAX] = "";
int cpu_clock_menu = -1;
int cpu_clock_powersave = -1;
int cpu_clock_normal = -1;
int cpu_clock_performance = -1;
int cpu_undervolt_supported = 0;

char audio_card[MINIME_TRAIT_NAME_MAX] = "default";
char audio_mixer[MINIME_TRAIT_NAME_MAX] = "Master";
char audio_jack_device_name[MINIME_TRAIT_NAME_MAX] = "";

char power_battery_sysfs[MINIME_TRAIT_PATH_MAX] = "";
char power_charger_online_path[MINIME_TRAIT_PATH_MAX] = "";
char power_led_path[MINIME_TRAIT_PATH_MAX] = "";

char wifi_interface[MINIME_TRAIT_NAME_MAX] = "";
char bluetooth_interface[MINIME_TRAIT_NAME_MAX] = "";

int button_keycodes[BTN_ID_COUNT];
int axis_lx = -1;
int axis_ly = -1;
int axis_rx = -1;
int axis_ry = -1;
int axis_hat_x = 16;
int axis_hat_y = 17;
int axis_min = -1;
int axis_center = -1;
int axis_max = -1;
int axis_lx_invert = 0;
int axis_ly_invert = 0;
int axis_rx_invert = 0;
int axis_ry_invert = 0;
char input_gamepad[MINIME_TRAIT_NAME_MAX] = "";
char input_stick[MINIME_TRAIT_NAME_MAX] = "";
char input_power[MINIME_TRAIT_NAME_MAX] = "";
char input_volume[MINIME_TRAIT_NAME_MAX] = "";
char input_menu[MINIME_TRAIT_NAME_MAX] = "";
char input_rumble[MINIME_TRAIT_NAME_MAX] = "";
char input_lid[MINIME_TRAIT_NAME_MAX] = "";

///////////////////////////////
// Internal Schema & Parser State

typedef struct {
	char device_id[MINIME_TRAIT_NAME_MAX];
	char device_model[MINIME_TRAIT_PATH_MAX];
	int screen_width;
	int screen_height;
	int screen_rotation;
	int screen_rotation_kernel;
	MinimeScreenAspect screen_aspect;
	int screen_refresh_rate;
	char screen_backlight_path[MINIME_TRAIT_PATH_MAX];
	int screen_backlight_max;
	char screen_blank_path[MINIME_TRAIT_PATH_MAX];
	int ui_padding;
	int ui_row_count;
	int screen2_width;
	int screen2_height;
	int screen2_rotation;
	MinimeScreenAspect screen2_aspect;
	int screen2_refresh_rate;
	char screen2_backlight_path[MINIME_TRAIT_PATH_MAX];
	char screen2_blank_path[MINIME_TRAIT_PATH_MAX];
	int screen2_touch;
	char screen2_touch_device_name[MINIME_TRAIT_NAME_MAX];

	char cpu_governor_path[MINIME_TRAIT_PATH_MAX];
	char cpu_clock_path[MINIME_TRAIT_PATH_MAX];
	int cpu_clock_menu;
	int cpu_clock_powersave;
	int cpu_clock_normal;
	int cpu_clock_performance;
	int cpu_undervolt_supported;
	char cpu_thermal_path[MINIME_TRAIT_PATH_MAX];

	char gpu_device[MINIME_TRAIT_PATH_MAX];
	char gpu_device2[MINIME_TRAIT_PATH_MAX];
	char gpu_hdmi_connector[MINIME_TRAIT_NAME_MAX];
	char gpu_hdmi_state_path[MINIME_TRAIT_PATH_MAX];
	char gpu_driver[MINIME_TRAIT_NAME_MAX];
	int gpu_clock_min;
	int gpu_clock_max;
	int gpu_hdmi_width;
	int gpu_hdmi_height;
	char gpu_devfreq_path[MINIME_TRAIT_PATH_MAX];

	char audio_card[MINIME_TRAIT_NAME_MAX];
	char audio_mixer[MINIME_TRAIT_NAME_MAX];
	char audio_jack_device_name[MINIME_TRAIT_NAME_MAX];
	int audio_mic;

	char input_gamepad[MINIME_TRAIT_NAME_MAX];
	char input_stick[MINIME_TRAIT_NAME_MAX];
	char input_power[MINIME_TRAIT_NAME_MAX];
	char input_volume[MINIME_TRAIT_NAME_MAX];
	char input_menu[MINIME_TRAIT_NAME_MAX];
	char input_lid[MINIME_TRAIT_NAME_MAX];
	char input_rumble_device_name[MINIME_TRAIT_NAME_MAX];
	int input_touch;
	char input_touch_device_name[MINIME_TRAIT_NAME_MAX];

	int key_up;
	int key_down;
	int key_left;
	int key_right;
	int key_a;
	int key_b;
	int key_c;
	int key_x;
	int key_y;
	int key_z;
	int key_l1;
	int key_r1;
	int key_l2;
	int key_r2;
	int key_l3;
	int key_r3;
	int key_start;
	int key_select;
	int key_menu;
	int key_power;
	int key_vol_up;
	int key_vol_down;

	int axis_lx;
	int axis_ly;
	int axis_rx;
	int axis_ry;
	int axis_min;
	int axis_center;
	int axis_max;
	int axis_lx_invert;
	int axis_ly_invert;
	int axis_rx_invert;
	int axis_ry_invert;
	int axis_hat_x;
	int axis_hat_y;

	char wifi_interface[MINIME_TRAIT_NAME_MAX];
	char bluetooth_interface[MINIME_TRAIT_NAME_MAX];

	char power_battery_sysfs[MINIME_TRAIT_PATH_MAX];
	char power_charger_online_path[MINIME_TRAIT_PATH_MAX];
	char power_led_path[MINIME_TRAIT_PATH_MAX];

	int usb_otg;
	int usb_host_ports;
	int usb_device_mode;
	int usb_controller_mode;

	char storage_sd_node[MINIME_TRAIT_PATH_MAX];
	char storage_sd2_node[MINIME_TRAIT_PATH_MAX];
	char storage_emmc_node[MINIME_TRAIT_PATH_MAX];
} TraitParserState;

static TraitParserState traits;
static int initialized = 0;
static int valid = 0;

typedef enum {
	TYPE_STRING,
	TYPE_INT,
	TYPE_ASPECT,
} TraitType;

typedef struct {
	const char* key;
	TraitType type;
	size_t offset;
} TraitField;

#define FIELD(type, name) {#name, type, offsetof(TraitParserState, name)}
#define STR_FIELD(name) FIELD(TYPE_STRING, name)
#define INT_FIELD(name) FIELD(TYPE_INT, name)
#define ASPECT_FIELD(name) FIELD(TYPE_ASPECT, name)
#define KEYED_STR(key, name) {key, TYPE_STRING, offsetof(TraitParserState, name)}
#define KEYED_INT(key, name) {key, TYPE_INT, offsetof(TraitParserState, name)}

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
	INT_FIELD(ui_padding),
	INT_FIELD(ui_row_count),
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
	STR_FIELD(gpu_hdmi_state_path),
	STR_FIELD(gpu_driver),
	INT_FIELD(gpu_clock_min),
	INT_FIELD(gpu_clock_max),
	INT_FIELD(gpu_hdmi_width),
	INT_FIELD(gpu_hdmi_height),
	STR_FIELD(gpu_devfreq_path),
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
	INT_FIELD(axis_hat_x),
	INT_FIELD(axis_hat_y),
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

///////////////////////////////
// Value Parsers & Helpers

static void copyText(char* dst, size_t size, const char* src) {
	if (!dst || !size)
		return;
	snprintf(dst, size, "%s", src ? src : "");
}

static int parseInt(const char* value) {
	char* end;
	long parsed;

	if (!value || !strcmp(value, NA))
		return -1;
	parsed = strtol(value, &end, 10);
	return *end ? -1 : (int)parsed;
}

static const TraitField* findField(const char* key) {
	for (size_t i = 0; i < TRAIT_FIELD_COUNT; i++) {
		if (!strcmp(key, TRAIT_FIELDS[i].key))
			return &TRAIT_FIELDS[i];
	}
	return NULL;
}

static MinimeScreenAspect parseAspect(const char* value) {
	if (!value) return MINIME_ASPECT_UNKNOWN;
	if (!strcmp(value, "4:3")) return MINIME_ASPECT_4x3;
	if (!strcmp(value, "3:2")) return MINIME_ASPECT_3x2;
	if (!strcmp(value, "16:9")) return MINIME_ASPECT_16x9;
	if (!strcmp(value, "1:1")) return MINIME_ASPECT_1x1;
	return MINIME_ASPECT_UNKNOWN;
}

static int setValue(const char* key, const char* value) {
	const TraitField* field = findField(key);
	if (!field) {
		fprintf(stderr, "Minime traits: unknown key '%s' in %s\n", key, TRAITS_PATH);
		return -1;
	}

	if (field->type == TYPE_STRING) {
		copyText((char*)&traits + field->offset, MINIME_TRAIT_NAME_MAX, value);
		return 0;
	}

	if (field->type == TYPE_ASPECT) {
		*(MinimeScreenAspect*)((char*)&traits + field->offset) = parseAspect(value);
		return 0;
	}

	int parsed = parseInt(value);
	if (parsed < 0 && strcmp(value, NA)) {
		fprintf(stderr, "Minime traits: invalid integer '%s' for '%s'\n", value, key);
		return -1;
	}
	*(int*)((char*)&traits + field->offset) = parsed;
	return 0;
}

int MINIME_traitAvailable(const char* value) {
	return value && value[0] && strcmp(value, NA);
}

///////////////////////////////
// Trait Validation

static int validateRequiredKeys(void) {
	const int required[] = {
		traits.key_up, traits.key_down, traits.key_left, traits.key_right,
		traits.key_a, traits.key_b, traits.key_x, traits.key_y,
		traits.key_start, traits.key_select, traits.key_menu,
		traits.key_power, traits.key_vol_up, traits.key_vol_down};
	for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); i++) {
		if (required[i] < 0) return 0;
	}
	return 1;
}

static int validateDisplay(void) {
	return (traits.screen_width > 0 && traits.screen_height > 0 &&
			traits.screen_rotation >= 0 &&
			MINIME_traitAvailable(traits.gpu_device) &&
			MINIME_traitAvailable(traits.screen_backlight_path) &&
			traits.screen_backlight_max > 0);
}

static int validateInputs(void) {
	return (MINIME_traitAvailable(traits.input_gamepad) &&
			MINIME_traitAvailable(traits.input_power) &&
			MINIME_traitAvailable(traits.input_volume) &&
			validateRequiredKeys());
}

static int validate(void) {
	if (!traits.device_id[0] || !traits.device_model[0] ||
		!validateDisplay() || !validateInputs()) {
		fprintf(stderr, "Invalid required Minime traits in %s\n", TRAITS_PATH);
		return -1;
	}
	return 0;
}

///////////////////////////////
// Trait Resolution & Export

static void exportResolvedTraits(void) {
	copyText(device_id, sizeof(device_id), traits.device_id);
	copyText(device_model, sizeof(device_model), traits.device_model);

	screen_width = traits.screen_width;
	screen_height = traits.screen_height;
	screen_rotation = (traits.screen_rotation > 0) ? traits.screen_rotation : 0;
	screen_padding = traits.ui_padding;
	screen_row_count = traits.ui_row_count;
	screen_aspect = traits.screen_aspect;
	screen_refresh_rate = traits.screen_refresh_rate;
	copyText(screen_backlight_path, sizeof(screen_backlight_path), traits.screen_backlight_path);
	screen_backlight_max = (traits.screen_backlight_max > 0) ? traits.screen_backlight_max : 255;
	copyText(screen_blank_path, sizeof(screen_blank_path), traits.screen_blank_path);

	copyText(gpu_device, sizeof(gpu_device), traits.gpu_device);
	copyText(gpu_hdmi_state_path, sizeof(gpu_hdmi_state_path), traits.gpu_hdmi_state_path);
	gpu_hdmi_width = (traits.gpu_hdmi_width > 0) ? traits.gpu_hdmi_width : 1280;
	gpu_hdmi_height = (traits.gpu_hdmi_height > 0) ? traits.gpu_hdmi_height : 720;
	copyText(gpu_devfreq_path, sizeof(gpu_devfreq_path), traits.gpu_devfreq_path);
	gpu_clock_min = traits.gpu_clock_min;
	gpu_clock_max = traits.gpu_clock_max;

	copyText(cpu_governor_path, sizeof(cpu_governor_path), traits.cpu_governor_path);
	copyText(cpu_clock_path, sizeof(cpu_clock_path), traits.cpu_clock_path);
	cpu_clock_menu = traits.cpu_clock_menu;
	cpu_clock_powersave = traits.cpu_clock_powersave;
	cpu_clock_normal = traits.cpu_clock_normal;
	cpu_clock_performance = traits.cpu_clock_performance;
	cpu_undervolt_supported = traits.cpu_undervolt_supported;

	copyText(audio_card, sizeof(audio_card), traits.audio_card);
	copyText(audio_mixer, sizeof(audio_mixer), traits.audio_mixer);
	copyText(audio_jack_device_name, sizeof(audio_jack_device_name), traits.audio_jack_device_name);

	copyText(power_battery_sysfs, sizeof(power_battery_sysfs), traits.power_battery_sysfs);
	copyText(power_charger_online_path, sizeof(power_charger_online_path), traits.power_charger_online_path);
	copyText(power_led_path, sizeof(power_led_path), traits.power_led_path);

	copyText(wifi_interface, sizeof(wifi_interface), traits.wifi_interface);
	copyText(bluetooth_interface, sizeof(bluetooth_interface), traits.bluetooth_interface);

	for (int i = 0; i < BTN_ID_COUNT; i++) button_keycodes[i] = -1;
	button_keycodes[BTN_ID_DPAD_UP] = traits.key_up;
	button_keycodes[BTN_ID_DPAD_DOWN] = traits.key_down;
	button_keycodes[BTN_ID_DPAD_LEFT] = traits.key_left;
	button_keycodes[BTN_ID_DPAD_RIGHT] = traits.key_right;
	button_keycodes[BTN_ID_A] = traits.key_a;
	button_keycodes[BTN_ID_B] = traits.key_b;
	button_keycodes[BTN_ID_X] = traits.key_x;
	button_keycodes[BTN_ID_Y] = traits.key_y;
	button_keycodes[BTN_ID_C] = traits.key_c;
	button_keycodes[BTN_ID_Z] = traits.key_z;
	button_keycodes[BTN_ID_START] = traits.key_start;
	button_keycodes[BTN_ID_SELECT] = traits.key_select;
	button_keycodes[BTN_ID_MENU] = traits.key_menu;
	button_keycodes[BTN_ID_L1] = traits.key_l1;
	button_keycodes[BTN_ID_L2] = traits.key_l2;
	button_keycodes[BTN_ID_L3] = traits.key_l3;
	button_keycodes[BTN_ID_R1] = traits.key_r1;
	button_keycodes[BTN_ID_R2] = traits.key_r2;
	button_keycodes[BTN_ID_R3] = traits.key_r3;
	button_keycodes[BTN_ID_PLUS] = traits.key_vol_up;
	button_keycodes[BTN_ID_MINUS] = traits.key_vol_down;
	button_keycodes[BTN_ID_POWER] = traits.key_power;

	axis_lx = traits.axis_lx;
	axis_ly = traits.axis_ly;
	axis_rx = traits.axis_rx;
	axis_ry = traits.axis_ry;
	axis_min = traits.axis_min;
	axis_center = traits.axis_center;
	axis_max = traits.axis_max;
	axis_hat_x = traits.axis_hat_x;
	axis_hat_y = traits.axis_hat_y;
	axis_lx_invert = traits.axis_lx_invert;
	axis_ly_invert = traits.axis_ly_invert;
	axis_rx_invert = traits.axis_rx_invert;
	axis_ry_invert = traits.axis_ry_invert;

	copyText(input_gamepad, sizeof(input_gamepad), traits.input_gamepad);
	copyText(input_stick, sizeof(input_stick), traits.input_stick);
	copyText(input_power, sizeof(input_power), traits.input_power);
	copyText(input_volume, sizeof(input_volume), traits.input_volume);
	copyText(input_menu, sizeof(input_menu), traits.input_menu);
	copyText(input_rumble, sizeof(input_rumble), traits.input_rumble_device_name);
	copyText(input_lid, sizeof(input_lid), traits.input_lid);
}

///////////////////////////////
// Initialization & Loading

static void initTraitDefaults(void) {
	memset(&traits, 0, sizeof(traits));
	traits.key_c = traits.key_z = -1;
	traits.key_l1 = traits.key_r1 = -1;
	traits.key_l2 = traits.key_r2 = -1;
	traits.key_l3 = traits.key_r3 = -1;
	traits.axis_lx = traits.axis_ly = -1;
	traits.axis_rx = traits.axis_ry = -1;
	traits.axis_min = traits.axis_center = traits.axis_max = -1;
	traits.axis_hat_x = 16; // ABS_HAT0X
	traits.axis_hat_y = 17; // ABS_HAT0Y
	traits.gpu_hdmi_width = 1280;
	traits.gpu_hdmi_height = 720;
}

static int parseTraitsFile(const char* path) {
	FILE* file = fopen(path, "r");
	if (!file) {
		fprintf(stderr, "Missing Minime traits: %s\n", path);
		return -1;
	}

	char line[512];
	while (fgets(line, sizeof(line), file)) {
		char* key = trimWhitespace(line);
		if (!key[0] || key[0] == '#' || key[0] == '[')
			continue;
		char* value = strchr(key, '=');
		if (!value)
			continue;
		*value++ = '\0';
		if (setValue(trimWhitespace(key), trimWhitespace(value)) != 0) {
			fclose(file);
			return -1;
		}
	}
	fclose(file);
	return 0;
}

static void deriveFallbacks(void) {
	if (traits.ui_padding <= 0)
		traits.ui_padding = (traits.screen_width >= 720) ? 40 : 10;
	if (traits.ui_row_count <= 0)
		traits.ui_row_count = (traits.screen_width >= 720) ? 8 : 6;
}

int MINIME_traitsInit(void) {
	if (initialized)
		return valid ? 0 : -1;
	initialized = 1;
	initTraitDefaults();

	if (parseTraitsFile(TRAITS_PATH) != 0)
		return -1;

	deriveFallbacks();

	valid = (validate() == 0);
	if (valid) {
		exportResolvedTraits();
	}
	return valid ? 0 : -1;
}
