//------------------------------------------------------------------------
//  PNG Image Handling
//------------------------------------------------------------------------
//
//  Copyright (c) 2003-2026  The EDGE Team.
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
#include "endianess.h"

#include "image_png.h"
#include "image_data.h"

#ifndef _arch_dreamcast
#include "stb_image.h"
#include "stb_image_write.h"
#else
// Use kos-ports PNG headers path
#include <png/png.h>
#include "memshim.h"
#endif

extern epi::image_data_c *epimg_load(epi::file_c *F, int read_flags);
extern bool epimg_info(epi::file_c *F, int *width, int *height, bool *solid);
extern bool epimg_write_png(FILE *fp, int w, int h, int c, void *data, int s);



namespace epi
{
#ifdef _arch_dreamcast
    struct png_mem_reader_t
    {
        const byte *data;
        size_t size;
        size_t pos;
    };

    static bool PNG_ReadWholeFile(file_c *f, byte **buffer, size_t *length)
    {
        f->Seek(0, epi::file_c::SEEKPOINT_START);
        int len = f->GetLength();
        if (len < 8)
            return false;

        byte *buf = new byte[len];
        unsigned int got = f->Read(buf, len);
        if (got != (unsigned int)len)
        {
            delete[] buf;
            return false;
        }

        if (png_sig_cmp(buf, 0, 8) != 0)
        {
            delete[] buf;
            return false;
        }

        *buffer = buf;
        *length = (size_t)len;
        return true;
    }

    static void PNG_MemRead(png_structp png_ptr, png_bytep out_bytes, png_size_t byte_count_to_read)
    {
        png_mem_reader_t *reader = (png_mem_reader_t *)png_get_io_ptr(png_ptr);
        if (!reader || reader->pos + byte_count_to_read > reader->size)
            png_error(png_ptr, "Unexpected EOF in PNG stream");

        memcpy(out_bytes, reader->data + reader->pos, byte_count_to_read);
        reader->pos += byte_count_to_read;
    }

    static void PNG_ConfigureReader(png_structp png_ptr)
    {
        png_set_crc_action(png_ptr, PNG_CRC_DEFAULT, PNG_CRC_QUIET_USE);
    }
#endif

	bool PNG_IsDataPNG(const byte *data, int length)
	{
        static byte png_sig[4] = { 0x89, 0x50, 0x4E, 0x47 };
		if (length < 4)
			return false;

		return memcmp(data, png_sig, 4) == 0;
	}

    image_data_c *PNG_Load(file_c *f, int read_flags)
	{
        image_data_c *img = NULL;

        byte sig_buf[4];

        /* check the signature */
        f->Seek(0,  epi::file_c::SEEKPOINT_START);
        f->Read(sig_buf, 4);
        if (!PNG_IsDataPNG(sig_buf, 4))
        {
            fprintf(stderr, "PNG_Load - File is not a PNG image!\n");
            return NULL;
        }

#ifndef _arch_dreamcast
        img = epimg_load(f, read_flags);
        if (!img)
        {
            fprintf(stderr, "PNG_Load - Couldn't load PNG image!\n");
            return NULL;
        }
#else
        byte *buf = NULL;
        size_t len = 0;
        if (!PNG_ReadWholeFile(f, &buf, &len))
        {
            fprintf(stderr, "PNG_Load - Couldn't read PNG image stream!\n");
            return NULL;
        }

        png_mem_reader_t mr = { buf, len, 8 };

        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_ptr) { delete[] buf; fprintf(stderr, "PNG_Load - png_create_read_struct failed.\n"); return NULL; }
        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) { png_destroy_read_struct(&png_ptr, NULL, NULL); delete[] buf; fprintf(stderr, "PNG_Load - png_create_info_struct failed.\n"); return NULL; }
        if (setjmp(png_jmpbuf(png_ptr))) { png_destroy_read_struct(&png_ptr, &info_ptr, NULL); delete[] buf; fprintf(stderr, "PNG_Load - libpng error.\n"); return NULL; }
        PNG_ConfigureReader(png_ptr);
        png_set_read_fn(png_ptr, &mr, PNG_MemRead);
        png_set_sig_bytes(png_ptr, 8);
        png_read_info(png_ptr, info_ptr);

        png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
        png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
        int color_type = png_get_color_type(png_ptr, info_ptr);
        int bit_depth = png_get_bit_depth(png_ptr, info_ptr);

        if (bit_depth == 16) png_set_strip_16(png_ptr);
        if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);
        if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY) png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
        if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png_ptr);

        png_read_update_info(png_ptr, info_ptr);

        int channels = png_get_channels(png_ptr, info_ptr);
        int bpp = channels;

        // Match the stb_image path: optionally pad to power-of-two and invert vertically
        int tot_W = (int)width;
        int tot_H = (int)height;
		if (read_flags == epi::IRF_Round_POW2)
		{
            tot_W = 8;
            while (tot_W < (int)width)
                tot_W <<= 1;

            tot_H = 8;
            while (tot_H < (int)height)
                tot_H <<= 1;
		}

        image_data_c *out = new image_data_c((int)tot_W, (int)tot_H, bpp);
        out->used_w = (int)width;
        out->used_h = (int)height;
        if (out->used_w != tot_W || out->used_h != tot_H)
            out->Clear();

        // Allocate row pointers without C++ vector to avoid header deps
        png_bytep *rows = (png_bytep*)malloc(sizeof(png_bytep) * height);
        if (!rows) { delete out; fprintf(stderr, "PNG_Load - row alloc failed.\n"); return NULL; }
        for (png_uint_32 y = 0; y < height; y++)
        {
            // Invert image vertically so (0,0) behaves like the stb_image loader.
            rows[y] = (png_bytep)(out->PixelAt(0, (int)height - 1 - (int)y));
        }
        png_read_image(png_ptr, rows);
        png_read_end(png_ptr, NULL);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        delete[] buf;
        free(rows);

        img = out;
