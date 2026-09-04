// Power settings PAK for Minime.
// Reads/writes the power policy (sleep/auto-shutdown/lid/power button
// behavior) via PWR_* in common/api.c + power.conf. Changes apply on the
// next minui/minarch launch (they read power.conf at PWR_init).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <msettings.h>

#include "defines.h"
#include "api.h"
#include "menu.h"
#include "utils.h"

#define POWER_POLICY_PATH USERDATA_PATH "/power.conf"

static const int power_timeout_values[] = {
	PWR_TIMEOUT_OFF, PWR_TIMEOUT_1_MIN, PWR_TIMEOUT_5_MIN, PWR_TIMEOUT_15_MIN, PWR_TIMEOUT_30_MIN, PWR_TIMEOUT_1_HOUR,
};

static int next_enum_index(const int* values, int count, int current, int direction) {
	int i;

	for (i = 0; i < count; i++) {
		if (values[i] == current)
			break;
	}
	if (i >= count)
		i = 0;
	if (direction < 0)
		return i == 0 ? count - 1 : i - 1;
	return i >= count - 1 ? 0 : i + 1;
}

static int next_timeout_value(int current, int direction) {
	return power_timeout_values[next_enum_index(power_timeout_values,
	                                            (int)(sizeof(power_timeout_values) / sizeof(power_timeout_values[0])),
	                                            current, direction)];
}

static int next_behavior_value(int current, int direction, int allow_auto) {
	int values[3];
	int count = 0;

	values[count++] = PWR_BEHAVIOR_SLEEP_ONLY;
	if (allow_auto)
		values[count++] = PWR_BEHAVIOR_AUTO_SHUTDOWN;
	values[count++] = PWR_BEHAVIOR_SHUT_DOWN_NOW;

	return values[next_enum_index(values, count, current, direction)];
}

static int write_power_policy(void) {
	char buf[320];
	char tmp[336];
	FILE* file;

	snprintf(buf, sizeof(buf),
	         "sleep_timeout_ms=%d\n"
	         "auto_shutdown_timeout_ms=%d\n"
	         "lid_behavior=%d\n"
	         "power_button_behavior=%d\n",
	         PWR_getSleepTimeoutMs(), PWR_getAutoShutdownTimeoutMs(), PWR_getLidBehavior(),
	         PWR_getPowerButtonBehavior());

	snprintf(tmp, sizeof(tmp), "%s.tmp", POWER_POLICY_PATH);
	file = fopen(tmp, "w");
	if (!file)
		return -1;
	fputs(buf, file);
	fclose(file);
	if (rename(tmp, POWER_POLICY_PATH) != 0) {
		unlink(tmp);
		return -1;
	}
	return 0;
}

// lid (clamshell only): the trait holds the evdev name of the lid switch
// device, e.g. "gpio-keys-lid". Devices without a clamshell (e.g. RG Arc)
// leave it "na" and must not offer a Lid Behavior setting.
static int lid_supported(void) {
	return PLAT_hasLid();
}

///////////////////////////////////////

enum {
	POWER_ITEM_SLEEP_TIMEOUT,
	POWER_ITEM_AUTO_SHUTDOWN_TIMEOUT,
	POWER_ITEM_LID_BEHAVIOR,
	POWER_ITEM_POWER_BUTTON_BEHAVIOR,
};

static const char* power_timeout_labels[] = {
	"Off", "1 min", "5 min", "15 min", "30 min", "1 hr", NULL,
};
static const char* power_behavior_labels[] = {
	"Sleep Only", "Auto Shutdown", "Shut Down Now", NULL,
};

