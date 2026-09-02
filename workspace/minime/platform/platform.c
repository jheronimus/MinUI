// minime platform
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <msettings.h>

#include "defines.h"
#include "api.h"
#include "platform.h"
#include "utils.h"
#include "scaler.h"
#include "traits.h"

///////////////////////////////
// Linux evdev definitions
// Extracted from <linux/input.h> to prevent naming collisions with MinUI's BTN_* enum.

#define EV_KEY 0x01 // Keyboard / gamepad button events
#define EV_ABS 0x03 // Absolute axis / thumbstick events
#define EV_SW 0x05	// Hardware switch events (clamshell lid)
#define SW_LID 0x00 // Clamshell lid switch code
#define SW_MAX 0x0f // Maximum switch code supported by kernel

#ifndef EVIOCGSW
#define EVIOCGSW(len) _IOC(_IOC_READ, 'E', 0x0b, len) // ioctl: query switch bitmask
#endif

struct input_event {
	struct timeval time;
	unsigned short type;
	unsigned short code;
	int value;
};

///////////////////////////////
// Platform Lifecycle & Device Traits

int plat_fixed_width = 640;
int plat_fixed_height = 480;
int plat_has_hdmi = 0;
int plat_main_row_count = 6;
int plat_padding = 10;
static int plat_screen_rotation = -1;
int on_hdmi = 0;
static int rotate = 0;
static const MinimeTraits* traits;

typedef struct {
	int code;
	int btn;
	int id;
} KeyMapping;

static KeyMapping key_map[] = {
	{-1, BTN_DPAD_UP, BTN_ID_DPAD_UP},
	{-1, BTN_DPAD_DOWN, BTN_ID_DPAD_DOWN},
	{-1, BTN_DPAD_LEFT, BTN_ID_DPAD_LEFT},
	{-1, BTN_DPAD_RIGHT, BTN_ID_DPAD_RIGHT},
	{-1, BTN_A, BTN_ID_A},
	{-1, BTN_B, BTN_ID_B},
	{-1, BTN_X, BTN_ID_X},
	{-1, BTN_Y, BTN_ID_Y},
	{-1, BTN_C, BTN_ID_C},
	{-1, BTN_Z, BTN_ID_Z},
	{-1, BTN_START, BTN_ID_START},
	{-1, BTN_SELECT, BTN_ID_SELECT},
	{-1, BTN_MENU, BTN_ID_MENU},
	{-1, BTN_L1, BTN_ID_L1},
	{-1, BTN_L2, BTN_ID_L2},
	{-1, BTN_L3, BTN_ID_L3},
	{-1, BTN_R1, BTN_ID_R1},
	{-1, BTN_R2, BTN_ID_R2},
	{-1, BTN_R3, BTN_ID_R3},
	{-1, BTN_PLUS, BTN_ID_PLUS},
	{-1, BTN_MINUS, BTN_ID_MINUS},
	{-1, BTN_POWER, BTN_ID_POWER},
};
#define KEY_MAP_COUNT (sizeof(key_map) / sizeof(key_map[0]))

static void load_traits(void) {
	if (MINIME_traitsInit() != 0) exit(1);
	traits = MINIME_traits();
	plat_fixed_width = traits->screen_width;
	plat_fixed_height = traits->screen_height;
	plat_screen_rotation = traits->screen_rotation;
	plat_has_hdmi = MINIME_traitAvailable(traits->gpu_hdmi_state_path);

	key_map[0].code = traits->key_up;
	key_map[1].code = traits->key_down;
	key_map[2].code = traits->key_left;
	key_map[3].code = traits->key_right;
	key_map[4].code = traits->key_a;
	key_map[5].code = traits->key_b;
	key_map[6].code = traits->key_x;
	key_map[7].code = traits->key_y;
	key_map[8].code = traits->key_c;
	key_map[9].code = traits->key_z;
	key_map[10].code = traits->key_start;
	key_map[11].code = traits->key_select;
	key_map[12].code = traits->key_menu;
	key_map[13].code = traits->key_l1;
	key_map[14].code = traits->key_l2;
	key_map[15].code = traits->key_l3;
	key_map[16].code = traits->key_r1;
	key_map[17].code = traits->key_r2;
	key_map[18].code = traits->key_r3;
	key_map[19].code = traits->key_vol_up;
	key_map[20].code = traits->key_vol_down;
	key_map[21].code = traits->key_power;

	// Derive layout properties from resolved traits.
	plat_padding = (plat_fixed_width >= 720) ? 40 : 10;
	plat_main_row_count = (plat_fixed_width >= 720) ? 8 : 6;
	if (plat_screen_rotation != -1) {
		rotate = plat_screen_rotation / 90;
	}
}

