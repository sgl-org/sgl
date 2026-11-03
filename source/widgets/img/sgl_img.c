/* source/widgets/sgl_img.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL  
 * Document reference link: https://sgl-docs.readthedocs.io
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_img.h"

/* Line-based block decoder, decoded output is RGB565 (little-endian) */
#define QOI_OP_RUN                   0xC0
#define QOI_OP_DIFF                  0x40
#define QOI_OP_LUMA                  0x80
#define QOI_OP_RGB565                0xFE

#define QOI_HDR_OFF_W                1
#define QOI_HDR_OFF_H                3
#define QOI_HDR_OFF_U16_IDX_SZ       7
#define QOI_HDR_OFF_U24_IDX_SZ       9
#define QOI_HDR_OFF_U32_IDX_SZ       11

static inline uint16_t qoi_read_u16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static inline uint32_t qoi_read_u24(const uint8_t *p)
{
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
}

static inline uint32_t qoi_read_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/**
 * @brief init QOI decoder - pre-compute line offset table
 * @param src QOI encoded data
 * @param offsets caller-provided offset table (at least height entries)
 * @return 0 on success
 */
static uint8_t qoi565_init(const uint8_t *src, uint32_t *offsets)
{
    if (src == NULL || offsets == NULL) return 1;

    uint16_t qoi_width     = qoi_read_u16(src + QOI_HDR_OFF_W);
    uint16_t qoi_height    = qoi_read_u16(src + QOI_HDR_OFF_H);
    uint16_t qoi_u16_idx   = qoi_read_u16(src + QOI_HDR_OFF_U16_IDX_SZ);
    uint16_t qoi_u24_idx   = qoi_read_u16(src + QOI_HDR_OFF_U24_IDX_SZ);
    uint16_t qoi_u32_idx   = qoi_read_u16(src + QOI_HDR_OFF_U32_IDX_SZ);

    if (qoi_width == 0 || qoi_height == 0 || qoi_width > 240) {
        return 2;
    }

    uint32_t idx_total = qoi_u16_idx + qoi_u24_idx + qoi_u32_idx;
    if (idx_total > 1024) return 3;

    /* Pre-compute offsets of all lines */
    uint32_t ptr_idx = 13;
    for (uint32_t i = 0; i < qoi_height; i++) {
        if (i < qoi_u16_idx / 2) {
            offsets[i] = qoi_read_u16(src + ptr_idx);
            ptr_idx += 2;
        } else if (i < (qoi_u16_idx / 2 + qoi_u24_idx / 3)) {
            offsets[i] = qoi_read_u24(src + ptr_idx);
            ptr_idx += 3;
        } else {
            offsets[i] = qoi_read_u32(src + ptr_idx);
            ptr_idx += 4;
        }
    }

    return 0;
}

/**
 * @brief decode one QOI line, emitting at most max_pix pixels
 * @param dst     output buffer, RGB565 little-endian
 * @param p       encoded line start (taken from the offset table)
 * @param max_pix number of pixels to decode; decoding stops once this many
 *                pixels have been emitted, so a clipped right edge skips the
 *                rest of the line
 */