static void rebuild(MenuList* list) {
	int count = 0;
	MenuItem* items = list->items;

	memset(items, 0, sizeof(MenuItem) * 6);

	items[count].name = "Sleep Timeout";
	items[count].id = POWER_ITEM_SLEEP_TIMEOUT;
	items[count].value = next_enum_index(power_timeout_values, 6, PWR_getSleepTimeoutMs(), 0);
	items[count].values = (char**)power_timeout_labels;
	count++;

	items[count].name = "Auto Shutdown Timeout";
	items[count].id = POWER_ITEM_AUTO_SHUTDOWN_TIMEOUT;
	items[count].value = next_enum_index(power_timeout_values, 6, PWR_getAutoShutdownTimeoutMs(), 0);
	items[count].values = (char**)power_timeout_labels;
	count++;

	if (lid_supported()) {
		items[count].name = "Lid Behavior";
		items[count].id = POWER_ITEM_LID_BEHAVIOR;
		items[count].value = next_enum_index((int[]){PWR_BEHAVIOR_SLEEP_ONLY, PWR_BEHAVIOR_AUTO_SHUTDOWN,
		                                             PWR_BEHAVIOR_SHUT_DOWN_NOW},
		                                     3, PWR_getLidBehavior(), 0);
		items[count].values = (char**)power_behavior_labels;
		count++;
	}

	items[count].name = "Power Button Behavior";
	items[count].id = POWER_ITEM_POWER_BUTTON_BEHAVIOR;
	items[count].value = next_enum_index((int[]){PWR_BEHAVIOR_SLEEP_ONLY, PWR_BEHAVIOR_AUTO_SHUTDOWN,
	                                             PWR_BEHAVIOR_SHUT_DOWN_NOW},
	                                     3, PWR_getPowerButtonBehavior(), 0);
	items[count].values = (char**)power_behavior_labels;
	count++;

	items[count].name = NULL;
	list->max_width = 0;
}

static int on_change(MenuList* list, int i) {
	int value = list->items[i].value;
	int behavior;
	int allow_auto = PWR_getAutoShutdownTimeoutMs() != PWR_TIMEOUT_OFF;

	switch (list->items[i].id) {
	case POWER_ITEM_SLEEP_TIMEOUT:
		PWR_setSleepTimeoutMs(power_timeout_values[value]);
		break;
	case POWER_ITEM_AUTO_SHUTDOWN_TIMEOUT:
		PWR_setAutoShutdownTimeoutMs(power_timeout_values[value]);
		break;
	case POWER_ITEM_LID_BEHAVIOR:
		// value 0 = sleep only, 1 = auto shutdown, 2 = shutdown now
		behavior = value == 2 ? PWR_BEHAVIOR_SHUT_DOWN_NOW : (value == 1 && allow_auto) ? PWR_BEHAVIOR_AUTO_SHUTDOWN
		                                                                                 : PWR_BEHAVIOR_SLEEP_ONLY;
		PWR_setLidBehavior(behavior);
		break;
	case POWER_ITEM_POWER_BUTTON_BEHAVIOR:
		behavior = value == 2 ? PWR_BEHAVIOR_SHUT_DOWN_NOW : (value == 1 && allow_auto) ? PWR_BEHAVIOR_AUTO_SHUTDOWN
		                                                                                 : PWR_BEHAVIOR_SLEEP_ONLY;
		PWR_setPowerButtonBehavior(behavior);
		break;
	}
	(void)write_power_policy();
	return MENU_CALLBACK_NOP;
}

int main(int argc, char* argv[]) {
	SDL_Surface* screen;
	MenuList menu = {0};

	(void)argc;
	(void)argv;

	screen = GFX_init(MODE_MAIN);
	PAD_init();
	InitSettings();
	PWR_init();

	menu_screen = screen;
	menu.type = MENU_FIXED;
	menu.desc = "Power settings. Changes apply on next launch.";
	menu.on_change = on_change;
	menu.items = calloc(7, sizeof(MenuItem));
	rebuild(&menu);

	Menu_options(&menu);

	free(menu.items);
	QuitSettings();
	PWR_quit();
	PAD_quit();
	GFX_quit();
	return 0;
}
