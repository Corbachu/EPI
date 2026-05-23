//------------------------------------------------------------------------
//  Basic image storage
//------------------------------------------------------------------------
//
//  Copyright (c) 2003-2008  The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include "epi.h"

#include "image_data.h"
#include "tables.h"

namespace epi
{

// hack?
#ifdef __OpenBSD__
// Use C++11 nullptr; legacy macro removed
#endif

image_data_c::image_data_c(int _w, int _h, int _bpp) :
    width(_w), height(_h), bpp(_bpp), flags(IDF_NONE),
    used_w(_w), used_h(_h), grAb(nullptr), lpic(nullptr)
{
	pixels = new u8_t[width * height * bpp];
}

image_data_c::~image_data_c()
{
	delete[] pixels;
	delete grAb;
	delete lpic;

	pixels = NULL;
	width = height = 0;
}

void image_data_c::Clear(u8_t val)
{
	memset(pixels, val, width * height * bpp);
}

void image_data_c::Whiten()
{
	SYS_ASSERT(bpp >= 3);

	// Optimized version using direct pointer arithmetic
	u8_t *src = pixels;
	u8_t *end = pixels + (width * height * bpp);

	for (; src < end; src += bpp)
	{
		int ity = MAX(src[0], MAX(src[1], src[2]));

		ity = (ity * 128 + src[0] * 38 + src[1] * 64 + src[2] * 26) >> 8;

		src[0] = src[1] = src[2] = ity;
	}
}

void image_data_c::Rotate90()
{
	//SYS_ASSERT(bpp >= 1);
	for (int y = 0, dest_col = height - 1; y < height; ++y, --dest_col)
	for (int x = 0; x < width; x++)
	{
		//u8_t *dest_pix = pixels + (dy * new_w + dx) * 3;
		u8_t *src = PixelAt(x, y);
		u8_t *dest = pixels;
		dest[(x * height) + dest_col] = src[y*width + x];
	}
	
}

void image_data_c::Invert()
{
	int line_size = used_w * bpp;

	u8_t *line_data = new u8_t[line_size + 1];

	for (int y=0; y < used_h/2; y++)
	{
		int y2 = used_h - 1 - y;

		memcpy(line_data,      PixelAt(0, y),  line_size);
		memcpy(PixelAt(0, y),  PixelAt(0, y2), line_size);
		memcpy(PixelAt(0, y2), line_data,      line_size);
	}

	delete[] line_data;
}

void image_data_c::Shrink(int new_w, int new_h)
{
	SYS_ASSERT(new_w <= width && new_h <= height);

	int step_x = width  / new_w;
	int step_y = height / new_h;
	int total  = step_x * step_y;

	// TODO: OPTIMISE this

	if (bpp == 1)
	{
		for (int dy=0; dy < new_h; dy++)
		for (int dx=0; dx < new_w; dx++)
		{
			u8_t *dest_pix = pixels + (dy * new_w + dx) * 3;

			int sx = dx * step_x;
			int sy = dy * step_y;

			const u8_t *src_pix = PixelAt(sx, sy);

			*dest_pix = *src_pix;
		}
	}
	else if (bpp == 3)
	{
		for (int dy=0; dy < new_h; dy++)
		for (int dx=0; dx < new_w; dx++)
		{
			u8_t *dest_pix = pixels + (dy * new_w + dx) * 3;

			int sx = dx * step_x;
			int sy = dy * step_y;

			int r=0, g=0, b=0;

			// compute average colour of block
			for (int x=0; x < step_x; x++)
			for (int y=0; y < step_y; y++)
			{
				const u8_t *src_pix = PixelAt(sx+x, sy+y);

				r += src_pix[0];
				g += src_pix[1];
				b += src_pix[2];
			}

			dest_pix[0] = r / total;
			dest_pix[1] = g / total;
			dest_pix[2] = b / total;
		}
	}
	else  /* bpp == 4 */
	{
		for (int dy=0; dy < new_h; dy++)
		for (int dx=0; dx < new_w; dx++)
		{
			u8_t *dest_pix = pixels + (dy * new_w + dx) * 4;

			int sx = dx * step_x;
			int sy = dy * step_y;

			int r=0, g=0, b=0, a=0;

			// compute average colour of block
			for (int x=0; x < step_x; x++)
			for (int y=0; y < step_y; y++)
			{
				const u8_t *src_pix = PixelAt(sx+x, sy+y);

				r += src_pix[0];
				g += src_pix[1];
				b += src_pix[2];
				a += src_pix[3];
			}

			dest_pix[0] = r / total;
			dest_pix[1] = g / total;
			dest_pix[2] = b / total;
			dest_pix[3] = a / total;
		}
	}

	used_w  = MAX(1, used_w * new_w / width);
	used_h  = MAX(1, used_h * new_h / height);

	width  = new_w;
	height = new_h;
}

void image_data_c::ShrinkMasked(int new_w, int new_h)
{
	if (bpp != 4)
	{
		Shrink(new_w, new_h);
		return;
	}

	SYS_ASSERT(new_w <= width && new_h <= height);

	int step_x = width  / new_w;
	int step_y = height / new_h;
	int total  = step_x * step_y;

	// TODO: OPTIMISE this

	for (int dy=0; dy < new_h; dy++)
	for (int dx=0; dx < new_w; dx++)
	{
		u8_t *dest_pix = pixels + (dy * new_w + dx) * 4;

		int sx = dx * step_x;
		int sy = dy * step_y;

		int r=0, g=0, b=0, a=0;

		// compute average colour of block
		for (int x=0; x < step_x; x++)
		for (int y=0; y < step_y; y++)
		{
			const u8_t *src_pix = PixelAt(sx+x, sy+y);

			int weight = src_pix[3];

			r += src_pix[0] * weight;
			g += src_pix[1] * weight;
			b += src_pix[2] * weight;

			a += weight;
		}

		if (a == 0)
		{
			dest_pix[0] = 0;
			dest_pix[1] = 0;
			dest_pix[2] = 0;
			dest_pix[3] = 0;
		}
		else
		{
			dest_pix[0] = r / a;
			dest_pix[1] = g / a;
			dest_pix[2] = b / a;
			dest_pix[3] = a / total;
		}
	}

	used_w  = MAX(1, used_w * new_w / width);
	used_h  = MAX(1, used_h * new_h / height);

	width  = new_w;
	height = new_h;
}

void image_data_c::Grow(int new_w, int new_h)
{
	SYS_ASSERT(new_w >= width && new_h >= height);

	u8_t *new_pixels = new u8_t[new_w * new_h * bpp];

	for (int dy = 0; dy < new_h; dy++)
	for (int dx = 0; dx < new_w; dx++)
	{
		int sx = dx * width  / new_w;
		int sy = dy * height / new_h;

		const u8_t *src = PixelAt(sx, sy);

		u8_t *dest = new_pixels + (dy * new_w + dx) * bpp;

		for (int i = 0; i < bpp; i++)
			*dest++ = *src++;
	}

	delete[] pixels;

	used_w  = used_w * new_w / width;
	used_h  = used_h * new_h / height;

	pixels  = new_pixels;
	width   = new_w;
	height  = new_h;
}

void image_data_c::RemoveAlpha()
{
	if (bpp != 4)
		return;

	u8_t *src   = pixels;
	u8_t *s_end = src + (width * height * bpp);
	u8_t *dest  = pixels;

	for (; src < s_end; src += 4)
	{
		// blend alpha with BLACK

		*dest++ = (int)src[0] * (int)src[3] / 255;
		*dest++ = (int)src[1] * (int)src[3] / 255;
		*dest++ = (int)src[2] * (int)src[3] / 255;
	}

	bpp = 3;
}

void image_data_c::SetAlpha(int alphaness)
{
	if (bpp < 3)
		return;

	if (bpp == 3)
	{
		u8_t *new_pixels = new u8_t[width * height * 4];
		u8_t *src   = pixels;
		u8_t *s_end = src + (width * height * 3);
		u8_t *dest  = new_pixels;
		for (; src < s_end; src += 3)
		{
			*dest++ = src[0];
			*dest++ = src[1];
			*dest++ = src[2];
			*dest++ = alphaness;
		}
		delete[] pixels;
		pixels = new_pixels;
		bpp = 4;
	}
	else
	{
		for (int i = 3; i < width * height * 4; i += 4)
		{
			pixels[i] = alphaness;
		}
	}
}
void image_data_c::ThresholdAlpha(u8_t alpha)
{
	if (bpp != 4)
		return;

	u8_t *src   = pixels;
	u8_t *s_end = src + (width * height * bpp);

	for (; src < s_end; src += 4)
	{
		src[3] = (src[3] < alpha) ? 0 : 255;
	}
}

void image_data_c::FourWaySymmetry()
{
	int w2 = (width  + 1) / 2;
	int h2 = (height + 1) / 2;

	for (int y = 0; y < h2; y++)
	for (int x = 0; x < w2; x++)
	{
		int ix = width  - 1 - x;
		int iy = height - 1 - y;

		CopyPixel(x, y, ix,  y);
		CopyPixel(x, y,  x, iy);
		CopyPixel(x, y, ix, iy);
	}
}

void image_data_c::EightWaySymmetry()
{
	//SYS_ASSERT(width == height);

	int hw = (width + 1) / 2;
	int hh = (height + 1) / 2;

	for (int y = 0;   y < hh; y++)
	for (int x = y+1; x < hw; x++)
	{
		CopyPixel(x, y, y, x);
	}

	FourWaySymmetry();
}

void image_data_c::AverageHue(u8_t *hue, u8_t *ity)
{
	// make sure we don't overflow
	SYS_ASSERT(used_w * used_h <= 2048 * 2048);

	int r_sum = 0;
	int g_sum = 0;
	int b_sum = 0;
	int i_sum = 0;

	int weight = 0;

	for (int y = 0; y < used_h; y++)
	{
		const u8_t *src = PixelAt(0, y);

		for (int x = 0; x < used_w; x++, src += bpp)
		{
			int r = src[0];
			int g = src[1];
			int b = src[2];
			int a = (bpp == 4) ? src[3] : 255;

			int v = MAX(r, MAX(g, b));

			i_sum += (v * (1 + a)) >> 9;

			// brighten color
			if (v > 0)
			{
				r = r * 255 / v;
				g = g * 255 / v;
				b = b * 255 / v;

				v = 255;
			}

			// compute weighting (based on saturation)
			if (v > 0)
			{
				int m = MIN(r, MIN(g, b));

				v = 4 + 12 * (v - m) / v;
			}

			// take alpha into account
			v = (v * (1 + a)) >> 8;

			r_sum += (r * v) >> 3;
			g_sum += (g * v) >> 3;
			b_sum += (b * v) >> 3;

			weight += v;
		}
	}

	weight = (weight + 7) >> 3;

	if (weight > 0)
	{
		hue[0] = r_sum / weight;
		hue[1] = g_sum / weight;
		hue[2] = b_sum / weight;
	}
	else
	{
		hue[0] = 0;
		hue[1] = 0;
		hue[2] = 0;
	}

	if (ity)
	{
		weight = (used_w * used_h + 1) / 2;

		*ity = i_sum / weight;
	}
}
void image_data_c::Swirl(int leveltime, int thickness)
{
	const int sizefactor = (height + width) / 128;
	const int swirlfactor =  8192 / 64;
    const int swirlfactor2 = 8192 / 32;
	const int amp = 1 + sizefactor;
    const int amp2 = 0 + sizefactor;
    int speed;

	if (thickness == 1) // Thin liquid
	{
		speed = 40;
	}
	else
	{
		speed = 10;
	}

	u8_t *old_pixels = new u8_t[width * height * bpp];

	memcpy(old_pixels, pixels, width * height * bpp * sizeof(u8_t));

    int x, y;

    // SMMU swirling algorithm
	for (x = 0; x < width; x++)
	{
	    for (y = 0; y < height; y++)
	    {
			int x1, y1;
			int sinvalue, sinvalue2;

			sinvalue = (y * swirlfactor + leveltime * speed * 5 + 900) & 8191;
			sinvalue2 = (x * swirlfactor2 + leveltime * speed * 4 + 300) & 8191;
			x1 = x + width + height
			+ ((finesine[sinvalue] * amp) >> FRACBITS)
			+ ((finesine[sinvalue2] * amp2) >> FRACBITS);

			sinvalue = (x * swirlfactor + leveltime * speed * 3 + 700) & 8191;
			sinvalue2 = (y * swirlfactor2 + leveltime * speed * 4 + 1200) & 8191;
			y1 = y + width + height
			+ ((finesine[sinvalue] * amp) >> FRACBITS)
			+ ((finesine[sinvalue2] * amp2) >> FRACBITS);

			x1 &= width - 1;
			y1 &= height - 1;

			u8_t *src = old_pixels + (y1 * width + x1) * bpp;
			u8_t *dest = pixels + (y * width + x) * bpp;

			for (int i = 0; i < bpp; i++)
				*dest++ = *src++;
		}
	}
	delete[] old_pixels;
	old_pixels = NULL;
}

void image_data_c::Blit(int src_x, int src_y, int src_w, int src_h,
                         image_data_c *dst, int dst_x, int dst_y)
{
	SYS_ASSERT(dst);
	SYS_ASSERT(bpp == dst->bpp);

	// Clip source rect against source bounds
	if (src_x < 0) { dst_x -= src_x; src_w += src_x; src_x = 0; }
	if (src_y < 0) { dst_y -= src_y; src_h += src_y; src_y = 0; }
	if (src_x + src_w > width)  src_w = width  - src_x;
	if (src_y + src_h > height) src_h = height - src_y;

	// Clip destination rect
	if (dst_x < 0) { src_x -= dst_x; src_w += dst_x; dst_x = 0; }
	if (dst_y < 0) { src_y -= dst_y; src_h += dst_y; dst_y = 0; }
	if (dst_x + src_w > dst->width)  src_w = dst->width  - dst_x;
	if (dst_y + src_h > dst->height) src_h = dst->height - dst_y;

	if (src_w <= 0 || src_h <= 0)
		return;

	int row_bytes = src_w * bpp;
	for (int row = 0; row < src_h; row++)
	{
		memcpy(dst->PixelAt(dst_x, dst_y + row),
		       PixelAt(src_x, src_y + row),
		       row_bytes);
	}
}

void image_data_c::FlipHorizontal()
{
	int half_w = width / 2;

	for (int y = 0; y < height; y++)
	{
		u8_t *row = pixels + y * width * bpp;
		u8_t *a   = row;
		u8_t *b   = row + (width - 1) * bpp;

		for (int x = 0; x < half_w; x++, a += bpp, b -= bpp)
		{
			for (int c = 0; c < bpp; c++)
			{
				u8_t tmp = a[c];
				a[c] = b[c];
				b[c] = tmp;
			}
		}
	}
}

void image_data_c::FlipVertical()
{
	Invert();
}

void image_data_c::Premultiply()
{
	if (bpp != 4)
		return;

	u8_t *p   = pixels;
	u8_t *end = pixels + width * height * 4;

	for (; p < end; p += 4)
	{
		unsigned int a = p[3];
		p[0] = (u8_t)((unsigned int)p[0] * a / 255u);
		p[1] = (u8_t)((unsigned int)p[1] * a / 255u);
		p[2] = (u8_t)((unsigned int)p[2] * a / 255u);
	}
}
void image_data_c::ConvertBpp(int new_bpp)
{
	if (new_bpp == bpp)
		return;

	SYS_ASSERT(new_bpp == 3 || new_bpp == 4);
	SYS_ASSERT(bpp == 3 || bpp == 4);

	int total = width * height;
	u8_t *new_pixels = new u8_t[total * new_bpp];

	const u8_t *src = pixels;
	u8_t       *dst = new_pixels;

	if (bpp == 3 && new_bpp == 4)
	{
		for (int i = 0; i < total; i++, src += 3, dst += 4)
		{
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = 255;
		}
	}
	else  // bpp==4, new_bpp==3: drop alpha
	{
		for (int i = 0; i < total; i++, src += 4, dst += 3)
		{
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
		}
	}

	delete[] pixels;
	pixels = new_pixels;
	bpp = (short)new_bpp;
}

void image_data_c::ApplyGamma(float gamma)
{
	SYS_ASSERT(bpp >= 3);
	SYS_ASSERT(gamma > 0.0f);

	// Build a look-up table for speed.
	u8_t lut[256];
	float inv_gamma = 1.0f / gamma;
	for (int i = 0; i < 256; i++)
		lut[i] = (u8_t)(int)(powf(i / 255.0f, inv_gamma) * 255.0f + 0.5f);

	u8_t *p   = pixels;
	u8_t *end = pixels + width * height * bpp;

	for (; p < end; p += bpp)
	{
		p[0] = lut[p[0]];
		p[1] = lut[p[1]];
		p[2] = lut[p[2]];
		// alpha (p[3] when bpp==4) is intentionally unchanged
	}
}

void image_data_c::ApplyBrightness(float brightness)
{
	SYS_ASSERT(bpp >= 3);

	u8_t *p   = pixels;
	u8_t *end = pixels + width * height * bpp;

	for (; p < end; p += bpp)
	{
		int r = (int)(p[0] * brightness + 0.5f);
		int g = (int)(p[1] * brightness + 0.5f);
		int b = (int)(p[2] * brightness + 0.5f);

		p[0] = (u8_t)MIN(255, r);
		p[1] = (u8_t)MIN(255, g);
		p[2] = (u8_t)MIN(255, b);
	}
}

void image_data_c::ApplyBrightmap(const image_data_c *bright)
{
	SYS_ASSERT(bright);
	SYS_ASSERT(bpp >= 3);
	SYS_ASSERT(bright->bpp >= 3);
	SYS_ASSERT(bright->width  == width);
	SYS_ASSERT(bright->height == height);

	u8_t *p   = pixels;
	u8_t *end = pixels + width * height * bpp;
	const u8_t *b = bright->pixels;

	for (; p < end; p += bpp, b += bright->bpp)
	{
		p[0] = (u8_t)((int)p[0] * (int)b[0] / 255);
		p[1] = (u8_t)((int)p[1] * (int)b[1] / 255);
		p[2] = (u8_t)((int)p[2] * (int)b[2] / 255);
	}
}

void image_data_c::ApplyColormap(const u8_t *colormap, int num_colors)
{
	SYS_ASSERT(bpp == 1);
	SYS_ASSERT(colormap);
	SYS_ASSERT(num_colors > 0);

	u8_t *p   = pixels;
	u8_t *end = pixels + width * height;

	for (; p < end; p++)
		*p = colormap[*p % num_colors];
}

image_data_c *image_data_c::MakeNormalMap(float scale) const
{
	SYS_ASSERT(bpp >= 3);

	image_data_c *nmap = new image_data_c(width, height, 3);

	// Compute height as average luminance.
	// Sobel kernel derivates give the gradient; scale controls depth.
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			// Wrap-around neighbours for seamless tiling.
			int xm1 = (x - 1 + width)  % width;
			int xp1 = (x + 1) % width;
			int ym1 = (y - 1 + height) % height;
			int yp1 = (y + 1) % height;

			auto height = [&](int px, int py) -> float
			{
				const u8_t *p = PixelAt(px, py);
				return (p[0] * 0.30f + p[1] * 0.59f + p[2] * 0.11f) / 255.0f;
			};

			float tl = height(xm1, ym1);  float t  = height(x,   ym1);
			float tr = height(xp1, ym1);  float l  = height(xm1, y);
			float r  = height(xp1, y);    float bl = height(xm1, yp1);
			float b  = height(x,   yp1);  float br = height(xp1, yp1);

			// Sobel
			float dx = (tr + 2.0f*r + br) - (tl + 2.0f*l + bl);
			float dy = (bl + 2.0f*b + br) - (tl + 2.0f*t + tr);
			float dz = 1.0f / scale;

			// Normalise
			float len = sqrtf(dx*dx + dy*dy + dz*dz);
			if (len < 1e-6f) len = 1e-6f;
			dx /= len;  dy /= len;  dz /= len;

			u8_t *dst = nmap->PixelAt(x, y);
			dst[0] = (u8_t)((dx * 0.5f + 0.5f) * 255.0f + 0.5f);
			dst[1] = (u8_t)((dy * 0.5f + 0.5f) * 255.0f + 0.5f);
			dst[2] = (u8_t)((dz * 0.5f + 0.5f) * 255.0f + 0.5f);
		}
	}

	return nmap;
}

