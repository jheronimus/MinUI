#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////

#include "sdl.h"

///////////////////////////////
// Display (Native panel & layout)

extern int plat_fixed_width;
extern int plat_fixed_height;
extern int plat_screen_rotation;
extern int plat_main_row_count;
extern int plat_padding;

#define FIXED_SCALE 2
#define FIXED_WIDTH plat_fixed_width
#define FIXED_HEIGHT plat_fixed_height
#define FIXED_BPP 2
#define FIXED_DEPTH (FIXED_BPP * 8)
#define FIXED_PITCH (FIXED_WIDTH * FIXED_BPP)
#define FIXED_SIZE (FIXED_PITCH * FIXED_HEIGHT)

#define MAIN_ROW_COUNT (plat_main_row_count + (on_hdmi ? 2 : 0))
#define PADDING (on_hdmi ? 40 : plat_padding)

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
void PLAT_setGLSwapInterval(int interval);

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
// Platform Constants

#define SDCARD_PATH "/mnt/sdcard"
#define MUTE_VOLUME_RAW 0
#define SAMPLES 400 // fix for (most) fceumm underruns

///////////////////////////////

#endif
