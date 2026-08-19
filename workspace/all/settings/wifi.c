// Wi-Fi settings PAK for Minime (iwd backend).
// Toggle, scan, connect (with passphrase keyboard for secure networks),
// disconnect, and forget known networks.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <msettings.h>

#include "defines.h"
#include "api.h"
#include "menu.h"
#include "keyboard.h"
#include "utils.h"
#include "wireless.h"

///////////////////////////////////////

static MenuList menu = {0};
static WifiNetwork networks[WIFI_MAX_NETWORKS];
static int network_count = 0;

enum {
	WIFI_ROW_TOGGLE = -1,
};

// the toggle row is index 0; network rows are 0..network_count-1
static int is_toggle_row(int i) {
	return i == 0;
}

static void set_badge(MenuItem* item, const WifiNetwork* net) {
	if (net->connected)
		strcpy(item->badge, "connected");
	else if (net->known)
		strcpy(item->badge, "known");
	else
		item->badge[0] = '\0';
}

static void rebuild(void) {
	MenuItem* items;
	int i;
	int count = 0;

	// 1 toggle + up to WIFI_MAX_NETWORKS networks + terminator
	items = calloc(1 + WIFI_MAX_NETWORKS + 1, sizeof(MenuItem));

	items[count].name = WIFI_enabled() ? "Disable Wi-Fi" : "Enable Wi-Fi";
	items[count].confirm_label = "TOGGLE";
	count++;

	if (WIFI_enabled()) {
		network_count = WIFI_getNetworks(networks, WIFI_MAX_NETWORKS);

		// connected first, then known, then others
		for (i = 0; i < network_count; i++) {
			if (networks[i].connected) {
				MenuItem* item = &items[count++];
				item->name = networks[i].ssid;
				item->confirm_label = "DISCONNECT";
				item->aux_label = "FORGET";
				set_badge(item, &networks[i]);
			}
		}
		for (i = 0; i < network_count; i++) {
			if (!networks[i].connected && networks[i].known) {
				MenuItem* item = &items[count++];
				item->name = networks[i].ssid;
				item->confirm_label = "CONNECT";
				item->aux_label = "FORGET";
				set_badge(item, &networks[i]);
			}
		}
		for (i = 0; i < network_count; i++) {
			if (!networks[i].connected && !networks[i].known) {
				MenuItem* item = &items[count++];
				item->name = networks[i].ssid;
				item->confirm_label = "CONNECT";
				set_badge(item, &networks[i]);
			}
		}
	}

	items[count].name = NULL;
	if (menu.items)
		free(menu.items);
	menu.items = items;
	menu.max_width = 0;
}

// iwd scans are asynchronous: `iwctl station scan` returns immediately and
// results populate over the next ~1-2s. The menu loop's on_update drives the
// shared SCAN_cycle state machine (never blocks the menu).
#define WIFI_RESCAN_MS 4000
#define WIFI_SETTLE_MS 1500
static uint32_t last_scan_at = 0;
static uint32_t scan_due_at = 0;

static void wifi_update(MenuList* list) {
	uint32_t now;
	WifiNetwork fresh[WIFI_MAX_NETWORKS];
	int fresh_count;
	int changed;

	(void)list;

	if (!WIFI_enabled())
		return;

	now = SDL_GetTicks();
	if (SCAN_cycle(&last_scan_at, &scan_due_at, WIFI_RESCAN_MS, WIFI_SETTLE_MS, WIFI_scan, now) != SCAN_CYCLE_RESULTS)
		return;

	fresh_count = WIFI_getNetworks(fresh, WIFI_MAX_NETWORKS);

	changed = fresh_count != network_count;
	if (!changed) {
		int i;
		for (i = 0; i < fresh_count; i++) {
			if (strcmp(fresh[i].ssid, networks[i].ssid) != 0 ||
			    fresh[i].connected != networks[i].connected ||
			    fresh[i].known != networks[i].known) {
				changed = 1;
				break;
			}
		}
	}

	if (changed) {
		network_count = fresh_count;
		memcpy(networks, fresh, sizeof(WifiNetwork) * fresh_count);
		rebuild();
	}
}

// find the network row at index i (skip toggle)
static WifiNetwork* network_for_index(int i) {
	int idx = i - 1;
	if (idx < 0 || idx >= network_count)
		return NULL;
	return &networks[idx];
}

static int do_connect(const char* ssid, int secure, const char* passphrase) {
	if (WIFI_connect(ssid, passphrase) != 0) {
		Menu_message("Connect failed", (char*[]){"B","BACK", NULL});
		return 0;
	}
	return 1;
}

