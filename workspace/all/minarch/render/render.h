#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "viewport.h"

typedef enum {
	RENDER_BACKEND_NONE = 0,
	RENDER_BACKEND_SW,
	RENDER_BACKEND_GL,
	RENDER_BACKEND_VK,
} render_backend_type_t;

typedef struct render_backend_ops {
	render_backend_type_t type;
	const char* name;

	bool (*init)(int screen_w, int screen_h);
	bool (*handle_environ)(unsigned cmd, void* data);
	void (*post_core_load)(unsigned initial_w, unsigned initial_h);
	void (*video_refresh)(const void* data, unsigned width, unsigned height, size_t pitch);
	void (*draw_menu)(SDL_Surface* surface);
	void (*post_menu)(void);
	void (*flip)(SDL_Surface* screen);
	SDL_Surface* (*capture_surface)(void);
	void (*set_scaling)(int mode);
	void (*set_sharpness)(int sharpness);
	void (*set_effect)(int effect);
	void (*set_aspect)(double aspect);
	void (*update_debug)(double fps, double cpu, double use, int show_debug);
	void (*quit)(void);
} render_backend_ops_t;

// Public lifecycle APIs for minarch.c
void RENDER_init(int device_w, int device_h);
bool RENDER_handle_environ(unsigned cmd, void* data);
void RENDER_post_core_load(unsigned initial_w, unsigned initial_h);
void RENDER_video_refresh(const void* data, unsigned width, unsigned height, size_t pitch);
void RENDER_draw_menu(SDL_Surface* surface);
void RENDER_post_menu(void);
void RENDER_flip(SDL_Surface* screen);
SDL_Surface* RENDER_capture_surface(void);
void RENDER_set_scaling(int mode);
void RENDER_set_sharpness(int sharpness);
void RENDER_set_effect(int effect);
void RENDER_set_aspect(double aspect);
void RENDER_update_debug(double fps, double cpu, double use, int show_debug);
void RENDER_set_preferred_backend(const char* name);
void RENDER_quit(void);

render_backend_type_t RENDER_get_backend_type(void);
bool RENDER_is_hw_active(void);

// Backend registration (internal to render module)
extern render_backend_ops_t render_sw_ops;
extern render_backend_ops_t render_gl_ops;
extern render_backend_ops_t render_vk_ops;

#endif // RENDER_H
