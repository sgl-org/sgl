/* source/draw/sgl_draw.c
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
#include <sgl_log.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <string.h>

/**
 * @brief calculate a point color by bilinear interpolate (with mask support)
 * @param buffer point to image pixmap start buffer (RGB)
 * @param w      width of buffer
 * @param h      height of buffer
 * @param fx     x coordinate of point (fixed point, SGL_FIXED_SHIFT bits fraction)
 * @param fy     y coordinate of point (fixed point, SGL_FIXED_SHIFT bits fraction)
 * @return point color (RGB: interpolated if mask non-0, transparent/black if mask 0)
 */
sgl_color_t sgl_draw_biln_color(const sgl_color_t *buffer, int16_t w, int16_t h, int32_t fx, int32_t fy)
{
    const int32_t max_x = ((int32_t)w - 1) << SGL_FIXED_SHIFT;
    const int32_t max_y = ((int32_t)h - 1) << SGL_FIXED_SHIFT;
    fx = (fx < 0) ? 0 : (fx > max_x) ? max_x : fx;
    fy = (fy < 0) ? 0 : (fy > max_y) ? max_y : fy;

    const int32_t x0 = fx >> SGL_FIXED_SHIFT;
    const int32_t y0 = fy >> SGL_FIXED_SHIFT;
    const int32_t dx = fx & SGL_FIXED_MASK;
    const int32_t dy = fy & SGL_FIXED_MASK;
    const int32_t dx1 = SGL_FIXED_ONE - dx;
    const int32_t dy1 = SGL_FIXED_ONE - dy;

    const int32_t idx00 = y0 * w + x0;
    const int32_t idx01 = idx00 + 1;
    const int32_t idx10 = idx00 + w;
    const int32_t idx11 = idx10 + 1;

    const sgl_color_t p00 = buffer[idx00];
    const sgl_color_t p01 = buffer[idx01];
    const sgl_color_t p10 = buffer[idx10];
    const sgl_color_t p11 = buffer[idx11];

    /* corner weights are normalized to 65536, see sgl_draw_biln_color_mix() */
    const uint32_t w00 = ((uint32_t)dx1 * (uint32_t)dy1) >> SGL_FIXED_SHIFT;
    const uint32_t w01 = ((uint32_t)dx  * (uint32_t)dy1) >> SGL_FIXED_SHIFT;
    const uint32_t w10 = ((uint32_t)dx1 * (uint32_t)dy ) >> SGL_FIXED_SHIFT;
    const uint32_t w11 = ((uint32_t)dx  * (uint32_t)dy ) >> SGL_FIXED_SHIFT;

    return sgl_draw_biln_color_mix(p00, p01, p10, p11, w00, w01, w10, w11);
}

/**
 * @brief draw a right facing chevron (cascade arrow) made of two lines
 * @param surf surface
 * @param area area that contains the chevron
 * @param x left edge of the chevron bounding box
 * @param y top edge of the chevron bounding box
 * @param size half size of the chevron, it spans 2 * size + 1 px vertically
 * @param color chevron color
 * @param width line width
 * @param alpha alpha of color
 * @return none
 */
void sgl_draw_chevron_right(sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, int16_t size, sgl_color_t color, uint8_t width, uint8_t alpha)
{
    if (unlikely(size <= 0)) {
        return;
    }

    sgl_draw_line_noaa(surf, area, x, y,
                       (int16_t)(x + size), (int16_t)(y + size), color, width, alpha);
    sgl_draw_line_noaa(surf, area, (int16_t)(x + size), (int16_t)(y + size),
                       x, (int16_t)(y + 2 * size), color, width, alpha);
}

/**
 * @brief draw a left facing chevron (cascade arrow) made of two lines
 * @param surf surface
 * @param area area that contains the chevron
 * @param x left edge of the chevron bounding box
 * @param y top edge of the chevron bounding box
 * @param size half size of the chevron, it spans 2 * size + 1 px vertically
 * @param color chevron color
 * @param width line width
 * @param alpha alpha of color
 * @return none
 */
