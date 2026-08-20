// MinUI Bluetooth backend for Minime (bluez/D-Bus).
// Tool dependencies: dbus-send / bluetoothctl (from bluez).
// Service: /etc/init.d/bluetooth, gated by
// /mnt/sdcard/.minime/config/bluetooth/enabled (see boards/common).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "traits.h"
#include "wireless.h"
#include "utils.h"

#define BT_ENABLE_FILE "/mnt/sdcard/.minime/config/bluetooth/enabled"
#define BT_SERVICE "/etc/init.d/bluetooth"
#define AUDIO_HELPER "/usr/share/minime/scripts/audio.sh"

static int bt_enabled = 0;
static BtDevice devices[BT_MAX_DEVICES];
static int device_count = 0;
static int scanning = 0;

#define BT_CMD_DBUS_OBJECTS "dbus-send --system --dest=org.bluez --print-reply / org.freedesktop.DBus.ObjectManager.GetManagedObjects 2>/dev/null"

static void bt_restore_alsa(void) {
	(void)system(AUDIO_HELPER " bt-off >/dev/null 2>&1 &");
}

static int is_bt_interface_present(void) {
	const MinimeTraits *traits = MINIME_traits();
	char path[256];

	if (!traits || !traits->bluetooth_interface[0] || strcmp(traits->bluetooth_interface, "na") == 0)
		return 0;
	snprintf(path, sizeof(path), "/sys/class/bluetooth/%s", traits->bluetooth_interface);
	return access(path, F_OK) == 0;
}

static int is_bt_service_up(void) {
	if (!is_bt_interface_present())
		return 0;
	return access("/run/bluetoothd.pid", F_OK) == 0;
}

static int is_mac_string(const char *s) {
	int i;
	if (!s || strlen(s) != 17)
		return 0;
	for (i = 0; i < 17; i++) {
		if (i == 2 || i == 5 || i == 8 || i == 11 || i == 14) {
			if (s[i] != ':' && s[i] != '-')
				return 0;
		} else {
			char c = s[i];
			if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
				return 0;
		}
	}
	return 1;
}

static const char *resolve_device_name(const char *name, const char *alias) {
	if (name && name[0] && !is_mac_string(name))
		return name;
	if (alias && alias[0] && !is_mac_string(alias))
		return alias;
	return NULL;
}

