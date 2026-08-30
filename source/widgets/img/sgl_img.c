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
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_img.h"

/* ==================== QOI-RGB565 decoder ==================== */
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

/* QOI image info - static storage */
static const uint8_t *s_qoi_src_data = NULL;
static uint16_t s_qoi_width   = 0;
static uint16_t s_qoi_height  = 0;
static uint16_t s_qoi_u16_idx = 0;
static uint16_t s_qoi_u24_idx = 0;
static uint16_t s_qoi_u32_idx = 0;
static const uint8_t *s_qoi_data_start = NULL;

/* Per-line offset table - points to the owning img's qoi_line_offsets,
 * allocated on demand only while a QOI pixmap is drawn */
static uint32_t *s_qoi_line_offsets = NULL;
static uint8_t s_qoi_line_offsets_ready = 0;

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
 */
static uint8_t qoi565_init(const uint8_t *src, uint32_t *offsets)
{
    if (src == NULL || offsets == NULL) return 1;

    s_qoi_width     = qoi_read_u16(src + QOI_HDR_OFF_W);
    s_qoi_height    = qoi_read_u16(src + QOI_HDR_OFF_H);
    s_qoi_u16_idx   = qoi_read_u16(src + QOI_HDR_OFF_U16_IDX_SZ);
    s_qoi_u24_idx   = qoi_read_u16(src + QOI_HDR_OFF_U24_IDX_SZ);
    s_qoi_u32_idx   = qoi_read_u16(src + QOI_HDR_OFF_U32_IDX_SZ);

    if (s_qoi_width == 0 || s_qoi_height == 0 || s_qoi_width > 240) {
        return 2;
    }

    uint32_t idx_total = s_qoi_u16_idx + s_qoi_u24_idx + s_qoi_u32_idx;
    if (idx_total > 1024) return 3;

    s_qoi_src_data = src;
    s_qoi_data_start = src + 13 + idx_total;

    /* Pre-compute offsets of all lines */
    uint32_t ptr_idx = 13;
    for (uint32_t i = 0; i < s_qoi_height; i++) {
        if (i < s_qoi_u16_idx / 2) {
            offsets[i] = qoi_read_u16(src + ptr_idx);
            ptr_idx += 2;
        } else if (i < (s_qoi_u16_idx / 2 + s_qoi_u24_idx / 3)) {
            offsets[i] = qoi_read_u24(src + ptr_idx);
            ptr_idx += 3;
        } else {
            offsets[i] = qoi_read_u32(src + ptr_idx);
            ptr_idx += 4;
        }
    }
    s_qoi_line_offsets = offsets;
    s_qoi_line_offsets_ready = 1;

    return 0;
}

/**
 * @brief decode one QOI line - using pre-computed offset table
 */