void sgl_draw_chevron_left(sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, int16_t size, sgl_color_t color, uint8_t width, uint8_t alpha)
{
    if (unlikely(size <= 0)) {
        return;
    }

    sgl_draw_line_noaa(surf, area, (int16_t)(x + size), y,
                       x, (int16_t)(y + size), color, width, alpha);
    sgl_draw_line_noaa(surf, area, x, (int16_t)(y + size),
                       (int16_t)(x + size), (int16_t)(y + 2 * size), color, width, alpha);
}

/**
 * @brief draw an up facing chevron (cascade arrow) made of two lines
 * @param surf surface
 * @param area area that contains the chevron
 * @param x left edge of the chevron bounding box
 * @param y top edge of the chevron bounding box
 * @param size half size of the chevron, it spans 2 * size + 1 px horizontally
 * @param color chevron color
 * @param width line width
 * @param alpha alpha of color
 * @return none
 */
void sgl_draw_chevron_up(sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, int16_t size, sgl_color_t color, uint8_t width, uint8_t alpha)
{
    if (unlikely(size <= 0)) {
        return;
    }

    sgl_draw_line_noaa(surf, area, x, (int16_t)(y + size),
                       (int16_t)(x + size), y, color, width, alpha);
    sgl_draw_line_noaa(surf, area, (int16_t)(x + size), y,
                       (int16_t)(x + 2 * size), (int16_t)(y + size), color, width, alpha);
}

/**
 * @brief draw a down facing chevron (cascade arrow) made of two lines
 * @param surf surface
 * @param area area that contains the chevron
 * @param x left edge of the chevron bounding box
 * @param y top edge of the chevron bounding box
 * @param size half size of the chevron, it spans 2 * size + 1 px horizontally
 * @param color chevron color
 * @param width line width
 * @param alpha alpha of color
 * @return none
 */
void sgl_draw_chevron_down(sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, int16_t size, sgl_color_t color, uint8_t width, uint8_t alpha)
{
    if (unlikely(size <= 0)) {
        return;
    }

    sgl_draw_line_noaa(surf, area, x, y,
                       (int16_t)(x + size), (int16_t)(y + size), color, width, alpha);
    sgl_draw_line_noaa(surf, area, (int16_t)(x + size), (int16_t)(y + size),
                       (int16_t)(x + 2 * size), y, color, width, alpha);
}

/**
 * @brief draw a cross (X) made of two diagonal lines
 * @param surf surface
 * @param area area that contains the cross
 * @param x left edge of the cross bounding box
 * @param y top edge of the cross bounding box
 * @param size half size of the cross, it spans 2 * size + 1 px in both directions
 * @param color cross color
 * @param width line width
 * @param alpha alpha of color
 * @return none
 */
void sgl_draw_cross(sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, int16_t size, sgl_color_t color, uint8_t width, uint8_t alpha)
{
    if (unlikely(size <= 0)) {
        return;
    }

    sgl_draw_line_noaa(surf, area, x, y,
                       (int16_t)(x + 2 * size), (int16_t)(y + 2 * size), color, width, alpha);
    sgl_draw_line_noaa(surf, area, (int16_t)(x + 2 * size), y,
                       x, (int16_t)(y + 2 * size), color, width, alpha);
}

/**
 * @brief transform a surface
 * @param dst destination surface
 * @param src source surface
 * @param area area of surface
 * @param x x coordinate of surface
 * @param y y coordinate of surface
 * @param rotation rotation angle
 * @return none
 * @note This function has implemented angle normalization to the range of 0 to 360 degrees.
 */
