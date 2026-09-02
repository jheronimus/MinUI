#include <linux/input.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "utils.h"
#include <sys/ioctl.h>
#include <linux/input.h>

#include <msettings.h>

#include "settings.h"
#include "traits.h"

//	for ev.value
#define RELEASED 0
#define PRESSED 1
#define REPEAT 2

// linux/input-event-codes.h
#define SW_HEADPHONE_INSERT 0x02

#define AUDIO_SH "/usr/share/minime/scripts/audio.sh"
#define BLUETOOTHD_PID "/run/bluetoothd.pid"

static int input_fds[7] = {0};
static int input_count = 0;

static uint32_t now_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static pthread_t hdmi_pt;
static pthread_t power_pt;
static pthread_t bt_pt;

static int isHDMIConnected(void) {
	if (!MINIME_traitAvailable(gpu_hdmi_state_path))
		return 0;
	char status[16] = "";
	getFile(gpu_hdmi_state_path, status, sizeof(status));
	if (status[0] != '\0') {
		if (prefixMatch("connected", status))
			return 1;
		if (prefixMatch("disconnected", status) || prefixMatch("unknown", status))
			return 0;
	}
	return getInt(gpu_hdmi_state_path);
}

static void* watchHDMI(void* arg) {
	int has_hdmi, had_hdmi;

	has_hdmi = had_hdmi = isHDMIConnected();
	SetHDMI(has_hdmi);

	if (!MINIME_traitAvailable(gpu_hdmi_state_path))
		return 0;

	while (1) {
		sleep(2);

		has_hdmi = isHDMIConnected();
		if (had_hdmi != has_hdmi) {
			had_hdmi = has_hdmi;
			SetHDMI(has_hdmi);
		}
	}

	return 0;
}

static int getBatteryStatus(int* charging, int* capacity) {
	if (!MINIME_traitAvailable(power_battery_sysfs) || !charging || !capacity)
		return -1;

	char path[MINIME_TRAIT_PATH_MAX + 32];
	snprintf(path, sizeof(path), "%s/capacity", power_battery_sysfs);
	*capacity = getInt(path);

	if (MINIME_traitAvailable(power_charger_online_path)) {
		*charging = getInt(power_charger_online_path);
		return 0;
	}

	snprintf(path, sizeof(path), "%s/status", power_battery_sysfs);
	FILE* f = fopen(path, "r");
	if (!f)
		return -1;
	char status[32] = "";
	(void)fgets(status, sizeof(status), f);
	fclose(f);
	*charging = (!strncmp(status, "Charging", 8) || !strncmp(status, "Full", 4));
	return 0;
}

static void* watchPower(void* arg) {
	while (1) {
		int charging = 0;
		int battery = 0;

		getBatteryStatus(&charging, &battery);
		SetCharging(charging);
		SetBattery(battery);
		sleep(2);
	}
	return 0;
}

static int find_bt_sink(char* out, size_t out_size) {
	FILE* p;
	char buf[1024];

	out[0] = '\0';
	// Delegate to BlueALSA via D-Bus: BlueALSA is the single source of truth
	// for connected audio endpoints (managed objects under /org/bluealsa).
	p = popen("gdbus call --system --dest org.bluealsa --object-path /org/bluealsa "
			  "--method org.freedesktop.DBus.ObjectManager.GetManagedObjects 2>/dev/null",
			  "r");
	if (!p)
		return 0;
	while (fgets(buf, sizeof(buf), p)) {
		char* dev = strstr(buf, "/dev_");
		if (dev) {
			dev += 5; // skip "/dev_"
			if (strlen(dev) >= 17) {
				for (int i = 0; i < 17 && i < (int)out_size - 1; i++) {
					out[i] = (dev[i] == '_') ? ':' : dev[i];
				}
				out[17] = '\0';
				break;
			}
		}
	}
	pclose(p);
	return out[0] != '\0';
}

static void* watchBT(void* arg) {
	char active[64] = "";
	char mac[64];

	for (;;) {
		// bluetoothd runs only when the user enabled Bluetooth; skip the
		// poll otherwise to avoid querying D-Bus pointlessly.
		if (access(BLUETOOTHD_PID, F_OK) == 0) {
			if (find_bt_sink(mac, sizeof(mac))) {
				if (strcmp(mac, active) != 0) {
					char cmd[512];

					snprintf(cmd, sizeof(cmd), "%s start-interface bluetooth %s >/dev/null 2>&1",
							 AUDIO_SH, mac);
					system(cmd);
					strncpy(active, mac, sizeof(active) - 1);
					SetBT(1);
				}
			} else if (active[0]) {
				char cmd[512];

				snprintf(cmd, sizeof(cmd), "%s start-interface %s >/dev/null 2>&1", AUDIO_SH,
						 GetJack() ? "headphones" : "speakers");
				system(cmd);
				active[0] = '\0';
				SetBT(0);
			}
		}
		sleep(2);
	}
	return 0;
}

