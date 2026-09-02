#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

//
//	arm NEON / C integer scalers for ARMv7 devices
//	args/	src :	src offset		address of top left corner
//		dst :	dst offset		address	of top left corner
//		sw  :	src width		pixels
//		sh  :	src height		pixels
//		sp  :	src pitch (stride)	bytes	if 0, (src width * [2|4]) is used
//		dw  :	dst width		pixels
//		dh  :	dst height		pixels
//		dp  :	dst pitch (stride)	bytes	if 0, (src width * [2|4] * multiplier) is used
//
//	** NOTE **
//	since 32bit aligned addresses need to be processed for NEON scalers,
//	x-offset and stride pixels must be even# in the case of 16bpp,
//	if odd#, then handled by the C scaler
//

static void dummy(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {}

// 
// C scalers for Trimui Model S and GKD Pixel
//
void scale1x_c16to32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } swl*=2; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<(sw/2); x++, dx+=2) {
			pix = s[x];
			dpix1 = 0xFF000000 | ((pix & 0xF800) << 8) | ((pix & 0x07E0) << 5) | ((pix & 0x001F) << 3);
			dpix2 = 0xFF000000 | ((pix & 0xF8000000) >> 8) | ((pix & 0x07E00000) >> 11) | ((pix & 0x001F0000) >> 13);			d[dx  ] = dpix1; d[dx+1] = dpix1;
			d[dx  ] = dpix1; d[dx+1] = dpix2;
		}
		if (sw&1) {
			uint16_t *s16 = (uint16_t*)s;
			uint16_t pix16 = s16[x*2];
			pix16 = 0xFF000000 | ((pix16 & 0xF800) << 8) | ((pix16 & 0x07E0) << 5) | ((pix16 & 0x001F) << 3);
			d[dx  ] = pix16; d[dx+1] = pix16;
		}
		dst = (uint8_t*)dst+dp;
	}
}
void scale2x_c16to32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } swl*=2; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<(sw/2); x++, dx+=4) {
			pix = s[x];
			dpix1 = 0xFF000000 | ((pix & 0xF800) << 8) | ((pix & 0x07E0) << 5) | ((pix & 0x001F) << 3);
			dpix2 = 0xFF000000 | ((pix & 0xF8000000) >> 8) | ((pix & 0x07E00000) >> 11) | ((pix & 0x001F0000) >> 13);			d[dx  ] = dpix1; d[dx+1] = dpix1;
			d[dx+2] = dpix2; d[dx+3] = dpix2;
		}
		if (sw&1) {
			uint16_t *s16 = (uint16_t*)s;
			uint16_t pix16 = s16[x*2];
			pix16 = 0xFF000000 | ((pix16 & 0xF800) << 8) | ((pix16 & 0x07E0) << 5) | ((pix16 & 0x001F) << 3);
			d[dx  ] = pix16; d[dx+1] = pix16;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		memcpy(dst, dstsrc, swl); dst = (uint8_t*)dst+dp;
	}
}

//
//	C scalers
//
void scale1x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*1; }
	if ((ymul == 1)&&(swl == sp)&&(sp == dp)) memcpy(dst, src, sp*sh);
	else {
		if (swl>dp) swl = dp;
		for (; sh>0; sh--, src=(uint8_t*)src+sp) {
			for (uint32_t i=ymul; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, src, swl);
		}
	}
}

void scale1x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale1x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale1x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale1x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale1x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*1; }
	if ((ymul == 1)&&(swl == sp)&&(sp == dp)) memcpy(dst, src, sp*sh);
	else {
		for (; sh>0; sh--, src=(uint8_t*)src+sp) {
			for (uint32_t i=ymul; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, src, swl);
		}
	}
}