void sgl_draw_xform_surf(sgl_surf_t *dst, sgl_surf_t *src, sgl_area_t *area, int16_t x, int16_t y, int16_t rotation)
{
    const int32_t sin_val = sgl_sin(rotation);
    const int32_t cos_val = sgl_cos(rotation);

    const int16_t half_w = src->w / 2;
    const int16_t half_h = src->h / 2;

    const int16_t x1r = (cos_val * (-half_w) - sin_val * (-half_h)) / SGL_SIN_FIXED_ONE;
    const int16_t y1r = (sin_val * (-half_w) + cos_val * (-half_h)) / SGL_SIN_FIXED_ONE;

    const int16_t x2r = (cos_val * half_w - sin_val * (-half_h)) / SGL_SIN_FIXED_ONE;
    const int16_t y2r = (sin_val * half_w + cos_val * (-half_h)) / SGL_SIN_FIXED_ONE;

    const int16_t x3r = (cos_val * half_w - sin_val * half_h) / SGL_SIN_FIXED_ONE;
    const int16_t y3r = (sin_val * half_w + cos_val * half_h) / SGL_SIN_FIXED_ONE;

    const int16_t x4r = (cos_val * (-half_w) - sin_val * half_h) / SGL_SIN_FIXED_ONE;
    const int16_t y4r = (sin_val * (-half_w) + cos_val * half_h) / SGL_SIN_FIXED_ONE;

    const int16_t min_x = sgl_min4(x1r, x2r, x3r, x4r);
    const int16_t min_y = sgl_min4(y1r, y2r, y3r, y4r);
    const int16_t max_x = sgl_max4(x1r, x2r, x3r, x4r);
    const int16_t max_y = sgl_max4(y1r, y2r, y3r, y4r);

    const int16_t center_x = x + half_w;
    const int16_t center_y = y + half_h;

    const int16_t buf_width = src->w > 0 ? src->w : 1;
    const int16_t buf_height = src->h > 0 ? src->h : 1;

    if (buf_width <= 0 || buf_height <= 0) {
        return;
    }

    for (int py = min_y; py <= max_y; py++) {
        for (int px = min_x; px <= max_x; px++) {
            const int dst_x = center_x + px;
            const int dst_y = center_y + py;
            if (dst_x < area->x1 || dst_x > area->x2 || dst_y < area->y1 || dst_y > area->y2 ||
                dst_x < dst->x1 || dst_x > dst->x2 || dst_y < dst->y1 || dst_y > dst->y2) {
                continue;
            }

            int32_t rel_x = px;
            int32_t rel_y = py;

            int32_t orig_x_fixed = cos_val * rel_x + sin_val * rel_y;
            int32_t orig_y_fixed = -sin_val * rel_x + cos_val * rel_y;

            int32_t src_x = (orig_x_fixed / SGL_SIN_FIXED_ONE) + half_w;
            int32_t src_y = (orig_y_fixed / SGL_SIN_FIXED_ONE) + half_h;

            int32_t src_x_fp = src_x << SGL_FIXED_SHIFT;
            int32_t src_y_fp = src_y << SGL_FIXED_SHIFT;

            if (src_x >= 0 && src_x < buf_width - 1 && src_y >= 0 && src_y < buf_height - 1) {
                sgl_color_t color = sgl_draw_biln_color(src->buffer, buf_width, buf_height, src_x_fp, src_y_fp);
                int dst_idx = (dst_y - dst->y1) * dst->w + (dst_x - dst->x1);
                dst->buffer[dst_idx] = color;
            }
        }
    }
}

/**
 * @brief draw icon with alpha
 * @param surf   surface
 * @param area   area of icon
 * @param coords coords of icon
 * @param alpha  alpha of icon
 * @param icon   icon pixmap
 * @return none
 * @note Each pixel is stored as RGB565 (little-endian) followed by A8.
 */