static void qoi565_decode_line(uint8_t *dst, const uint8_t *p, uint32_t max_pix)
{
    uint8_t *out_ptr = dst;
    uint8_t pr = 0, pg = 0, pb = 0;
    uint8_t hi = 0, lo = 0;  /* last emitted pixel, little-endian byte order */

    while (max_pix > 0) {
        uint8_t op = *p++;

        /* RGB565 full color */
        if (op == QOI_OP_RGB565) {
            hi = *p++;
            lo = *p++;
            pr = (hi >> 3) & 0x1F;
            pg = ((hi & 0x07) << 3) | ((lo >> 5) & 0x07);
            pb = lo & 0x1F;
            *out_ptr++ = lo;
            *out_ptr++ = hi;
            max_pix--;
        }
        /* RUN repeat */
        else if ((op & 0xC0) == QOI_OP_RUN) {
            uint32_t run_len = (op & 0x3F) + 1;
            if (run_len > max_pix) run_len = max_pix;
            max_pix -= run_len;
            while (run_len--) {
                *out_ptr++ = lo;
                *out_ptr++ = hi;
            }
        }
        /* DIFF delta */
        else if ((op & 0xC0) == QOI_OP_DIFF) {
            int8_t dr = (int8_t)((op >> 4) & 0x03) - 2;
            int8_t dg = (int8_t)((op >> 2) & 0x03) - 2;
            int8_t db = (int8_t)(op & 0x03) - 2;
            pr = (uint8_t)(pr + dr) & 0x1F;
            pg = (uint8_t)(pg + dg) & 0x3F;
            pb = (uint8_t)(pb + db) & 0x1F;
            lo = ((pg & 0x07) << 5) | pb;
            hi = (pr << 3) | (pg >> 3);
            *out_ptr++ = lo;
            *out_ptr++ = hi;
            max_pix--;
        }
        /* LUMA delta */
        else if ((op & 0xC0) == QOI_OP_LUMA) {
            int8_t dg = (int8_t)(op & 0x3F) - 32;
            uint8_t sub = *p++;
            int8_t dr_dg = (int8_t)((sub >> 4) & 0x0F) - 8;
            int8_t db_dg = (int8_t)(sub & 0x0F) - 8;
            pr = (uint8_t)(pr + dr_dg + dg) & 0x1F;
            pg = (uint8_t)(pg + dg) & 0x3F;
            pb = (uint8_t)(pb + db_dg + dg) & 0x1F;
            lo = ((pg & 0x07) << 5) | pb;
            hi = (pr << 3) | (pg >> 3);
            *out_ptr++ = lo;
            *out_ptr++ = hi;
            max_pix--;
        }
        /* Invalid opcode */
        else {
            *out_ptr++ = 0x00;
            *out_ptr++ = 0x00;
            max_pix--;
        }
    }
}

/**
 * @brief ensure flash_buffer holds at least size bytes, growing it if needed.
 *        flash_buf_size tracks the allocation so switching between pixmap
 *        formats / sizes can never write past the buffer.
 * @return flash_buffer pointer, NULL on allocation failure
 */
static uint8_t* img_flash_buf_ensure(sgl_img_t *img, uint32_t size)
{
    if (img->flash_buffer != NULL && img->flash_buf_size >= size)
        return img->flash_buffer;
    if (img->flash_buffer != NULL)
        sgl_free(img->flash_buffer);
    img->flash_buffer = (uint8_t*)sgl_malloc(size);
    img->flash_buf_size = (img->flash_buffer != NULL) ? size : 0;
    return img->flash_buffer;
}

/**
 * @brief number of compressed bytes prefetched per flash access for RLE.
 *        One block covers SGL_IMG_BUFFER_SIZE lines worth of worst-case encoded
 *        pixels: (value bytes per pixel + 1 run header) * width * lines.
 *        Clamped into uint16 range so it fits the reused buffer counters.
 */
static inline uint16_t rle_cache_capacity(const sgl_img_t *img)
{
    const sgl_pixmap_t *pm = &img->pixmap[img->pixmap_idx];
    uint32_t pix_byte = sgl_pixmal_get_pixel_bytes(pm);
    uint32_t lines = SGL_IMG_BUFFER_SIZE;
    uint32_t cap;

    if (lines == 0) {
        lines = pm->height;
    }
    cap = (pix_byte + 1u) * pm->width * lines;
    if (cap < 8u)     cap = 8u;
    if (cap > 0xFFFFu) cap = 0xFFFFu;
    return (uint16_t)cap;
}

/**
 * @brief fetch one byte of the RLE compressed stream at img->index, advancing
 *        the cursor. When an external read op is present, bytes are served from
 *        a block cache in flash_buffer (reusing buffer_lines / buffer_line as the
 *        fill size / read offset) so the flash is touched only once per cache
 *        block instead of once per token.
 */
static inline uint8_t rle_stream_fetch(sgl_img_t *img, uintptr_t start_addr)
{
    if (img->read == NULL) {
        /* memory-mapped source: read straight from the array */
        return ((const uint8_t*)img->pixmap[img->pixmap_idx].bitmap.array)[img->index++];
    }
    if (img->flash_buffer == NULL) {
        /* cache unavailable: fall back to a single-byte read */
        uint8_t b = 0;
        img->read(start_addr + img->index, &b, 1);
        img->index++;
        return b;
    }
    if (img->buffer_line >= img->buffer_lines) {
        /* cache exhausted: prefetch a fresh block at the current stream position */
        uint16_t cap = rle_cache_capacity(img);
        img->read(start_addr + img->index, img->flash_buffer, cap);
        img->buffer_line = 0;
        img->buffer_lines = cap;
    }
    img->index++;
    return img->flash_buffer[img->buffer_line++];
}

