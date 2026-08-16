// Generic Bluetooth backend for Minime (bluez/bluetoothctl).
// Tool dependencies: bluetoothctl (from bluez).
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

#define BT_TIMEOUT "timeout 3 "
#define BT_CMD_DEVICES BT_TIMEOUT "bluetoothctl devices 2>/dev/null"
#define BT_CMD_INFO BT_TIMEOUT "bluetoothctl info %s 2>/dev/null"
#define BT_CMD_POWER BT_TIMEOUT "bluetoothctl power %s >/dev/null 2>&1 &"
#define BT_CMD_SCAN_ON BT_TIMEOUT "bluetoothctl scan on >/dev/null 2>&1 &"
#define BT_CMD_PAIRABLE BT_TIMEOUT "bluetoothctl pairable on >/dev/null 2>&1 &"
#define BT_CMD_CONNECT BT_TIMEOUT "bluetoothctl connect %s >/dev/null 2>&1"
#define BT_CMD_DISCONNECT BT_TIMEOUT "bluetoothctl disconnect %s >/dev/null 2>&1"
#define BT_CMD_PAIR BT_TIMEOUT "bluetoothctl pair %s >/dev/null 2>&1"
#define BT_CMD_TRUST BT_TIMEOUT "bluetoothctl trust %s >/dev/null 2>&1"
#define BT_CMD_REMOVE BT_TIMEOUT "bluetoothctl remove %s >/dev/null 2>&1"



static void bt_restore_alsa(void) {
	// Firmware owns ALSA routing (see boards/common/scripts/audio.sh).
	(void)system(AUDIO_HELPER " bt-off >/dev/null 2>&1");
}

static void bt_route_audio_alsa(const char *addr) {
	char cmd[256];

	if (!addr || !addr[0])
		return;
	snprintf(cmd, sizeof(cmd), AUDIO_HELPER " bt-on %s >/dev/null 2>&1", addr);
	(void)system(cmd);
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
	char line[32];
	FILE *f;

	if (!is_bt_interface_present())
		return 0;
	f = cmdOutput(BT_TIMEOUT "pgrep bluetoothd 2>/dev/null");
	if (!f)
		return 0;
	if (!fgets(line, sizeof(line), f)) {
		pclose(f);
		return 0;
	}
	pclose(f);
	return 1;
}

static int parse_device_info(const char *addr, char *name_out, size_t name_max, int *paired_out, int *connected_out,
                            BtDeviceKind *kind_out) {
	char cmd[256];
	char line[256];
	FILE *f;
	BtDeviceKind kind = BT_DEVICE_UNKNOWN;
	int paired = 0;
	int connected = 0;
	char alias[BT_MAX_NAME] = "";
	char name[BT_MAX_NAME] = "";

	snprintf(cmd, sizeof(cmd), BT_CMD_INFO, addr);
	f = cmdOutput(cmd);
	if (!f)
		return 0;

	while (fgets(line, sizeof(line), f)) {
		char *val;

		if (strstr(line, "Paired: yes"))
			paired = 1;
		else if (strstr(line, "Connected: yes"))
			connected = 1;
		else if ((val = strstr(line, "Alias: "))) {
			val += 7;
			while (*val == ' ' || *val == '\t')
				val++;
			strncpy(alias, val, sizeof(alias) - 1);
			alias[sizeof(alias) - 1] = '\0';
			{
				size_t len = strlen(alias);
				while (len > 0 && (alias[len - 1] == '\n' || alias[len - 1] == '\r'))
					alias[--len] = '\0';
			}
		} else if ((val = strstr(line, "Name: "))) {
			val += 6;
			while (*val == ' ' || *val == '\t')
				val++;
			strncpy(name, val, sizeof(name) - 1);
			name[sizeof(name) - 1] = '\0';
			{
				size_t len = strlen(name);
				while (len > 0 && (name[len - 1] == '\n' || name[len - 1] == '\r'))
					name[--len] = '\0';
			}
		} else if ((val = strstr(line, "Icon: "))) {
			val += 6;
			if (strstr(val, "audio") || strstr(val, "headphone") || strstr(val, "headset"))
				kind = BT_DEVICE_AUDIO;
			else if (strstr(val, "game") || strstr(val, "input"))
				kind = BT_DEVICE_GAMEPAD;
		} else if ((val = strstr(line, "Class: ")) && kind == BT_DEVICE_UNKNOWN) {
			if (strstr(val, "Audio") || strstr(val, "Headphone") || strstr(val, "Headset"))
				kind = BT_DEVICE_AUDIO;
			else if (strstr(val, "Peripheral") || strstr(val, "Gamepad") || strstr(val, "Joystick"))
				kind = BT_DEVICE_GAMEPAD;
		}
	}
	pclose(f);

	if (kind == BT_DEVICE_UNKNOWN)
		return 0; // filter out non-audio/input devices

	*paired_out = paired;
	*connected_out = connected;
	*kind_out = kind;
	if (alias[0])
		strncpy(name_out, alias, name_max - 1);
	else if (name[0])
		strncpy(name_out, name, name_max - 1);
	else
		strncpy(name_out, addr, name_max - 1);
	name_out[name_max - 1] = '\0';
	return 1;
}

