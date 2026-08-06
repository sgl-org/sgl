/* source/widgets/sgl_img_ext/sgl_img_ext.c
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
#include "sgl_img_ext.h"

/* Scale with correct rounding for negative values */
#define SGL_IMG_SCALE(val, sc) \
    ((val) >= 0 ? (((int64_t)(val) * (sc) + SGL_FIXED_ONE - 1) >> SGL_FIXED_SHIFT) \
                : (((int64_t)(val) * (sc) - SGL_FIXED_ONE + 1) >> SGL_FIXED_SHIFT))

/**
 * Decode a single pixel from the pixmap buffer.
 * Returns the decoded color and writes per-pixel opacity to *out_opa
 * (SGL_ALPHA_MAX for opaque formats).
 */
static inline sgl_color_t decode_pixel(const sgl_pixmap_t *pixmap, const uint8_t *buf, size_t offset, uint8_t *out_opa)
{
    uint16_t pv;
    *out_opa = SGL_ALPHA_MAX;

    switch (pixmap->format) {
    case SGL_PIXMAP_FMT_RGB332:
        return sgl_rgb332_to_color(buf[offset]);
    case SGL_PIXMAP_FMT_RGB565:
        pv = buf[offset] | (buf[offset + 1] << 8);
        return sgl_rgb565_to_color(pv);
    case SGL_PIXMAP_FMT_ARGB2222:
        pv = buf[offset];
        *out_opa = sgl_opa2_table[pv >> 6];
        return sgl_rgb222_to_color(pv);
    case SGL_PIXMAP_FMT_ARGB4444:
        pv = buf[offset] | (buf[offset + 1] << 8);
        *out_opa = sgl_opa4_table[pv >> 12];
        return sgl_rgb444_to_color(pv);
    case SGL_PIXMAP_FMT_RGB888:
        pv = buf[offset] | (buf[offset + 1] << 8) | (buf[offset + 2] << 16);
        return sgl_rgb888_to_color(pv);
    case SGL_PIXMAP_FMT_ARGB8888:
        pv = buf[offset] | (buf[offset + 1] << 8) | (buf[offset + 2] << 16);
        *out_opa = buf[offset + 3];
        return sgl_rgb888_to_color(pv);
    default:
        return (sgl_color_t){0};
    }
}

/**
 * Blend a decoded pixel onto the destination, applying per-pixel and
 * global alpha as needed.
 */
static inline void blend_pixel(sgl_color_t *dst, sgl_color_t src, uint8_t pix_opa, uint8_t global_alpha)
{
    if (pix_opa != SGL_ALPHA_MAX) {
        src = sgl_color_mixer(src, *dst, pix_opa);
    }
    *dst = (global_alpha == SGL_ALPHA_MAX) ? src : sgl_color_mixer(src, *dst, global_alpha);
}

#if (CONFIG_SGL_PIXMAP_BILINEAR_INTERP)
/**
 * @brief Bilinear interpolation on raw pixmap data.
 * Decodes a 2x2 pixel neighborhood and interpolates using fixed-point
 * fractional coordinates, matching the logic of sgl_draw_biln_color.
 *
 * @param pixmap  Source pixmap descriptor
 * @param buf     Pixmap pixel buffer
 * @param fx      X coordinate in fixed-point (SGL_FIXED_SHIFT fraction bits)
 * @param fy      Y coordinate in fixed-point (SGL_FIXED_SHIFT fraction bits)
 * @param out_opa Pointer to receive the interpolated per-pixel opacity
 * @return Interpolated color
 */