void sgl_draw_icon(sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, uint8_t alpha, const sgl_icon_pixmap_t *icon)
{
    sgl_area_t clip = SGL_AREA_MAX;
    sgl_area_t icon_rect = {
        .x1 = x,
        .x2 = x + icon->width - 1,
        .y1 = y,
        .y2 = y + icon->height - 1,
    };

    if (!sgl_surf_clip(surf, &icon_rect, &clip)) {
        return;
    }

    if (!sgl_area_selfclip(&clip, area)) {
        return;
    }

    const int source_x = clip.x1 - icon_rect.x1;
    const int source_y = clip.y1 - icon_rect.y1;
    const size_t source_row_skip = (size_t)(icon->width - (clip.x2 - clip.x1 + 1)) * 3;
    const uint8_t *dot = icon->bitmap + ((size_t)source_y * icon->width + source_x) * 3;
    sgl_color_t *blend, *buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);

    for (int row = clip.y1; row <= clip.y2; ++row, dot += source_row_skip, buf += surf->w) {
        blend = buf;
        for (int col = clip.x1; col <= clip.x2; ++col, dot += 3, ++blend) {
            const uint16_t rgb565 = (uint16_t)dot[0] | ((uint16_t)dot[1] << 8);
            const uint8_t alpha_dot = (uint8_t)(((uint16_t)dot[2] * alpha + 127) / SGL_ALPHA_MAX);
            if (alpha_dot != 0) {
                sgl_color_t color = sgl_color_mixer(sgl_rgb565_to_color(rgb565), *blend, alpha_dot);
                *blend = color;
            }
        }
    }
}

#if (CONFIG_SGL_FONT_COMPRESSED)
/**
 * @brief RLE state, refrence LVGL
 */
typedef enum {
    RLE_STATE_SINGLE = 0,
    RLE_STATE_REPEATED,
    RLE_STATE_COUNTER,
} sgl_font_rle_state_t;

/**
 * @brief RLE decompress information structure
 */
typedef struct {
    uint32_t rdp;
    const uint8_t * in;
    uint8_t bpp;
    uint8_t prev_v;
    uint8_t count;
    sgl_font_rle_state_t state;
} sgl_font_rle_t;

static sgl_font_rle_t font_rle = {
    .rdp = 0,
    .in = NULL,
    .bpp = 0,
    .prev_v = 0,
    .count = 0,
    .state = RLE_STATE_SINGLE,
};

/**
 * @brief Get bits from a byte array
 * @param in the byte array
 * @param bit_pos the bit position
 * @param len the bit length
 * @return the bits of a byte
 */
static uint8_t get_bits(const uint8_t * in, uint32_t bit_pos, uint8_t len)
{
    const uint8_t bit_mask = (uint16_t)((uint16_t) 1 << len) - 1;
    const uint32_t byte_pos = bit_pos >> 3;
    bit_pos = bit_pos & 0x7;

    if(bit_pos + len >= 8) {
        uint16_t in16 = (in[byte_pos] << 8) + in[byte_pos + 1];
        return (in16 >> (16 - bit_pos - len)) & bit_mask;
    }
    else {
        return (in[byte_pos] >> (8 - bit_pos - len)) & bit_mask;
    }
}

/**
 * @brief Decompress a line of RLE data
 * @param out the decompressed data
 * @param w the width of the decompressed data
 * @return none
 */