char* PLAT_getModel(void) {
	return traits ? (char*)traits->device_model : "Minime Handheld";
}

int PLAT_getScreenRotation(void) {
	if (!traits) load_traits();
	return (plat_screen_rotation > 0) ? plat_screen_rotation : 0;
}

int PLAT_hasUndervolt(void) {
	if (!traits) load_traits();
	return traits && traits->cpu_undervolt_supported > 0;
}

///////////////////////////////
// Input Handling & Gamepad

#define RAW_HATY 17
#define RAW_HATX 16

#define INPUT_COUNT 5
static int inputs[INPUT_COUNT];

#define kRawIndex 1
#define kVolumeIndex 2
#define kMenuIndex 3
#define kStickIndex 4

static void drainInputFd(int input);
static void drainAllInputs(void);

static void updateButtonState(int btn, int pressed, int id, uint32_t tick) {
	if (btn == BTN_NONE || id < 0 || id >= BTN_ID_COUNT) return;

	if (pressed) {
		if ((pad.is_pressed & btn) == BTN_NONE) {
			pad.just_pressed |= btn;
			pad.just_repeated |= btn;
			pad.is_pressed |= btn;
			pad.repeat_at[id] = tick + PAD_REPEAT_DELAY;
		}
	} else if (pad.is_pressed & btn) {
		pad.is_pressed &= ~btn;
		pad.just_released |= btn;
		pad.just_repeated &= ~btn;
	}
}

static void drainInputFd(int input) {
	struct input_event event;
	if (input < 0) return;
	while (read(input, &event, sizeof(event)) == sizeof(event)) {
	}
}

static void drainAllInputs(void) {
	for (int i = 0; i < INPUT_COUNT; i++) {
		drainInputFd(inputs[i]);
	}
}

void PLAT_initInput(void) {
	inputs[0] = MINIME_inputOpenByName(traits->input_power);
	inputs[kRawIndex] = MINIME_inputOpenByName(traits->input_gamepad);
	inputs[kVolumeIndex] = MINIME_inputOpenByName(traits->input_volume);
	inputs[kMenuIndex] = MINIME_inputOpenByName(traits->input_menu);
	inputs[kStickIndex] = MINIME_inputOpenByName(traits->input_stick);
	drainAllInputs();
}

void PLAT_quitInput(void) {
	for (int i = 0; i < INPUT_COUNT; i++) {
		if (inputs[i] >= 0) close(inputs[i]);
	}
}

static void handleAbsEvent(int code, int value, uint32_t tick) {
	if (code == RAW_HATY || code == RAW_HATX) {
		if (value > 1) return; // ignore repeats

		int hats[4] = {-1, -1, -1, -1}; // up, down, left, right
		if (code == RAW_HATY) {
			hats[0] = value == -1; // up
			hats[1] = value == 1;  // down
		} else if (code == RAW_HATX) {
			hats[2] = value == -1; // left
			hats[3] = value == 1;  // right
		}

		for (int id = 0; id < 4; id++) {
			int state = hats[id];
			if (state == -1) continue;
			int btn = 1 << id;
			updateButtonState(btn, state, id, tick);
		}
		return;
	}

	if (code == traits->axis_lx) {
		pad.laxis.x = MINIME_inputNormalizeAxis(value, traits->axis_lx_invert);
		PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x, tick + PAD_REPEAT_DELAY);
	} else if (code == traits->axis_ly) {
		pad.laxis.y = MINIME_inputNormalizeAxis(value, traits->axis_ly_invert);
		PAD_setAnalog(BTN_ID_ANALOG_UP, BTN_ID_ANALOG_DOWN, pad.laxis.y, tick + PAD_REPEAT_DELAY);
	} else if (code == traits->axis_rx) {
		pad.raxis.x = MINIME_inputNormalizeAxis(value, traits->axis_rx_invert);
	} else if (code == traits->axis_ry) {
		pad.raxis.y = MINIME_inputNormalizeAxis(value, traits->axis_ry_invert);
	}
}

static void handleKeyEvent(int code, int value, uint32_t tick) {
	if (value > 1) return; // ignore repeats
	int pressed = value;

	for (size_t i = 0; i < KEY_MAP_COUNT; i++) {
		if (key_map[i].code >= 0 && code == key_map[i].code) {
			updateButtonState(key_map[i].btn, pressed, key_map[i].id, tick);
			break;
		}
	}

	// On devices without a dedicated F/Menu button, SELECT doubles as MENU modifier
	if (code == traits->key_select && traits->key_menu < 0) {
		updateButtonState(BTN_MENU, pressed, BTN_ID_MENU, tick);
	}
}