static inline sgl_color_t pixmap_biln_color(const sgl_pixmap_t *pixmap,
                                             const uint8_t *buf,
                                             int32_t fx, int32_t fy,
                                             uint8_t *out_opa)
{
    int32_t w = (int32_t)pixmap->width;
    int32_t h = (int32_t)pixmap->height;

    /* Clamp to valid interpolation range [0, (w-1)<<SHIFT] */
    int32_t max_x = (w - 1) << SGL_FIXED_SHIFT;
    int32_t max_y = (h - 1) << SGL_FIXED_SHIFT;
    fx = (fx < 0) ? 0 : (fx > max_x) ? max_x : fx;
    fy = (fy < 0) ? 0 : (fy > max_y) ? max_y : fy;

    int32_t x0 = fx >> SGL_FIXED_SHIFT;
    int32_t y0 = fy >> SGL_FIXED_SHIFT;
    int32_t dx = fx & SGL_FIXED_MASK;
    int32_t dy = fy & SGL_FIXED_MASK;
    int32_t dx1 = SGL_FIXED_ONE - dx;
    int32_t dy1 = SGL_FIXED_ONE - dy;

    uint8_t pix_byte = sgl_pixmal_get_pixel_bytes(pixmap);

    /* Offsets for the 2x2 neighborhood */
    size_t off00 = ((size_t)y0 * w + x0) * pix_byte;
    size_t off01 = off00 + pix_byte;
    size_t off10 = off00 + (size_t)w * pix_byte;
    size_t off11 = off10 + pix_byte;

    uint8_t opa00, opa01, opa10, opa11;
    sgl_color_t p00 = decode_pixel(pixmap, buf, off00, &opa00);
    sgl_color_t p01 = decode_pixel(pixmap, buf, off01, &opa01);
    sgl_color_t p10 = decode_pixel(pixmap, buf, off10, &opa10);
    sgl_color_t p11 = decode_pixel(pixmap, buf, off11, &opa11);

    sgl_color_t ret;

    /* Interpolate R channel */
    int32_t r_y0 = (p00.ch.red * dx1 + p01.ch.red * dx) >> SGL_FIXED_SHIFT;
    int32_t r_y1 = (p10.ch.red * dx1 + p11.ch.red * dx) >> SGL_FIXED_SHIFT;
    ret.ch.red = (r_y0 * dy1 + r_y1 * dy) >> SGL_FIXED_SHIFT;

    /* Interpolate G channel */
    int32_t g_y0 = (p00.ch.green * dx1 + p01.ch.green * dx) >> SGL_FIXED_SHIFT;
    int32_t g_y1 = (p10.ch.green * dx1 + p11.ch.green * dx) >> SGL_FIXED_SHIFT;
    ret.ch.green = (g_y0 * dy1 + g_y1 * dy) >> SGL_FIXED_SHIFT;

    /* Interpolate B channel */
    int32_t b_y0 = (p00.ch.blue * dx1 + p01.ch.blue * dx) >> SGL_FIXED_SHIFT;
    int32_t b_y1 = (p10.ch.blue * dx1 + p11.ch.blue * dx) >> SGL_FIXED_SHIFT;
    ret.ch.blue = (b_y0 * dy1 + b_y1 * dy) >> SGL_FIXED_SHIFT;

    /* Interpolate per-pixel alpha */
    int32_t a_y0 = (opa00 * dx1 + opa01 * dx) >> SGL_FIXED_SHIFT;
    int32_t a_y1 = (opa10 * dx1 + opa11 * dx) >> SGL_FIXED_SHIFT;
    *out_opa = (a_y0 * dy1 + a_y1 * dy) >> SGL_FIXED_SHIFT;

    return ret;
}
#endif /* CONFIG_SGL_IMG_EXT_BILN */

/**
 * @brief Recompute obj->coords to the axis-aligned bounding box of the
 *        rotated + scaled image, using the pivot as rotation center.
 */
