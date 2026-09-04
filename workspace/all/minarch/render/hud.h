#ifndef RENDER_HUD_H
#define RENDER_HUD_H

#include <stdint.h>

void RENDER_blitBitmapTextRGB565(const char* text, int ox, int oy, uint16_t* data,
								 int stride, int width, int height);

void RENDER_blitBitmapTextRGBA(const char* text, int ox, int oy, uint32_t* data,
							   int stride, int width, int height);

#endif // RENDER_HUD_H