static inline void decompress_line(uint8_t *out, int32_t w)
{
    int32_t i;
    uint8_t v = 0;
    uint8_t ret = 0;
    sgl_font_rle_t *rle = &font_rle;

    for(i = 0; i < w; i++) {
        if(rle->state == RLE_STATE_SINGLE) {
            ret = get_bits(rle->in, rle->rdp, rle->bpp);
            if(rle->rdp != 0 && rle->prev_v == ret) {
                rle->count = 0;
                rle->state = RLE_STATE_REPEATED;
            }

            rle->prev_v = ret;
            rle->rdp += rle->bpp;
        }
        else if(rle->state == RLE_STATE_REPEATED) {
            v = get_bits(rle->in, rle->rdp, 1);
            rle->count++;
            rle->rdp += 1;
            if(v == 1) {
                ret = rle->prev_v;
                if(rle->count == 11) {
                    rle->count = get_bits(rle->in, rle->rdp, 6);
                    rle->rdp += 6;
                    if(rle->count != 0) {
                        rle->state = RLE_STATE_COUNTER;
                    }
                    else {
                        ret = get_bits(rle->in, rle->rdp, rle->bpp);
                        rle->prev_v = ret;
                        rle->rdp += rle->bpp;
                        rle->state = RLE_STATE_SINGLE;
                    }
                }
            }
            else {
                ret = get_bits(rle->in, rle->rdp, rle->bpp);
                rle->prev_v = ret;
                rle->rdp += rle->bpp;
                rle->state = RLE_STATE_SINGLE;
            }
        }
        else if(rle->state == RLE_STATE_COUNTER) {
            ret = rle->prev_v;
            rle->count--;
            if(rle->count == 0) {
                ret = get_bits(rle->in, rle->rdp, rle->bpp);
                rle->prev_v = ret;
                rle->rdp += rle->bpp;
                rle->state = RLE_STATE_SINGLE;
            }
        }
        if (out != NULL) {
            out[i] = ret;
        }
    }
}

/**
 * @brief Initialize the RLE decompression state
 * @param in Pointer to the input data
 * @param bpp Bits per pixel of the input data
 * @return none
 */
static inline void font_rle_init(const uint8_t * in, uint8_t bpp)
{
    font_rle.in = in;
    font_rle.bpp = bpp;
    font_rle.state = RLE_STATE_SINGLE;
    font_rle.rdp = 0;
    font_rle.prev_v = 0;
    font_rle.count = 0;
}
#endif // (!CONFIG_SGL_FONT_COMPRESSED)

#if (CONFIG_SGL_FLASH_FONT)
/**
 * @brief Get the bitmap data of a glyph, load it from external flash when needed
 * @param font pointer to the font structure
 * @param ch_index index of the character in the font table
 * @return pointer to the glyph bitmap data, or NULL on failure
 */
static const uint8_t *sgl_font_get_glyph_bitmap(const sgl_font_t *font, uint32_t ch_index)
{
    static uint8_t flash_cache[CONFIG_SGL_FLASH_FONT_CACHE_SIZE];
    static const sgl_font_t *flash_cache_font = NULL;
    static uint32_t flash_cache_start = 0;    /* blob offset of the first cached byte */
    static uint32_t flash_cache_len   = 0;    /* bytes held by the window */

    const sgl_font_table_t *dsc = sgl_font_get_ch_table(font, ch_index);
    uint32_t bmp_size, offset, want;
    int32_t ret;

    if (font->bitmap != NULL) {
        return &font->bitmap[sgl_font_get_ch_bitmap_index(font, ch_index)];
    }

    SGL_ASSERT(font->flash_read != NULL);

    bmp_size = (((uint32_t)dsc->box_w * dsc->box_h * font->bpp) + 7) >> 3;
    if (bmp_size == 0) {
        return NULL;
    }
    if (bmp_size > sizeof(flash_cache)) {
        SGL_LOG_WARN("sgl_font_get_glyph_bitmap: glyph [%u] needs %u bytes, enlarge CONFIG_SGL_FLASH_FONT_CACHE_SIZE", ch_index, bmp_size);
        return NULL;
    }

    offset = sgl_font_get_ch_bitmap_index(font, ch_index);

    if (flash_cache_font == font && offset >= flash_cache_start
            && offset + bmp_size <= flash_cache_start + flash_cache_len) {
        return &flash_cache[offset - flash_cache_start];
    }

    want = (uint32_t)sizeof(flash_cache);
    ret = font->flash_read(font->flash_addr + offset, flash_cache, want);
    if (ret < (int32_t)bmp_size) {
        ret = font->flash_read(font->flash_addr + offset, flash_cache, bmp_size);
    }
    if (ret < (int32_t)bmp_size) {
        SGL_LOG_WARN("sgl_font_get_glyph_bitmap: external flash read fail, ret = %d", ret);
        flash_cache_font = NULL;
        flash_cache_len = 0;
        return NULL;
    }

    flash_cache_font = font;
    flash_cache_start = offset;
    flash_cache_len = (uint32_t)ret;
    return flash_cache;
}
#endif /* CONFIG_SGL_FLASH_FONT */

