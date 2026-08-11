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

#define BT_ENABLE_FILE "/mnt/sdcard/.minime/config/bluetooth/enabled"
#define BT_SERVICE "/etc/init.d/bluetooth"

#define BT_CMD_DEVICES "bluetoothctl devices 2>/dev/null"
#define BT_CMD_INFO "bluetoothctl info %s 2>/dev/null"
#define BT_CMD_POWER "bluetoothctl power %s >/dev/null 2>&1"
#define BT_CMD_SCAN "bluetoothctl scan %s >/dev/null 2>&1"
#define BT_CMD_DISCOVERABLE "bluetoothctl discoverable %s >/dev/null 2>&1"
#define BT_CMD_PAIRABLE "bluetoothctl pairable %s >/dev/null 2>&1"
#define BT_CMD_CONNECT "bluetoothctl connect %s >/dev/null 2>&1"
#define BT_CMD_DISCONNECT "bluetoothctl disconnect %s >/dev/null 2>&1"
#define BT_CMD_TRUST "bluetoothctl trust %s >/dev/null 2>&1"
#define BT_CMD_REMOVE "bluetoothctl remove %s >/dev/null 2>&1"

static int bt_enabled = 0;
static BtDevice devices[BT_MAX_DEVICES];
static int device_count = 0;
static int busy = 0;

static FILE *bt_popen(const char *cmd) {
	return popen(cmd, "r");
}

static void trim(char *s) {
	size_t len = strlen(s);

	while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
		s[len - 1] = '\0';
		len--;
	}
}

static int is_bt_interface_present(void) {
	const MinimeTraits *traits = MINIME_traits();
	char path[256];

	if (!traits || !traits->bluetooth_interface[0] || strcmp(traits->bluetooth_interface, "na") == 0)
		return 0;
	snprintf(path, sizeof(path), "/sys/class/bluetooth/%s", traits->bluetooth_interface);
	return access(path, F_OK) == 0;
}

int BT_hasBluetooth(void) {
	const MinimeTraits *traits = MINIME_traits();

	return traits && traits->bluetooth_interface[0] && strcmp(traits->bluetooth_interface, "na") != 0;
}

static BtDeviceKind bt_device_kind(const char *addr) {
	char cmd[256];
	char line[256];
	FILE *f;
	BtDeviceKind kind = BT_DEVICE_UNKNOWN;

	snprintf(cmd, sizeof(cmd), BT_CMD_INFO, addr);
	f = bt_popen(cmd);
	if (!f)
		return BT_DEVICE_UNKNOWN;

	while (fgets(line, sizeof(line), f)) {
		if (strstr(line, "Icon:")) {
			char *value = strchr(line, ':');
			if (value) {
				value++;
				while (*value == ' ' || *value == '\t')
					value++;
				if (strstr(value, "audio"))
					kind = BT_DEVICE_AUDIO;
				else if (strstr(value, "game") || strstr(value, "input"))
					kind = BT_DEVICE_GAMEPAD;
			}
		} else if (strstr(line, "Class:") && kind == BT_DEVICE_UNKNOWN) {
			// Class: 0x000524 (Major: Peripheral, Minor: ...)
			char *value = strchr(line, ':');
			if (value && strstr(value, "Peripheral"))
				kind = BT_DEVICE_GAMEPAD;
			else if (value && strstr(value, "Audio"))
				kind = BT_DEVICE_AUDIO;
		}
	}
	pclose(f);
	return kind;
}

static int bt_device_exists(const char *addr) {
	int i;

	for (i = 0; i < device_count; i++) {
		if (strcmp(devices[i].addr, addr) == 0)
			return 1;
	}
	return 0;
}