void scale1x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale1x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale1x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale1x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale2x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } swl*=2; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<(sw/2); x++, dx+=2) {
			pix = s[x];
			dpix1=(pix & 0x0000FFFF)|(pix<<16);
			dpix2=(pix & 0xFFFF0000)|(pix>>16);
			d[dx] = dpix1; d[dx+1] = dpix2;
		}
		if (sw&1) {
			uint16_t *s16 = (uint16_t*)s;
			uint16_t pix16 = s16[x*2];
			d[dx] = pix16|(pix16<<16);
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale2x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale2x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale2x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale2x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale2x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } swl*=2; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<sw; x++, dx+=2) {
			pix = s[x];
			d[dx] = pix; d[dx+1] = pix;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale2x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale2x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale2x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale2x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale3x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } swl*=3; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<(sw/2); x++, dx+=3) {
			pix = s[x];
			dpix1=(pix & 0x0000FFFF)|(pix<<16);
			dpix2=(pix & 0xFFFF0000)|(pix>>16);
			d[dx] = dpix1; d[dx+1] = pix; d[dx+2] = dpix2;
		}
		if (sw&1) {
			uint16_t *s16 = (uint16_t*)s;
			uint16_t *d16 = (uint16_t*)d;
			uint16_t pix16 = s16[x*2];
			dpix1 = pix16|(pix16<<16);
			d[dx] = dpix1; d16[(dx+1)*2] = pix16;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale3x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale3x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale3x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale3x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale3x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } swl*=3; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<sw; x++, dx+=3) {
			pix = s[x];
			d[dx] = pix; d[dx+1] = pix; d[dx+2] = pix;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale3x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale3x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale3x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale3x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale4x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } swl*=4; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<(sw/2); x++, dx+=4) {
			pix = s[x];
			dpix1=(pix & 0x0000FFFF)|(pix<<16);
			dpix2=(pix & 0xFFFF0000)|(pix>>16);
			d[dx] = dpix1; d[dx+1] = dpix1; d[dx+2] = dpix2; d[dx+3] = dpix2;
		}
		if (sw&1) {
			uint16_t *s16 = (uint16_t*)s;
			uint16_t pix16 = s16[x*2];
			dpix1 = pix16|(pix16<<16);
			d[dx] = dpix1; d[dx+1] = dpix1;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale4x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale4x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale4x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale4x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale4x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } swl*=4; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<sw; x++, dx+=4) {
			pix = s[x];
			d[dx] = pix; d[dx+1] = pix; d[dx+2] = pix; d[dx+3] = pix;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale4x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale4x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale4x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale4x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale5x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } swl*=5; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<(sw/2); x++, dx+=5) {
			pix = s[x];
			dpix1=(pix & 0x0000FFFF)|(pix<<16);
			dpix2=(pix & 0xFFFF0000)|(pix>>16);
			d[dx] = dpix1; d[dx+1] = dpix1; d[dx+2] = pix; d[dx+3] = dpix2; d[dx+4] = dpix2;
		}
		if (sw&1) {
			uint16_t *s16 = (uint16_t*)s;
			uint16_t *d16 = (uint16_t*)d;
			uint16_t pix16 = s16[x*2];
			dpix1 = pix16|(pix16<<16);
			d[dx] = dpix1; d[dx+1] = dpix1; d16[(dx+2)*2] = pix16;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale5x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale5x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale5x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale5x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4); }
void scale5x5_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 5); }

void scale5x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } swl*=5; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<sw; x++, dx+=5) {
			pix = s[x];
			d[dx] = pix; d[dx+1] = pix; d[dx+2] = pix; d[dx+3] = pix; d[dx+4] = pix;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale5x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale5x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale5x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale5x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4); }
void scale5x5_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 5); }

void scale6x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } swl*=6; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<(sw/2); x++, dx+=6) {
			pix = s[x];
			dpix1=(pix & 0x0000FFFF)|(pix<<16);
			dpix2=(pix & 0xFFFF0000)|(pix>>16);
			d[dx] = dpix1; d[dx+1] = dpix1; d[dx+2] = dpix1; d[dx+3] = dpix2; d[dx+4] = dpix2; d[dx+5] = dpix2;
		}
		if (sw&1) {
			uint16_t *s16 = (uint16_t*)s;
			uint16_t pix16 = s16[x*2];
			dpix1 = pix16|(pix16<<16);
			d[dx] = dpix1; d[dx+1] = dpix1; d[dx+2] = dpix1;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale6x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale6x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale6x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale6x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4); }
void scale6x5_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 5); }
void scale6x6_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 6); }

