#ifndef REWIND_H
#define REWIND_H

#include "libretro.h"

// rewind.c entry points (called from minarch.c)
void Rewind_init(void);
void Rewind_quit(void);
void Rewind_applyConfig(void);
void Rewind_afterFrame(void);
int Rewind_processFrame(void);
void Rewind_onStateChange(void);
enum retro_savestate_context Rewind_getSavestateContext(void);
int Rewind_audioEnabled(void);

// rewind flags (read/written by minarch.c input handling)
int setRewindToggle(int enable);
int setRewindPressed(int enable);

#endif
