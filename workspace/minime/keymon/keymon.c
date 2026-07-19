/*
 * Minime keymon — minimal stub.
 * Polls input devices for brightness/volume key combos.
 * Feature imports will replace hardcoded paths with traits-based device names.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <linux/input.h>

#include "msettings.h"
#include "platform.h"

#define POLL_INTERVAL_US 16666 // ~60fps

static int input_fd = -1;

static void* keymon_thread(void* arg) {
	(void)arg;
	struct input_event ev;
	while (1) {
		if (read(input_fd, &ev, sizeof(ev))==sizeof(ev)) {
			if (ev.type==EV_KEY && ev.value==1) {
				if (ev.code==CODE_POWER) {
					// TODO: implement power button handling
				}
			}
		}
		usleep(POLL_INTERVAL_US);
	}
	return NULL;
}

int main(int argc, char* argv[]) {
	(void)argc; (void)argv;

	// TODO: open input devices by name from traits instead of hardcoded
	input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (input_fd<0) return 1;

	InitSettings();

	pthread_t tid;
	pthread_create(&tid, NULL, keymon_thread, NULL);
	pthread_join(tid, NULL);

	QuitSettings();
	close(input_fd);
	return 0;
}