static int on_confirm(MenuList* list, int i) {
	WifiNetwork* net;

	(void)list;

	if (is_toggle_row(i)) {
		int target = WIFI_enabled() ? 0 : 1;
		if (WIFI_setEnabled(target) != 0) {
			Menu_message(target ? "Enable failed" : "Disable failed", (char*[]){"B","BACK", NULL});
		}
		last_scan_at = 0; // re-scan on next frame
		scan_due_at = 0;
		rebuild();
		return MENU_CALLBACK_NOP;
	}

	net = network_for_index(i);
	if (!net)
		return MENU_CALLBACK_NOP;

	if (net->connected) {
		WIFI_disconnect();
		rebuild();
		return MENU_CALLBACK_NOP;
	}

	if (net->known) {
		do_connect(net->ssid, net->security, NULL);
		rebuild();
		return MENU_CALLBACK_NOP;
	}

	if (net->security == WIFI_SECURITY_WPA) {
		// passphrase keyboard
		struct ui_keyboard kb;
		char pass[UI_KEYBOARD_TEXT_MAX];
		int done = 0;

		UI_KEYBOARD_init(&kb);
		GFX_clear(menu_screen);
		GFX_flip(menu_screen);

		while (!done) {
			GFX_startFrame();
			PAD_poll();

			GFX_clear(menu_screen);
			GFX_blitText(font.small, "Enter Wi-Fi password", SCALE1(12), COLOR_WHITE, menu_screen,
			             &(SDL_Rect){SCALE1(PADDING), SCALE1(PADDING), menu_screen->w - SCALE1(PADDING*2), SCALE1(20)});
			{
				char display[UI_KEYBOARD_TEXT_MAX];
				UI_KEYBOARD_copyDisplay(&kb, display, sizeof(display));
				GFX_blitText(font.small, display, SCALE1(12), COLOR_WHITE, menu_screen,
				             &(SDL_Rect){SCALE1(PADDING), SCALE1(PADDING + PILL_SIZE), menu_screen->w - SCALE1(PADDING*2),
				                         SCALE1(20)});
			}
			UI_KEYBOARD_draw(&kb, menu_screen, &(SDL_Rect){0, SCALE1(PADDING + PILL_SIZE + 24), menu_screen->w,
			                                              menu_screen->h - SCALE1(PADDING + PILL_SIZE + 40)});

			if (PAD_justPressed(BTN_B)) {
				done = 1;
			} else if (PAD_justRepeated(BTN_UP)) {
				UI_KEYBOARD_move(&kb, 0, -1);
			} else if (PAD_justRepeated(BTN_DOWN)) {
				UI_KEYBOARD_move(&kb, 0, 1);
			} else if (PAD_justRepeated(BTN_LEFT)) {
				UI_KEYBOARD_move(&kb, -1, 0);
			} else if (PAD_justRepeated(BTN_RIGHT)) {
				UI_KEYBOARD_move(&kb, 1, 0);
			} else if (PAD_justPressed(BTN_A)) {
				const char* key = UI_KEYBOARD_getSelectedKey(&kb);
				if (!strcmp(key, "DONE")) {
					UI_KEYBOARD_copyDisplay(&kb, pass, sizeof(pass));
					done = 1;
					if (pass[0] && do_connect(net->ssid, net->security, pass))
						Menu_message("Connecting...", (char*[]){"B","BACK", NULL});
				} else if (!strcmp(key, "SHIFT")) {
					UI_KEYBOARD_toggleShift(&kb);
				} else if (!strcmp(key, "DEL")) {
					UI_KEYBOARD_backspace(&kb);
				} else {
					UI_KEYBOARD_insertSelected(&kb);
				}
			}

			GFX_flip(menu_screen);
		}
		rebuild();
		return MENU_CALLBACK_NOP;
	}

	// open network
	if (net->security == WIFI_SECURITY_OPEN) {
		if (!Menu_message("This is an unprotected open network,\nare you sure you want to connect?", (char*[]){"B","CANCEL", "A","CONNECT", NULL})) {
			rebuild();
			return MENU_CALLBACK_NOP;
		}
	}
	do_connect(net->ssid, net->security, NULL);
	rebuild();
	return MENU_CALLBACK_NOP;
}

static int on_aux(MenuList* list, int i) {
	WifiNetwork* net;

	(void)list;

	net = network_for_index(i);
	if (!net)
		return MENU_CALLBACK_NOP;
	if (net->known) {
		WIFI_forget(net->ssid);
		rebuild();
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
	menu.on_aux = on_aux; // X on a known network forgets it
	menu.on_update = wifi_update;
	WIFI_init();
	last_scan_at = 0;
	scan_due_at = 0;
	wifi_update(&menu); // fire initial scan (non-blocking)
	rebuild();

	Menu_options(&menu);

	if (menu.items)
		free(menu.items);
	QuitSettings();
	PWR_quit();
	PAD_quit();
	GFX_quit();
	return 0;
}
