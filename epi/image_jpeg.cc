//------------------------------------------------------------------------
//  JPEG Image Handling
//------------------------------------------------------------------------
//
//  Copyright (c) 2003-2025  The EDGE Team.
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

#include "image_jpeg.h"
#include "image_data.h"

#ifdef _arch_dreamcast
#include <jpeg/jpeglib.h>
#include <fastmem/fastmem.h>
#ifndef fm_memcpy
#define fm_memcpy memcpy
#endif
#else
#include "stb_image.h"
#include "stb_image_write.h"
#endif
extern epi::image_data_c *epimg_load(epi::file_c *f, int read_flags);
extern bool epimg_info(epi::file_c *F, int *width, int *height, bool *solid);
extern bool epimg_write_jpg(FILE *fp, int w, int h, int c, void *data, int q);

namespace epi
{
        image_data_c *JPEG_Load(file_c *f, int read_flags)
        {
#ifndef _arch_dreamcast
                image_data_c *img = NULL;
                img = epimg_load(f, read_flags);
                if (!img)
		{
			fprintf(stderr, "Image Loader - Couldn't load image!\n");
			return NULL;
		}
		return img;
#else
                // Dreamcast: decode via kos-ports libjpeg from memory
                (void)read_flags;
                f->Seek(0, epi::file_c::SEEKPOINT_START);
                int len = f->GetLength();
                byte *buf = new byte[len];
                f->Read(buf, len);

                struct jpeg_decompress_struct cinfo;
                struct jpeg_error_mgr jerr;
                cinfo.err = jpeg_std_error(&jerr);
                jpeg_create_decompress(&cinfo);

                // Create a custom source manager for memory buffer
                struct jpeg_source_mgr src;
                src.next_input_byte = (const JOCTET*)buf;
                src.bytes_in_buffer = (size_t)len;
                src.init_source = [](j_decompress_ptr){};
                src.fill_input_buffer = [](j_decompress_ptr c){
                        static const JOCTET EOI[2] = { 0xFF, JPEG_EOI };
                        c->src->next_input_byte = EOI;
                        c->src->bytes_in_buffer = 2;
                        return TRUE;
                };
                src.skip_input_data = [](j_decompress_ptr c, long num_bytes){
                        if (num_bytes > 0) {
                                if ((size_t)num_bytes <= c->src->bytes_in_buffer) {
                                        c->src->next_input_byte += num_bytes;
                                        c->src->bytes_in_buffer -= num_bytes;
                                } else {
                                        c->src->next_input_byte += c->src->bytes_in_buffer;
                                        c->src->bytes_in_buffer = 0;
                                }
                        }
                };
                src.resync_to_restart = jpeg_resync_to_restart;
                src.term_source = [](j_decompress_ptr){};
                cinfo.src = &src;

                if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
                        jpeg_destroy_decompress(&cinfo);
                        delete[] buf;
                        return NULL;
                }
                jpeg_start_decompress(&cinfo);

                int width = (int)cinfo.output_width;
                int height = (int)cinfo.output_height;
                int channels = (int)cinfo.output_components; // Usually 3 (RGB)
                if (channels != 3) {
                        // Force RGB output
                        cinfo.out_color_space = JCS_RGB;
                        channels = 3;
                }

                image_data_c *out = new image_data_c(width, height, channels);
                int row_stride = width * channels;
                JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);
                for (int y = 0; y < height; y++) {
                        if (jpeg_read_scanlines(&cinfo, buffer, 1) != 1) {
                                jpeg_finish_decompress(&cinfo);
                                jpeg_destroy_decompress(&cinfo);
                                delete[] buf;
                                delete out;
                                return NULL;
                        }
                        fm_memcpy(out->pixels + y * row_stride, buffer[0], (size_t)row_stride);
                }
                jpeg_finish_decompress(&cinfo);
                jpeg_destroy_decompress(&cinfo);
                delete[] buf;
                out->used_w = out->width;
                out->used_h = out->height;
                return out;
#endif
        }

        bool JPEG_GetInfo(file_c *f, int *width, int *height, bool *solid)
        {
#ifndef _arch_dreamcast
                return epimg_info(f, width, height, solid);
#else
                // Dreamcast: read header via libjpeg
                f->Seek(0, epi::file_c::SEEKPOINT_START);
                int len = f->GetLength();
                byte *buf = new byte[len];
                f->Read(buf, len);
                struct jpeg_decompress_struct cinfo;
                struct jpeg_error_mgr jerr;
                cinfo.err = jpeg_std_error(&jerr);
                jpeg_create_decompress(&cinfo);
                struct jpeg_source_mgr src;
                src.next_input_byte = (const JOCTET*)buf;
                src.bytes_in_buffer = (size_t)len;
                src.init_source = [](j_decompress_ptr){};
                src.fill_input_buffer = [](j_decompress_ptr c){
                        static const JOCTET EOI[2] = { 0xFF, JPEG_EOI };
                        c->src->next_input_byte = EOI;
                        c->src->bytes_in_buffer = 2;
                        return TRUE;
                };
                src.skip_input_data = [](j_decompress_ptr c, long num_bytes){
                        if (num_bytes > 0) {
                                if ((size_t)num_bytes <= c->src->bytes_in_buffer) {
                                        c->src->next_input_byte += num_bytes;
                                        c->src->bytes_in_buffer -= num_bytes;
                                } else {
                                        c->src->next_input_byte += c->src->bytes_in_buffer;
                                        c->src->bytes_in_buffer = 0;
                                }
                        }
                };
                src.resync_to_restart = jpeg_resync_to_restart;
                src.term_source = [](j_decompress_ptr){};
                cinfo.src = &src;
                bool ok = (jpeg_read_header(&cinfo, TRUE) == JPEG_HEADER_OK);
                if (ok) {
                        if (width) *width = (int)cinfo.image_width;
                        if (height) *height = (int)cinfo.image_height;
                        if (solid) *solid = false;
                }
                jpeg_destroy_decompress(&cinfo);
                delete[] buf;
                return ok;
#endif
        }

    bool JPEG_Save(FILE *fp, const image_data_c *img, int quality)
    {
        SYS_ASSERT(img->bpp == 3);
#ifndef _arch_dreamcast
        stbi_flip_vertically_on_write(1);
        if (epimg_write_jpg(fp, img->width, img->height, img->bpp, img->PixelAt(0, 0), quality))
            return true;
        return false;
#else
        // Dreamcast: JPEG saving not needed right now
        (void)fp; (void)img; (void)quality;
        return false;
#endif
    }

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
