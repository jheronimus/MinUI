// minime platform
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <msettings.h>

#include "defines.h"
#include "api.h"
#include "platform.h"
#include "utils.h"
#include "scaler.h"
#include "traits.h"

#include <linux/input.h>

//////////////////////////////////////
// Platform Lifecycle & Device Traits

int on_hdmi = 0;

static inline void load_traits(void) {
	if (MINIME_traitsInit() != 0)
		exit(1);
}

static inline void ensure_traits(void) {
	if (device_id[0] == '\0') {
		load_traits();
	}
}

static int openShortcutDevices(int* fds, size_t max_fds) {
	if (!fds)
		return 0;
	const char* names[] = {
		input_gamepad,
		input_stick,
		input_power,
		input_volume,
		input_menu,
	};
	int count = 0;
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]) && (size_t)count < max_fds; i++) {
		int fd = MINIME_inputOpenByName(names[i]);
		if (fd >= 0)
			fds[count++] = fd;
	}
	return count;
}

static int normalizeAxis(int value, int invert) {
	if (axis_min >= axis_center || axis_center >= axis_max)
		return 0;
	int normalized;
	if (value < axis_center) {
		normalized = -((axis_center - value) * 32767) / (axis_center - axis_min);
	} else {
		normalized = ((value - axis_center) * 32767) / (axis_max - axis_center);
	}
	return invert ? -normalized : normalized;
}

char* PLAT_getModel(void) {
	ensure_traits();
	return device_model;
}

int PLAT_getScreenRotation(void) {
	return screen_rotation;
}

//////////////////////////////////////
// Input Handling & Gamepad

#define INPUT_COUNT 5
static int inputs[INPUT_COUNT];

static void updateButtonState(int btn, int pressed, int id, uint32_t tick) {
	if (btn == BTN_NONE || id < 0 || id >= BTN_ID_COUNT)
		return;

	if (pressed) {
		if ((pad.is_pressed & btn) == BTN_NONE) {
			pad.just_pressed |= btn;
			pad.just_repeated |= btn;
			pad.is_pressed |= btn;
			pad.repeat_at[id] = tick + PAD_REPEAT_DELAY;
		}
	} else if (pad.is_pressed & btn) {
		pad.just_released |= btn;
		pad.is_pressed &= ~btn;
	}
}

static void drainInputFd(int input) {
	if (input < 0) return;
	struct input_event event;
	while (read(input, &event, sizeof(event)) == sizeof(event)) {
	}
}

static void drainAllInputs(void) {
	for (int i = 0; i < INPUT_COUNT; i++) {
		drainInputFd(inputs[i]);
	}
}

void PLAT_initInput(void) {
	for (int i = 0; i < INPUT_COUNT; i++) inputs[i] = -1;
	openShortcutDevices(inputs, INPUT_COUNT);
	drainAllInputs();
}

void PLAT_quitInput(void) {
	for (int i = 0; i < INPUT_COUNT; i++) {
		if (inputs[i] >= 0) close(inputs[i]);
	}
}

static void handleHatEvent(int code, int value, uint32_t tick) {
	if (value > 1) return;

	int hats[4] = {-1, -1, -1, -1}; // up, down, left, right
	if (code == axis_hat_y) {
		hats[0] = (value == -1);
		hats[1] = (value == 1);
	} else if (code == axis_hat_x) {
		hats[2] = (value == -1);
		hats[3] = (value == 1);
	}

	for (int id = 0; id < 4; id++) {
		if (hats[id] != -1) {
			updateButtonState(1 << id, hats[id], id, tick);
		}
	}
}

static void handleStickAxisEvent(int code, int value, uint32_t tick) {
	if (code == axis_lx) {
		pad.laxis.x = normalizeAxis(value, axis_lx_invert);
		PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x, tick + PAD_REPEAT_DELAY);
	} else if (code == axis_ly) {
		pad.laxis.y = normalizeAxis(value, axis_ly_invert);
		PAD_setAnalog(BTN_ID_ANALOG_UP, BTN_ID_ANALOG_DOWN, pad.laxis.y, tick + PAD_REPEAT_DELAY);
	} else if (code == axis_rx) {
		pad.raxis.x = normalizeAxis(value, axis_rx_invert);
	} else if (code == axis_ry) {
		pad.raxis.y = normalizeAxis(value, axis_ry_invert);
	}
}

