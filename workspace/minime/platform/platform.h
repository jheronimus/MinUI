#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////

#include <SDL2/SDL.h>

///////////////////////////////
// Display (Native panel & layout)

extern int screen_width;
extern int screen_height;
extern int screen_pitch;
extern int screen_padding;
extern int screen_row_count;

#define FIXED_SCALE 2
#define FIXED_BPP 2
#define FIXED_DEPTH (FIXED_BPP * 8)
#define FIXED_WIDTH screen_width
#define FIXED_HEIGHT screen_height
#define FIXED_PITCH screen_pitch
#define FIXED_SIZE (screen_pitch * screen_height)

#define MAIN_ROW_COUNT (screen_row_count + (on_hdmi ? 2 : 0))
#define PADDING (on_hdmi ? 40 : screen_padding)

int PLAT_getScreenRotation(void);
extern void (*plat_custom_flip)(SDL_Surface* surface);

///////////////////////////////
// HDMI Output

extern int plat_has_hdmi;
extern int on_hdmi;

#define HAS_HDMI plat_has_hdmi
#define HDMI_WIDTH 1280
#define HDMI_HEIGHT 720
#define HDMI_PITCH (HDMI_WIDTH * FIXED_BPP)
#define HDMI_SIZE (HDMI_PITCH * HDMI_HEIGHT)

// TODO: if HDMI_HEIGHT > FIXED_HEIGHT then MAIN_ROW_COUNT will be insufficient

///////////////////////////////
// Hardware Acceleration (GLES / KMS)

SDL_GLContext PLAT_initGLContext(int major, int minor, int gles);
void PLAT_quitGLContext(void);
void PLAT_swapGL(void);
void* PLAT_getGLProcAddress(const char* proc);

///////////////////////////////
// Gamepad & Button Mapping

int PLAT_is6Button(void);
int PLAT_hasMenuButton(void);
int PLAT_hasL3(void);
int PLAT_hasR3(void);
int PLAT_hasLeftStick(void);
int PLAT_hasRightStick(void);

#define BTN_RESUME BTN_X
#define BTN_SLEEP BTN_POWER
#define BTN_WAKE BTN_POWER
#define BTN_MOD_VOLUME BTN_NONE
#define BTN_MOD_BRIGHTNESS BTN_MENU
#define BTN_MOD_PLUS BTN_PLUS
#define BTN_MOD_MINUS BTN_MINUS

///////////////////////////////
// Hardware & Peripheral Capabilities

int PLAT_hasBluetooth(void);
int PLAT_hasWifi(void);
const char* PLAT_getWifiInterface(void);
int PLAT_hasUndervolt(void);
int PLAT_hasLid(void);

///////////////////////////////
// Platform Constants

#define SDCARD_PATH "/mnt/sdcard"
#define MUTE_VOLUME_RAW 0
#define SAMPLES 400 // fix for (most) fceumm underruns

///////////////////////////////

#endif
