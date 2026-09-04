#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "hud.h"
#include "libretro.h"
#include "render.h"
#include "utils.h"
#include "viewport.h"

static struct retro_hw_render_callback hw_render;
static int hw_render_enabled = 0;
static GLuint hw_fbo = 0;
static GLuint hw_fbo_tex = 0;
static GLuint hw_fbo_depth = 0;
static unsigned hw_fbo_w = 0;
static unsigned hw_fbo_h = 0;

static GLuint comp_prog = 0;
static GLuint comp_vbo = 0;
static GLint comp_u_tex = -1;
static GLint comp_u_sharpness = -1;
static GLint comp_u_tex_size = -1;
static GLint comp_u_out_size = -1;
static GLint comp_u_effect = -1;
static GLint comp_a_pos = -1;
static GLint comp_a_texcoord = -1;

static GLuint menu_prog = 0;
static GLuint menu_vbo = 0;
static GLuint menu_tex = 0;
static GLint menu_u_tex = -1;
static GLint menu_a_pos = -1;
static GLint menu_a_texcoord = -1;

static GLuint hud_vbo = 0;
static GLuint hw_hud_tex = 0;
static SDL_Surface* hw_hud_surf = NULL;

static int dev_w = 640;
static int dev_h = 480;
static int gl_scaling_mode = 1; // SCALE_ASPECT
static int gl_sharpness = 0;
static int gl_effect = 0;
static double gl_aspect = 0.0;
static double gl_fps = 0.0;
static double gl_cpu = 0.0;
static double gl_use = 0.0;
static int gl_show_debug = 0;

static uintptr_t gl_get_current_framebuffer(void) {
	return (uintptr_t)hw_fbo;
}

static GLuint compile_shader(GLenum type, const char* src) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);
	GLint ok = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		LOG_error("gl: compile_shader failed: %s\n", log);
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

static GLuint create_program(const char* vsrc, const char* fsrc) {
	GLuint vs = compile_shader(GL_VERTEX_SHADER, vsrc);
	GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fsrc);
	if (!vs || !fs) return 0;

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint ok = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetProgramInfoLog(prog, sizeof(log), NULL, log);
		LOG_error("gl: create_program link failed: %s\n", log);
		glDeleteProgram(prog);
		return 0;
	}
	return prog;
}

static const char* common_vsrc =
	"#version 100\n"
	"precision mediump float;\n"
	"attribute vec2 a_pos;\n"
	"attribute vec2 a_texcoord;\n"
	"varying vec2 v_texcoord;\n"
	"void main() {\n"
	"  gl_Position = vec4(a_pos, 0.0, 1.0);\n"
	"  v_texcoord = a_texcoord;\n"
	"}\n";

static const char* comp_fsrc =
	"#version 100\n"
	"precision mediump float;\n"
	"varying vec2 v_texcoord;\n"
	"uniform sampler2D u_tex;\n"
	"uniform int u_sharpness;\n"
	"uniform vec2 u_tex_size;\n"
	"uniform vec2 u_out_size;\n"
	"uniform int u_effect;\n"
	"void main() {\n"
	"  vec4 color;\n"
	"  if (u_sharpness == 1) {\n"
	"    vec2 p = v_texcoord * u_tex_size - 0.5;\n"
	"    vec2 i = floor(p);\n"
	"    vec2 f = p - i;\n"
	"    vec2 f2 = f * f;\n"
	"    vec2 f3 = f2 * f;\n"
	"    vec2 w0 = f2 - 0.5 * (f3 + f);\n"
	"    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;\n"
	"    vec2 tc = (i + 0.5 + f) / u_tex_size;\n"
	"    color = texture2D(u_tex, tc);\n"
	"  } else {\n"
	"    color = texture2D(u_tex, v_texcoord);\n"
	"  }\n"
	"  if (u_effect == 1) {\n"
	"    if (mod(gl_FragCoord.y, 2.0) < 1.0) color.rgb *= 0.7;\n"
	"  } else if (u_effect == 2) {\n"
	"    vec2 grid_val = mod(gl_FragCoord.xy, 2.0);\n"
	"    if (grid_val.x < 1.0 || grid_val.y < 1.0) color.rgb *= 0.8;\n"
	"  }\n"
	"  gl_FragColor = color;\n"
	"}\n";

