// Bluetooth settings PAK for Minime (bluetoothctl backend).
// Toggle, scan, pair/connect/disconnect devices, forget paired devices.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <msettings.h>

#include "defines.h"
#include "api.h"
#include "menu.h"
#include "utils.h"
#include "wireless.h"

///////////////////////////////////////

static MenuList menu = {0};
static BtDevice devices[BT_MAX_DEVICES];
static int device_count = 0;

static int is_toggle_row(int i) {
	return i == 0;
}

static void set_badge(MenuItem* item, const BtDevice* dev) {
	if (dev->connected)
		strcpy(item->badge, "connected");
	else if (dev->paired)
		strcpy(item->badge, "paired");
	else
		item->badge[0] = '\0';
}

static void rebuild(void) {
	MenuItem* items;
	int i;
	int count = 0;

	items = calloc(1 + BT_MAX_DEVICES + 1, sizeof(MenuItem));

	items[count].name = BT_enabled() ? "Disable Bluetooth" : "Enable Bluetooth";
	items[count].confirm_label = "TOGGLE";
	if (BT_isBusy()) {
		items[count].name = BT_enabled() ? "Disabling Bluetooth" : "Enabling Bluetooth";
	}
	count++;

	if (BT_enabled()) {
		// connected first, then paired, then new
		for (i = 0; i < device_count; i++) {
			if (devices[i].connected) {
				MenuItem* item = &items[count++];
				item->name = devices[i].name[0] ? devices[i].name : devices[i].addr;
				item->confirm_label = "DISCONNECT";
				item->aux_label = "UNPAIR";
				item->icon = (devices[i].kind == BT_DEVICE_GAMEPAD) ? ASSET_GAMEPAD : ASSET_HEADPHONES;
				set_badge(item, &devices[i]);
			}
		}
		for (i = 0; i < device_count; i++) {
			if (devices[i].paired && !devices[i].connected) {
				MenuItem* item = &items[count++];
				item->name = devices[i].name[0] ? devices[i].name : devices[i].addr;
				item->confirm_label = "CONNECT";
				item->aux_label = "UNPAIR";
				item->icon = (devices[i].kind == BT_DEVICE_GAMEPAD) ? ASSET_GAMEPAD : ASSET_HEADPHONES;
				set_badge(item, &devices[i]);
			}
		}
		for (i = 0; i < device_count; i++) {
			if (!devices[i].paired) {
				MenuItem* item = &items[count++];
				item->name = devices[i].name[0] ? devices[i].name : devices[i].addr;
				item->confirm_label = "CONNECT";
				item->icon = (devices[i].kind == BT_DEVICE_GAMEPAD) ? ASSET_GAMEPAD : ASSET_HEADPHONES;
				set_badge(item, &devices[i]);
			}
		}
	}

	items[count].name = NULL;
	if (menu.items)
		free(menu.items);
	menu.items = items;
	menu.max_width = 0;
}

// bluetoothctl scans are asynchronous. The menu loop's on_update drives the
// shared SCAN_cycle state machine (never blocks the menu).
#define BT_RESCAN_MS 5000
#define BT_SETTLE_MS 1500
static uint32_t last_scan_at = 0;
static uint32_t scan_due_at = 0;
static char tracking_addr[BT_MAX_ADDR] = "";

static void bt_update(MenuList* list) {
	uint32_t now;
	BtDevice fresh[BT_MAX_DEVICES];
	int fresh_count;
	int changed;

	(void)list;

	if (!BT_enabled())
		return;

	now = SDL_GetTicks();
	if (SCAN_cycle(&last_scan_at, &scan_due_at, BT_RESCAN_MS, BT_SETTLE_MS, BT_scan, now) != SCAN_CYCLE_RESULTS)
		return;

	fresh_count = BT_getDevices(fresh, BT_MAX_DEVICES);

	changed = fresh_count != device_count;
	if (!changed) {
		int i;
		for (i = 0; i < fresh_count; i++) {
			if (strcmp(fresh[i].addr, devices[i].addr) != 0 ||
			    strcmp(fresh[i].name, devices[i].name) != 0 ||
			    fresh[i].connected != devices[i].connected ||
			    fresh[i].paired != devices[i].paired) {
				changed = 1;
				break;
			}
		}
	}

	if (changed) {
		char selected_addr[BT_MAX_ADDR] = "";
		if (tracking_addr[0]) {
			strncpy(selected_addr, tracking_addr, sizeof(selected_addr) - 1);
		} else {
			BtDevice* sel_dev = device_for_index(menu.selected);
			if (sel_dev)
				strncpy(selected_addr, sel_dev->addr, sizeof(selected_addr) - 1);
		}

		device_count = fresh_count;
		memcpy(devices, fresh, sizeof(BtDevice) * fresh_count);
		rebuild();

		if (selected_addr[0]) {
			int j;
			for (j = 0; j < device_count; j++) {
				if (strcmp(devices[j].addr, selected_addr) == 0) {
					menu.selected = j + 1;
					break;
				}
			}
		}
	}
}

static BtDevice* device_for_index(int i) {
	int idx = i - 1;
	if (idx < 0 || idx >= device_count)
		return NULL;
	return &devices[idx];
}

static int on_confirm(MenuList* list, int i) {
	BtDevice* dev;

	(void)list;

	if (is_toggle_row(i)) {
		int target = BT_enabled() ? 0 : 1;
		if (BT_setEnabled(target) != 0) {
			Menu_message(target ? "Enable failed" : "Disable failed", (char*[]){"B","BACK", NULL});
		}
		last_scan_at = 0;
		scan_due_at = 0;
		device_count = 0;
		tracking_addr[0] = '\0';
		rebuild();
		return MENU_CALLBACK_NOP;
	}

	dev = device_for_index(i);
	if (!dev)
		return MENU_CALLBACK_NOP;

	strncpy(tracking_addr, dev->addr, sizeof(tracking_addr) - 1);
	if (BT_toggleDevice(dev->addr) != 0) {
		Menu_message("Device action failed", (char*[]){"B","BACK", NULL});
	}
	return MENU_CALLBACK_NOP;
}

static int on_aux(MenuList* list, int i) {
	BtDevice* dev;

	(void)list;

	if (is_toggle_row(i))
		return MENU_CALLBACK_NOP;

	dev = device_for_index(i);
	if (!dev)
		return MENU_CALLBACK_NOP;

	tracking_addr[0] = '\0';
	if (BT_forgetDevice(dev->addr) != 0) {
		Menu_message("Forget failed", (char*[]){"B","BACK", NULL});
	}
	return MENU_CALLBACK_NOP;
}

int main(int argc, char* argv[]) {
	SDL_Surface* screen;

	(void)argc;
	(void)argv;

	PWR_setCPUSpeed(CPU_SPEED_MENU);
	screen = GFX_init(MODE_MAIN);
	PAD_init();
	InitSettings();
	PWR_init();

	menu_screen = screen;
	menu.type = MENU_FIXED;
	menu.desc = NULL;
	menu.on_confirm = on_confirm;
	menu.on_aux = on_aux; // X on a paired device forgets it
	menu.on_update = bt_update;
	BT_init();
	last_scan_at = 0;
	scan_due_at = 0;
	bt_update(&menu);
	rebuild();

	Menu_options(&menu);

	if (menu.items)
		free(menu.items);
	BT_quit();
	QuitSettings();
	PWR_quit();
	PAD_quit();
	GFX_quit();
	return 0;
}