// Single-pass device parser using D-Bus ObjectManager output (zero per-device subprocesses).
static void bt_refresh_devices(void) {
	char line[256];
	FILE *f;
	BtDevice raw[BT_MAX_DEVICES];
	int raw_count = 0;
	int i;

	char current_addr[BT_MAX_ADDR] = "";
	char current_name[BT_MAX_NAME] = "";
	char current_alias[BT_MAX_NAME] = "";
	int current_paired = 0;
	int current_connected = 0;
	BtDeviceKind current_kind = BT_DEVICE_UNKNOWN;
	int in_device = 0;

	f = cmdOutput(BT_CMD_DBUS_OBJECTS);
	if (!f)
		return;

	while (fgets(line, sizeof(line), f)) {
		char *p;

		if (strstr(line, "object path \"/org/bluez/") && strstr(line, "/dev_")) {
			// Save previous device
			const char *resolved_name = resolve_device_name(current_name, current_alias);
			if (in_device && current_addr[0] && resolved_name && raw_count < BT_MAX_DEVICES) {
				memset(&raw[raw_count], 0, sizeof(BtDevice));
				strncpy(raw[raw_count].addr, current_addr, sizeof(raw[raw_count].addr) - 1);
				strncpy(raw[raw_count].name, resolved_name, sizeof(raw[raw_count].name) - 1);
				raw[raw_count].paired = current_paired;
				raw[raw_count].connected = current_connected;
				raw[raw_count].kind = (current_kind != BT_DEVICE_UNKNOWN) ? current_kind : BT_DEVICE_AUDIO;
				raw_count++;
			}

			// Start new device
			in_device = 1;
			current_addr[0] = '\0';
			current_name[0] = '\0';
			current_alias[0] = '\0';
			current_paired = 0;
			current_connected = 0;
			current_kind = BT_DEVICE_UNKNOWN;
			continue;
		}

		if (!in_device)
			continue;

		if ((p = strstr(line, "string \"Address\""))) {
			if (fgets(line, sizeof(line), f)) {
				char *val = strstr(line, "string \"");
				if (val) {
					val += 8;
					char *end = strchr(val, '"');
					if (end)
						*end = '\0';
					strncpy(current_addr, val, sizeof(current_addr) - 1);
				}
			}
		} else if ((p = strstr(line, "string \"Name\""))) {
			if (fgets(line, sizeof(line), f)) {
				char *val = strstr(line, "string \"");
				if (val) {
					val += 8;
					char *end = strchr(val, '"');
					if (end)
						*end = '\0';
					strncpy(current_name, val, sizeof(current_name) - 1);
				}
			}
		} else if ((p = strstr(line, "string \"Alias\""))) {
			if (fgets(line, sizeof(line), f)) {
				char *val = strstr(line, "string \"");
				if (val) {
					val += 8;
					char *end = strchr(val, '"');
					if (end)
						*end = '\0';
					strncpy(current_alias, val, sizeof(current_alias) - 1);
				}
			}
		} else if ((p = strstr(line, "string \"Icon\""))) {
			if (fgets(line, sizeof(line), f)) {
				if (strstr(line, "audio") || strstr(line, "headphone") || strstr(line, "headset"))
					current_kind = BT_DEVICE_AUDIO;
				else if (strstr(line, "game") || strstr(line, "input"))
					current_kind = BT_DEVICE_GAMEPAD;
			}
		} else if ((p = strstr(line, "string \"Class\""))) {
			if (fgets(line, sizeof(line), f) && current_kind == BT_DEVICE_UNKNOWN) {
				if (strstr(line, "Audio") || strstr(line, "Headphone") || strstr(line, "Headset"))
					current_kind = BT_DEVICE_AUDIO;
				else if (strstr(line, "Peripheral") || strstr(line, "Gamepad") || strstr(line, "Joystick"))
					current_kind = BT_DEVICE_GAMEPAD;
			}
		} else if ((p = strstr(line, "string \"Paired\""))) {
			if (fgets(line, sizeof(line), f)) {
				if (strstr(line, "boolean true"))
					current_paired = 1;
			}
		} else if ((p = strstr(line, "string \"Connected\""))) {
			if (fgets(line, sizeof(line), f)) {
				if (strstr(line, "boolean true"))
					current_connected = 1;
			}
		}
	}

	// Flush trailing device
	const char *resolved_name = resolve_device_name(current_name, current_alias);
	if (in_device && current_addr[0] && resolved_name && raw_count < BT_MAX_DEVICES) {
		memset(&raw[raw_count], 0, sizeof(BtDevice));
		strncpy(raw[raw_count].addr, current_addr, sizeof(raw[raw_count].addr) - 1);
		strncpy(raw[raw_count].name, resolved_name, sizeof(raw[raw_count].name) - 1);
		raw[raw_count].paired = current_paired;
		raw[raw_count].connected = current_connected;
		raw[raw_count].kind = (current_kind != BT_DEVICE_UNKNOWN) ? current_kind : BT_DEVICE_AUDIO;
		raw_count++;
	}

	pclose(f);

	// Sort: connected first, then paired, then discovered
	device_count = 0;
	for (i = 0; i < raw_count && device_count < BT_MAX_DEVICES; i++) {
		if (raw[i].connected)
			devices[device_count++] = raw[i];
	}
	for (i = 0; i < raw_count && device_count < BT_MAX_DEVICES; i++) {
		if (raw[i].paired && !raw[i].connected)
			devices[device_count++] = raw[i];
	}
	for (i = 0; i < raw_count && device_count < BT_MAX_DEVICES; i++) {
		if (!raw[i].paired)
			devices[device_count++] = raw[i];
	}
}

///////////////////////////////////////

static FILE* scan_pipe = NULL;

static void bt_start_scan_session(void) {
	if (!scan_pipe) {
		scan_pipe = popen("bluetoothctl >/dev/null 2>&1", "w");
		if (scan_pipe) {
			fputs("scan on\n", scan_pipe);
			fflush(scan_pipe);
		}
	}
}

static void bt_stop_scan_session(void) {
	if (scan_pipe) {
		fputs("scan off\nquit\n", scan_pipe);
		fflush(scan_pipe);
		pclose(scan_pipe);
		scan_pipe = NULL;
	}
}