static void bt_refresh_devices(void) {
	char line[256];
	char cmd[256];
	FILE *f;
	BtDevice raw[BT_MAX_DEVICES];
	int raw_count = 0;
	int i;

	snprintf(cmd, sizeof(cmd), BT_CMD_DEVICES);
	f = cmdOutput(cmd);
	if (!f)
		return;

	while (fgets(line, sizeof(line), f) && raw_count < BT_MAX_DEVICES) {
		char *tok;
		char *addr;
		char name[BT_MAX_NAME] = "";
		int paired = 0;
		int connected = 0;
		BtDeviceKind kind = BT_DEVICE_UNKNOWN;

		tok = strtok(line, " \t");
		if (!tok || strcmp(tok, "Device") != 0)
			continue;
		addr = strtok(NULL, " \t\r\n");
		if (!addr)
			continue;

		if (!parse_device_info(addr, name, sizeof(name), &paired, &connected, &kind))
			continue;

		memset(&raw[raw_count], 0, sizeof(BtDevice));
		strncpy(raw[raw_count].addr, addr, sizeof(raw[raw_count].addr) - 1);
		strncpy(raw[raw_count].name, name, sizeof(raw[raw_count].name) - 1);
		raw[raw_count].paired = paired;
		raw[raw_count].connected = connected;
		raw[raw_count].kind = kind;
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

int BT_init(void) {
	bt_enabled = is_bt_service_up();
	device_count = 0;
	scanning = 0;
	return 0;
}

int BT_quit(void) {
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
	} else {
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
	scanning = 1;
	(void)system(BT_CMD_POWER " on");
	(void)system(BT_CMD_SCAN_ON);
	(void)system(BT_CMD_PAIRABLE);
	scanning = 0;
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
	char cmd[256];
	int i;
	BtDeviceKind kind = BT_DEVICE_UNKNOWN;

	if (!addr)
		return -1;

	for (i = 0; i < device_count; i++) {
		if (strcmp(devices[i].addr, addr) == 0) {
			kind = devices[i].kind;
			break;
		}
	}

	// if connected, disconnect
	{
		char info_cmd[256];
		char info_line[256];
		FILE *f;
		int is_connected = 0;

		snprintf(info_cmd, sizeof(info_cmd), BT_CMD_INFO, addr);
		f = cmdOutput(info_cmd);
		if (f) {
			while (fgets(info_line, sizeof(info_line), f)) {
				if (strstr(info_line, "Connected: yes")) {
					is_connected = 1;
					break;
				}
			}
			pclose(f);
		}
		if (is_connected) {
			snprintf(cmd, sizeof(cmd), BT_CMD_DISCONNECT, addr);
			(void)system(cmd);
			if (kind == BT_DEVICE_AUDIO)
				bt_restore_alsa();
			return 0;
		}
	}

	// pair, trust, connect
	snprintf(cmd, sizeof(cmd), BT_CMD_PAIR, addr);
	(void)system(cmd);
	snprintf(cmd, sizeof(cmd), BT_CMD_TRUST, addr);
	(void)system(cmd);
	snprintf(cmd, sizeof(cmd), BT_CMD_CONNECT, addr);
	(void)system(cmd);

	if (kind == BT_DEVICE_AUDIO)
		bt_route_audio_alsa(addr);

	return 0;
}

int BT_forgetDevice(const char *addr) {
	char cmd[256];

	if (!addr)
		return -1;
	snprintf(cmd, sizeof(cmd), BT_CMD_DISCONNECT, addr);
	(void)system(cmd);
	snprintf(cmd, sizeof(cmd), BT_CMD_REMOVE, addr);
	(void)system(cmd);
	bt_restore_alsa();
	return 0;
}

int BT_isBusy(void) {
	return scanning;
}
