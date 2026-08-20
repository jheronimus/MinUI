#include <linux/input.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

static void *watchHDMI(void *arg) {
    int has_hdmi, had_hdmi;

    has_hdmi = had_hdmi = MINIME_videoHDMIConnected();
    SetHDMI(has_hdmi);

    while (1) {
        sleep(1);

        has_hdmi = MINIME_videoHDMIConnected();
        if (had_hdmi != has_hdmi) {
            had_hdmi = has_hdmi;
            SetHDMI(has_hdmi);
        }
    }

    return 0;
}

static void *watchPower(void *arg) {
    // Poll the sysfs power traits and publish via shared memory. The UI
    // reads GetCharging()/GetBattery() in PLAT_getBatteryStatus, so the
    // charging icon updates within ~1s of a charger being plugged in.
    while (1) {
        int charging = 0;
        int battery = 0;

        MINIME_powerGetBattery(&charging, &battery);
        SetCharging(charging);
        SetBattery(battery);
        sleep(1);
    }
    return 0;
}

static int find_bt_sink(char *out, size_t out_size) {
    FILE *p;
    char line[256];

    out[0] = '\0';
    p = popen("bluetoothctl devices Connected 2>/dev/null", "r");
    if (!p)
        return 0;
    while (fgets(line, sizeof(line), p)) {
        // bluetoothctl output: "Device AA:BB:CC:DD:EE:FF Name..." — the MAC
        // is the SECOND whitespace-separated token, after the literal
        // "Device" label.
        char *dev = strtok(line, " \t\n");

        if (!dev)
            continue;
        dev = strtok(NULL, " \t\n");
        if (!dev || !strchr(dev, ':'))
            continue;
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "bluetoothctl info %s 2>/dev/null | grep -q 'Audio Sink'", dev);
        if (system(cmd) == 0) {
            snprintf(out, out_size, "%s", dev);
            break;
        }
    }
    pclose(p);
    return out[0] != '\0';
}

static void *watchBT(void *arg) {
    char active[64] = "";
    char mac[64];

    for (;;) {
        // bluetoothd runs only when the user enabled Bluetooth; skip the
        // poll otherwise to avoid spawning bluetoothctl pointlessly.
        if (access(BLUETOOTHD_PID, F_OK) == 0) {
            if (find_bt_sink(mac, sizeof(mac))) {
                if (strcmp(mac, active) != 0) {
                    char cmd[512];

                    snprintf(cmd, sizeof(cmd), "%s bt-on %s >/dev/null 2>&1", AUDIO_SH, mac);
                    system(cmd);
                    strncpy(active, mac, sizeof(active) - 1);
                    SetBT(1);
                }
            } else if (active[0]) {
                char cmd[512];

                snprintf(cmd, sizeof(cmd), "%s bt-off >/dev/null 2>&1", AUDIO_SH);
                system(cmd);
                active[0] = '\0';
                SetBT(0);
            }
        }
        sleep(2);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    const MinimeTraits *traits;

    (void)argc;
    (void)argv;
    if (MINIME_traitsInit() != 0)
        return 1;
    traits = MINIME_traits();
    InitSettings();
    pthread_create(&hdmi_pt, NULL, &watchHDMI, NULL);
    pthread_create(&power_pt, NULL, &watchPower, NULL);
    pthread_create(&bt_pt, NULL, &watchBT, NULL);

    // Sync the codec headphone/speaker mux with the initial jack state.
    MINIME_audioSetJackPath(MINIME_audioJackConnected());

    input_count =
        MINIME_inputOpenShortcutDevices(input_fds, sizeof(input_fds) / sizeof(input_fds[0]));
    if (traits && MINIME_traitAvailable(traits->audio_jack_device_name) &&
        input_count < (int)(sizeof(input_fds) / sizeof(input_fds[0]))) {
        int fd = MINIME_inputOpenByNameOrPath(traits->audio_jack_device_name);

        if (fd >= 0)
            input_fds[input_count++] = fd;
    }

    int menu_code = (traits->key_menu >= 0 ? traits->key_menu : traits->key_select);
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
                    // Codec headphone jack switch (rk817/H616). Report the
                    // state to the UI and drive the Playback Mux HP/SPK
                    // route (no-op on boards that auto-switch).
                    if (ev.code == SW_HEADPHONE_INSERT) {
                        SetJack(val);
                        MINIME_audioSetJackPath(val);
                    }
                    continue;
                }
                if (ev.type != EV_KEY || val > REPEAT)
                    continue;
                if (ev.code == menu_code) {
                    menu_pressed = val;
                } else if (ev.code == traits->key_vol_up) {
                    up_pressed = up_just_pressed = val;
                    if (val)
                        up_repeat_at = now + 300;
                } else if (ev.code == traits->key_vol_down) {
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