#ifndef SETTINGS_H
#define SETTINGS_H

// Shared volume/brightness setting scale (minui units, NOT raw hardware
// values). Single source of truth for the UI tools (defines.h), the keymon
// daemon, and libmsettings (which maps the scale to raw hardware values).
#define VOLUME_MIN 		0
#define VOLUME_MAX 		20
#define BRIGHTNESS_MIN 	0
#define BRIGHTNESS_MAX 	10

#endif