static void handleAbsEvent(int code, int value, uint32_t tick) {
	if (code == axis_hat_y || code == axis_hat_x) {
		handleHatEvent(code, value, tick);
	} else {
		handleStickAxisEvent(code, value, tick);
	}
}

static void handleKeyEvent(int code, int value, uint32_t tick) {
	if (value > 1) return; // ignore repeats
	int pressed = value;

	for (int id = 0; id < BTN_ID_COUNT; id++) {
		if (button_keycodes[id] >= 0 && code == button_keycodes[id]) {
			updateButtonState(1 << id, pressed, id, tick);
			break;
		}
	}

	// On devices without a dedicated F/Menu button, SELECT doubles as MENU modifier
	if (code == button_keycodes[BTN_ID_SELECT] && button_keycodes[BTN_ID_MENU] < 0) {
		updateButtonState(BTN_MENU, pressed, BTN_ID_MENU, tick);
	}
}

static void updateButtonRepeats(uint32_t tick) {
	for (int i = 0; i < BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick >= pad.repeat_at[i])) {
			pad.just_repeated |= btn;
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}
}

static void pollInputEvents(uint32_t tick) {
	static struct input_event event;
	for (int i = 0; i < INPUT_COUNT; i++) {
		int input = inputs[i];
		if (input < 0) continue;
		while (read(input, &event, sizeof(event)) == sizeof(event)) {
			if (event.type == EV_KEY) {
				handleKeyEvent(event.code, event.value, tick);
			} else if (event.type == EV_ABS) {
				handleAbsEvent(event.code, event.value, tick);
			}
		}
	}
}

void PLAT_pollInput(void) {
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	updateButtonRepeats(tick);
	pollInputEvents(tick);

	if (lid.has_lid && PLAT_lidChanged(NULL) && !lid.is_open) {
		PWR_requestLidAction();
	}
}

static int checkPowerRelease(int input_fd) {
	struct input_event event;
	while (read(input_fd, &event, sizeof(event)) == sizeof(event)) {
		if (event.type == EV_KEY && event.code == button_keycodes[BTN_ID_POWER] && event.value == 0) {
			return 1;
		}
	}
	return 0;
}

int PLAT_shouldWake(void) {
	int lid_open = 1;
	if (lid.has_lid && PLAT_lidChanged(&lid_open) && lid_open) return 1;

	for (int i = 0; i < INPUT_COUNT; i++) {
		if (inputs[i] >= 0 && checkPowerRelease(inputs[i])) {
			if (lid.has_lid && !lid.is_open) return 0;
			return 1;
		}
	}
	return 0;
}

int PLAT_is6Button(void) {
	ensure_traits();
	return (button_keycodes[BTN_ID_C] >= 0 && button_keycodes[BTN_ID_Z] >= 0);
}

int PLAT_hasMenuButton(void) {
	ensure_traits();
	return button_keycodes[BTN_ID_MENU] >= 0;
}

int PLAT_hasL3(void) {
	ensure_traits();
	return button_keycodes[BTN_ID_L3] >= 0;
}

int PLAT_hasR3(void) {
	ensure_traits();
	return button_keycodes[BTN_ID_R3] >= 0;
}

int PLAT_hasLeftStick(void) {
	ensure_traits();
	return axis_lx >= 0;
}

int PLAT_hasRightStick(void) {
	ensure_traits();
	return axis_rx >= 0;
}

//////////////////////////////////////
// Clamshell Lid Sensor

static int lid_fd = -1;

int PLAT_hasLid(void) {
	ensure_traits();
	return MINIME_traitAvailable(input_lid);
}

