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
	char state[16];
	char buf[40];

	strcpy(state, net->connected ? "Connected" : (net->known ? "Known" : (net->security == WIFI_SECURITY_OPEN ? "Open" : "Secure")));
	if (net->signal > 0 && !net->connected)
		snprintf(buf, sizeof(buf), "%s | %d%%", state, net->signal);
	else
		snprintf(buf, sizeof(buf), "%s", state);
	snprintf(item->badge, sizeof(item->badge), "%s", buf);
}

static void rebuild(void) {
	MenuItem* items;
	int i;
	int count = 0;

	// 1 toggle + up to WIFI_MAX_NETWORKS networks + terminator
	items = calloc(1 + WIFI_MAX_NETWORKS + 1, sizeof(MenuItem));

	items[count].name = WIFI_enabled() ? "Disable Wi-Fi" : "Enable Wi-Fi";
	items[count].desc = "Turn the Wi-Fi radio on or off.\nPress A to toggle.";
	if (WIFI_isBusy()) {
		items[count].name = WIFI_enabled() ? "Disabling Wi-Fi" : "Enabling Wi-Fi";
	}
	count++;

	if (WIFI_enabled()) {
		WIFI_scan();
		network_count = WIFI_getNetworks(networks, WIFI_MAX_NETWORKS);

		// connected first, then known, then others
		for (i = 0; i < network_count; i++) {
			if (networks[i].connected) {
				MenuItem* item = &items[count++];
				item->name = networks[i].ssid;
				item->desc = "Connected. Press A to disconnect.";
				set_badge(item, &networks[i]);
			}
		}
		for (i = 0; i < network_count; i++) {
			if (!networks[i].connected && networks[i].known) {
				MenuItem* item = &items[count++];
				item->name = networks[i].ssid;
				item->desc = "Known network. Press A to connect.";
				set_badge(item, &networks[i]);
			}
		}
		for (i = 0; i < network_count; i++) {
			if (!networks[i].connected && !networks[i].known) {
				MenuItem* item = &items[count++];
				item->name = networks[i].ssid;
				item->desc = "Press A to connect.";
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
	menu.type = MENU_LIST;
	menu.desc = "Wi-Fi";
	menu.on_confirm = on_confirm;
	menu.on_aux = on_aux; // X on a known network forgets it
	WIFI_init();
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