static void img_ext_update_coords(sgl_img_ext_t *img_ext)
{
    sgl_obj_t *obj = &img_ext->obj;

    if (!img_ext->pixmap) return;

    /* Resolve effective pivot (INT32_MIN = use image center) */
    int32_t pv_x = img_ext->pivot_x;
    int32_t pv_y = img_ext->pivot_y;
    if (pv_x == INT32_MIN && pv_y == INT32_MIN) {
        pv_x = img_ext->pixmap->width / 2;
        pv_y = img_ext->pixmap->height / 2;
    }

    /* screen_pivot = original widget position + pivot offset */
    int32_t sp_x = img_ext->orig_x1 + pv_x;
    int32_t sp_y = img_ext->orig_y1 + pv_y;

    int32_t scaled_w = SGL_IMG_SCALE(img_ext->pixmap->width, img_ext->scale_x);
    int32_t scaled_h = SGL_IMG_SCALE(img_ext->pixmap->height, img_ext->scale_y);
    if (scaled_w == 0) scaled_w = img_ext->pixmap->width;
    if (scaled_h == 0) scaled_h = img_ext->pixmap->height;

    if (img_ext->rotation == 0) {
        int32_t pv_sx = SGL_IMG_SCALE(pv_x, img_ext->scale_x);
        int32_t pv_sy = SGL_IMG_SCALE(pv_y, img_ext->scale_y);
        obj->coords.x1 = sp_x - pv_sx;
        obj->coords.y1 = sp_y - pv_sy;
        obj->coords.x2 = obj->coords.x1 + scaled_w + 2;
        obj->coords.y2 = obj->coords.y1 + scaled_h + 2;
    } else {
        int16_t rotation = sgl_mod360(img_ext->rotation);
        int32_t sin_val = sgl_sin(rotation);
        int32_t cos_val = sgl_cos(rotation);

        int32_t x0 = -pv_x,              y0 = -pv_y;
        int32_t x1 = img_ext->pixmap->width-1 - pv_x,  y1 = -pv_y;
        int32_t x2 = -pv_x,              y2 = img_ext->pixmap->height-1 - pv_y;
        int32_t x3 = img_ext->pixmap->width-1 - pv_x,  y3 = img_ext->pixmap->height-1 - pv_y;

        int32_t sx0 = SGL_IMG_SCALE(x0, img_ext->scale_x), sy0 = SGL_IMG_SCALE(y0, img_ext->scale_y);
        int32_t sx1 = SGL_IMG_SCALE(x1, img_ext->scale_x), sy1 = SGL_IMG_SCALE(y1, img_ext->scale_y);
        int32_t sx2 = SGL_IMG_SCALE(x2, img_ext->scale_x), sy2 = SGL_IMG_SCALE(y2, img_ext->scale_y);
        int32_t sx3 = SGL_IMG_SCALE(x3, img_ext->scale_x), sy3 = SGL_IMG_SCALE(y3, img_ext->scale_y);

        int32_t rx0 = ((int64_t)cos_val*sx0 - (int64_t)sin_val*sy0) >> SGL_FIXED_SHIFT;
        int32_t ry0 = ((int64_t)sin_val*sx0 + (int64_t)cos_val*sy0) >> SGL_FIXED_SHIFT;
        int32_t rx1 = ((int64_t)cos_val*sx1 - (int64_t)sin_val*sy1) >> SGL_FIXED_SHIFT;
        int32_t ry1 = ((int64_t)sin_val*sx1 + (int64_t)cos_val*sy1) >> SGL_FIXED_SHIFT;
        int32_t rx2 = ((int64_t)cos_val*sx2 - (int64_t)sin_val*sy2) >> SGL_FIXED_SHIFT;
        int32_t ry2 = ((int64_t)sin_val*sx2 + (int64_t)cos_val*sy2) >> SGL_FIXED_SHIFT;
        int32_t rx3 = ((int64_t)cos_val*sx3 - (int64_t)sin_val*sy3) >> SGL_FIXED_SHIFT;
        int32_t ry3 = ((int64_t)sin_val*sx3 + (int64_t)cos_val*sy3) >> SGL_FIXED_SHIFT;

        int32_t min_x = sgl_min(rx0, sgl_min(rx1, sgl_min(rx2, rx3)));
        int32_t max_x = sgl_max(rx0, sgl_max(rx1, sgl_max(rx2, rx3)));
        int32_t min_y = sgl_min(ry0, sgl_min(ry1, sgl_min(ry2, ry3)));
        int32_t max_y = sgl_max(ry0, sgl_max(ry1, sgl_max(ry2, ry3)));

        obj->coords.x1 = sp_x + min_x;
        obj->coords.y1 = sp_y + min_y;
        obj->coords.x2 = sp_x + max_x;
        obj->coords.y2 = sp_y + max_y;
    }
}