void PLAT_initLid(void) {
	ensure_traits();
	lid_fd = MINIME_inputOpenByName(input_lid);
	lid.has_lid = lid_fd >= 0;
	if (lid.has_lid) {
		unsigned long sw[SW_MAX / 8 / sizeof(unsigned long) + 1] = {0};
		if (ioctl(lid_fd, EVIOCGSW(sizeof(sw)), sw) >= 0)
			lid.is_open = !((sw[0] >> SW_LID) & 1);
	}
}

int PLAT_lidChanged(int* state) {
	if (!lid.has_lid) return 0;
	struct input_event event;
	while (read(lid_fd, &event, sizeof(event)) == sizeof(event)) {
		if (event.type == EV_SW && event.code == SW_LID) {
			int lid_open = !event.value;
			if (lid_open != lid.is_open) {
				lid.is_open = lid_open;
				if (state) *state = lid_open;
				return 1;
			}
		}
	}
	return 0;
}

//////////////////////////////////////
// Video Pipeline (KMSDRM & Scaler)

static struct VID_Context {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_Texture* target;
	SDL_Texture* effect;
	SDL_Surface* screen;
	SDL_GLContext gl_ctx;
	GFX_Renderer* blit;
	int tex_w;
	int tex_h;
	int tex_p;
	int sharpness;
} vid;

static inline int getScreenWidth(void) {
	return on_hdmi ? gpu_hdmi_width : screen_width;
}

static inline int getScreenHeight(void) {
	return on_hdmi ? gpu_hdmi_height : screen_height;
}

static void PLAT_computeRendererRects(const GFX_Renderer* renderer, SDL_Rect* src_rect,
									  SDL_Rect* dst_rect) {
	int screen_w = getScreenWidth();
	int screen_h = getScreenHeight();
	int x = 0;
	int y = 0;
	int w = screen_w;
	int h = screen_h;

	if (renderer->aspect == 0) {
		if (renderer->scale == 1) {
			w = MIN(renderer->src_w, screen_w);
			h = MIN(renderer->src_h, screen_h);
			x = (screen_w - w) / 2;
			y = (screen_h - h) / 2;
		} else if (renderer->scale > 0) {
			w = renderer->src_w * renderer->scale;
			h = renderer->src_h * renderer->scale;
			x = (screen_w - w) / 2;
			y = (screen_h - h) / 2;
		} else {
			w = MIN(renderer->src_w, screen_w);
			h = MIN(renderer->src_h, screen_h);
			x = renderer->dst_x;
			y = renderer->dst_y;
			src_rect->w = w;
			src_rect->h = h;
		}

		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
		return;
	}

	if (renderer->aspect > 0) {
		h = screen_h;
		w = h * renderer->aspect;
		if (w > screen_w) {
			double ratio = 1 / renderer->aspect;
			w = screen_w;
			h = w * ratio;
		}
		x = (screen_w - w) / 2;
		y = (screen_h - h) / 2;

		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
	}
}

SDL_Surface* PLAT_initVideo(void) {
	load_traits();

	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	if (MINIME_isHDMIConnected()) {
		w = gpu_hdmi_width;
		h = gpu_hdmi_height;
		p = gpu_hdmi_width * FIXED_BPP;
		on_hdmi = 1;
	}

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
		LOG_error("SDL video init failed: %s\n", SDL_GetError());
		exit(1);
	}
	SDL_ShowCursor(0);

	vid.window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h,
								  SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
	if (!vid.window) {
		LOG_error("SDL window creation failed: %s\n", SDL_GetError());
		exit(1);
	}
	vid.renderer =
		SDL_CreateRenderer(vid.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!vid.renderer) {
		LOG_error("SDL renderer creation failed: %s\n", SDL_GetError());
		exit(1);
	}

	SDL_RendererInfo info;
	SDL_GetRendererInfo(vid.renderer, &info);
	LOG_info("SDL Renderer: %s (flags: 0x%08x)\n", info.name, info.flags);

	SDL_Surface* screen = SDL_CreateRGBSurface(
		SDL_SWSURFACE, FIXED_WIDTH, FIXED_HEIGHT, FIXED_DEPTH,
		0x0000f800, 0x000007e0, 0x0000001f, 0x00000000);
	if (!screen) {
		LOG_error("SDL screen surface creation failed: %s\n", SDL_GetError());
		exit(1);
	}

	vid.screen = screen;
	vid.tex_w = w;
	vid.tex_h = h;
	vid.tex_p = p;
	vid.sharpness = SHARPNESS_SOFT;

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	vid.texture =
		SDL_CreateTexture(vid.renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);
	if (!vid.texture) {
		LOG_error("SDL texture creation failed: %s\n", SDL_GetError());
		exit(1);
	}

	return vid.screen;
}

