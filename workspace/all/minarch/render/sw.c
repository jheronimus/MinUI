#include <SDL2/SDL.h>
#include <stdbool.h>

#include "defines.h"
#include "api.h"
#include "render.h"

static int sw_dev_w = 640;
static int sw_dev_h = 480;
static int sw_scaling_mode = 1;
static double sw_aspect = 0.0;

static bool sw_init(int screen_w, int screen_h) {
	sw_dev_w = screen_w;
	sw_dev_h = screen_h;
	return true;
}

static bool sw_handle_environ(unsigned cmd, void* data) {
	(void)cmd;
	(void)data;
	return false;
}

static void sw_post_core_load(unsigned initial_w, unsigned initial_h) {
	(void)initial_w;
	(void)initial_h;
}

static void sw_video_refresh(const void* data, unsigned width, unsigned height, size_t pitch) {
	(void)data;
	(void)width;
	(void)height;
	(void)pitch;
}

static void sw_draw_menu(SDL_Surface* surface) {
	if (surface) {
		GFX_flip(surface);
	}
}

static void sw_flip(SDL_Surface* screen) {
	if (screen) {
		GFX_flip(screen);
	}
}

static SDL_Surface* sw_capture_surface(void) {
	return NULL;
}

static void sw_set_scaling(int mode) {
	sw_scaling_mode = mode;
}

static void sw_set_sharpness(int sharpness) {
	(void)sharpness;
}

static void sw_set_effect(int effect) {
	(void)effect;
}

static void sw_set_aspect(double aspect) {
	sw_aspect = aspect;
}

static void sw_update_debug(double fps, double cpu, double use, int show_debug) {
	(void)fps;
	(void)cpu;
	(void)use;
	(void)show_debug;
}

static void sw_quit(void) {
}

render_backend_ops_t render_sw_ops = {
	.type = RENDER_BACKEND_SW,
	.name = "software",
	.init = sw_init,
	.handle_environ = sw_handle_environ,
	.post_core_load = sw_post_core_load,
	.video_refresh = sw_video_refresh,
	.draw_menu = sw_draw_menu,
	.flip = sw_flip,
	.capture_surface = sw_capture_surface,
	.set_scaling = sw_set_scaling,
	.set_sharpness = sw_set_sharpness,
	.set_effect = sw_set_effect,
	.set_aspect = sw_set_aspect,
	.update_debug = sw_update_debug,
	.quit = sw_quit,
};