// list devices from `bluetoothctl devices` output; fill paired/connected via info
static void bt_list_devices(int want_paired, int want_connected) {
	char line[256];
	char cmd[256];
	FILE *f;

	snprintf(cmd, sizeof(cmd), BT_CMD_DEVICES);
	f = bt_popen(cmd);
	if (!f)
		return;

	while (fgets(line, sizeof(line), f) && device_count < BT_MAX_DEVICES) {
		BtDevice *dev;
		char *tok;
		char *addr;
		char *name;
		int paired = 0;
		int connected = 0;
		char info_cmd[256];
		char info_line[256];
		FILE *info_f;

		tok = strtok(line, " \t");
		if (!tok || strcmp(tok, "Device") != 0)
			continue;
		addr = strtok(NULL, " \t");
		if (!addr)
			continue;
		name = strtok(NULL, "\r\n");
		if (!name)
			name = addr;

		if (bt_device_exists(addr))
			continue;

		snprintf(info_cmd, sizeof(info_cmd), BT_CMD_INFO, addr);
		info_f = bt_popen(info_cmd);
		if (info_f) {
			while (fgets(info_line, sizeof(info_line), info_f)) {
				if (strstr(info_line, "Paired: yes"))
					paired = 1;
				else if (strstr(info_line, "Connected: yes"))
					connected = 1;
			}
			pclose(info_f);
		}

		if (paired != want_paired)
			continue;
		if (want_paired && connected != want_connected)
			continue;

		dev = &devices[device_count];
		memset(dev, 0, sizeof(*dev));
		strncpy(dev->addr, addr, sizeof(dev->addr));
		{
			char *p = name;
			while (*p == ' ' || *p == '\t')
				p++;
			if (p[0] == '-')
				strncpy(dev->name, dev->addr, sizeof(dev->name));
			else
				strncpy(dev->name, p, sizeof(dev->name));
		}
		dev->paired = paired;
		dev->connected = connected;
		dev->kind = bt_device_kind(dev->addr);
		if (dev->kind == BT_DEVICE_UNKNOWN)
			continue;
		device_count++;
	}
	pclose(f);
}

static void bt_refresh_devices(void) {
	device_count = 0;

	// connected paired devices
	bt_list_devices(1, 1);
	// paired but not connected
	bt_list_devices(1, 0);
	// discovered but not paired
	bt_list_devices(0, 0);
}

///////////////////////////////////////

int BT_init(void) {
	bt_enabled = is_bt_interface_present();
	device_count = 0;
	busy = 0;
	return 0;
}

int BT_quit(void) {
	device_count = 0;
	return 0;
}

int BT_enabled(void) {
	return bt_enabled;
}

int BT_setEnabled(int enabled) {
	int rc;

	if (enabled) {
		FILE *f;
		char cmd[256];

		snprintf(cmd, sizeof(cmd), "mkdir -p /mnt/sdcard/.minime/config/bluetooth");
		(void)system(cmd);
		f = fopen(BT_ENABLE_FILE, "w");
		if (f) {
			fputs("1\n", f);
			fclose(f);
		}
		rc = system(BT_SERVICE " start");
		if (rc != 0 && !is_bt_interface_present()) {
			(void)system(BT_SERVICE " stop");
			bt_enabled = 0;
			return -1;
		}
		bt_enabled = 1;
	} else {
		unlink(BT_ENABLE_FILE);
		rc = system(BT_SERVICE " stop");
		if (rc != 0)
			return -1;
		bt_enabled = 0;
	}
	return 0;
}

int BT_scan(void) {
	if (!bt_enabled)
		return -1;
	busy = 1;
	(void)system("bluetoothctl power on >/dev/null 2>&1");
	(void)system("bluetoothctl scan on >/dev/null 2>&1");
	(void)system("bluetoothctl discoverable on >/dev/null 2>&1");
	(void)system("bluetoothctl pairable on >/dev/null 2>&1");
	// wait a moment for discovery to populate
	sleep(2);
	(void)system("bluetoothctl scan off >/dev/null 2>&1");
	busy = 0;
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

	if (!addr)
		return -1;

	// if connected, disconnect
	{
		char info_cmd[256];
		char info_line[256];
		FILE *f;
		int is_connected = 0;

		snprintf(info_cmd, sizeof(info_cmd), BT_CMD_INFO, addr);
		f = bt_popen(info_cmd);
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
			return 0;
		}
	}

	// pair if not paired, then connect
	snprintf(cmd, sizeof(cmd), BT_CMD_POWER, "on");
	(void)system(cmd);
	snprintf(cmd, sizeof(cmd), BT_CMD_PAIRABLE, "on");
	(void)system(cmd);
	snprintf(cmd, sizeof(cmd), BT_CMD_CONNECT, addr);
	(void)system(cmd);
	busy = 0;
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
	return 0;
}

int BT_isBusy(void) {
	return busy;
}
