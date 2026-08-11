#ifndef MENU_H
#define MENU_H

///////////////////////////////

#include <SDL2/SDL.h>

#include "api.h"

///////////////////////////////

#define MENU_BADGE_SIZE 32

typedef struct MenuList MenuList;
typedef struct MenuItem MenuItem;

enum {
	MENU_CALLBACK_NOP,
	MENU_CALLBACK_EXIT,
	MENU_CALLBACK_NEXT_ITEM,
};
typedef int (*MenuList_callback_t)(MenuList* list, int i);

typedef struct MenuItem {
	char* name;
	char* desc;
	char** values;
	char* key; // optional, used by options
	int id; // optional, used by bindings
	int value;
	char badge[MENU_BADGE_SIZE]; // optional right-aligned text
	MenuList* submenu;
	MenuList_callback_t on_confirm;
	MenuList_callback_t on_change;
} MenuItem;

enum {
	MENU_LIST, // eg. save and main menu
	MENU_VAR, // eg. frontend
	MENU_FIXED, // eg. emulator
	MENU_INPUT, // eg. renders like but MENU_VAR but handles input differently
};

typedef struct MenuList {
	int type;
	int max_width; // cached on first draw
	char* desc;
	MenuItem* items;
	MenuList_callback_t on_confirm;
	MenuList_callback_t on_change;
	MenuList_callback_t on_aux; // optional, X button (eg. forget network)
} MenuList;

///////////////////////////////

// hooks, set once by each binary that uses the menu
extern SDL_Surface* menu_screen;
extern PWR_callback_t menu_before_sleep;
extern PWR_callback_t menu_after_sleep;
extern void (*menu_hdmi_monitor)(void);
extern void (*menu_update_desc)(void);
extern char** menu_button_labels; // used to detect input bindings (MENU_INPUT)

int Menu_options(MenuList* list);
int Menu_message(char* message, char** pairs);

///////////////////////////////

#endif
