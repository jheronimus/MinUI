#ifndef MINIME_TRAITS_H
#define MINIME_TRAITS_H

#include <stddef.h>

#define MINIME_TRAIT_PATH_MAX 256
#define MINIME_TRAIT_NAME_MAX 64

typedef enum {
    MINIME_ASPECT_4x3,
    MINIME_ASPECT_3x2,
    MINIME_ASPECT_16x9,
    MINIME_ASPECT_1x1,
    MINIME_ASPECT_UNKNOWN,
} MinimeScreenAspect;

typedef struct MinimeTraits {
    // [device]
    char device_id[MINIME_TRAIT_NAME_MAX];
    char device_model[MINIME_TRAIT_PATH_MAX];

    // [screen]
    int screen_width;
    int screen_height;
    int screen_rotation;
    int screen_rotation_kernel;
    MinimeScreenAspect screen_aspect;
    int screen_refresh_rate;
    char screen_backlight_path[MINIME_TRAIT_PATH_MAX];
    int screen_backlight_max;
    char screen_blank_path[MINIME_TRAIT_PATH_MAX];
    int screen2_width;
    int screen2_height;
    int screen2_rotation;
    int screen2_refresh_rate;
    MinimeScreenAspect screen2_aspect;
    char screen2_backlight_path[MINIME_TRAIT_PATH_MAX];
    char screen2_blank_path[MINIME_TRAIT_PATH_MAX];
    int screen2_touch;
    char screen2_touch_device_name[MINIME_TRAIT_NAME_MAX];

    // [cpu]
    char cpu_governor_path[MINIME_TRAIT_PATH_MAX];
    char cpu_clock_path[MINIME_TRAIT_PATH_MAX];
    int cpu_clock_menu;
    int cpu_clock_powersave;
    int cpu_clock_normal;
    int cpu_clock_performance;
    int cpu_undervolt_supported;
    char cpu_thermal_path[MINIME_TRAIT_PATH_MAX];

    // [gpu]
    char gpu_device[MINIME_TRAIT_PATH_MAX];
    char gpu_device2[MINIME_TRAIT_PATH_MAX];
    // Stable connector identifier from the file, e.g. "HDMI-A-1" ("na" if
    // the device has no HDMI). The card number prefix is NOT part of it.
    char gpu_hdmi_connector[MINIME_TRAIT_NAME_MAX];
    // Resolved at init by traits.c: the actual DRM sysfs status path, e.g.
    // "/sys/class/drm/card0-HDMI-A-1/status". Empty when no connector exists.
    char gpu_hdmi_state_path[MINIME_TRAIT_PATH_MAX];
    char gpu_driver[MINIME_TRAIT_NAME_MAX];
    int gpu_clock_min;
    int gpu_clock_max;

    // [audio]
    char audio_card[MINIME_TRAIT_NAME_MAX];
    char audio_mixer[MINIME_TRAIT_NAME_MAX];
    char audio_jack_device_name[MINIME_TRAIT_NAME_MAX];
    int audio_mic;

    // [input]
    char input_gamepad[MINIME_TRAIT_NAME_MAX];
    char input_stick[MINIME_TRAIT_NAME_MAX];
    char input_power[MINIME_TRAIT_NAME_MAX];
    char input_volume[MINIME_TRAIT_NAME_MAX];
    char input_menu[MINIME_TRAIT_NAME_MAX];
    char input_lid[MINIME_TRAIT_NAME_MAX];
    // evdev device name of the rumble motor, exposed as an input device with
    // FF_RUMBLE force feedback (e.g. "pwm-vibrator"). "na" when no rumble.
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

    // [wireless]
    char wifi_interface[MINIME_TRAIT_NAME_MAX];
    char bluetooth_interface[MINIME_TRAIT_NAME_MAX];

    // [power]
    char power_battery_sysfs[MINIME_TRAIT_PATH_MAX];
    char power_charger_online_path[MINIME_TRAIT_PATH_MAX];
    char power_led_path[MINIME_TRAIT_PATH_MAX];

    // [usb]
    int usb_otg;
    int usb_host_ports;
    int usb_device_mode;
    int usb_controller_mode;

    // [storage]
    char storage_sd_node[MINIME_TRAIT_PATH_MAX];
    char storage_sd2_node[MINIME_TRAIT_PATH_MAX];
    char storage_emmc_node[MINIME_TRAIT_PATH_MAX];
} MinimeTraits;

int MINIME_traitsInit(void);
const MinimeTraits *MINIME_traits(void);
int MINIME_traitAvailable(const char *value);

///////////////////////////////
// traits-driven hardware HAL

int MINIME_audioJackConnected(void);
void MINIME_audioSetRawVolume(int value);

int MINIME_videoHDMIConnected(void);
void MINIME_videoSetBacklight(int value);
void MINIME_videoBlank(int blank);

int MINIME_inputOpenByName(const char *expected);
int MINIME_inputOpenByNameOrPath(const char *name_or_path);
int MINIME_inputOpenShortcutDevices(int *fds, size_t max_fds);
int MINIME_inputNormalizeAxis(int value, int invert);

int MINIME_powerGetBattery(int *charging, int *capacity);
void MINIME_powerSetLED(int enabled);
void MINIME_powerSetRumble(int enabled);
void MINIME_powerSetCPUSpeed(int speed);

#endif