static const char* menu_fsrc =
	"#version 100\n"
	"precision mediump float;\n"
	"varying vec2 v_texcoord;\n"
	"uniform sampler2D u_tex;\n"
	"void main() {\n"
	"  gl_FragColor = texture2D(u_tex, v_texcoord);\n"
	"}\n";

static void init_compositor(void) {
	if (comp_prog) return;

	comp_prog = create_program(common_vsrc, comp_fsrc);
	if (comp_prog) {
		comp_u_tex = glGetUniformLocation(comp_prog, "u_tex");
		comp_u_sharpness = glGetUniformLocation(comp_prog, "u_sharpness");
		comp_u_tex_size = glGetUniformLocation(comp_prog, "u_tex_size");
		comp_u_out_size = glGetUniformLocation(comp_prog, "u_out_size");
		comp_u_effect = glGetUniformLocation(comp_prog, "u_effect");
		comp_a_pos = glGetAttribLocation(comp_prog, "a_pos");
		comp_a_texcoord = glGetAttribLocation(comp_prog, "a_texcoord");
		glGenBuffers(1, &comp_vbo);
		glGenBuffers(1, &hud_vbo);
	}

	menu_prog = create_program(common_vsrc, menu_fsrc);
	if (menu_prog) {
		menu_u_tex = glGetUniformLocation(menu_prog, "u_tex");
		menu_a_pos = glGetAttribLocation(menu_prog, "a_pos");
		menu_a_texcoord = glGetAttribLocation(menu_prog, "a_texcoord");
		glGenBuffers(1, &menu_vbo);
		glGenTextures(1, &menu_tex);
		glBindTexture(GL_TEXTURE_2D, menu_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
}

static void resize_fbo_attachments(unsigned width, unsigned height) {
	glBindTexture(GL_TEXTURE_2D, hw_fbo_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	if (hw_fbo_depth) {
		glBindRenderbuffer(GL_RENDERBUFFER, hw_fbo_depth);
		GLenum depth_fmt = hw_render.stencil ? GL_DEPTH24_STENCIL8_OES : GL_DEPTH_COMPONENT16;
		glRenderbufferStorage(GL_RENDERBUFFER, depth_fmt, width, height);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, hw_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hw_fbo_tex, 0);
	if (hw_fbo_depth) {
		if (hw_render.depth) {
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, hw_fbo_depth);
		}
		if (hw_render.stencil) {
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, hw_fbo_depth);
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void resize_fbo(unsigned width, unsigned height) {
	if (width == 0 || height == 0) return;
	if (hw_fbo && hw_fbo_w == width && hw_fbo_h == height) return;

	hw_fbo_w = width;
	hw_fbo_h = height;

	if (!hw_fbo) {
		glGenFramebuffers(1, &hw_fbo);
		glGenTextures(1, &hw_fbo_tex);
		if (hw_render.depth || hw_render.stencil) {
			glGenRenderbuffers(1, &hw_fbo_depth);
		}
	}
	resize_fbo_attachments(width, height);
}

static void draw_hud_text(void) {
	static double prev_fps = -1.0, prev_cpu = -1.0, prev_use = -1.0;
	static unsigned prev_fw = 0, prev_fh = 0;
	int hud_changed = (gl_fps != prev_fps || gl_cpu != prev_cpu || gl_use != prev_use ||
					   hw_fbo_w != prev_fw || hw_fbo_h != prev_fh);
	if (!hud_changed) return;

	prev_fps = gl_fps;
	prev_cpu = gl_cpu;
	prev_use = gl_use;
	prev_fw = hw_fbo_w;
	prev_fh = hw_fbo_h;

	memset(hw_hud_surf->pixels, 0, hw_hud_surf->pitch * hw_hud_surf->h);
	char buf[128];

	sprintf(buf, "%ux%u 1x", hw_fbo_w, hw_fbo_h);
	RENDER_blitBitmapTextRGBA(buf, 2, 2, (uint32_t*)hw_hud_surf->pixels,
							  hw_hud_surf->pitch / 4, dev_w, dev_h);

	sprintf(buf, "0,0 %ux%u", hw_fbo_w, hw_fbo_h);
	RENDER_blitBitmapTextRGBA(buf, -2, 2, (uint32_t*)hw_hud_surf->pixels,
							  hw_hud_surf->pitch / 4, dev_w, dev_h);

	sprintf(buf, "%.01f/%.01f %i%%", gl_fps, gl_cpu, (int)gl_use);
	RENDER_blitBitmapTextRGBA(buf, 2, -2, (uint32_t*)hw_hud_surf->pixels,
							  hw_hud_surf->pitch / 4, dev_w, dev_h);

	sprintf(buf, "%ix%i", dev_w, dev_h);
	RENDER_blitBitmapTextRGBA(buf, -2, -2, (uint32_t*)hw_hud_surf->pixels,
							  hw_hud_surf->pitch / 4, dev_w, dev_h);

	glBindTexture(GL_TEXTURE_2D, hw_hud_tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, dev_w, dev_h, GL_RGBA, GL_UNSIGNED_BYTE, hw_hud_surf->pixels);
}

static void draw_hud(void) {
	if (!gl_show_debug) return;

	if (!hw_hud_surf) {
		hw_hud_surf = SDL_CreateRGBSurfaceWithFormat(0, dev_w, dev_h, 32, SDL_PIXELFORMAT_RGBA32);
		if (!hw_hud_surf) return;
	}

	if (!hw_hud_tex) {
		glGenTextures(1, &hw_hud_tex);
		glBindTexture(GL_TEXTURE_2D, hw_hud_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dev_w, dev_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	}

	draw_hud_text();

	render_viewport_t vp;
	RENDER_computeViewport(dev_w, dev_h, (double)dev_w / (double)dev_h, 2, dev_w, dev_h,
						   PLAT_getScreenRotation(), false, &vp);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GLuint prog = menu_prog ? menu_prog : comp_prog;
	glUseProgram(prog);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hw_hud_tex);
	if (menu_prog) {
		glUniform1i(menu_u_tex, 0);
	}

	GLint pos_attr = menu_prog ? menu_a_pos : comp_a_pos;
	GLint uv_attr = menu_prog ? menu_a_texcoord : comp_a_texcoord;

	glBindBuffer(GL_ARRAY_BUFFER, hud_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vp.quad_verts), vp.quad_verts, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(pos_attr);
	glVertexAttribPointer(pos_attr, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(uv_attr);
	glVertexAttribPointer(uv_attr, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(pos_attr);
	glDisableVertexAttribArray(uv_attr);
	glDisable(GL_BLEND);
}

// Backend interface implementations

static bool gl_init(int screen_w, int screen_h) {
	dev_w = screen_w;
	dev_h = screen_h;
	return true;
}

static bool gl_handle_environ(unsigned cmd, void* data) {
	if (cmd != RETRO_ENVIRONMENT_SET_HW_RENDER) return false;

	struct retro_hw_render_callback* cb = (struct retro_hw_render_callback*)data;
	if (!cb) return false;

	if (cb->context_type != RETRO_HW_CONTEXT_OPENGLES2 &&
		cb->context_type != RETRO_HW_CONTEXT_OPENGLES3 &&
		cb->context_type != RETRO_HW_CONTEXT_OPENGL &&
		cb->context_type != RETRO_HW_CONTEXT_OPENGL_CORE) {
		LOG_info("gl: unsupported context type %u\n", cb->context_type);
		return false;
	}

	memcpy(&hw_render, cb, sizeof(hw_render));
	hw_render.get_proc_address = (retro_hw_get_proc_address_t)PLAT_getGLProcAddress;
	hw_render.get_current_framebuffer = gl_get_current_framebuffer;
	cb->get_proc_address = hw_render.get_proc_address;
	cb->get_current_framebuffer = hw_render.get_current_framebuffer;
	hw_render_enabled = 1;

	LOG_info("gl: HW render enabled type=%u v%u.%u depth=%d stencil=%d\n",
			 cb->context_type, cb->version_major, cb->version_minor, cb->depth, cb->stencil);
	return true;
}

static void gl_post_core_load(unsigned initial_w, unsigned initial_h) {
	if (!hw_render_enabled) return;

	int gles = (hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES2 ||
				hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES3);
	int major = hw_render.version_major ? hw_render.version_major : 3;

	LOG_info("gl: calling PLAT_initGLContext\n");
	PLAT_initGLContext(major, hw_render.version_minor, gles);

	LOG_info("gl: initializing compositor\n");
	init_compositor();

	unsigned w = initial_w ? initial_w : 640;
	unsigned h = initial_h ? initial_h : 480;
	LOG_info("gl: resizing FBO to %ux%u\n", w, h);
	resize_fbo(w, h);

	glBindFramebuffer(GL_FRAMEBUFFER, hw_fbo);
	glViewport(0, 0, w, h);
	if (hw_render.context_reset) {
		LOG_info("gl: calling core context_reset\n");
		hw_render.context_reset();
	}
	plat_custom_flip = RENDER_draw_menu;
}

static void gl_video_refresh(const void* data, unsigned width, unsigned height, size_t pitch) {
	(void)data;
	(void)pitch;
	if (!hw_render_enabled) return;

	resize_fbo(width, height);

	render_viewport_t vp;
	RENDER_computeViewport((int)width, (int)height, gl_aspect, gl_scaling_mode,
						   dev_w, dev_h, PLAT_getScreenRotation(),
						   hw_render.bottom_left_origin, &vp);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, vp.phys_w, vp.phys_h);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(comp_prog);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hw_fbo_tex);
	glUniform1i(comp_u_tex, 0);
	glUniform1i(comp_u_sharpness, gl_sharpness);
	glUniform2f(comp_u_tex_size, (float)width, (float)height);
	glUniform2f(comp_u_out_size, (float)vp.dst_w, (float)vp.dst_h);
	glUniform1i(comp_u_effect, gl_effect);

	glBindBuffer(GL_ARRAY_BUFFER, comp_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vp.quad_verts), vp.quad_verts, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(comp_a_pos);
	glVertexAttribPointer(comp_a_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(comp_a_texcoord);
	glVertexAttribPointer(comp_a_texcoord, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(comp_a_pos);
	glDisableVertexAttribArray(comp_a_texcoord);

	draw_hud();
	PLAT_swapGL();
}

static void gl_draw_menu(SDL_Surface* surface) {
	if (!surface || !menu_prog || !menu_tex || !menu_vbo) return;

	render_viewport_t vp;
	RENDER_computeViewport(dev_w, dev_h, (double)dev_w / (double)dev_h, 2, dev_w, dev_h,
						   PLAT_getScreenRotation(), false, &vp);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, vp.phys_w, vp.phys_h);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(menu_prog);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, menu_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, surface->w, surface->h, 0, GL_RGB,
				 GL_UNSIGNED_SHORT_5_6_5, surface->pixels);
	glUniform1i(menu_u_tex, 0);

	glBindBuffer(GL_ARRAY_BUFFER, menu_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vp.quad_verts), vp.quad_verts, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(menu_a_pos);
	glVertexAttribPointer(menu_a_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(menu_a_texcoord);
	glVertexAttribPointer(menu_a_texcoord, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(menu_a_pos);
	glDisableVertexAttribArray(menu_a_texcoord);

	PLAT_swapGL();
}

static void gl_post_menu(void) {
	if (hw_render_enabled) {
		PLAT_initGLContext(3, 0, 1);
		glBindFramebuffer(GL_FRAMEBUFFER, hw_fbo);
		glViewport(0, 0, hw_fbo_w, hw_fbo_h);
	}
}

static SDL_Surface* gl_capture_surface(void) {
	if (!hw_fbo || hw_fbo_w == 0 || hw_fbo_h == 0) return NULL;
	SDL_Surface* s = SDL_CreateRGBSurface(SDL_SWSURFACE, hw_fbo_w, hw_fbo_h, FIXED_DEPTH, RGBA_MASK_565);
	if (!s) return NULL;

	uint32_t* rgba = malloc(hw_fbo_w * hw_fbo_h * 4);
	if (!rgba) {
		SDL_FreeSurface(s);
		return NULL;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, hw_fbo);
	glReadPixels(0, 0, hw_fbo_w, hw_fbo_h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	uint16_t* dst = (uint16_t*)s->pixels;
	for (int y = 0; y < (int)hw_fbo_h; y++) {
		int src_y = hw_render.bottom_left_origin ? ((int)hw_fbo_h - 1 - y) : y;
		uint32_t* src_row = rgba + (src_y * hw_fbo_w);
		uint16_t* dst_row = dst + (y * (s->pitch / 2));
		for (int x = 0; x < (int)hw_fbo_w; x++) {
			uint32_t px = src_row[x];
			uint8_t r = px & 0xFF;
			uint8_t g = (px >> 8) & 0xFF;
			uint8_t b = (px >> 16) & 0xFF;
			dst_row[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
		}
	}

	free(rgba);
	return s;
}

static void gl_set_scaling(int mode) {
	gl_scaling_mode = mode;
}

static void gl_set_sharpness(int sharpness) {
	gl_sharpness = sharpness;
}

static void gl_set_effect(int effect) {
	gl_effect = effect;
}

static void gl_set_aspect(double aspect) {
	gl_aspect = aspect;
}

static void gl_update_debug(double fps, double cpu, double use, int show_debug) {
	gl_fps = fps;
	gl_cpu = cpu;
	gl_use = use;
	gl_show_debug = show_debug;
}

static void gl_destroy_hud_resources(void) {
	if (hw_hud_surf) {
		SDL_FreeSurface(hw_hud_surf);
		hw_hud_surf = NULL;
	}
	if (hw_hud_tex) {
		glDeleteTextures(1, &hw_hud_tex);
		hw_hud_tex = 0;
	}
	if (hud_vbo) {
		glDeleteBuffers(1, &hud_vbo);
		hud_vbo = 0;
	}
}

static void gl_destroy_fbo_resources(void) {
	if (hw_fbo) {
		glDeleteFramebuffers(1, &hw_fbo);
		hw_fbo = 0;
	}
	if (hw_fbo_tex) {
		glDeleteTextures(1, &hw_fbo_tex);
		hw_fbo_tex = 0;
	}
	if (hw_fbo_depth) {
		glDeleteRenderbuffers(1, &hw_fbo_depth);
		hw_fbo_depth = 0;
	}
	hw_fbo_w = 0;
	hw_fbo_h = 0;
}

static void gl_destroy_compositor_resources(void) {
	if (comp_prog) {
		glDeleteProgram(comp_prog);
		comp_prog = 0;
	}
	if (comp_vbo) {
		glDeleteBuffers(1, &comp_vbo);
		comp_vbo = 0;
	}
	if (menu_prog) {
		glDeleteProgram(menu_prog);
		menu_prog = 0;
	}
	if (menu_vbo) {
		glDeleteBuffers(1, &menu_vbo);
		menu_vbo = 0;
	}
	if (menu_tex) {
		glDeleteTextures(1, &menu_tex);
		menu_tex = 0;
	}
}

static void gl_quit(void) {
	if (hw_render_enabled && hw_render.context_destroy) {
		hw_render.context_destroy();
	}
	plat_custom_flip = NULL;

	gl_destroy_hud_resources();
	gl_destroy_fbo_resources();
	gl_destroy_compositor_resources();

	PLAT_quitGLContext();
	hw_render_enabled = 0;
}

render_backend_ops_t render_gl_ops = {
	.type = RENDER_BACKEND_GL,
	.name = "opengl",
	.init = gl_init,
	.handle_environ = gl_handle_environ,
	.post_core_load = gl_post_core_load,
	.video_refresh = gl_video_refresh,
	.draw_menu = gl_draw_menu,
	.post_menu = gl_post_menu,
	.flip = NULL,
	.capture_surface = gl_capture_surface,
	.set_scaling = gl_set_scaling,
	.set_sharpness = gl_set_sharpness,
	.set_effect = gl_set_effect,
	.set_aspect = gl_set_aspect,
	.update_debug = gl_update_debug,
	.quit = gl_quit,
};
