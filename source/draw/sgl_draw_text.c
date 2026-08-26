/* source/draw/sgl_draw_text.c
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
    int offset_y2 = font->font_height - font->table[ch_index].ofs_y - font->base_line;
    const uint8_t *dot = &font->bitmap[font->table[ch_index].bitmap_index];
    const uint8_t font_w = font->table[ch_index].box_w;
    const uint8_t font_h = font->table[ch_index].box_h;

    uint8_t shift = 0;
    uint32_t pixel_index, rel_x, rel_y;
    uint16_t byte_index, alpha_dot = 0;
    sgl_color_t color_mix, *buf = NULL, *blend = NULL;
    sgl_area_t clip;

    sgl_area_t text_rect = {
        .x1 = x + font->table[ch_index].ofs_x,
        .x2 = x + font->table[ch_index].ofs_x + font_w - 1,
        .y1 = y + offset_y2 - font_h,
        .y2 = y + offset_y2 - 1,
    };

    if (!sgl_surf_clip(surf, &text_rect, &clip)) {
        return;
    }

    if (!sgl_area_selfclip(&clip, area)) {
        return;
    }

    buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
#if (CONFIG_SGL_FONT_COMPRESSED)
    if (font->compress == 0) {
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
        x += ((font->table[ch_index].adv_w + 8)>> 4);
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
    int16_t ch_index, ch_width;
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

        ch_width = (font->table[ch_index].adv_w + 8) >> 4;

        if ((x_off + ch_width) > area->x2) {
            x_off = x;
            y += (font->font_height + line_margin);
        }

        sgl_draw_character(surf, area, x_off, y, ch_index, color, alpha, font);
        x_off += ch_width;
    }
}

/**
 * @brief generate mask for an character
 * @param mask Pointer to the mask buffer
 * @param area Pointer to the area where the character will be drawn
 * @param ch_index Index of the character
 * @param font Pointer to the font structure containing character data
 * @return none
 */
void sgl_draw_label_mask(uint8_t *mask, sgl_area_t *area,  int16_t x, int16_t y, uint32_t ch_index, const sgl_font_t *font)
{
    int offset_y2 = font->font_height - font->table[ch_index].ofs_y - font->base_line;
    const uint8_t *dot = &font->bitmap[font->table[ch_index].bitmap_index];
    const uint8_t font_w = font->table[ch_index].box_w;
    const uint8_t font_h = font->table[ch_index].box_h;
    const int16_t buf_w = area->x2 - area->x1 + 1;
    uint8_t *alpha, *alpha_buf;

    uint8_t shift = 0;
    uint32_t pixel_index, rel_x, rel_y;
    uint16_t byte_index, alpha_dot = 0;

    sgl_area_t clip = {
        .x1 = x + font->table[ch_index].ofs_x,
        .x2 = x + font->table[ch_index].ofs_x + font_w - 1,
        .y1 = y + offset_y2 - font_h,
        .y2 = y + offset_y2 - 1,
    };

    if (!sgl_area_selfclip(&clip, area)) {
        return;
    }

    alpha_buf = mask + buf_w * area->y1 + area->x1;
    for (int y = clip.y1; y <= clip.y2; y++) {
        rel_y = y - clip.y1;
        alpha = alpha_buf;

        for (int x = clip.x1; x <= clip.x2; x++) {
            rel_x = x - clip.x1;
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

            *alpha = alpha_dot;
            alpha ++;
        }

        alpha_buf += buf_w;
    }
}

void sgl_draw_string_mask(uint8_t *mask, sgl_area_t *area, int16_t x, int16_t y, const char *str, const sgl_font_t *font)
{
    uint32_t ch_index;
    uint32_t unicode = 0;

    while (*str) {
        str += sgl_utf8_to_unicode(str, &unicode);
        ch_index = sgl_search_unicode_ch_index(font, unicode);
        sgl_draw_label_mask(mask, area,x, y, ch_index, font);
        x += ((font->table[ch_index].adv_w + 8)>> 4);
    }
}
