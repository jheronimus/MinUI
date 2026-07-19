#ifndef PLATFORM_H
#define PLATFORM_H

#include "sdl.h"

/*
 * Minime platform header.
 *
 * Initial version: compile-time constants matching RG35XX SP v1 (640x480).
 * Subsequent feature imports will replace these with runtime traits reads.
 * See IMPORT branch src/platform/minime/traits.h for the target state.
 */

// Screen — will be populated from traits at runtime
extern int plat_screen_width;
extern int plat_screen_height;
extern int plat_screen_rotation;

#define FIXED_WIDTH   plat_screen_width
#define FIXED_HEIGHT  plat_screen_height
#define FIXED_SCALE   2
#define FIXED_BPP     2
#define FIXED_DEPTH   (FIXED_BPP * 8)
#define FIXED_PITCH   (FIXED_WIDTH * FIXED_BPP)
#define FIXED_SIZE    (FIXED_PITCH * FIXED_HEIGHT)

#define HDMI_WIDTH    1280
#define HDMI_HEIGHT   720
#define HAS_HDMI      1

// UI layout
#define MAIN_ROW_COUNT  8
#define PADDING         40

// Paths
#define SDCARD_PATH     "/mnt/sdcard"
#define MUTE_VOLUME_RAW 0

// Buttons — will be populated from traits keycodes at runtime
// Placeholder values for RG35XX SP v1
#define BUTTON_POWER    116
#define BUTTON_UP       103
#define BUTTON_DOWN     108
#define BUTTON_LEFT     105
#define BUTTON_RIGHT    106
#define BUTTON_A        304
#define BUTTON_B        305
#define BUTTON_X        307
#define BUTTON_Y        306
#define BUTTON_L1       310
#define BUTTON_R1       311
#define BUTTON_L2       312
#define BUTTON_R2       313
#define BUTTON_L3       314
#define BUTTON_R3       315
#define BUTTON_SELECT   316
#define BUTTON_START    317
#define BUTTON_MENU     318
#define BUTTON_PLUS     BUTTON_NA
#define BUTTON_MINUS    BUTTON_NA

// Evdev keycodes — will be populated from traits at runtime
#define CODE_POWER      116
#define CODE_PLUS       BUTTON_NA
#define CODE_MINUS      BUTTON_NA

// Joystick indices — will be populated from traits at runtime
#define JOY_A           0
#define JOY_B           1
#define JOY_X           2
#define JOY_Y           3
#define JOY_L1          4
#define JOY_R1          5
#define JOY_L3          6
#define JOY_R3          7
#define JOY_SELECT      8
#define JOY_START       9
#define JOY_MENU        10
#define JOY_POWER       CODE_POWER
#define JOY_PLUS        JOY_NA
#define JOY_MINUS       JOY_NA

// Axes — will be populated from traits at runtime
#define AXIS_LX         0
#define AXIS_LY         1
#define AXIS_RX         2
#define AXIS_RY         3
#define AXIS_L2         4
#define AXIS_R2         5

// Modifier combos
#define BTN_RESUME          BTN_X
#define BTN_SLEEP           BTN_POWER
#define BTN_WAKE            BTN_POWER
#define BTN_MOD_VOLUME      BTN_NONE
#define BTN_MOD_BRIGHTNESS  BTN_MENU
#define BTN_MOD_PLUS        BTN_PLUS
#define BTN_MOD_MINUS       BTN_MINUS

#endif