static int openInputDevice(const char* name_or_path) {
	if (!MINIME_traitAvailable(name_or_path))
		return -1;
	int fd = open(name_or_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd >= 0)
		return fd;
	char name[256];
	char path[64];
	for (int i = 0; i < 32; i++) {
		snprintf(path, sizeof(path), "/dev/input/event%d", i);
		fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			continue;
		if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0 && !strcmp(name, name_or_path))
			return fd;
		close(fd);
	}
	return -1;
}

int main(int argc, char* argv[]) {

	(void)argc;
	(void)argv;
	if (MINIME_traitsInit() != 0)
		return 1;
	InitSettings();
	pthread_create(&hdmi_pt, NULL, &watchHDMI, NULL);
	pthread_create(&power_pt, NULL, &watchPower, NULL);
	pthread_create(&bt_pt, NULL, &watchBT, NULL);

	const char* dev_names[] = {
		input_gamepad,
		input_stick,
		input_power,
		input_volume,
		input_menu,
		audio_jack_device_name,
	};
	input_count = 0;
	for (size_t i = 0; i < sizeof(dev_names) / sizeof(dev_names[0]) &&
					   (size_t)input_count < (sizeof(input_fds) / sizeof(input_fds[0]));
		 i++) {
		int fd = openInputDevice(dev_names[i]);
		if (fd >= 0)
			input_fds[input_count++] = fd;
	}

	int menu_code = (button_keycodes[BTN_ID_MENU] >= 0 ? button_keycodes[BTN_ID_MENU] : button_keycodes[BTN_ID_SELECT]);
	uint32_t val;
	uint32_t menu_pressed = 0;

	uint32_t up_pressed = 0;
	uint32_t up_just_pressed = 0;
	uint32_t up_repeat_at = 0;

	uint32_t down_pressed = 0;
	uint32_t down_just_pressed = 0;
	uint32_t down_repeat_at = 0;

	uint8_t ignore;
	uint32_t then;
	uint32_t now;

	then = now_ms();
	ignore = 0;

	while (1) {
		now = now_ms();
		if (now - then > 1000)
			ignore = 1; // ignore input that arrived during sleep

		struct input_event ev;
		for (int i = 0; i < input_count; i++) {
			if (input_fds[i] <= 0)
				continue;
			while (read(input_fds[i], &ev, sizeof(ev)) == sizeof(ev)) {
				if (ignore)
					continue;
				val = ev.value;

				if (ev.type == EV_SW) {
					if (ev.code == SW_HEADPHONE_INSERT) {
						SetJack(val);
					}
					continue;
				}
				if (ev.type != EV_KEY || val > REPEAT)
					continue;
				if (ev.code == menu_code) {
					menu_pressed = val;
				} else if (ev.code == button_keycodes[BTN_ID_PLUS]) {
					up_pressed = up_just_pressed = val;
					if (val)
						up_repeat_at = now + 300;
				} else if (ev.code == button_keycodes[BTN_ID_MINUS]) {
					down_pressed = down_just_pressed = val;
					if (val)
						down_repeat_at = now + 300;
				}
			}
		}

		if (ignore) {
			menu_pressed = 0;
			up_pressed = up_just_pressed = 0;
			down_pressed = down_just_pressed = 0;
			up_repeat_at = 0;
			down_repeat_at = 0;
		}

		if (up_just_pressed || (up_pressed && now >= up_repeat_at)) {
			if (menu_pressed) {
				val = GetBrightness();
				if (val < BRIGHTNESS_MAX)
					SetBrightness(++val);
			} else {
				val = GetVolume();
				if (val < VOLUME_MAX)
					SetVolume(++val);
			}

			if (up_just_pressed)
				up_just_pressed = 0;
			else
				up_repeat_at += 100;
		}

		if (down_just_pressed || (down_pressed && now >= down_repeat_at)) {
			if (menu_pressed) {
				val = GetBrightness();
				if (val > BRIGHTNESS_MIN)
					SetBrightness(--val);
			} else {
				val = GetVolume();
				if (val > VOLUME_MIN)
					SetVolume(--val);
			}

			if (down_just_pressed)
				down_just_pressed = 0;
			else
				down_repeat_at += 100;
		}

		then = now;
		ignore = 0;

		usleep(16666); // 60fps
	}
}