static inline void rle_decompress_line(sgl_img_t *img, sgl_area_t *coords, sgl_area_t *area, sgl_color_t *out)
{
    uintptr_t start_addr = img->pixmap[img->pixmap_idx].bitmap.addr;
    uint8_t format = img->pixmap[img->pixmap_idx].format;
    uint32_t pix_value;
    uint8_t v0, v1, v2, v3;

    for (int i = coords->x1; i <= coords->x2; i++) {
        if (img->remainder == 0) {
            /* first byte of a token is the run-length header */
            img->remainder = rle_stream_fetch(img, start_addr);

            switch (format) {
            case SGL_PIXMAP_FMT_RLE_RGB332:
                pix_value = rle_stream_fetch(img, start_addr);
                img->color = sgl_rgb332_to_color(pix_value);
                img->pix_alpha = SGL_ALPHA_MAX;
                break;
            case SGL_PIXMAP_FMT_RLE_ARGB2222:
                pix_value = rle_stream_fetch(img, start_addr);
                img->color = sgl_rgb222_to_color(pix_value);
                img->pix_alpha = sgl_opa2_table[pix_value >> 6];
                break;
            case SGL_PIXMAP_FMT_RLE_RGB565:
                v0 = rle_stream_fetch(img, start_addr);
                v1 = rle_stream_fetch(img, start_addr);
                pix_value = v0 | (v1 << 8);
                img->color = sgl_rgb565_to_color(pix_value);
                img->pix_alpha = SGL_ALPHA_MAX;
                break;
            case SGL_PIXMAP_FMT_RLE_ARGB4444:
                v0 = rle_stream_fetch(img, start_addr);
                v1 = rle_stream_fetch(img, start_addr);
                pix_value = v0 | (v1 << 8);
                img->color = sgl_rgb444_to_color(pix_value);
                img->pix_alpha = sgl_opa4_table[pix_value >> 12];
                break;
            case SGL_PIXMAP_FMT_RLE_RGB888:
                v0 = rle_stream_fetch(img, start_addr);
                v1 = rle_stream_fetch(img, start_addr);
                v2 = rle_stream_fetch(img, start_addr);
                pix_value = v0 | (v1 << 8) | (v2 << 16);
                img->color = sgl_rgb888_to_color(pix_value);
                img->pix_alpha = SGL_ALPHA_MAX;
                break;
            case SGL_PIXMAP_FMT_RLE_ARGB8888:
                v0 = rle_stream_fetch(img, start_addr);
                v1 = rle_stream_fetch(img, start_addr);
                v2 = rle_stream_fetch(img, start_addr);
                v3 = rle_stream_fetch(img, start_addr);
                pix_value = v0 | (v1 << 8) | (v2 << 16);
                img->color = sgl_rgb888_to_color(pix_value);
                img->pix_alpha = v3;
                break;
            default:
                break;
            }
        }

        if (out != NULL && i >= area->x1 && i <= area->x2) {
            img->color = (img->pix_alpha == SGL_ALPHA_MAX ? img->color : sgl_color_mixer(img->color, *out, img->pix_alpha));
            *out = (img->alpha == SGL_ALPHA_MAX ? img->color : sgl_color_mixer(img->color, *out, img->alpha));
            out ++;
        }
        img->remainder --;
    }
}