int BT_init(void) {
	bt_enabled = is_bt_service_up();
	device_count = 0;
	scanning = 0;
	if (bt_enabled) {
		(void)system("bluetoothctl power on >/dev/null 2>&1");
		(void)system("bluetoothctl pairable on >/dev/null 2>&1");
		(void)system("bluetoothctl system-alias minime >/dev/null 2>&1");
		bt_start_scan_session();
	}
	return 0;
}

int BT_quit(void) {
	if (bt_enabled) {
		bt_stop_scan_session();
	}
	device_count = 0;
	return 0;
}

int BT_enabled(void) {
	return is_bt_service_up();
}

int BT_setEnabled(int enabled) {
	FILE *f;
	char cmd[256];

	if (enabled) {
		snprintf(cmd, sizeof(cmd), "mkdir -p /mnt/sdcard/.minime/config/bluetooth");
		(void)system(cmd);
		f = fopen(BT_ENABLE_FILE, "w");
		if (f) {
			fputs("1\n", f);
			fclose(f);
		}
		snprintf(cmd, sizeof(cmd), BT_SERVICE " restart >/dev/null 2>&1 &");
		(void)system(cmd);
		bt_enabled = 1;
		(void)system("bluetoothctl power on >/dev/null 2>&1");
		(void)system("bluetoothctl pairable on >/dev/null 2>&1");
		(void)system("bluetoothctl system-alias minime >/dev/null 2>&1");
		bt_start_scan_session();
	} else {
		bt_stop_scan_session();
		unlink(BT_ENABLE_FILE);
		snprintf(cmd, sizeof(cmd), BT_SERVICE " stop >/dev/null 2>&1 &");
		(void)system(cmd);
		bt_restore_alsa();
		bt_enabled = 0;
	}
	return 0;
}

int BT_scan(void) {
	if (!BT_enabled())
		return -1;
	bt_start_scan_session();
	return 0;
}

int BT_getDevices(BtDevice *out, int max) {
	int i;

	if (!out)
		return 0;
	bt_refresh_devices();
	for (i = 0; i < device_count && i < max; i++)
		out[i] = devices[i];
	return device_count < max ? device_count : max;
}

int BT_toggleDevice(const char *addr) {
	char cmd[512];
	int i;
	BtDeviceKind kind = BT_DEVICE_AUDIO;
	int is_connected = 0;
	int is_paired = 0;

	if (!addr || !addr[0])
		return -1;

	for (i = 0; i < device_count; i++) {
		if (strcmp(devices[i].addr, addr) == 0) {
			kind = devices[i].kind;
			is_connected = devices[i].connected;
			is_paired = devices[i].paired;
			break;
		}
	}

	if (is_connected) {
		snprintf(cmd, sizeof(cmd), "bluetoothctl disconnect %s >/dev/null 2>&1 &", addr);
		(void)system(cmd);
		if (kind == BT_DEVICE_AUDIO)
			bt_restore_alsa();
		return 0;
	}

	if (is_paired) {
		if (kind == BT_DEVICE_AUDIO) {
			snprintf(cmd, sizeof(cmd), "bluetoothctl connect %s && " AUDIO_HELPER " bt-on %s >/dev/null 2>&1 &", addr, addr);
		} else {
			snprintf(cmd, sizeof(cmd), "bluetoothctl connect %s >/dev/null 2>&1 &", addr);
		}
		(void)system(cmd);
		return 0;
	}

	// New unpaired device: pair -> trust -> connect -> route audio
	if (kind == BT_DEVICE_AUDIO) {
		snprintf(cmd, sizeof(cmd), "bluetoothctl pair %s && bluetoothctl trust %s && bluetoothctl connect %s && " AUDIO_HELPER " bt-on %s >/dev/null 2>&1 &", addr, addr, addr, addr);
	} else {
		snprintf(cmd, sizeof(cmd), "bluetoothctl pair %s && bluetoothctl trust %s && bluetoothctl connect %s >/dev/null 2>&1 &", addr, addr, addr);
	}
	(void)system(cmd);
	return 0;
}

int BT_forgetDevice(const char *addr) {
	char cmd[256];

	if (!addr || !addr[0])
		return -1;
	snprintf(cmd, sizeof(cmd), "bluetoothctl disconnect %s; bluetoothctl remove %s >/dev/null 2>&1 &", addr, addr);
	(void)system(cmd);
	bt_restore_alsa();
	return 0;
}

int BT_isBusy(void) {
	return scanning;
}