static uint8_t qoi565_decode_line(uint16_t line_idx, uint8_t *dst)
{
    if (s_qoi_src_data == NULL || dst == NULL) return 1;
    if (line_idx >= s_qoi_height || !s_qoi_line_offsets_ready) return 2;

    /* Use pre-computed offset */
    const uint8_t *p = s_qoi_data_start + s_qoi_line_offsets[line_idx];
    uint32_t line_pix = s_qoi_width;
    uint8_t *out_ptr = dst;
    uint8_t pr = 0, pg = 0, pb = 0;

    while (line_pix > 0) {
        uint8_t op = *p++;

        /* RGB565 full color */
        if (op == QOI_OP_RGB565) {
            uint8_t b1 = *p++;
            uint8_t b2 = *p++;
            pr = (b1 >> 3) & 0x1F;
            pg = ((b1 & 0x07) << 3) | ((b2 >> 5) & 0x07);
            pb = b2 & 0x1F;
            *out_ptr++ = b2;
            *out_ptr++ = b1;
            line_pix--;
        }
        /* RUN repeat */
        else if ((op & 0xC0) == QOI_OP_RUN) {
            uint8_t run_len = (op & 0x3F) + 1;
            uint8_t hi = (pr << 3) | (pg >> 3);
            uint8_t lo = ((pg & 0x07) << 5) | pb;
            while (run_len-- > 0 && line_pix > 0) {
                *out_ptr++ = lo;
                *out_ptr++ = hi;
                line_pix--;
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
            *out_ptr++ = ((pg & 0x07) << 5) | pb;
            *out_ptr++ = (pr << 3) | (pg >> 3);
            line_pix--;
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
            *out_ptr++ = ((pg & 0x07) << 5) | pb;
            *out_ptr++ = (pr << 3) | (pg >> 3);
            line_pix--;
        }
        /* Invalid opcode */
        else {
            *out_ptr++ = 0x00;
            *out_ptr++ = 0x00;
            line_pix--;
        }
    }
    return 0;
}

static inline void img_rle_init(sgl_img_t *img)
{
    SGL_ASSERT(img != NULL);
    img->index = 0;
    img->remainder = 0;
}

static inline void rle_decompress_line(sgl_img_t *img, sgl_area_t *coords, sgl_area_t *area, sgl_color_t *out)
{
    uint8_t tmp_buf[8];
    uint8_t* read_ptr = NULL;
    uint8_t* start_ptr = (uint8_t*)img->pixmap[img->pixmap_idx].bitmap.array;
    uintptr_t start_addr = img->pixmap[img->pixmap_idx].bitmap.addr;
    uint8_t format = img->pixmap->format;
    uint32_t pix_value;

    for (int i = coords->x1; i <= coords->x2; i++) {
        if (img->remainder == 0) {
            if (img->read != NULL) {
                read_ptr = tmp_buf;
                img->read(start_addr + img->index, tmp_buf, sizeof(tmp_buf));
            }
            else {
                read_ptr = start_ptr + img->index;
            }

            img->index ++;
            img->remainder = read_ptr[0];

            switch (format) {
            case SGL_PIXMAP_FMT_RLE_RGB332:
                pix_value = read_ptr[1];
                img->color = sgl_rgb332_to_color(pix_value);
                img->pix_alpha = SGL_ALPHA_MAX;
                img->index ++;
                break;
            case SGL_PIXMAP_FMT_RLE_ARGB2222:
                pix_value = read_ptr[1];
                img->color = sgl_rgb222_to_color(pix_value);
                img->pix_alpha = sgl_opa2_table[pix_value >> 6];
                img->index ++;
                break;
            case SGL_PIXMAP_FMT_RLE_RGB565:
                pix_value = read_ptr[1] | (read_ptr[2] << 8);
                img->color = sgl_rgb565_to_color(pix_value);
                img->pix_alpha = SGL_ALPHA_MAX;
                img->index += 2;
                break;
            case SGL_PIXMAP_FMT_RLE_ARGB4444:
                pix_value = read_ptr[1] | (read_ptr[2] << 8);
                img->color = sgl_rgb444_to_color(pix_value);
                img->pix_alpha = sgl_opa4_table[pix_value >> 12];
                img->index += 2;
                break;
            case SGL_PIXMAP_FMT_RLE_RGB888:
                pix_value = read_ptr[1] | (read_ptr[2] << 8) | (read_ptr[3] << 16);
                img->color = sgl_rgb888_to_color(pix_value);
                img->pix_alpha = SGL_ALPHA_MAX;
                img->index += 3;
                break;
            case SGL_PIXMAP_FMT_RLE_ARGB8888:
                pix_value = read_ptr[1] | (read_ptr[2] << 8) | (read_ptr[3] << 16);
                img->color = sgl_rgb888_to_color(pix_value);
                img->pix_alpha = read_ptr[4];
                img->index += 4;
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
    };
}

static void sgl_img_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    sgl_img_t *img = sgl_container_of(obj, sgl_img_t, obj);
    const sgl_pixmap_t *pixmap = &img->pixmap[img->pixmap_idx];
    uintptr_t read_addr = pixmap->bitmap.addr;
    uint8_t pix_byte = sgl_pixmal_get_pixel_bytes(pixmap);
    sgl_color_t tmp_color, *buf = NULL, *blend = NULL;
    uint8_t *pixmap_buf = (uint8_t*)pixmap->bitmap.array;
    size_t offset = 0;
    uint8_t format = pixmap->format;
    uint8_t ga = img->alpha;

    sgl_area_t area = {
        .x1 = obj->coords.x1,
        .y1 = obj->coords.y1,
        .x2 = obj->coords.x1 + pixmap->width - 1,
        .y2 = obj->coords.y1 + pixmap->height - 1,
    };

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        if (!sgl_surf_clip(surf, &obj->area, &clip)) {
            return;
        }

        /* QOI-RGB565 block decode: decode line by line into RGB565,
         * flash_buffer is used as the single-line decode buffer */
        uint8_t qoi_initialized = 0;
        if (format == SGL_PIXMAP_FMT_QOI_RGB565) {
            const uint8_t *qoi_src = (const uint8_t*)pixmap->bitmap.array;
            /* Allocate the per-line offset table on demand (height entries) */
            if (img->qoi_line_offsets == NULL && qoi_src != NULL) {
                uint16_t h = qoi_read_u16(qoi_src + QOI_HDR_OFF_H);
                if (h != 0) {
                    img->qoi_line_offsets = (uint32_t*)sgl_malloc(sizeof(uint32_t) * h);
                }
            }
            if (img->qoi_line_offsets != NULL && qoi565_init(qoi_src, img->qoi_line_offsets) == 0) {
                qoi_initialized = 1;
                pix_byte = 2;
                format = SGL_PIXMAP_FMT_RGB565;
                if (img->flash_buffer == NULL) {
                    img->flash_buffer = (uint8_t*)sgl_malloc(2 * s_qoi_width);
                }
            }
        }

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

            if (img->flash_buffer == NULL) {
                img->flash_buffer = (uint8_t*)sgl_malloc(pix_byte * (clip.x2 - clip.x1 + 1) * buf_lines);
            }
            pixmap_buf = img->flash_buffer;
        }
        else {
            /* No external read: use 1-line buffer mode, read directly from memory */
            img->buffer_lines = 1;
        }

        if (format < SGL_PIXMAP_FMT_RLE_RGB332) {
            buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
            uint16_t row_width_bytes = pix_byte * (uint16_t)(clip.x2 - clip.x1 + 1);
            uint16_t buf_lines = img->buffer_lines;
            uint16_t lines_in_buf = 0;

            if (qoi_initialized && img->flash_buffer != NULL) {
                /* QOI block decode: decode current line then copy visible columns.
                 * Blend only when global alpha < 255 (decoded pixels are opaque). */
                uint8_t *line_buf = img->flash_buffer;
                for (int y = clip.y1; y <= clip.y2; y++) {
                    qoi565_decode_line(y - area.y1, line_buf);
                    if (ga == SGL_ALPHA_MAX) {
                        memcpy(buf, line_buf + (clip.x1 - area.x1) * 2, row_width_bytes);
                    } else {
                        blend = buf;
                        const uint8_t *src = line_buf + (clip.x1 - area.x1) * 2;
                        for (int x = clip.x1; x <= clip.x2; x++) {
                            tmp_color = sgl_rgb565_to_color(src[0] | (src[1] << 8));
                            src += 2;
                            *blend = sgl_color_mixer(tmp_color, *blend, ga);
                            blend ++;
                        }
                    }
                    buf += surf->w;
                }
                goto qoi_draw_done;
            }

/* Y-loop header: refill buffer, compute blend/offset for current row.
 * Usage: DRAW_Y_HEAD() followed by a for(x...) pixel loop, then DRAW_Y_TAIL() */
#define DRAW_Y_HEAD()                                                        \
            for (int y = clip.y1; y <= clip.y2; y++) {                       \
                if (lines_in_buf == 0) {                                     \
                    uint16_t _ltr = buf_lines;                               \
                    if ((uint16_t)(clip.y2 - y + 1) < _ltr)                  \
                        _ltr = (uint16_t)(clip.y2 - y + 1);                  \
                    offset = (((y - area.y1) * pixmap->width)                \
                        + (clip.x1 - area.x1)) * pix_byte;                   \
                    if (img->read != NULL) {                                 \
                        img->read(read_addr + offset, pixmap_buf,            \
                            row_width_bytes * _ltr);                         \
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

            switch (pixmap->format) {
            case SGL_PIXMAP_FMT_RGB332:
                DRAW_Y_HEAD();
                    for (int x = clip.x1; x <= clip.x2; x++) {
                        tmp_color = sgl_rgb332_to_color(pixmap_buf[offset]);
                        *blend = (img->alpha == SGL_ALPHA_MAX) ? tmp_color
                            : sgl_color_mixer(tmp_color, *blend, img->alpha);
                        offset += 1; blend ++;
                    }
                DRAW_Y_TAIL();
                break;
            case SGL_PIXMAP_FMT_RGB565:
                DRAW_Y_HEAD();
                    for (int x = clip.x1; x <= clip.x2; x++) {
                        tmp_color = sgl_rgb565_to_color(pixmap_buf[offset] | (pixmap_buf[offset + 1] << 8));
                        *blend = (img->alpha == SGL_ALPHA_MAX) ? tmp_color
                            : sgl_color_mixer(tmp_color, *blend, img->alpha);
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
                        *blend = (img->alpha == SGL_ALPHA_MAX) ? tmp_color
                            : sgl_color_mixer(tmp_color, *blend, img->alpha);
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
                        *blend = (img->alpha == SGL_ALPHA_MAX) ? tmp_color
                            : sgl_color_mixer(tmp_color, *blend, img->alpha);
                        offset += 2; blend ++;
                    }
                DRAW_Y_TAIL();
                break;
            case SGL_PIXMAP_FMT_RGB888:
                DRAW_Y_HEAD();
                    for (int x = clip.x1; x <= clip.x2; x++) {
                        tmp_color = sgl_rgb888_to_color(
                            pixmap_buf[offset] | (pixmap_buf[offset + 1] << 8) | (pixmap_buf[offset + 2] << 16));
                        *blend = (img->alpha == SGL_ALPHA_MAX) ? tmp_color
                            : sgl_color_mixer(tmp_color, *blend, img->alpha);
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
                        *blend = (img->alpha == SGL_ALPHA_MAX) ? tmp_color
                            : sgl_color_mixer(tmp_color, *blend, img->alpha);
                        offset += 4; blend ++;
                    }
                DRAW_Y_TAIL();
                break;
            default:
                break;
            }

#undef DRAW_Y_HEAD
#undef DRAW_Y_TAIL
qoi_draw_done: ;
        }
        else {
            /* RLE pixmap support */
            if (clip.y1 == surf->dirty->y1 || clip.y1 == obj->area.y1) {
                img->index = 0;
                img->remainder = 0;
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
        }
        if (img->qoi_line_offsets != NULL) {
            /* Reset shared decoder state if it points to this img's table */
            if (s_qoi_line_offsets == img->qoi_line_offsets) {
                s_qoi_line_offsets = NULL;
                s_qoi_line_offsets_ready = 0;
                s_qoi_src_data = NULL;
            }
            sgl_free(img->qoi_line_offsets);
            img->qoi_line_offsets = NULL;
        }
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
    ((sgl_img_t*)obj)->pixmap = pixmap;
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
    ((sgl_img_t*)obj)->pixmap_num = num;
    ((sgl_img_t*)obj)->pixmap_auto = (uint8_t)auto_refresh;
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
    img->pixmap_idx = sgl_min(index, img->pixmap_num - 1);
    sgl_obj_set_dirty(obj);
}