void PLAT_pollInput(void) {
	// reset transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	for (int i = 0; i < BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick >= pad.repeat_at[i])) {
			pad.just_repeated |= btn;
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}

	int input;
	static struct input_event event;
	for (int i = 0; i < INPUT_COUNT; i++) {
		input = inputs[i];
		if (input < 0) continue;
		while (read(input, &event, sizeof(event)) == sizeof(event)) {
			if (event.type == EV_KEY) {
				handleKeyEvent(event.code, event.value, tick);
			} else if (event.type == EV_ABS) {
				handleAbsEvent(event.code, event.value, tick);
			}
		}
	}

	if (lid.has_lid && PLAT_lidChanged(NULL) && !lid.is_open) {
		PWR_requestLidAction();
	}
}

int PLAT_shouldWake(void) {
	int lid_open = 1;
	if (lid.has_lid && PLAT_lidChanged(&lid_open) && lid_open) return 1;

	int input;
	static struct input_event event;
	for (int i = 0; i < INPUT_COUNT; i++) {
		input = inputs[i];
		if (input < 0) continue;
		while (read(input, &event, sizeof(event)) == sizeof(event)) {
			if (event.type == EV_KEY && event.code == traits->key_power && event.value == 0) {
				if (lid.has_lid && !lid.is_open) return 0;
				return 1;
			}
		}
	}
	return 0;
}

int PLAT_is6Button(void) {
	if (!traits) load_traits();
	return (traits && traits->key_c >= 0 && traits->key_z >= 0);
}

int PLAT_hasMenuButton(void) {
	if (!traits) load_traits();
	return (traits && traits->key_menu >= 0);
}

int PLAT_hasL3(void) {
	if (!traits) load_traits();
	return (traits && traits->key_l3 >= 0);
}

int PLAT_hasR3(void) {
	if (!traits) load_traits();
	return (traits && traits->key_r3 >= 0);
}

int PLAT_hasLeftStick(void) {
	if (!traits) load_traits();
	return (traits && traits->axis_lx >= 0);
}

int PLAT_hasRightStick(void) {
	if (!traits) load_traits();
	return (traits && traits->axis_rx >= 0);
}

///////////////////////////////
// Clamshell Lid Sensor

static int lid_fd = -1;

int PLAT_hasLid(void) {
	if (!traits) load_traits();
	return traits && MINIME_traitAvailable(traits->input_lid);
}

void PLAT_initLid(void) {
	if (!traits) load_traits();
	lid_fd = MINIME_inputOpenByName(traits->input_lid);
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

///////////////////////////////
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
	return on_hdmi ? HDMI_WIDTH : plat_fixed_width;
}