static void sgl_img_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    sgl_img_t *img = sgl_container_of(obj, sgl_img_t, obj);
    const sgl_pixmap_t *pixmap;
    uintptr_t read_addr;
    uint8_t pix_byte;
    sgl_color_t tmp_color, *buf = NULL, *blend = NULL;
    uint8_t *pixmap_buf;
    size_t offset = 0;
    uint8_t format;
    uint8_t ga = img->alpha;
    sgl_area_t area;

    if (img->pixmap == NULL) {
        return;
    }
    pixmap = &img->pixmap[img->pixmap_idx];
    read_addr = pixmap->bitmap.addr;
    pix_byte = sgl_pixmal_get_pixel_bytes(pixmap);
    pixmap_buf = (uint8_t*)pixmap->bitmap.array;
    format = pixmap->format;

    area.x1 = obj->coords.x1;
    area.y1 = obj->coords.y1;
    area.x2 = obj->coords.x1 + pixmap->width - 1;
    area.y2 = obj->coords.y1 + pixmap->height - 1;

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        if (!sgl_surf_clip(surf, &obj->area, &clip)) {
            return;
        }

        /* QOI-RGB565 block decode: the per-line offset table is built once
         * per source image (keyed by qoi_src), not on every draw. flash_buffer
         * serves as the single-line decode buffer */
        uint8_t qoi_initialized = 0;
        const uint8_t *qoi_data = NULL;
        if (format == SGL_PIXMAP_FMT_QOI_RGB565) {
            const uint8_t *qoi_src = (const uint8_t*)pixmap->bitmap.array;
            if (qoi_src != NULL) {
                if (img->qoi_src != qoi_src) {
                    uint16_t qoi_h = qoi_read_u16(qoi_src + QOI_HDR_OFF_H);
                    if (img->qoi_line_offsets != NULL) {
                        sgl_free(img->qoi_line_offsets);
                        img->qoi_line_offsets = NULL;
                    }
                    if (qoi_h != 0)
                        img->qoi_line_offsets = (uint32_t*)sgl_malloc(sizeof(uint32_t) * qoi_h);
                    if (img->qoi_line_offsets != NULL && qoi565_init(qoi_src, img->qoi_line_offsets) == 0) {
                        img->qoi_src = qoi_src;
                    }
                    else if (img->qoi_line_offsets != NULL) {
                        sgl_free(img->qoi_line_offsets);
                        img->qoi_line_offsets = NULL;
                    }
                }
                if (img->qoi_src == qoi_src &&
                    img_flash_buf_ensure(img, 2u * pixmap->width) != NULL) {
                    uint32_t idx_total = qoi_read_u16(qoi_src + QOI_HDR_OFF_U16_IDX_SZ)
                                       + qoi_read_u16(qoi_src + QOI_HDR_OFF_U24_IDX_SZ)
                                       + qoi_read_u16(qoi_src + QOI_HDR_OFF_U32_IDX_SZ);
                    qoi_data = qoi_src + 13 + idx_total;
                    qoi_initialized = 1;
                }
            }
        }

        if (qoi_initialized) {
            uint16_t clip_w = (uint16_t)(clip.x2 - clip.x1 + 1);
            uint32_t skip_pix = (uint32_t)(clip.x1 - area.x1);
            uint32_t want = skip_pix + clip_w;
            uint8_t *line_buf = img->flash_buffer;
#if (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB565)
            uint16_t row_width_bytes = 2 * clip_w;
#endif

            if (want > pixmap->width) want = pixmap->width;

            buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
            for (int y = clip.y1; y <= clip.y2; y++) {
                const uint8_t *lp = qoi_data + img->qoi_line_offsets[y - area.y1];
#if (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB565)
                if (ga == SGL_ALPHA_MAX && skip_pix == 0) {
                    qoi565_decode_line((uint8_t*)buf, lp, want);
                }
                else
#endif
                {
                    qoi565_decode_line(line_buf, lp, want);
#if (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB565)
                    if (ga == SGL_ALPHA_MAX) {
                        memcpy(buf, line_buf + skip_pix * 2, row_width_bytes);
                    }
                    else
#endif
                    {
                        const uint8_t *src = line_buf + skip_pix * 2;
                        blend = buf;
                        for (int x = 0; x < clip_w; x++) {
                            tmp_color = sgl_rgb565_to_color(src[0] | (src[1] << 8));
                            src += 2;
                            *blend = (ga == SGL_ALPHA_MAX) ? tmp_color
                                : sgl_color_mixer(tmp_color, *blend, ga);
                            blend ++;
                        }
                    }
                }
                buf += surf->w;
            }
        }
        else if (format < SGL_PIXMAP_FMT_RLE_RGB332) {
            if (img->read != NULL) {
                /* Calculate buffer lines: 0 means use full clip height, otherwise use configured value capped at clip height */
                uint16_t buf_lines = SGL_IMG_BUFFER_SIZE;
                if (buf_lines == 0) {
                    buf_lines = (uint16_t)(clip.y2 - clip.y1 + 1);
                }
                else if (buf_lines > (uint16_t)(clip.y2 - clip.y1 + 1)) {
                    buf_lines = (uint16_t)(clip.y2 - clip.y1 + 1);
                }
                img->buffer_lines = buf_lines;

                if (img_flash_buf_ensure(img, (uint32_t)pix_byte * (clip.x2 - clip.x1 + 1) * buf_lines) == NULL) {
                    return;
                }
                pixmap_buf = img->flash_buffer;
            }
            else {
                img->buffer_lines = 1;
            }

            buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
            uint16_t row_width_bytes = pix_byte * (uint16_t)(clip.x2 - clip.x1 + 1);
            uint16_t buf_lines = img->buffer_lines;
            uint16_t lines_in_buf = 0;

            if (img->read == NULL && ga == SGL_ALPHA_MAX &&
                ((CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB332 && pixmap->format == SGL_PIXMAP_FMT_RGB332) ||
                 (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB565 && pixmap->format == SGL_PIXMAP_FMT_RGB565) ||
                 (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB888 && pixmap->format == SGL_PIXMAP_FMT_RGB888))) {
                const uint8_t *src = (const uint8_t*)pixmap->bitmap.array;
                for (int y = clip.y1; y <= clip.y2; y++) {
                    memcpy(buf, src + (((size_t)(y - area.y1) * pixmap->width + (clip.x1 - area.x1)) * pix_byte), row_width_bytes);
                    buf += surf->w;
                }
                goto draw_done;
            }

#define DRAW_Y_HEAD()                                                        \
            for (int y = clip.y1; y <= clip.y2; y++) {                       \
                if (lines_in_buf == 0) {                                     \
                    uint16_t _ltr = buf_lines;                               \
                    if ((uint16_t)(clip.y2 - y + 1) < _ltr)                  \
                        _ltr = (uint16_t)(clip.y2 - y + 1);                  \
                    offset = (((y - area.y1) * pixmap->width)                \
                        + (clip.x1 - area.x1)) * pix_byte;                   \
                    if (img->read != NULL) {                                 \
                        if ((uint32_t)row_width_bytes                        \
                            == (uint32_t)pixmap->width * pix_byte) {         \
                            img->read(read_addr + offset, pixmap_buf,        \
                                row_width_bytes * _ltr);                     \
                        } else {                                             \
                            for (uint16_t _i = 0; _i < _ltr; _i++) {         \
                                img->read(read_addr + offset                 \
                                    + (size_t)_i * pixmap->width * pix_byte, \
                                    pixmap_buf + (size_t)_i * row_width_bytes, \
                                    row_width_bytes);                        \
                            }                                                \
                        }                                                    \
                        img->buffer_line = (uint16_t)(y - clip.y1);          \
                    } else {                                                 \
                        pixmap_buf = (uint8_t*)pixmap->bitmap.array + offset;\
                    }                                                        \
                    lines_in_buf = _ltr;                                     \
                }                                                            \
                blend = buf;                                                 \
                offset = (img->read != NULL)                                 \
                    ? (uint16_t)(y - clip.y1 - img->buffer_line) * row_width_bytes : 0

#define DRAW_Y_TAIL()                                                        \
                buf += surf->w;                                              \
                lines_in_buf --;                                             \
            }

#define DRAW_ROW_NATIVE(bpb)                                                      \
                    if (ga == SGL_ALPHA_MAX &&                                    \
                        CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB##bpb) {     \
                        memcpy(blend, pixmap_buf + offset, row_width_bytes);      \
                    }                                                             \
                    else

            switch (pixmap->format) {
            case SGL_PIXMAP_FMT_RGB332:
                DRAW_Y_HEAD();
                    DRAW_ROW_NATIVE(332)
                    for (int x = clip.x1; x <= clip.x2; x++) {
                        tmp_color = sgl_rgb332_to_color(pixmap_buf[offset]);
                        *blend = (ga == SGL_ALPHA_MAX) ? tmp_color
                            : sgl_color_mixer(tmp_color, *blend, ga);
                        offset += 1; blend ++;
                    }
                DRAW_Y_TAIL();
                break;
            case SGL_PIXMAP_FMT_RGB565:
                DRAW_Y_HEAD();
                    DRAW_ROW_NATIVE(565)
                    for (int x = clip.x1; x <= clip.x2; x++) {
                        tmp_color = sgl_rgb565_to_color(pixmap_buf[offset] | (pixmap_buf[offset + 1] << 8));
                        *blend = (ga == SGL_ALPHA_MAX) ? tmp_color
                            : sgl_color_mixer(tmp_color, *blend, ga);
                        offset += 2; blend ++;
                    }
                DRAW_Y_TAIL();
                break;
            case SGL_PIXMAP_FMT_ARGB2222:
                DRAW_Y_HEAD();
                    for (int x = clip.x1; x <= clip.x2; x++) {
                        uint8_t _v = pixmap_buf[offset];
                        tmp_color = sgl_rgb222_to_color(_v);
                        tmp_color = sgl_color_mixer(tmp_color, *blend, sgl_opa2_table[_v >> 6]);
                        *blend = (ga == SGL_ALPHA_MAX) ? tmp_color
                            : sgl_color_mixer(tmp_color, *blend, ga);
                        offset += 1; blend ++;
                    }
                DRAW_Y_TAIL();
                break;
            case SGL_PIXMAP_FMT_ARGB4444:
                DRAW_Y_HEAD();
                    for (int x = clip.x1; x <= clip.x2; x++) {
                        uint16_t _v = pixmap_buf[offset] | (pixmap_buf[offset + 1] << 8);
                        tmp_color = sgl_rgb444_to_color(_v);
                        tmp_color = sgl_color_mixer(tmp_color, *blend, sgl_opa4_table[_v >> 12]);
                        *blend = (ga == SGL_ALPHA_MAX) ? tmp_color
                            : sgl_color_mixer(tmp_color, *blend, ga);
                        offset += 2; blend ++;
                    }
                DRAW_Y_TAIL();
                break;
            case SGL_PIXMAP_FMT_RGB888:
                DRAW_Y_HEAD();
                    DRAW_ROW_NATIVE(888)
                    for (int x = clip.x1; x <= clip.x2; x++) {
                        tmp_color = sgl_rgb888_to_color(
                            pixmap_buf[offset] | (pixmap_buf[offset + 1] << 8) | (pixmap_buf[offset + 2] << 16));
                        *blend = (ga == SGL_ALPHA_MAX) ? tmp_color
                            : sgl_color_mixer(tmp_color, *blend, ga);
                        offset += 3; blend ++;
                    }
                DRAW_Y_TAIL();
                break;
            case SGL_PIXMAP_FMT_ARGB8888:
                DRAW_Y_HEAD();
                    for (int x = clip.x1; x <= clip.x2; x++) {
                        tmp_color = sgl_rgb888_to_color(
                            pixmap_buf[offset] | (pixmap_buf[offset + 1] << 8) | (pixmap_buf[offset + 2] << 16));
                        tmp_color = sgl_color_mixer(tmp_color, *blend, pixmap_buf[offset + 3]);
                        *blend = (ga == SGL_ALPHA_MAX) ? tmp_color
                            : sgl_color_mixer(tmp_color, *blend, ga);
                        offset += 4; blend ++;
                    }
                DRAW_Y_TAIL();
                break;
            default:
                break;
            }

#undef DRAW_Y_HEAD
#undef DRAW_Y_TAIL
#undef DRAW_ROW_NATIVE
draw_done: ;
        }
        else {
            /* RLE pixmap support: prefetch cache reuses flash_buffer plus the
             * buffer_lines / buffer_line counters (fill size / read offset). */
            if (img->read != NULL && img_flash_buf_ensure(img, rle_cache_capacity(img)) == NULL) {
                return;
            }

            if (clip.y1 == surf->dirty->y1 || clip.y1 == obj->area.y1) {
                img->index = 0;
                img->remainder = 0;
                img->buffer_line = 0;   /* mark cache empty: force refetch from stream start */
                img->buffer_lines = 0;
                for (int y = area.y1; y < clip.y1; y++) {
                    rle_decompress_line(img, &area, &clip, NULL);
                }
            }

            buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, (clip.y1 - surf->y1));

            for (int y = clip.y1; y <= clip.y2; y++) {
                rle_decompress_line(img, &area, &clip, buf);
                buf += surf->w;
            }
        }

        if (img->pixmap_auto && sgl_obj_draw_complete(obj, surf, &clip)) {
            uint32_t pixmap_idx = img->pixmap_idx + 1;
            img->pixmap_idx = pixmap_idx >= img->pixmap_num ? 0 : pixmap_idx;
            sgl_update_area(&area);
        }
    }
    else if (evt->type == SGL_EVENT_DESTROYED) {
        if (img->flash_buffer != NULL) {
            sgl_free(img->flash_buffer);
            img->flash_buffer = NULL;
            img->flash_buf_size = 0;
        }
        if (img->qoi_line_offsets != NULL) {
            sgl_free(img->qoi_line_offsets);
            img->qoi_line_offsets = NULL;
        }
        img->qoi_src = NULL;
    }
}