/**
 * @brief Construct callback for the image extension widget.
 * Handles two rendering paths:
 *   1. Simple blit when no rotation and no scaling (1:1 copy).
 *   2. Inverse-transform path for arbitrary rotation + scaling.
 */
static void sgl_img_ext_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_img_ext_t *img_ext = sgl_container_of(obj, sgl_img_ext_t, obj);
    if (!img_ext->pixmap) {
        return;
    }
    const sgl_pixmap_t *pixmap = img_ext->pixmap;
    uint8_t pix_byte = sgl_pixmal_get_pixel_bytes(pixmap);
    uint8_t *pixmap_buf = (uint8_t*)pixmap->bitmap.array;

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN: {
        /* Simple draw mode: no rotation, no scaling (centered on widget) */
        if (img_ext->rotation == 0 && img_ext->scale_x == SGL_FIXED_ONE && img_ext->scale_y == SGL_FIXED_ONE) {
            /* Compute draw area centered on the widget */
            int32_t center_x = (obj->coords.x1 + obj->coords.x2) / 2;
            int32_t center_y = (obj->coords.y1 + obj->coords.y2) / 2;
            int32_t start_x = center_x - pixmap->width / 2;
            int32_t start_y = center_y - pixmap->height / 2;

            sgl_area_t clip = SGL_AREA_INVALID;
            sgl_area_t area = {
                .x1 = start_x,
                .y1 = start_y,
                .x2 = start_x + pixmap->width - 1,
                .y2 = start_y + pixmap->height - 1,
            };

            if (!sgl_surf_clip(surf, &area, &clip)) {
                return;
            }

            if (img_ext->read != NULL) {
                if (img_ext->flash_buffer == NULL) {
                    img_ext->flash_buffer = (uint8_t*)sgl_malloc(pix_byte * (clip.x2 - clip.x1 + 1));
                }
                pixmap_buf = img_ext->flash_buffer;
            }

            sgl_color_t *buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);

            for (int y = clip.y1; y <= clip.y2; y++) {
                sgl_color_t *blend = buf;
                size_t offset = ((((y - area.y1) * pixmap->width) + (clip.x1 - area.x1)) * pix_byte);

                if (img_ext->read != NULL) {
                    size_t read_addr = (size_t)pixmap->bitmap.addr;
                    img_ext->read(read_addr + offset, pixmap_buf, pix_byte * (clip.x2 - clip.x1 + 1));
                    offset = 0;
                }

                for (int x = clip.x1; x <= clip.x2; x++) {
                    uint8_t pix_opa;
                    sgl_color_t tmp_color = decode_pixel(pixmap, pixmap_buf, offset, &pix_opa);
                    blend_pixel(blend, tmp_color, pix_opa, img_ext->alpha);
                    offset += pix_byte;
                    blend++;
                }
                buf += surf->w;
            }

            if (img_ext->read != NULL && pixmap_buf != NULL) {
                sgl_free(pixmap_buf);
            }
            return;
        }

        /* Rotation + scaling mode */
        int16_t rotation = sgl_mod360(img_ext->rotation);
        int32_t sin_val = sgl_sin(rotation);
        int32_t cos_val = sgl_cos(rotation);

        /* Screen pivot = original widget pos + pivot offset */
        int32_t pv_x = img_ext->pivot_x;
        int32_t pv_y = img_ext->pivot_y;
        if (pv_x == INT32_MIN && pv_y == INT32_MIN) {
            pv_x = pixmap->width / 2;
            pv_y = pixmap->height / 2;
        }
        int32_t sp_x = img_ext->orig_x1 + pv_x;
        int32_t sp_y = img_ext->orig_y1 + pv_y;

        sgl_area_t clip = SGL_AREA_INVALID;

        if (!sgl_surf_clip(surf, &obj->coords, &clip)) {
            return;
        }

        /* Allocate temporary buffer if external read callback is set */
        if (img_ext->read != NULL && img_ext->flash_buffer != NULL) {
            img_ext->flash_buffer = (uint8_t*)sgl_malloc(pix_byte * (clip.x2 - clip.x1 + 1));
            pixmap_buf = img_ext->flash_buffer;
        }

        sgl_color_t *dst = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);

        /* Compute inverse scale factors for reverse mapping */
        int32_t inv_scale_x = (SGL_FIXED_ONE << SGL_FIXED_SHIFT) / img_ext->scale_x;
        int32_t inv_scale_y = (SGL_FIXED_ONE << SGL_FIXED_SHIFT) / img_ext->scale_y;

        int32_t pivot_x_fixed = pv_x << SGL_FIXED_SHIFT;
        int32_t pivot_y_fixed = pv_y << SGL_FIXED_SHIFT;
        int32_t sp_x_fixed = sp_x << SGL_FIXED_SHIFT;
        int32_t sp_y_fixed = sp_y << SGL_FIXED_SHIFT;