void image_data_c::PackRGB565(u16_t *out) const
{
	SYS_ASSERT(out);
	SYS_ASSERT(bpp >= 3);

	int total = width * height;
	const u8_t *p = pixels;

	for (int i = 0; i < total; i++, p += bpp)
	{
		u16_t r = (u16_t)(p[0] >> 3);   // 5 bits
		u16_t g = (u16_t)(p[1] >> 2);   // 6 bits
		u16_t b = (u16_t)(p[2] >> 3);   // 5 bits
		out[i] = (r << 11) | (g << 5) | b;
	}
}

void image_data_c::PackARGB1555(u16_t *out) const
{
	SYS_ASSERT(out);
	SYS_ASSERT(bpp == 4);

	int total = width * height;
	const u8_t *p = pixels;

	for (int i = 0; i < total; i++, p += 4)
	{
		u16_t a = (p[3] >= 128) ? 1u : 0u;
		u16_t r = (u16_t)(p[0] >> 3);
		u16_t g = (u16_t)(p[1] >> 3);
		u16_t b = (u16_t)(p[2] >> 3);
		out[i] = (a << 15) | (r << 10) | (g << 5) | b;
	}
}

void image_data_c::PackARGB4444(u16_t *out) const
{
	SYS_ASSERT(out);
	SYS_ASSERT(bpp == 4);

	int total = width * height;
	const u8_t *p = pixels;

	for (int i = 0; i < total; i++, p += 4)
	{
		u16_t a = (u16_t)(p[3] >> 4);
		u16_t r = (u16_t)(p[0] >> 4);
		u16_t g = (u16_t)(p[1] >> 4);
		u16_t b = (u16_t)(p[2] >> 4);
		out[i] = (a << 12) | (r << 8) | (g << 4) | b;
	}
}

} // namespace epi


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