void PLAT_quitVideo(void) {
	PLAT_quitGLContext();
	SDL_FreeSurface(vid.screen);
	if (vid.target) SDL_DestroyTexture(vid.target);
	if (vid.effect) SDL_DestroyTexture(vid.effect);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0);
}

void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen);
	SDL_RenderClear(vid.renderer);
}

void PLAT_setVsync(int vsync) {
	if (vid.gl_ctx) {
		if (vsync == 0)
			SDL_GL_SetSwapInterval(0);
		else if (vsync == 1)
			SDL_GL_SetSwapInterval(SDL_GL_SetSwapInterval(-1) == 0 ? -1 : 0);
		else
			SDL_GL_SetSwapInterval(1);
	}
}

static int hard_scale = 4;

static void resizeVideo(int w, int h, int p) {
	if (w == vid.tex_w && h == vid.tex_h && p == vid.tex_p) return;

	if (w >= getScreenWidth() && h >= getScreenHeight())
		hard_scale = 1;
	else if (h >= 160)
		hard_scale = 2;
	else
		hard_scale = 4;

	SDL_DestroyTexture(vid.texture);
	if (vid.target) SDL_DestroyTexture(vid.target);

	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY,
							vid.sharpness == SHARPNESS_SOFT ? "1" : "0", SDL_HINT_OVERRIDE);
	vid.texture =
		SDL_CreateTexture(vid.renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);

	if (vid.sharpness == SHARPNESS_CRISP) {
		SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "1", SDL_HINT_OVERRIDE);
		vid.target = SDL_CreateTexture(vid.renderer, SDL_PIXELFORMAT_RGB565,
									   SDL_TEXTUREACCESS_TARGET, w * hard_scale, h * hard_scale);
	} else {
		vid.target = NULL;
	}

	vid.tex_w = w;
	vid.tex_h = h;
	vid.tex_p = p;
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w, h, p);
	return vid.screen;
}

void PLAT_setSharpness(int sharpness) {
	if (vid.sharpness == sharpness) return;
	int p = vid.tex_p;
	vid.tex_p = 0;
	vid.sharpness = sharpness;
	resizeVideo(vid.tex_w, vid.tex_h, p);
}

//////////////////////////////////////
// Video Effects & Scanlines

static struct FX_Context {
	int scale;
	int type;
	int color;
	int next_scale;
	int next_type;
	int next_color;
	int live_type;
} effect = {
	.scale = 1,
	.next_scale = 1,
	.type = EFFECT_NONE,
	.next_type = EFFECT_NONE,
	.live_type = EFFECT_NONE,
	.color = 0,
	.next_color = 0,
};

static void rgb565_to_rgb888(uint32_t rgb565, uint8_t* r, uint8_t* g, uint8_t* b) {
	uint8_t red = (rgb565 >> 11) & 0x1F;
	uint8_t green = (rgb565 >> 5) & 0x3F;
	uint8_t blue = rgb565 & 0x1F;

	*r = (red << 3) | (red >> 2);
	*g = (green << 2) | (green >> 4);
	*b = (blue << 3) | (blue >> 2);
}

typedef struct {
	int max_scale;
	int opacity;
	const char* path;
} EffectEntry;

static const char* lookupEffect(const EffectEntry* table, size_t count, int scale, int* opacity) {
	for (size_t i = 0; i < count; i++) {
		if (scale < table[i].max_scale || table[i].max_scale == 0) {
			*opacity = table[i].opacity;
			return table[i].path;
		}
	}
	return NULL;
}