/**
 * @brief Draw a character on the surface with alpha blending
 * @param surf Pointer to the surface where the character will be drawn
 * @param area Pointer to the area where the character will be drawn
 * @param x X coordinate where the character will be drawn
 * @param y Y coordinate where the character will be drawn
 * @param ch_index Index of the character in the font table
 * @param color Foreground color of the character
 * @param alpha Alpha value for blending
 * @param font Pointer to the font structure containing character data
 * @return none
 * @note this function is only support bpp:4
 */
void sgl_draw_character(sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, uint32_t ch_index, sgl_color_t color, uint8_t alpha, const sgl_font_t *font)
{
#if (CONFIG_SGL_FLASH_FONT)
    const uint8_t *dot = NULL;
#else
    const uint8_t *dot = &font->bitmap[sgl_font_get_ch_bitmap_index(font, ch_index)];
#endif
    const sgl_font_table_t *dsc = sgl_font_get_ch_table(font, ch_index);
    int offset_y2 = font->font_height - dsc->ofs_y - font->base_line;
    const uint8_t font_w = dsc->box_w;
    const uint8_t font_h = dsc->box_h;

    uint8_t shift = 0;
    uint32_t pixel_index, rel_x, rel_y;
    uint16_t byte_index, alpha_dot = 0;
    sgl_color_t color_mix, *buf = NULL, *blend = NULL;
    sgl_area_t clip;

    sgl_area_t text_rect = {
        .x1 = x + dsc->ofs_x,
        .x2 = x + dsc->ofs_x + font_w - 1,
        .y1 = y + offset_y2 - font_h,
        .y2 = y + offset_y2 - 1,
    };

    if (!sgl_surf_clip(surf, &text_rect, &clip)) {
        return;
    }

    if (!sgl_area_selfclip(&clip, area)) {
        return;
    }

#if (CONFIG_SGL_FLASH_FONT)
    dot = sgl_font_get_glyph_bitmap(font, ch_index);
    if (dot == NULL) {
        return;
    }
#endif

    buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
#if (CONFIG_SGL_FONT_COMPRESSED)
    if (font->format != SGL_FONT_FMT_COMPRESSED) {
#endif // (!CONFIG_SGL_FONT_COMPRESSED == 0)
        for (int y = clip.y1; y <= clip.y2; y++) {
            blend = buf;
            rel_y = y - text_rect.y1;

            for (int x = clip.x1; x <= clip.x2; x++) {
                rel_x = x - text_rect.x1;
                pixel_index = rel_y * font_w + rel_x;

                if  (font->bpp == 4) {
                    byte_index = pixel_index >> 1;
                    alpha_dot = sgl_opa4_table[(pixel_index & 1) ? (dot[byte_index] & 0x0F) : (dot[byte_index] >> 4)];
                }
                else if (font->bpp == 2) {
                    byte_index = pixel_index >> 2;
                    shift = (3 - (pixel_index & 0x3)) * 2;
                    alpha_dot = sgl_opa2_table[(dot[byte_index] >> shift) & 0x03];
                }
                else if (font->bpp == 1) {
                    byte_index = pixel_index >> 3;
                    shift = 7 - (pixel_index & 0x7);
                    alpha_dot = ((dot[byte_index] >> shift) & 0x01) ? SGL_ALPHA_MAX : SGL_ALPHA_MIN;
                }

                color_mix = sgl_color_mixer(color, *blend, alpha_dot);
                *blend = sgl_color_mixer(color_mix, *blend, alpha);
                blend++;
            }
            buf += surf->w;
        }
#if (CONFIG_SGL_FONT_COMPRESSED)
    }  /* support compressed font */
    else {
        uint8_t line_buf[180] = {0};
        font_rle_init(dot, font->bpp);

        for (int y = text_rect.y1; y < clip.y1; y++) {
            decompress_line(NULL, font_w);
        }

        for (int y = clip.y1; y <= clip.y2; y++) {
            blend = buf;
            decompress_line(line_buf, font_w);

            for (int x = clip.x1; x <= clip.x2; x++) {
                if (font->bpp == 4) {
                    color_mix = sgl_color_mixer(color, *blend, sgl_opa4_table[line_buf[x - text_rect.x1]]);
                }
                else if (font->bpp == 2) {
                    color_mix = sgl_color_mixer(color, *blend, sgl_opa2_table[line_buf[x - text_rect.x1]]);
                }
                else if (font->bpp == 1) {
                    color_mix = sgl_color_mixer(color, *blend, line_buf[x - text_rect.x1] ? SGL_ALPHA_MAX : SGL_ALPHA_MIN);
                }
                *blend = sgl_color_mixer(color_mix, *blend, alpha);
                blend++;
            }
            buf += surf->w;
        }
    }
#endif
}