#endif

		/* check for grAb chunk */
        int i = 0, j = 0;

        f->Seek(0,  epi::file_c::SEEKPOINT_START);
        int flen = f->GetLength();
        for ( ; i<flen && j<5; i++)
        {
            static byte pgs[5] = { 0x08, 'g', 'r', 'A', 'b' };
            byte tc;
            f->Read(&tc, 1);
            if (tc == pgs[j])
                j++;
            else
                j = 0;
        }

        if (j == 5)
        {
            int x, y;
            png_grAb_t *grAb = new png_grAb_t;

            f->Read(&x, 4);
            f->Read(&y, 4);
            grAb->x = EPI_BE_S32(x);
            grAb->y = EPI_BE_S32(y);
#if DEBUG
            I_Printf("Got grAb struct: %d/%d\n", grAb->x, grAb->y);
#endif
            if (grAb->x)
                grAb->x = img->used_w / 2 - grAb->x;
            if (grAb->y)
                grAb->y = grAb->y - img->used_h;
 
            img->grAb = grAb;
        }

		return img;
	}

    bool PNG_GetInfo(file_c *f, int *width, int *height, bool *solid)
	{
		byte sig_buf[4];

		/* check the signature */
        f->Seek(0,  epi::file_c::SEEKPOINT_START);
		f->Read(sig_buf, 4);
        if (!PNG_IsDataPNG(sig_buf, 4))
            return false;

    #ifndef _arch_dreamcast
        return epimg_info(f, width, height, solid);
    #else
        byte *buf = NULL;
        size_t len = 0;
        if (!PNG_ReadWholeFile(f, &buf, &len))
            return false;

        png_mem_reader_t mr = { buf, len, 8 };
        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_ptr) { delete[] buf; return false; }
        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) { png_destroy_read_struct(&png_ptr, NULL, NULL); delete[] buf; return false; }
        if (setjmp(png_jmpbuf(png_ptr))) { png_destroy_read_struct(&png_ptr, &info_ptr, NULL); delete[] buf; return false; }
        PNG_ConfigureReader(png_ptr);
        png_set_read_fn(png_ptr, &mr, PNG_MemRead);
        png_set_sig_bytes(png_ptr, 8);
        png_read_info(png_ptr, info_ptr);
        *width = (int)png_get_image_width(png_ptr, info_ptr);
        *height = (int)png_get_image_height(png_ptr, info_ptr);
        if (solid) *solid = false;
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        delete[] buf;
        return true;
    #endif
	}


	//------------------------------------------------------------------------

    bool PNG_Save(FILE *fp, const image_data_c *img, int compress)
	{
		SYS_ASSERT(img->bpp >= 3);
#ifndef _arch_dreamcast
        stbi_write_png_compression_level = compress;
        stbi_flip_vertically_on_write(1);

        if (epimg_write_png(fp, img->used_w, img->used_h, img->bpp, img->PixelAt(0, 0), (int)img->width*img->bpp))
            return true;
        return false;
#else
        // Dreamcast: simple no-op or implement if needed later
        return false;
#endif
	}

}  // namespace epi

   //--- editor settings ---
   // vi:ts=4:sw=4:noexpandtab