static const char* getEffectPath(int type, int scale, int* opacity) {
	static const EffectEntry line_effects[] = {
		{3, 128, RES_PATH "/line-2.png"},
		{4, 128, RES_PATH "/line-3.png"},
		{5, 128, RES_PATH "/line-4.png"},
		{6, 128, RES_PATH "/line-5.png"},
		{8, 128, RES_PATH "/line-6.png"},
		{0, 128, RES_PATH "/line-8.png"},
	};
	static const EffectEntry grid_effects[] = {
		{3, 64, RES_PATH "/grid-2.png"},
		{4, 112, RES_PATH "/grid-3.png"},
		{5, 144, RES_PATH "/grid-4.png"},
		{6, 160, RES_PATH "/grid-5.png"},
		{8, 112, RES_PATH "/grid-6.png"},
		{11, 144, RES_PATH "/grid-8.png"},
		{0, 136, RES_PATH "/grid-11.png"},
	};

	if (type == EFFECT_LINE)
		return lookupEffect(line_effects, sizeof(line_effects) / sizeof(line_effects[0]), scale, opacity);
	if (type == EFFECT_GRID)
		return lookupEffect(grid_effects, sizeof(grid_effects) / sizeof(grid_effects[0]), scale, opacity);
	return NULL;
}

static void recolorGridSurface(SDL_Surface* surface, uint16_t rgb565) {
	uint8_t r, g, b;
	rgb565_to_rgb888(rgb565, &r, &g, &b);

	uint32_t* pixels = (uint32_t*)surface->pixels;
	int total_pixels = surface->w * surface->h;
	for (int i = 0; i < total_pixels; i++) {
		uint8_t a = (pixels[i] >> 24) & 0xFF;
		pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
	}
}

static void applyEffectTexture(const char* effect_path, int opacity) {
	SDL_Surface* tmp = IMG_Load(effect_path);
	if (!tmp) return;

	if (effect.type == EFFECT_GRID && effect.color) {
		recolorGridSurface(tmp, effect.color);
	}

	if (vid.effect) SDL_DestroyTexture(vid.effect);
	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "0", SDL_HINT_OVERRIDE);
	vid.effect = SDL_CreateTextureFromSurface(vid.renderer, tmp);
	SDL_SetTextureBlendMode(vid.effect, SDL_BLENDMODE_BLEND);
	SDL_SetTextureAlphaMod(vid.effect, opacity);
	SDL_FreeSurface(tmp);

	effect.live_type = effect.type;
}

static inline int effectStateMatches(void) {
	return effect.next_scale == effect.scale && effect.next_type == effect.type &&
		   effect.next_color == effect.color;
}

static inline int effectMatchesLive(int live_scale, int live_color) {
	return effect.type == effect.live_type && effect.scale == live_scale &&
		   effect.color == live_color;
}

static void updateEffect(void) {
	if (effectStateMatches()) return;

	int live_scale = effect.scale;
	int live_color = effect.color;
	effect.scale = effect.next_scale;
	effect.type = effect.next_type;
	effect.color = effect.next_color;

	if (effect.type == EFFECT_NONE || effectMatchesLive(live_scale, live_color)) return;

	int opacity = 128;
	const char* effect_path = getEffectPath(effect.type, effect.scale, &opacity);
	if (effect_path) {
		applyEffectTexture(effect_path, opacity);
	}
}

void PLAT_setEffect(int next_type) {
	effect.next_type = next_type;
}

void PLAT_setEffectColor(int next_color) {
	effect.next_color = next_color;
}

void PLAT_vsync(int remaining) {
	if (remaining > 0) usleep(remaining * 1000);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	effect.next_scale = renderer->scale;
	return scale1x1_c16;
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w, vid.blit->true_h, vid.blit->src_p);
}

//////////////////////////////////////
// Screen Presentation & Rotation

void (*plat_custom_flip)(SDL_Surface* surface) = NULL;

