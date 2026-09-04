#ifndef RENDER_VIEWPORT_H
#define RENDER_VIEWPORT_H

#include <stdbool.h>

typedef struct {
	int dst_x;
	int dst_y;
	int dst_w;
	int dst_h;
	int phys_w;
	int phys_h;
	int rotation;
	float quad_verts[16]; // 4 vertices: [x, y, u, v]
} render_viewport_t;

void RENDER_computeViewport(int src_w, int src_h, double aspect, int scale_mode,
							int screen_w, int screen_h, int rotation,
							bool bottom_left_origin, render_viewport_t* out_vp);

#endif // RENDER_VIEWPORT_H