#if (CONFIG_SGL_PIXMAP_BILINEAR_INTERP)
        /* Image bounds in fixed-point (for edge AA) */
        int32_t max_x_fp = (int32_t)pixmap->width  << SGL_FIXED_SHIFT;
        int32_t max_y_fp = (int32_t)pixmap->height << SGL_FIXED_SHIFT;
#endif

        for (int py = clip.y1; py <= clip.y2; py++) {
            sgl_color_t *blend = dst;

            for (int px = clip.x1; px <= clip.x2; px++, blend++) {
                /* Offset from screen pivot (fixed-point) */
                int32_t rel_x = ((int32_t)px << SGL_FIXED_SHIFT) - sp_x_fixed;
                int32_t rel_y = ((int32_t)py << SGL_FIXED_SHIFT) - sp_y_fixed;

                /* Inverse transform: first inverse-rotate, then inverse-scale */
                int32_t rx_rot = ((int64_t)cos_val * rel_x + (int64_t)sin_val * rel_y) >> SGL_FIXED_SHIFT;
                int32_t ry_rot = ((int64_t)-sin_val * rel_x + (int64_t)cos_val * rel_y) >> SGL_FIXED_SHIFT;

                int32_t rx = ((int64_t)rx_rot * inv_scale_x) >> SGL_FIXED_SHIFT;
                int32_t ry = ((int64_t)ry_rot * inv_scale_y) >> SGL_FIXED_SHIFT;

                int32_t src_x_fixed = rx + pivot_x_fixed;
                int32_t src_y_fixed = ry + pivot_y_fixed;

                /* Decode and blend pixel */
                if (pixmap->format <= SGL_PIXMAP_FMT_ARGB8888) {
#if (CONFIG_SGL_PIXMAP_BILINEAR_INTERP)
                    /* --- Edge anti-aliasing: compute sub-pixel coverage ---
                     * For pixels within 1 source-pixel of any image edge,
                     * compute a [0..1] coverage factor used to modulate alpha. */
                    if (src_x_fixed < 0 || src_x_fixed >= max_x_fp || src_y_fixed < 0 || src_y_fixed >= max_y_fp) {
                        continue;  /* Fully outside the image */
                    }

                    /* Signed distance (fixed-point) to each edge */
                    int32_t d_left   = src_x_fixed;
                    int32_t d_right  = max_x_fp - 1 - src_x_fixed;
                    int32_t d_top    = src_y_fixed;
                    int32_t d_bottom = max_y_fp - 1 - src_y_fixed;

                    /* Min distance, clamped to [0, FIXED_ONE] = [0, 1.0] */
                    int32_t edge_dist = sgl_min4(d_left, d_right, d_top, d_bottom);
                    edge_dist = (edge_dist < 0) ? 0 : (edge_dist > SGL_FIXED_ONE) ? SGL_FIXED_ONE : edge_dist;

                    /* Convert to 8-bit edge coverage [0..SGL_ALPHA_MAX] */
                    uint8_t edge_cov = (uint8_t)((edge_dist * SGL_ALPHA_MAX) >> SGL_FIXED_SHIFT);

                    /* Bilinear: clamp coords to valid 2x2 range for edge pixels */
                    int32_t bx = (src_x_fixed < 0) ? 0 : (src_x_fixed > max_x_fp - (1 << SGL_FIXED_SHIFT)) ? max_x_fp - (1 << SGL_FIXED_SHIFT) : src_x_fixed;
                    int32_t by = (src_y_fixed < 0) ? 0 : (src_y_fixed > max_y_fp - (1 << SGL_FIXED_SHIFT)) ? max_y_fp - (1 << SGL_FIXED_SHIFT) : src_y_fixed;

                    if (img_ext->read != NULL) {
                        size_t row_offset = (size_t)(by >> SGL_FIXED_SHIFT) * pixmap->width * pix_byte;
                        img_ext->read((size_t)(pixmap->bitmap.addr + row_offset),
                                      pixmap_buf, pix_byte * pixmap->width * 2);
                    }
                    uint8_t pix_opa;
                    sgl_color_t color = pixmap_biln_color(pixmap, pixmap_buf, bx, by, &pix_opa);
                    /* Apply edge coverage to per-pixel alpha */
                    pix_opa = (uint8_t)(((uint32_t)pix_opa * edge_cov) >> 8);
                    blend_pixel(blend, color, pix_opa, img_ext->alpha);
#else
                    /* Nearest-neighbor: single pixel sample, no edge AA */
                    int32_t src_x = src_x_fixed >> SGL_FIXED_SHIFT;
                    int32_t src_y = src_y_fixed >> SGL_FIXED_SHIFT;

                    if (src_x < 0 || src_x >= pixmap->width ||
                        src_y < 0 || src_y >= pixmap->height) {
                        continue;
                    }
                    if (img_ext->read != NULL) {
                        size_t row_offset = (size_t)src_y * pixmap->width * pix_byte;
                        img_ext->read((size_t)(pixmap->bitmap.addr + row_offset), pixmap_buf, pix_byte * pixmap->width);
                    }
                    size_t offset = (size_t)src_y * pixmap->width * pix_byte + (size_t)src_x * pix_byte;
                    uint8_t pix_opa;
                    sgl_color_t color = decode_pixel(pixmap, pixmap_buf, offset, &pix_opa);
                    if (pix_opa != SGL_ALPHA_MAX) {
                        *blend = sgl_color_mixer(color, *blend, pix_opa);
                    } else {
                        *blend = color;
                    }
#endif /* CONFIG_SGL_IMG_EXT_BILN */
                }
            }
            dst += surf->w;
        }
    }
    break;

    case SGL_EVENT_DESTROYED:
        if (img_ext->flash_buffer != NULL) {
            sgl_free(img_ext->flash_buffer);
        }
    break;
    default: return;
    }
}