static void renderCopy(SDL_Texture* texture, const SDL_Rect* src, const SDL_Rect* dst) {
	if (screen_rotation && !on_hdmi) {
		int screen_w = screen_width;
		int screen_h = screen_height;
		int oy = (screen_w - screen_h) / 2;
		int ox = -oy;
		SDL_Rect target = dst ? (SDL_Rect){ox + dst->x, oy + dst->y, dst->w, dst->h}
							  : (SDL_Rect){ox, oy, screen_w, screen_h};
		SDL_RenderCopyEx(vid.renderer, texture, src, &target, screen_rotation, NULL, SDL_FLIP_NONE);
	} else {
		SDL_RenderCopy(vid.renderer, texture, src, dst);
	}
}

static void flipUI(void) {
	int screen_w = getScreenWidth();
	int screen_h = getScreenHeight();
	resizeVideo(screen_w, screen_h, screen_w * FIXED_BPP);
	SDL_UpdateTexture(vid.texture, NULL, vid.screen->pixels, vid.screen->pitch);
	renderCopy(vid.texture, NULL, NULL);
	SDL_RenderPresent(vid.renderer);
}

static void flipGame(void) {
	SDL_UpdateTexture(vid.texture, NULL, vid.blit->src, vid.blit->src_p);

	SDL_Texture* target = vid.texture;
	int x = vid.blit->src_x;
	int y = vid.blit->src_y;
	int w = vid.blit->src_w;
	int h = vid.blit->src_h;

	if (vid.sharpness == SHARPNESS_CRISP) {
		SDL_SetRenderTarget(vid.renderer, vid.target);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL, NULL);
		SDL_SetRenderTarget(vid.renderer, NULL);
		x *= hard_scale;
		y *= hard_scale;
		w *= hard_scale;
		h *= hard_scale;
		target = vid.target;
	}

	int screen_w = getScreenWidth();
	int screen_h = getScreenHeight();
	SDL_Rect src_rect = {x, y, w, h};
	SDL_Rect dst_rect = {0, 0, screen_w, screen_h};
	PLAT_computeRendererRects(vid.blit, &src_rect, &dst_rect);

	renderCopy(target, &src_rect, &dst_rect);

	updateEffect();
	if (vid.blit && effect.type != EFFECT_NONE && vid.effect) {
		renderCopy(vid.effect, &(SDL_Rect){0, 0, dst_rect.w, dst_rect.h}, &dst_rect);
	}

	SDL_RenderPresent(vid.renderer);
	vid.blit = NULL;
}

void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
	if (plat_custom_flip && vid.screen) {
		plat_custom_flip(vid.screen);
		return;
	}

	on_hdmi = GetHDMI();
	if (!vid.blit)
		flipUI();
	else
		flipGame();
}

int PLAT_supportsOverscan(void) {
	return screen_aspect == MINIME_ASPECT_1x1;
}

//////////////////////////////////////
// Hardware Acceleration (EGL / OpenGL ES)

SDL_GLContext PLAT_initGLContext(int major, int minor, int gles) {
	if (vid.gl_ctx) {
		SDL_GL_MakeCurrent(vid.window, vid.gl_ctx);
		return vid.gl_ctx;
	}

	int profile = gles ? SDL_GL_CONTEXT_PROFILE_ES : SDL_GL_CONTEXT_PROFILE_CORE;
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major > 0 ? major : 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor >= 0 ? minor : 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	vid.gl_ctx = SDL_GL_CreateContext(vid.window);
	if (!vid.gl_ctx && gles && major > 2) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		vid.gl_ctx = SDL_GL_CreateContext(vid.window);
	}
	if (vid.gl_ctx) {
		SDL_GL_MakeCurrent(vid.window, vid.gl_ctx);
		SDL_GL_SetSwapInterval(0);
	}
	return vid.gl_ctx;
}

void PLAT_quitGLContext(void) {
	if (vid.gl_ctx) {
		SDL_GL_DeleteContext(vid.gl_ctx);
		vid.gl_ctx = NULL;
	}
}

void PLAT_swapGL(void) {
	if (vid.window) {
		SDL_GL_SwapWindow(vid.window);
	}
}

void* PLAT_getGLProcAddress(const char* proc) {
	return SDL_GL_GetProcAddress(proc);
}

//////////////////////////////////////
// UI Overlay