/**
 * @brief Draw a string on the surface with alpha blending
 * @param surf Pointer to the surface where the string will be drawn
 * @param area Pointer to the area where the string will be drawn
 * @param x X coordinate of the top-left corner of the string
 * @param y Y coordinate of the top-left corner of the string
 * @param str Pointer to the string to be drawn
 * @param color Foreground color of the string
 * @param alpha Alpha value for blending
 * @param font Pointer to the font structure containing character data
 * @return none
 */
void sgl_draw_string(sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, const char *str, sgl_color_t color, uint8_t alpha, const sgl_font_t *font)
{
    uint32_t ch_index;
    uint32_t unicode = 0;

    while (*str) {
        str += sgl_utf8_to_unicode(str, &unicode);
        ch_index = sgl_search_unicode_ch_index(font, unicode);
        sgl_draw_character(surf, area, x, y, ch_index, color, alpha, font);
        x += ((sgl_font_get_ch_table(font, ch_index)->adv_w + 8)>> 4);
    }
}

/**
 * @brief Draw a string on the surface with alpha blending and multiple lines
 * @param surf Pointer to the surface where the string will be drawn
 * @param area Pointer to the area where the string will be drawn
 * @param x X coordinate of the top-left corner of the string
 * @param y Y coordinate of the top-left corner of the string
 * @param str Pointer to the string to be drawn
 * @param color Foreground color of the string
 * @param alpha Alpha value for blending
 * @param font Pointer to the font structure containing character data
 * @param line_margin Margin between lines
 * @return none
 */
void sgl_draw_string_mult_line(sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, const char *str, sgl_color_t color, uint8_t alpha, const sgl_font_t *font, uint8_t line_margin)
{
    uint32_t ch_index;
    int16_t ch_width;
    int16_t x_off = x;
    uint32_t unicode = 0;

    while (*str) {
        if (*str == '\n') {
            x_off = x;
            y += (font->font_height + line_margin);
            str ++;
            continue;
        }

        str += sgl_utf8_to_unicode(str, &unicode);
        ch_index = sgl_search_unicode_ch_index(font, unicode);
        ch_width = (int16_t)((sgl_font_get_ch_table(font, ch_index)->adv_w + 8) >> 4);

        if ((x_off + ch_width) > area->x2) {
            x_off = x;
            y += (font->font_height + line_margin);
        }

        sgl_draw_character(surf, area, x_off, y, ch_index, color, alpha, font);
        x_off += ch_width;
    }
}