void scale6x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } swl*=6; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<sw; x++, dx+=6) {
			pix = s[x];
			d[dx] = pix; d[dx+1] = pix; d[dx+2] = pix; d[dx+3] = pix; d[dx+4] = pix; d[dx+5] = pix;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale6x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale6x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale6x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale6x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4); }
void scale6x5_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 5); }
void scale6x6_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 6); }


void scaler_c16(uint32_t xmul, uint32_t ymul, void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	void (* const func[6][8])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = {
			{ &scale1x1_c16, &scale1x2_c16, &scale1x3_c16, &scale1x4_c16, &dummy, &dummy, &dummy, &dummy },
			{ &scale2x1_c16, &scale2x2_c16, &scale2x3_c16, &scale2x4_c16, &dummy, &dummy, &dummy, &dummy },
			{ &scale3x1_c16, &scale3x2_c16, &scale3x3_c16, &scale3x4_c16, &dummy, &dummy, &dummy, &dummy },
			{ &scale4x1_c16, &scale4x2_c16, &scale4x3_c16, &scale4x4_c16, &dummy, &dummy, &dummy, &dummy },
			{ &scale5x1_c16, &scale5x2_c16, &scale5x3_c16, &scale5x4_c16, &scale5x5_c16, &dummy, &dummy, &dummy },
			{ &scale6x1_c16, &scale6x2_c16, &scale6x3_c16, &scale6x4_c16, &scale6x5_c16, &scale6x6_c16, &dummy, &dummy }
		   };
	if ((--xmul < 6)&&(--ymul < 6)) func[xmul][ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scaler_c32(uint32_t xmul, uint32_t ymul, void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	void (* const func[6][8])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = {
			{ &scale1x1_c32, &scale1x2_c32, &scale1x3_c32, &scale1x4_c32, &dummy, &dummy, &dummy, &dummy },
			{ &scale2x1_c32, &scale2x2_c32, &scale2x3_c32, &scale2x4_c32, &dummy, &dummy, &dummy, &dummy },
			{ &scale3x1_c32, &scale3x2_c32, &scale3x3_c32, &scale3x4_c32, &dummy, &dummy, &dummy, &dummy },
			{ &scale4x1_c32, &scale4x2_c32, &scale4x3_c32, &scale4x4_c32, &dummy, &dummy, &dummy, &dummy },
			{ &scale5x1_c32, &scale5x2_c32, &scale5x3_c32, &scale5x4_c32, &scale5x5_c32, &dummy, &dummy, &dummy },
			{ &scale6x1_c32, &scale6x2_c32, &scale6x3_c32, &scale6x4_c32, &scale6x5_c32, &scale6x6_c32, &dummy, &dummy }
		   };
	if ((--xmul < 6)&&(--ymul < 6)) func[xmul][ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}


// from gambatte-dms
//from RGB565
#define cR(A) (((A) & 0xf800) >> 11)
#define cG(A) (((A) & 0x7e0) >> 5)
#define cB(A) ((A) & 0x1f)
//to RGB565
#define Weight2_3(A, B)  (((((cR(A) << 1) + (cR(B) * 3)) / 5) & 0x1f) << 11 | ((((cG(A) << 1) + (cG(B) * 3)) / 5) & 0x3f) << 5 | ((((cB(A) << 1) + (cB(B) * 3)) / 5) & 0x1f))
#define Weight3_1(A, B)  ((((cR(B) + (cR(A) * 3)) >> 2) & 0x1f) << 11 | (((cG(B) + (cG(A) * 3)) >> 2) & 0x3f) << 5 | (((cB(B) + (cB(A) * 3)) >> 2) & 0x1f))
#define Weight3_2(A, B)  (((((cR(B) << 1) + (cR(A) * 3)) / 5) & 0x1f) << 11 | ((((cG(B) << 1) + (cG(A) * 3)) / 5) & 0x3f) << 5 | ((((cB(B) << 1) + (cB(A) * 3)) / 5) & 0x1f))

#define MIN(a, b) (a) < (b) ? (a) : (b)
void scale1x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	// pitch of src image not src buffer!
	// eg. gb has a 160 pixel wide image but 
	// gambatte uses a 256 pixel wide buffer
	// (only matters when using memcpy) 
	int ip = sw * FIXED_BPP; 
	int src_stride = 2 * sp / FIXED_BPP;
	int dst_stride = 2 * dp / FIXED_BPP;
	int cpy_pitch = MIN(ip, dp);
	
	uint16_t k = 0x0000;
	uint16_t* restrict src_row = (uint16_t*)src;
	uint16_t* restrict dst_row = (uint16_t*)dst;
	for (int y=0; y<sh; y+=2) {
		memcpy(dst_row, src_row, cpy_pitch);
		dst_row += dst_stride;
		src_row += src_stride;
		for (unsigned x=0; x<sw; x++) {
			uint16_t s = *(src_row + x);
			*(dst_row + x) = Weight3_1(s, k);
		}
	}
}
void scale2x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	uint16_t k = 0x0000;
	for (unsigned y=0; y<sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 2;
		for (unsigned x=0; x<sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2( c1, k);
			
			*(dst_row     ) = c1;
			*(dst_row + 1 ) = c1;
			
			*(dst_row + dw    ) = c2;
			*(dst_row + dw + 1) = c2;
			
			src_row += 1;
			dst_row += 2;
		}
	}
}
void scale3x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	uint16_t k = 0x0000;
	for (unsigned y=0; y<sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 3;
		for (unsigned x=0; x<sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2( c1, k);
			
			// row 1
			*(dst_row             ) = c2;
			*(dst_row          + 1) = c2;
			*(dst_row          + 2) = c2;

			// row 2
			*(dst_row + dw * 1    ) = c1;
			*(dst_row + dw * 1 + 1) = c1;
			*(dst_row + dw * 1 + 2) = c1;

			// row 3
			*(dst_row + dw * 2    ) = c1;
			*(dst_row + dw * 2 + 1) = c1;
			*(dst_row + dw * 2 + 2) = c1;

			src_row += 1;
			dst_row += 3;
		}
	}
}
void scale4x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	int row3 = dw * 2;
	int row4 = dw * 3;
	uint16_t k = 0x0000;
	for (unsigned y=0; y<sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 4;
		for (unsigned x=0; x<sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2( c1, k);
			
			// row 1
			*(dst_row    ) = c1;
			*(dst_row + 1) = c1;
			*(dst_row + 2) = c1;
			*(dst_row + 3) = c1;
			
			// row 2
			*(dst_row + dw    ) = c2;
			*(dst_row + dw + 1) = c2;
			*(dst_row + dw + 2) = c2;
			*(dst_row + dw + 3) = c2;

			// row 3
			*(dst_row + row3    ) = c1;
			*(dst_row + row3 + 1) = c1;
			*(dst_row + row3 + 2) = c1;
			*(dst_row + row3 + 3) = c1;

			// row 4
			*(dst_row + row4    ) = c2;
			*(dst_row + row4 + 1) = c2;
			*(dst_row + row4 + 2) = c2;
			*(dst_row + row4 + 3) = c2;

			src_row += 1;
			dst_row += 4;
		}
	}
}

void scale2x_grid(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	uint16_t k = 0x0000;
	for (unsigned y=0; y<sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 2;
		for (unsigned x=0; x<sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_1( c1, k);
			
			*(dst_row     ) = c2;
			*(dst_row + 1 ) = c2;
			
			*(dst_row + dw    ) = c2;
			*(dst_row + dw + 1) = c1;
			
			src_row += 1;
			dst_row += 2;
		}
	}
}
void scale3x_grid(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	uint16_t k = 0x0000;
	for (unsigned y=0; y<sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 3;
		for (unsigned x=0; x<sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2( c1, k);
			uint16_t c3 = Weight2_3( c1, k);
			
			// row 1
			*(dst_row                       ) = c2;
			*(dst_row                    + 1) = c1;
			*(dst_row                    + 2) = c1;

			// row 2
			*(dst_row + dw * 1    ) = c2;
			*(dst_row + dw * 1 + 1) = c1;
			*(dst_row + dw * 1 + 2) = c1;

			// row 3
			*(dst_row + dw * 2    ) = c3;
			*(dst_row + dw * 2 + 1) = c2;
			*(dst_row + dw * 2 + 2) = c2;

			src_row += 1;
			dst_row += 3;
		}
	}
}