static inline int getScreenHeight(void) {
	return on_hdmi ? HDMI_HEIGHT : plat_fixed_height;
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
	if (MINIME_videoHDMIConnected()) {
		w = HDMI_WIDTH;
		h = HDMI_HEIGHT;
		p = HDMI_PITCH;
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

static const char* getEffectPath(int type, int scale, int* opacity) {
	if (type == EFFECT_LINE) {
		*opacity = 128;
		if (scale < 3) return RES_PATH "/line-2.png";
		if (scale < 4) return RES_PATH "/line-3.png";
		if (scale < 5) return RES_PATH "/line-4.png";
		if (scale < 6) return RES_PATH "/line-5.png";
		if (scale < 8) return RES_PATH "/line-6.png";
		return RES_PATH "/line-8.png";
	}
	if (type == EFFECT_GRID) {
		if (scale < 3) {
			*opacity = 64;
			return RES_PATH "/grid-2.png";
		}
		if (scale < 4) {
			*opacity = 112;
			return RES_PATH "/grid-3.png";
		}
		if (scale < 5) {
			*opacity = 144;
			return RES_PATH "/grid-4.png";
		}
		if (scale < 6) {
			*opacity = 160;
			return RES_PATH "/grid-5.png";
		}
		if (scale < 8) {
			*opacity = 112;
			return RES_PATH "/grid-6.png";
		}
		if (scale < 11) {
			*opacity = 144;
			return RES_PATH "/grid-8.png";
		}
		*opacity = 136;
		return RES_PATH "/grid-11.png";
	}
	return NULL;
}

static void updateEffect(void) {
	if (effect.next_scale == effect.scale && effect.next_type == effect.type &&
		effect.next_color == effect.color)
		return;

	int live_scale = effect.scale;
	int live_color = effect.color;
	effect.scale = effect.next_scale;
	effect.type = effect.next_type;
	effect.color = effect.next_color;

	if (effect.type == EFFECT_NONE) return;
	if (effect.type == effect.live_type && effect.scale == live_scale && effect.color == live_color)
		return;

	int opacity = 128;
	const char* effect_path = getEffectPath(effect.type, effect.scale, &opacity);
	if (!effect_path) return;

	SDL_Surface* tmp = IMG_Load(effect_path);
	if (tmp) {
		if (effect.type == EFFECT_GRID && effect.color) {
			uint8_t r, g, b;
			rgb565_to_rgb888(effect.color, &r, &g, &b);

			uint32_t* pixels = (uint32_t*)tmp->pixels;
			int width = tmp->w;
			int height = tmp->h;
			for (int y = 0; y < height; ++y) {
				for (int x = 0; x < width; ++x) {
					uint32_t pixel = pixels[y * width + x];
					uint8_t a = (pixel >> 24) & 0xFF;
					pixels[y * width + x] = (a << 24) | (r << 16) | (g << 8) | b;
				}
			}
		}

		if (vid.effect) SDL_DestroyTexture(vid.effect);
		SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "0", SDL_HINT_OVERRIDE);
		vid.effect = SDL_CreateTextureFromSurface(vid.renderer, tmp);
		SDL_SetTextureBlendMode(vid.effect, SDL_BLENDMODE_BLEND);
		SDL_SetTextureAlphaMod(vid.effect, opacity);
		SDL_FreeSurface(tmp);

		effect.live_type = effect.type;
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

// Screen Presentation & Rotation

void (*plat_custom_flip)(SDL_Surface* surface) = NULL;

static void renderCopy(SDL_Texture* texture, const SDL_Rect* src, const SDL_Rect* dst) {
	if (rotate && !on_hdmi) {
		int screen_w = plat_fixed_width;
		int screen_h = plat_fixed_height;
		int oy = (screen_w - screen_h) / 2;
		int ox = -oy;
		SDL_Rect target = dst ? (SDL_Rect){ox + dst->x, oy + dst->y, dst->w, dst->h}
							  : (SDL_Rect){ox, oy, screen_w, screen_h};
		SDL_RenderCopyEx(vid.renderer, texture, src, &target, rotate * 90, NULL, SDL_FLIP_NONE);
	} else {
		SDL_RenderCopy(vid.renderer, texture, src, dst);
	}
}

static void flipUI(void) {
	int screen_w = getScreenWidth();
	int screen_h = getScreenHeight();
	resizeVideo(screen_w, screen_h, FIXED_PITCH);
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
	return traits && traits->screen_aspect == MINIME_ASPECT_1x1;
}

///////////////////////////////
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

///////////////////////////////
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

///////////////////////////////
// Wireless Networking (Wi-Fi & Bluetooth)

static int online = 0;
static int bt_up = 0;

int PLAT_hasWifi(void) {
	if (!traits) load_traits();
	return traits && traits->wifi_interface[0] && strcmp(traits->wifi_interface, "na") != 0;
}

const char* PLAT_getWifiInterface(void) {
	if (!traits) load_traits();
	return PLAT_hasWifi() ? traits->wifi_interface : "wlan0";
}

static void updateWifiStatus(void) {
	if (MINIME_traitAvailable(traits->wifi_interface)) {
		char path[256];
		char status[16] = "";
		snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", traits->wifi_interface);
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
	char path[256];
	if (!traits) load_traits();
	if (!traits || !traits->bluetooth_interface[0] || strcmp(traits->bluetooth_interface, "na") == 0)
		return 0;
	snprintf(path, sizeof(path), "/sys/class/bluetooth/%s", traits->bluetooth_interface);
	return access(path, F_OK) == 0;
}

static void updateBluetoothStatus(void) {
	if (MINIME_traitAvailable(traits->bluetooth_interface)) {
		char path[256];
		snprintf(path, sizeof(path), "/sys/class/bluetooth/%s", traits->bluetooth_interface);
		bt_up = (access(path, F_OK) == 0 && access("/run/bluetoothd.pid", F_OK) == 0);
	} else {
		bt_up = 0;
	}
}

int PLAT_isBluetoothUp(void) {
	return bt_up;
}

///////////////////////////////
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
		MINIME_videoBlank(0);
		SetBrightness(GetBrightness());
		MINIME_powerSetLED(0);
	} else {
		MINIME_videoBlank(1);
		SetRawBrightness(0);
		MINIME_powerSetLED(1);
	}
}

void PLAT_powerOff(void) {
	system("rm -f /tmp/minui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	MINIME_powerSetLED(1);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	exit(0);
}

void PLAT_setCPUSpeed(int speed) {
	MINIME_powerSetCPUSpeed(speed);
}

void PLAT_setRumble(int strength) {
	if (GetHDMI()) return;
	MINIME_powerSetRumble(strength ? 1 : 0);
}

///////////////////////////////
// Audio

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}