#define OVERLAY_WIDTH PILL_SIZE
#define OVERLAY_HEIGHT PILL_SIZE
#define OVERLAY_BPP 4
#define OVERLAY_DEPTH 16
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP)
#define OVERLAY_RGBA_MASK 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000
static struct OVL_Context {
	SDL_Surface* overlay;
} ovl;

SDL_Surface* PLAT_initOverlay(void) {
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH, OVERLAY_HEIGHT),
									   OVERLAY_DEPTH, OVERLAY_RGBA_MASK);
	return ovl.overlay;
}

void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}

void PLAT_enableOverlay(int enable) {
	(void)enable;
}

//////////////////////////////////////
// Wireless Networking (Wi-Fi & Bluetooth)

static int online = 0;
static int bt_up = 0;

int PLAT_hasWifi(void) {
	ensure_traits();
	return MINIME_traitAvailable(wifi_interface);
}

const char* PLAT_getWifiInterface(void) {
	ensure_traits();
	return PLAT_hasWifi() ? wifi_interface : "wlan0";
}

static void updateWifiStatus(void) {
	if (MINIME_traitAvailable(wifi_interface)) {
		char path[256];
		char status[16] = "";
		snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", wifi_interface);
		getFile(path, status, sizeof(status));
		online = prefixMatch("up", status);
	} else {
		online = 0;
	}
}

int PLAT_isOnline(void) {
	return online;
}

int PLAT_hasBluetooth(void) {
	ensure_traits();
	return MINIME_traitAvailable(bluetooth_interface);
}

static void updateBluetoothStatus(void) {
	if (MINIME_traitAvailable(bluetooth_interface)) {
		char path[256];
		snprintf(path, sizeof(path), "/sys/class/bluetooth/%s", bluetooth_interface);
		bt_up = (access(path, F_OK) == 0 && access("/run/bluetoothd.pid", F_OK) == 0);
	} else {
		bt_up = 0;
	}
}

int PLAT_isBluetoothUp(void) {
	return bt_up;
}

//////////////////////////////////////
// Power, Battery & Thermal Management

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	int i = GetBattery();
	if (!is_charging || !charge) return;

	*is_charging = GetCharging();

	if (i > 80)
		*charge = 100;
	else if (i > 60)
		*charge = 80;
	else if (i > 40)
		*charge = 60;
	else if (i > 20)
		*charge = 40;
	else if (i > 10)
		*charge = 20;
	else
		*charge = 10;

	updateWifiStatus();
	updateBluetoothStatus();
}

void PLAT_enableBacklight(int enable) {
	if (enable) {
		if (MINIME_traitAvailable(screen_blank_path))
			putInt(screen_blank_path, 0);
		SetBrightness(GetBrightness());
		if (MINIME_traitAvailable(power_led_path))
			putInt(power_led_path, 0);
	} else {
		if (MINIME_traitAvailable(screen_blank_path))
			putInt(screen_blank_path, 4);
		SetRawBrightness(0);
		if (MINIME_traitAvailable(power_led_path))
			putInt(power_led_path, 1);
	}
}

void PLAT_powerOff(void) {
	system("rm -f /tmp/minui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	if (MINIME_traitAvailable(power_led_path))
		putInt(power_led_path, 1);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	exit(0);
}

//////////////////////////////////////
// Haptics

void PLAT_setRumble(int strength) {
	if (GetHDMI())
		return;
	if (!MINIME_traitAvailable(input_rumble))
		return;

	int fd = MINIME_inputOpenByName(input_rumble);
	if (fd < 0)
		return;

	struct ff_effect effect;
	memset(&effect, 0, sizeof(effect));
	effect.type = FF_RUMBLE;
	effect.id = -1;
	if (strength > 0) {
		effect.u.rumble.strong_magnitude = 0xffff;
		effect.u.rumble.weak_magnitude = 0xffff;
	}
	if (ioctl(fd, EVIOCSFF, &effect) < 0 && strength > 0) {
		if (errno != ENODEV)
			close(fd);
		return;
	}
	close(fd);
}

//////////////////////////////////////
// Audio

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}
