#ifndef __msettings_h__
#define __msettings_h__

#include <stddef.h>

void InitSettings(void);
void QuitSettings(void);

int GetBrightness(void);
int GetVolume(void);

void SetRawBrightness(int value); // 0-255
void SetRawVolume(int value);     // 0-100

void SetBrightness(int value); // 0-10
void SetVolume(int value);     // 0-20

int GetJack(void);
void SetJack(int value); // 0-1

int GetHDMI(void);
void SetHDMI(int value); // 0-1

int GetMute(void);
void SetMute(int value); // 0-1

///////////////////////////////
// traits-driven hardware HAL

int MINIME_audioJackConnected(void);
void MINIME_audioSetRawVolume(int value);

int MINIME_videoHDMIConnected(void);
void MINIME_videoSetBacklight(int value);
void MINIME_videoBlank(int blank);

int MINIME_inputOpenByName(const char *expected);
int MINIME_inputOpenShortcutDevices(int *fds, size_t max_fds);
int MINIME_inputHasCZ(void);
int MINIME_inputNormalizeAxis(int value, int invert);

int MINIME_powerGetBattery(int *charging, int *capacity);
void MINIME_powerSetLED(int enabled);
void MINIME_powerSetRumble(int enabled);
void MINIME_powerSetCPUSpeed(int speed);

#endif
