#include <fcntl.h>
#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "msettings.h"

#define MAX_INPUT_FDS 16
#define VOLUME_MIN 0
#define VOLUME_MAX 20
#define BRIGHTNESS_MIN 0
#define BRIGHTNESS_MAX 10

#define CODE_MENU 312
#define CODE_MENU_ALT 354
#define CODE_PLUS 115      // KEY_VOLUMEUP
#define CODE_MINUS 114     // KEY_VOLUMEDOWN
#define CODE_BRIGHT_UP 225 // KEY_BRIGHTNESSUP
#define CODE_BRIGHT_DN 224 // KEY_BRIGHTNESSDOWN

#define RELEASED 0
#define PRESSED 1
#define REPEAT 2

static int fds[MAX_INPUT_FDS];
static int num_fds = 0;

static void open_input_devices(void) {
    char path[32];
    for (int i = 0; i < MAX_INPUT_FDS; i++) {
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0) {
            fds[num_fds++] = fd;
        }
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    InitSettings();
    open_input_devices();
    if (num_fds == 0)
        return 1;

    struct input_event ev;
    uint32_t val;
    uint32_t menu_pressed = 0;
    uint32_t up_pressed = 0;
    uint32_t up_just_pressed = 0;
    uint32_t up_repeat_at = 0;
    uint32_t down_pressed = 0;
    uint32_t down_just_pressed = 0;
    uint32_t down_repeat_at = 0;

    struct timeval tod;
    gettimeofday(&tod, NULL);
    uint32_t then = tod.tv_sec * 1000 + tod.tv_usec / 1000;

    while (1) {
        gettimeofday(&tod, NULL);
        uint32_t now = tod.tv_sec * 1000 + tod.tv_usec / 1000;
        int ignore = (now - then > 1000);

        for (int i = 0; i < num_fds; i++) {
            while (read(fds[i], &ev, sizeof(ev)) == sizeof(ev)) {
                if (ignore)
                    continue;
                val = ev.value;
                if (ev.type != EV_KEY || val > REPEAT)
                    continue;

                if (ev.code == CODE_MENU || ev.code == CODE_MENU_ALT) {
                    menu_pressed = val;
                } else if (ev.code == CODE_PLUS || ev.code == CODE_BRIGHT_UP) {
                    up_pressed = up_just_pressed = val;
                    if (val)
                        up_repeat_at = now + 300;
                } else if (ev.code == CODE_MINUS || ev.code == CODE_BRIGHT_DN) {
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
            up_repeat_at = down_repeat_at = 0;
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
        usleep(16666); // ~60fps
    }

    QuitSettings();
    return 0;
}
