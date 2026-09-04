#include "viewport.h"
#include <math.h>

enum {
	SCALE_NATIVE = 0,
	SCALE_ASPECT,
	SCALE_FULLSCREEN,
	SCALE_CROPPED,
	SCALE_COUNT,
};

static void compute_rect(int src_w, int src_h, double aspect, int scale_mode,
						 int screen_w, int screen_h, int* out_x, int* out_y,
						 int* out_w, int* out_h) {
	int dst_x = 0;
	int dst_y = 0;
	int dst_w = screen_w;
	int dst_h = screen_h;

	if (scale_mode == SCALE_ASPECT) {
		dst_h = screen_h;
		dst_w = (int)(dst_h * aspect);
		if (dst_w > screen_w) {
			dst_w = screen_w;
			dst_h = (int)(dst_w / aspect);
		}
		dst_x = (screen_w - dst_w) / 2;
		dst_y = (screen_h - dst_h) / 2;
	} else if (scale_mode == SCALE_NATIVE) {
		dst_w = (src_w > 0) ? src_w : screen_w;
		dst_h = (src_h > 0) ? src_h : screen_h;
		dst_x = (screen_w - dst_w) / 2;
		dst_y = (screen_h - dst_h) / 2;
	} else if (scale_mode == SCALE_CROPPED) {
		dst_w = screen_w;
		dst_h = (int)(dst_w / aspect);
		if (dst_h < screen_h) {
			dst_h = screen_h;
			dst_w = (int)(dst_h * aspect);
		}
		dst_x = (screen_w - dst_w) / 2;
		dst_y = (screen_h - dst_h) / 2;
	}

	*out_x = dst_x;
	*out_y = dst_y;
	*out_w = dst_w;
	*out_h = dst_h;
}

#define ROT_X(lx, ly, rot)                     \
	((rot == 90) ? (ly) : (rot == 180) ? -(lx) \
					  : (rot == 270)   ? -(ly) \
									   : (lx))
#define ROT_Y(lx, ly, rot)                      \
	((rot == 90) ? -(lx) : (rot == 180) ? -(ly) \
					   : (rot == 270)	? (lx)  \
										: (ly))

static void compute_quad(int dst_x, int dst_y, int dst_w, int dst_h,
						 int screen_w, int screen_h, int rotation,
						 bool bottom_left_origin, float quad[16]) {
	float x0 = (float)dst_x / (float)screen_w * 2.0f - 1.0f;
	float x1 = (float)(dst_x + dst_w) / (float)screen_w * 2.0f - 1.0f;
	float y0 = 1.0f - (float)(dst_y + dst_h) / (float)screen_h * 2.0f;
	float y1 = 1.0f - (float)dst_y / (float)screen_h * 2.0f;

	float v0 = bottom_left_origin ? 0.0f : 1.0f;
	float v1 = bottom_left_origin ? 1.0f : 0.0f;

	quad[0] = ROT_X(x0, y0, rotation);
	quad[1] = ROT_Y(x0, y0, rotation);
	quad[2] = 0.0f;
	quad[3] = v0;

	quad[4] = ROT_X(x1, y0, rotation);
	quad[5] = ROT_Y(x1, y0, rotation);
	quad[6] = 1.0f;
	quad[7] = v0;

	quad[8] = ROT_X(x0, y1, rotation);
	quad[9] = ROT_Y(x0, y1, rotation);
	quad[10] = 0.0f;
	quad[11] = v1;

	quad[12] = ROT_X(x1, y1, rotation);
	quad[13] = ROT_Y(x1, y1, rotation);
	quad[14] = 1.0f;
	quad[15] = v1;
}

static double sanitize_aspect(int src_w, int src_h, double aspect, int screen_w, int screen_h) {
	if (aspect > 0.0) return aspect;
	if (src_w > 0 && src_h > 0) return (double)src_w / (double)src_h;
	return (double)screen_w / (double)screen_h;
}

void RENDER_computeViewport(int src_w, int src_h, double aspect, int scale_mode,
							int screen_w, int screen_h, int rotation,
							bool bottom_left_origin, render_viewport_t* out_vp) {
	if (!out_vp) return;
	if (screen_w <= 0 || screen_h <= 0) return;

	aspect = sanitize_aspect(src_w, src_h, aspect, screen_w, screen_h);
	compute_rect(src_w, src_h, aspect, scale_mode, screen_w, screen_h,
				 &out_vp->dst_x, &out_vp->dst_y, &out_vp->dst_w, &out_vp->dst_h);

	bool rotated = (rotation == 90 || rotation == 270);
	out_vp->rotation = rotation;
	out_vp->phys_w = rotated ? screen_h : screen_w;
	out_vp->phys_h = rotated ? screen_w : screen_h;

	compute_quad(out_vp->dst_x, out_vp->dst_y, out_vp->dst_w, out_vp->dst_h,
				 screen_w, screen_h, rotation, bottom_left_origin, out_vp->quad_verts);
}