/**
 * @brief create an img object
 * @param parent parent of the img
 * @return img object
 */
sgl_obj_t* sgl_img_create(sgl_obj_t* parent)
{
    sgl_img_t *img = sgl_malloc(sizeof(sgl_img_t));
    if(img == NULL) {
        SGL_LOG_ERROR("sgl_img_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(img, 0, sizeof(sgl_img_t));

    sgl_obj_t *obj = &img->obj;
    sgl_obj_init(&img->obj, parent);
    obj->construct_fn = sgl_img_construct_cb;

    img->alpha = SGL_ALPHA_MAX;
    img->pixmap_idx = 0;
    img->pixmap_num = 1;
    img->pixmap_auto = 0;

    return obj;
}

/**
 * @brief set img pixmap
 * @param obj img object
 * @param pixmap img pixmap
 * @return none
 */
void sgl_img_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    SGL_ASSERT(obj != NULL);
    sgl_img_t *img = (sgl_img_t*)obj;
    img->pixmap = pixmap;
    /* drop all decoder caches: they are sized/keyed for the old pixmap */
    if (img->flash_buffer != NULL) {
        sgl_free(img->flash_buffer);
        img->flash_buffer = NULL;
        img->flash_buf_size = 0;
    }
    if (img->qoi_line_offsets != NULL) {
        sgl_free(img->qoi_line_offsets);
        img->qoi_line_offsets = NULL;
    }
    img->qoi_src = NULL;
    img->index = 0;
    img->remainder = 0;
    img->buffer_line = 0;
    img->buffer_lines = 0;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set img read operation
 * @param obj img object
 * @param read img read operation
 * @return none
 */
void sgl_img_set_read_ops(sgl_obj_t *obj, void (*read)(const size_t addr, uint8_t *out, uint32_t len_bytes))
{
    SGL_ASSERT(obj != NULL);
    ((sgl_img_t*)obj)->read = read;
}

/**
 * @brief set img alpha
 * @param obj img object
 * @param alpha img alpha
 * @return none
 */
void sgl_img_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    SGL_ASSERT(obj != NULL);
    ((sgl_img_t*)obj)->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set img pixmap number
 * @param obj img object
 * @param num img pixmap number
 * @param auto_refresh img pixmap auto refresh
 * @return none
 * @note if auto_refresh is true, the img will refresh automatically after pixmap flush conplete
 * @warning the num max is 255
 */
void sgl_img_set_pixmap_num(sgl_obj_t *obj, uint8_t num, bool auto_refresh)
{
    SGL_ASSERT(obj != NULL);
    sgl_img_t *img = (sgl_img_t*)obj;
    img->pixmap_num = num;
    img->pixmap_auto = (uint8_t)auto_refresh;
    if (num != 0 && img->pixmap_idx >= num)
        img->pixmap_idx = num - 1;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set img next pixmap
 * @param obj img object
 * @return none
 */
void sgl_img_set_pixmap_next(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_img_t *img = sgl_container_of(obj, sgl_img_t, obj);
    uint32_t pixmap_idx = img->pixmap_idx + 1;
    img->pixmap_idx = pixmap_idx >= img->pixmap_num ? 0 : pixmap_idx;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set img pixmap current index
 * @param obj img object
 * @param index img pixmap index
 * @return none
 */
void sgl_img_set_pixmap_index(sgl_obj_t *obj, uint8_t index)
{
    SGL_ASSERT(obj != NULL);
    sgl_img_t *img = sgl_container_of(obj, sgl_img_t, obj);
    if (img->pixmap_num == 0)
        return;
    img->pixmap_idx = sgl_min(index, img->pixmap_num - 1);
    sgl_obj_set_dirty(obj);
}
