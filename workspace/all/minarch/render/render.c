#include "render.h"
#include <stdio.h>
#include <string.h>

static render_backend_ops_t* current_backend = &render_sw_ops;
static const char* preferred_backend = NULL;
static int dev_w = 640;
static int dev_h = 480;

void RENDER_set_preferred_backend(const char* name) {
	preferred_backend = name;
}

void RENDER_init(int device_w, int device_h) {
	dev_w = device_w;
	dev_h = device_h;
	render_sw_ops.init(device_w, device_h);
	render_gl_ops.init(device_w, device_h);
	render_vk_ops.init(device_w, device_h);
	current_backend = &render_sw_ops;
}

bool RENDER_handle_environ(unsigned cmd, void* data) {
	if (preferred_backend && strcmp(preferred_backend, "gl") == 0) {
		if (render_gl_ops.handle_environ(cmd, data)) {
			current_backend = &render_gl_ops;
			return true;
		}
		if (render_vk_ops.handle_environ(cmd, data)) {
			current_backend = &render_vk_ops;
			return true;
		}
		return false;
	}

	if (render_vk_ops.handle_environ(cmd, data)) {
		current_backend = &render_vk_ops;
		return true;
	}
	if (render_gl_ops.handle_environ(cmd, data)) {
		current_backend = &render_gl_ops;
		return true;
	}
	return false;
}

void RENDER_post_core_load(unsigned initial_w, unsigned initial_h) {
	if (current_backend && current_backend->post_core_load) {
		current_backend->post_core_load(initial_w, initial_h);
	}
}

void RENDER_video_refresh(const void* data, unsigned width, unsigned height, size_t pitch) {
	if (current_backend && current_backend->video_refresh) {
		current_backend->video_refresh(data, width, height, pitch);
	}
}

void RENDER_draw_menu(SDL_Surface* surface) {
	if (current_backend && current_backend->draw_menu) {
		current_backend->draw_menu(surface);
	}
}

void RENDER_post_menu(void) {
	if (current_backend && current_backend->post_menu) {
		current_backend->post_menu();
	}
}

void RENDER_flip(SDL_Surface* screen) {
	if (current_backend && current_backend->flip) {
		current_backend->flip(screen);
	}
}

SDL_Surface* RENDER_capture_surface(void) {
	if (current_backend && current_backend->capture_surface) {
		return current_backend->capture_surface();
	}
	return NULL;
}

void RENDER_set_scaling(int mode) {
	render_sw_ops.set_scaling(mode);
	render_gl_ops.set_scaling(mode);
	render_vk_ops.set_scaling(mode);
}

void RENDER_set_sharpness(int sharpness) {
	render_sw_ops.set_sharpness(sharpness);
	render_gl_ops.set_sharpness(sharpness);
	render_vk_ops.set_sharpness(sharpness);
}

void RENDER_set_effect(int effect) {
	render_sw_ops.set_effect(effect);
	render_gl_ops.set_effect(effect);
	render_vk_ops.set_effect(effect);
}

void RENDER_set_aspect(double aspect) {
	render_sw_ops.set_aspect(aspect);
	render_gl_ops.set_aspect(aspect);
	render_vk_ops.set_aspect(aspect);
}

void RENDER_update_debug(double fps, double cpu, double use, int show_debug) {
	if (current_backend && current_backend->update_debug) {
		current_backend->update_debug(fps, cpu, use, show_debug);
	}
}

void RENDER_quit(void) {
	if (current_backend && current_backend->quit) {
		current_backend->quit();
	}
	current_backend = &render_sw_ops;
}

render_backend_type_t RENDER_get_backend_type(void) {
	return current_backend ? current_backend->type : RENDER_BACKEND_NONE;
}

bool RENDER_is_hw_active(void) {
	return current_backend && current_backend->type != RENDER_BACKEND_SW &&
		   current_backend->type != RENDER_BACKEND_NONE;
}
