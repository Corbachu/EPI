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

#ifndef __EPI_IMAGEDATA_H__
#define __EPI_IMAGEDATA_H__

struct png_grAb_t
{
    int x, y;
};

struct rott_lpic_t
{
	//short width, height;
	short orgx, orgy;
	byte data;
} ;

namespace epi
{

	
class image_data_c
{
// IMAGE LOADING FLAGS

public:
	short width;
	short height;

	// Bytes Per Pixel, determines image mode:
	// 1 = palettised
	// 3 = format is RGB
	// 4 = format is RGBA
	short bpp;

	short flags;

	// for image loading, these will be the actual image size
	short used_w;
	short used_h;

	u8_t *pixels;

    	png_grAb_t *grAb;

	rott_lpic_t *lpic;
	

	// Needed for access to origsize for determination of offsets.
	// rottpatch_t *origsize;

	// TODO: color_c *palette;

public:
	image_data_c(int _w, int _h, int _bpp = 3);
	~image_data_c();

	void Clear(u8_t val = 0);

	inline u8_t *PixelAt(int x, int y) const
	{
		// Note: DOES NOT CHECK COORDS

		return pixels + (y * width + x) * bpp;
	}

	inline void CopyPixel(int sx, int sy, int dx, int dy)
	{
		u8_t *src  = PixelAt(sx, sy);
		u8_t *dest = PixelAt(dx, dy);

		for (int i = 0; i < bpp; i++)
			*dest++ = *src++;
	}

	void Whiten();
	// convert all RGB(A) pixels to a greyscale equivalent.

	void Rotate90();
	// flip the image 90 degrees to the right.

	void RotatePicture();
	// flip the image 90 degrees to the right.
	
	void Invert();
	// turn the image up-side-down.

	void Shrink(int new_w, int new_h);
	// shrink an image to a smaller image.
	// The old size and the new size must be powers of two.
	// For RGB(A) images the pixel values are averaged.
	// Palettised images are not averaged, instead the bottom
	// left pixel in each group becomes the final pixel.

	void ShrinkMasked(int new_w, int new_h);
	// like Shrink(), but for RGBA images the source alpha is
	// used as a weighting factor for the shrunken color, hence
	// purely transparent pixels never affect the final color
	// of a pixel group.

	void Grow(int new_w, int new_h);
	// scale the image up to a larger size.
	// The old size and the new size must be powers of two.

	void RemoveAlpha();
	// convert an RGBA image to RGB.  Partially transparent colors
	// (alpha < 255) are blended with black.
	void SetAlpha(int alphaness);
	// Set uniform alpha value for all pixels in an image
	// If RGB, will convert to RGBA
	
	void ThresholdAlpha(u8_t alpha = 128);
	// test each alpha value in the RGBA image against the threshold:
	// lesser values become 0, and greater-or-equal values become 255.

	void FourWaySymmetry();
	// mirror the already-drawn corner (lowest x/y values) into the
	// other three corners.  When width or height is odd, the middle
	// column/row must already be drawn.

	void EightWaySymmetry();
	// mirror the already-drawn half corner (1/8th of the image)
	// into the rest of the image.  The source corner has lowest x/y
	// values, and the triangle piece is where y <= x, including the
	// pixels along the diagonal where (x == y).
	// NOTE: the image must be SQUARE (width == height).

	void AverageHue(u8_t *hue, u8_t *ity = NULL);
	// compute the average Hue of the RGB(A) image, storing the
	// result in the 'hue' array (r, g, b).  The average intensity
	// will be stored in 'ity' when given.
	void Swirl(int leveltime, int thickness);
	// SMMU-style swirling

	void Blit(int src_x, int src_y, int src_w, int src_h,
	          image_data_c *dst, int dst_x, int dst_y);
	// Copy a rectangular region of this image into another image at
	// (dst_x, dst_y).  Both images must have the same bpp.
	// Out-of-bounds coordinates are clamped silently.

	void FlipHorizontal();
	// Mirror the image left-to-right.

	void FlipVertical();
	// Alias for Invert() – flip the image up-side-down.

	void Premultiply();
	// Multiply each RGB channel by the alpha value (for pre-multiplied
	// alpha compositing).  Image must be RGBA (bpp == 4).

	void ConvertBpp(int new_bpp);
	// Convert between RGB (bpp==3) and RGBA (bpp==4).
	// RGB→RGBA sets alpha to 255; RGBA→RGB discards the alpha channel.

	void ApplyGamma(float gamma);
	// Apply gamma correction to RGB(A) data.  Values < 1.0 darken the
	// image; values > 1.0 brighten it.  Alpha is not affected.

	void ApplyBrightness(float brightness);
	// Scale all RGB channels by 'brightness' (1.0 = unchanged, > 1.0
	// brightens, clamped to 255).  Alpha is not affected.

	void ApplyBrightmap(const image_data_c *bright);
	// Multiply this image's RGB by the RGB values of 'bright'
	// (a brightmap / lightmap overlay).  'bright' must be the same
	// width and height.  Alpha is not affected.

	void ApplyColormap(const u8_t *colormap, int num_colors);
	// Remap a paletted (bpp==1) image through 'colormap'.  Each pixel
	// value p becomes colormap[p] (wrapping at num_colors).

	image_data_c *MakeNormalMap(float scale = 1.0f) const;
	// Generate an RGB normal map from this image treated as a height
	// map (luminance → height).  Returns a new bpp==3 image the same
	// size; caller owns the result.  'scale' amplifies the depth.

	void PackRGB565(u16_t *out) const;
	// Pack this RGB or RGBA image into width*height RGB565 words.
	// 'out' must point to at least width*height u16_t values.

	void PackARGB1555(u16_t *out) const;
	// Pack this RGBA image into width*height ARGB1555 words.
	// Pixels with alpha >= 128 are considered opaque.

	void PackARGB4444(u16_t *out) const;
	// Pack this RGBA image into width*height ARGB4444 words.
};

// IMAGE LOADING FLAGS
	typedef enum 
	{
		IRF_NONE = 0,

		IRF_Round_POW2 = (1 << 0),  // convert width / height to powers of two
	}
	image_read_flags_e;
	
	typedef enum 
	{
		IDF_NONE = 0,

		IDF_ExtPalette = (1 << 0),  // palette is external (do not free)
	}
	image_data_flags_e;

} // namespace epi

#endif  /* __EPI_IMAGEDATA_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