/**
 * @brief Create a transformable image widget.
 * @param parent Parent object
 * @return New image extension object, or NULL on allocation failure
 */
sgl_obj_t* sgl_img_ext_create(sgl_obj_t* parent)
{
    sgl_img_ext_t *img_ext = sgl_malloc(sizeof(sgl_img_ext_t));
    if (img_ext == NULL) {
        SGL_LOG_ERROR("sgl_img_ext_create: malloc failed");
        return NULL;
    }

    memset(img_ext, 0, sizeof(sgl_img_ext_t));

    sgl_obj_t *obj = &img_ext->obj;
    sgl_obj_init(&img_ext->obj, parent);
    obj->construct_fn = sgl_img_ext_construct_cb;

    img_ext->alpha = SGL_ALPHA_MAX;
    img_ext->rotation = 0;
    img_ext->scale_x = SGL_FIXED_ONE;  // 1.0
    img_ext->scale_y = SGL_FIXED_ONE;  // 1.0
    img_ext->scale_uniform = true;
    img_ext->pivot_x = INT32_MIN;  /* sentinel: use image center */
    img_ext->pivot_y = INT32_MIN;

    return obj;
}

/**
 * @brief Set the pixmap source for the image widget.
 * @param obj Image extension object
 * @param pixmap Pixmap data to render
 */
