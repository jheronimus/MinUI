#include "hud.h"
#include <string.h>

#define CHAR_WIDTH 5
#define CHAR_HEIGHT 9
#define LETTERSPACING 1

static const char* bitmap_font[] = {
	['0'] = " 111 1   11   11  111 1 111  11   11   1 111 ",
	['1'] = "   1  111    1    1    1    1    1    1    1 ",
	['2'] = " 111 1   1    1   1   1   1   1    1    11111",
	['3'] = " 111 1   1    1    1 111     1    11   1 111 ",
	['4'] = "1   11   11   11   11   11   111111    1    1",
	['5'] = "111111    1    1111     1    1    11   1 111 ",
	['6'] = " 111 1    1    1111 1   11   11   11   1 111 ",
	['7'] = "11111    1    1   1   1   1   1   1   1   1  ",
	['8'] = " 111 1   11   11   1 111 1   11   11   1 111 ",
	['9'] = " 111 1   11   11   1 1111    1    1 111 ",
	['.'] = "                               11   11  ",
	[','] = "                                 1    1   1 ",
	[' '] = "                                             ",
	['('] = "   1   1   1   1   1   1   1    1    1   ",
	[')'] = " 1    1    1    1    1    1    1   1   1   ",
	['/'] = "   1    1    1   1   1   1  1    1    1      ",
	['x'] = "          1   1 1 1   1   1 1   1 1          ",
	['%'] = "11  111 1    1    1   1    1   1 1  111  11  ",
};

static void resolve_offsets(int* ox, int* oy, int w, int h, int width, int height) {
	if (*ox < 0) *ox = width - w + *ox;
	if (*oy < 0) *oy = height - h + *oy;
	if (*ox < 1) *ox = 1;
	if (*oy < 1) *oy = 1;
	if (*ox + w >= width) *ox = width - w - 1;
	if (*oy + h >= height) *oy = height - h - 1;
}

void RENDER_blitBitmapTextRGB565(const char* text, int ox, int oy, uint16_t* data,
								 int stride, int width, int height) {
	if (!text || !data) return;
	int len = strlen(text);
	int w = ((CHAR_WIDTH + LETTERSPACING) * len) - 1;
	int h = CHAR_HEIGHT;
	resolve_offsets(&ox, &oy, w, h, width, height);

	data += oy * stride + ox;
	uint16_t* row = data - stride;
	memset(row - 1, 0, (w + 2) * 2);

	for (int y = 0; y < CHAR_HEIGHT; y++) {
		row = data + y * stride;
		memset(row - 1, 0, (w + 2) * 2);
		for (int i = 0; i < len; i++) {
			unsigned char ch = (unsigned char)text[i];
			const char* c = (ch < sizeof(bitmap_font) / sizeof(bitmap_font[0])) ? bitmap_font[ch] : NULL;
			if (!c) continue;
			for (int x = 0; x < CHAR_WIDTH; x++) {
				if (c[y * CHAR_WIDTH + x] == '1') *row = 0xffff;
				row++;
			}
			row += LETTERSPACING;
		}
	}
	row = data + CHAR_HEIGHT * stride;
	memset(row - 1, 0, (w + 2) * 2);
}

static void draw_border_row_rgba(uint32_t* row, int w) {
	for (int i = -1; i <= w; i++) row[i] = 0xFF000000;
}

static void draw_char_row_rgba(uint32_t* row, const char* text, int len, int y) {
	for (int i = 0; i < len; i++) {
		unsigned char ch = (unsigned char)text[i];
		const char* c = (ch < sizeof(bitmap_font) / sizeof(bitmap_font[0])) ? bitmap_font[ch] : NULL;
		if (!c) continue;
		for (int x = 0; x < CHAR_WIDTH; x++) {
			if (c[y * CHAR_WIDTH + x] == '1') {
				row[i * (CHAR_WIDTH + LETTERSPACING) + x] = 0xFFFFFFFF;
			}
		}
	}
}

void RENDER_blitBitmapTextRGBA(const char* text, int ox, int oy, uint32_t* data,
							   int stride, int width, int height) {
	if (!text || !data) return;
	int len = strlen(text);
	int w = ((CHAR_WIDTH + LETTERSPACING) * len) - 1;
	int h = CHAR_HEIGHT;
	resolve_offsets(&ox, &oy, w, h, width, height);

	data += oy * stride + ox;
	draw_border_row_rgba(data - stride, w);

	for (int y = 0; y < CHAR_HEIGHT; y++) {
		uint32_t* row = data + y * stride;
		draw_border_row_rgba(row, w);
		draw_char_row_rgba(row, text, len, y);
	}
	draw_border_row_rgba(data + CHAR_HEIGHT * stride, w);
}