void sgl_img_ext_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    SGL_ASSERT(obj != NULL);
    sgl_img_ext_t *img_ext = (sgl_img_ext_t*)obj;
    img_ext->pixmap = pixmap;

    /* Capture original widget position for pivot calculation */
    img_ext->orig_x1 = obj->coords.x1;
    img_ext->orig_y1 = obj->coords.y1;

    obj->coords.x2 = obj->coords.x1 + pixmap->width - 1;
    obj->coords.y2 = obj->coords.y1 + pixmap->height - 1;

    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set global opacity of the image widget.
 * @param obj Image extension object
 * @param alpha Opacity value (0 = transparent, 255 = opaque)
 */
void sgl_img_ext_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    SGL_ASSERT(obj != NULL);
    ((sgl_img_ext_t*)obj)->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set rotation angle of the image widget.
 * @param obj Image extension object
 * @param rotation Angle in degrees (normalized to 0-360)
 */
void sgl_img_ext_set_rotation(sgl_obj_t *obj, int16_t rotation)
{
    SGL_ASSERT(obj != NULL);
    sgl_img_ext_t *img_ext = (sgl_img_ext_t*)obj;
    img_ext->rotation = sgl_mod360(rotation);
    img_ext_update_coords(img_ext);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set independent X/Y scale factors (fixed-point).
 * @param obj Image extension object
 * @param scale_x X-axis scale
 * @param scale_y Y-axis scale
 */
void sgl_img_ext_set_scale(sgl_obj_t *obj, float scale_x, float scale_y)
{
    SGL_ASSERT(obj != NULL);
    sgl_img_ext_t *img_ext = (sgl_img_ext_t*)obj;
    img_ext->scale_x = scale_x * SGL_FIXED_ONE;
    img_ext->scale_y = scale_y * SGL_FIXED_ONE;
    img_ext->scale_uniform = false;
    img_ext_update_coords(img_ext);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set uniform scale from a signed byte.
 * @param obj Image extension object
 * @param scale Scale value: 0 = 1x, positive = zoom in, negative = zoom out
 */
void sgl_img_ext_set_scale_uniform(sgl_obj_t *obj, int8_t scale)
{
    SGL_ASSERT(obj != NULL);
    sgl_img_ext_t *img_ext = (sgl_img_ext_t*)obj;

    /*
     * Map -127..128 range to a fixed-point scale factor:
     *   0   = 1.0x (no scaling)
     *   >0  = zoom in  (e.g. 64 ~ 1.5x, 128 = 2.0x)
     *   <0  = zoom out (e.g. -64 ~ 0.5x)
     */
    int32_t fixed_scale = SGL_FIXED_ONE + ((int32_t)scale << (SGL_FIXED_SHIFT - 7));

    /* Clamp scale range */
    if (fixed_scale < 1) fixed_scale = 1;
    if (fixed_scale > SGL_FIXED_ONE * 4) fixed_scale = SGL_FIXED_ONE * 4;

    img_ext->scale_x = fixed_scale;
    img_ext->scale_y = fixed_scale;

    img_ext->scale_uniform = true;
    img_ext_update_coords(img_ext);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set pivot point for rotation and scaling.
 * @param obj Image extension object
 * @param pivot_x Pivot X coordinate (image coords, 0,0 = top-left)
 * @param pivot_y Pivot Y coordinate (image coords, 0,0 = top-left)
 */
void sgl_img_ext_set_pivot(sgl_obj_t *obj, int32_t pivot_x, int32_t pivot_y)
{
    SGL_ASSERT(obj != NULL);
    sgl_img_ext_t *img_ext = (sgl_img_ext_t*)obj;
    img_ext->pivot_x = pivot_x;
    img_ext->pivot_y = pivot_y;
    if (img_ext->pixmap != NULL) {
        img_ext_update_coords(img_ext);
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set external flash read callback for memory-mapped pixmap access.
 * @param obj Image extension object
 * @param read Read function: (addr, out_buf, len_bytes)
 */
void sgl_img_ext_set_read_ops(sgl_obj_t *obj, void (*read)(const size_t addr, uint8_t *out, uint32_t len_bytes))
{
    SGL_ASSERT(obj != NULL);
    ((sgl_img_ext_t*)obj)->read = read;
}
