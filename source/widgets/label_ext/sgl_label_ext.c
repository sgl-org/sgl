/* source/widgets/sgl_label_ext.c
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
#include <stdarg.h>
#include "sgl_label_ext.h"

/**
 * @brief compute the un-rotated content box (text + offset) in absolute coords
 * @param label_ext pointer to the label_ext object
 * @param text pointer to the text (may be NULL, keeps area invalid)
 * @param area pointer to the area to fill
 * @return none
 */
static void sgl_label_ext_content_rect(sgl_label_ext_t *label_ext, const char *text, sgl_area_t *area)
{
    sgl_pos_t align_pos;

    if (label_ext->font == NULL || text == NULL) {
        return;
    }

    align_pos = sgl_get_text_pos(&label_ext->layout, label_ext->font, text,
                                 0, (sgl_align_type_t)label_ext->align);
    area->x1 = align_pos.x + label_ext->offset_x;
    area->x2 = area->x1 + sgl_font_get_string_width(text, label_ext->font) - 1;
    area->y1 = align_pos.y + label_ext->offset_y;
    area->y2 = area->y1 + sgl_font_get_height(label_ext->font) - 1;

    /* guard against degenerate strings (width 0) */
    if (area->x2 < area->x1) {
        area->x2 = area->x1;
    }
    if (area->y2 < area->y1) {
        area->y2 = area->y1;
    }
}

/**
 * @brief recompute obj->coords from the layout rect, the current text and the
 *        current rotation angle
 * @param label_ext pointer to the label_ext object
 * @return none
 * @note with rotation == 0 coords == the full layout rect; otherwise coords
 *       becomes the exact rotated bounding box around the fixed content center,
 *       so invalidation always covers the drawn pixels (never more, never less).
 */
static void sgl_label_ext_update_coords(sgl_label_ext_t *label_ext)
{
    sgl_obj_t *obj = &label_ext->obj;
    sgl_area_t content = SGL_AREA_INVALID;
    int32_t cx, cy, half_w, half_h;
    int32_t sin_val, cos_val, abs_sin, abs_cos;
    int32_t bound_w, bound_h;
    int16_t rotation;

    /* adopt external position/size changes: if obj->coords was moved by
     * sgl_obj_set_pos/size (or any other code) since we last wrote it, treat
     * it as the new layout rect. This keeps set_pos/set_size working as
     * users expect even after a rotation was applied. */
    if (obj->coords.x1 != label_ext->last_bbox.x1 || obj->coords.y1 != label_ext->last_bbox.y1 ||
        obj->coords.x2 != label_ext->last_bbox.x2 || obj->coords.y2 != label_ext->last_bbox.y2) {
        label_ext->layout = obj->coords;
        label_ext->last_bbox = obj->coords;
    }

    /* keep the widget rect as base (background + alignment reference) */
    content = label_ext->layout;

    /* when no background, coords track the text box itself */
    if (!label_ext->bg_flag) {
        sgl_area_t text_rect = SGL_AREA_INVALID;
        sgl_label_ext_content_rect(label_ext, label_ext->text, &text_rect);
        if (text_rect.x1 <= text_rect.x2 && text_rect.y1 <= text_rect.y2) {
            content = text_rect;
        }
    }

    cx = ((int32_t)content.x1 + content.x2) / 2;
    cy = ((int32_t)content.y1 + content.y2) / 2;
    half_w = ((int32_t)content.x2 - content.x1 + 2) / 2;   /* ceil(w/2) */
    half_h = ((int32_t)content.y2 - content.y1 + 2) / 2;   /* ceil(h/2) */

    rotation = sgl_mod360(label_ext->rotation);

    if (rotation == 0) {
        /* un-rotated: keep the full layout rect so glyph overhang is not
         * clipped and the widget behaves exactly like a plain label */
        obj->coords.x1 = content.x1;
        obj->coords.y1 = content.y1;
        obj->coords.x2 = content.x2;
        obj->coords.y2 = content.y2;
        label_ext->last_bbox = obj->coords;
        return;
    }

    if (rotation == 90 || rotation == 270) {
        int32_t t = half_w;
        half_w = half_h;
        half_h = t;
    }
    else if (rotation != 180) {
        sin_val = sgl_sin(rotation);
        cos_val = sgl_cos(rotation);
        abs_sin = (sin_val < 0) ? -sin_val : sin_val;
        abs_cos = (cos_val < 0) ? -cos_val : cos_val;
        /* rotated half-extents, rounded outward so no pixel is lost */
        bound_w = (half_w * abs_cos + half_h * abs_sin + SGL_SIN_FIXED_ONE / 2) / SGL_SIN_FIXED_ONE;
        bound_h = (half_w * abs_sin + half_h * abs_cos + SGL_SIN_FIXED_ONE / 2) / SGL_SIN_FIXED_ONE;
        half_w = bound_w + 1;   /* safety margin for bilinear edge */
        half_h = bound_h + 1;
    }

    obj->coords.x1 = (int16_t)(cx - half_w);
    obj->coords.y1 = (int16_t)(cy - half_h);
    obj->coords.x2 = (int16_t)(cx + half_w);
    obj->coords.y2 = (int16_t)(cy + half_h);
    label_ext->last_bbox = obj->coords;
}

/**
 * @brief rasterize a string into a 1-byte-per-pixel coverage buffer
 * @param cov_buf coverage buffer of text_w x text_h bytes
 * @param text_w buffer width in pixels
 * @param text_h buffer height in pixels
 * @param str pointer to the text
 * @param font pointer to the font
 * @return none
 * @note walks the glyphs manually (same advance as sgl_draw_string) and blends
 *       the glyph alpha (4bpp table) into the coverage, so anti-aliasing of
 *       the source font is preserved. Supports compressed fonts too.
 */
static int32_t sgl_label_ext_fill_coverage(uint8_t *cov_buf, int16_t text_w, int16_t text_h,
                                        const char *str, const sgl_font_t *font)
{
    uint32_t unicode = 0;
    int32_t pen_x = 0;

    /* coverage rasterization decodes raw glyph bitmaps, which only works for
     * normal (RAM) fonts - anything else falls back to the un-rotated path */
    if (font->format != SGL_FONT_FMT_NORMAL || font->bitmap == NULL) {
        SGL_LOG_WARN("sgl_label_ext: font not rotatable (compressed/flash), drawing unrotated");
        return -1;
    }

    while (*str) {
        str += sgl_utf8_to_unicode(str, &unicode);
        uint32_t ch_index = sgl_search_unicode_ch_index(font, unicode);
        const sgl_font_table_t *dsc = &font->table[ch_index];
        const uint8_t *dot = &font->bitmap[dsc->bitmap_index];

        const int32_t glyph_x = pen_x + dsc->ofs_x;
        /* glyph box sits at baseline: bottom at font_height - base_line - ofs_y */
        const int32_t glyph_y = font->font_height - dsc->ofs_y - font->base_line - dsc->box_h;

        const int32_t x_start = sgl_max(glyph_x, 0);
        const int32_t x_end = sgl_min(glyph_x + dsc->box_w, text_w);
        const int32_t y_start = sgl_max(glyph_y, 0);
        const int32_t y_end = sgl_min(glyph_y + dsc->box_h, text_h);

        if (x_end > x_start && y_end > y_start) {
            for (int32_t y = y_start; y < y_end; y++) {
                const int32_t src_row = y - glyph_y;
                uint8_t *dst_row = &cov_buf[y * text_w];
                for (int32_t x = x_start; x < x_end; x++) {
                    const int32_t pixel_index = src_row * dsc->box_w + (x - glyph_x);
                    uint8_t alpha_dot;
                    if (font->bpp == 4) {
                        const uint8_t byte = dot[pixel_index >> 1];
                        alpha_dot = sgl_opa4_table[(pixel_index & 1) ? (byte & 0x0F) : (byte >> 4)];
                    }
                    else if (font->bpp == 2) {
                        const uint8_t byte = dot[pixel_index >> 2];
                        alpha_dot = sgl_opa2_table[(byte >> ((3 - (pixel_index & 0x3)) * 2)) & 0x03];
                    }
                    else {
                        const uint8_t byte = dot[pixel_index >> 3];
                        alpha_dot = ((byte >> (7 - (pixel_index & 0x7))) & 0x01) ? SGL_ALPHA_MAX : SGL_ALPHA_MIN;
                    }
                    /* keep the strongest coverage when glyphs overlap */
                    if (alpha_dot > dst_row[x]) {
                        dst_row[x] = alpha_dot;
                    }
                }
            }
        }

        pen_x += (dsc->adv_w + 8) >> 4;
    }

    return 0;
}

#if (CONFIG_SGL_PIXMAP_BILINEAR_INTERP)
/**
 * @brief sample the coverage buffer with bilinear interpolation
 * @param cov_buf coverage buffer of w x h bytes
 * @param w buffer width in pixels
 * @param h buffer height in pixels
 * @param u_fp horizontal sample position, fixed point (SGL_FIXED_SHIFT)
 * @param v_fp vertical sample position, fixed point (SGL_FIXED_SHIFT)
 * @return interpolated coverage [0..255]
 * @note keeps the sub-pixel position of the inverse-mapped coordinate and
 *       blends the 4 neighboring texels, so rotated glyph edges become
 *       smooth instead of jagged. Neighbors outside the buffer contribute 0.
 */
static inline uint8_t sgl_label_ext_cov_biln(const uint8_t *cov_buf, int16_t w, int16_t h,
                                             int32_t u_fp, int32_t v_fp)
{
    const int32_t u0 = u_fp >> SGL_FIXED_SHIFT;
    const int32_t v0 = v_fp >> SGL_FIXED_SHIFT;
    const int32_t fx = u_fp & SGL_FIXED_MASK;
    const int32_t fy = v_fp & SGL_FIXED_MASK;
    const int32_t w1 = SGL_FIXED_ONE - fx;
    const int32_t h1 = SGL_FIXED_ONE - fy;
    const int32_t u1 = (u0 + 1 < w) ? u0 + 1 : u0;
    const int32_t v1 = (v0 + 1 < h) ? v0 + 1 : v0;
    const uint8_t *row0 = &cov_buf[(int32_t)v0 * w];
    const uint8_t *row1 = &cov_buf[(int32_t)v1 * w];

    const int32_t top = (row0[u0] * w1 + row0[u1] * fx) >> SGL_FIXED_SHIFT;
    const int32_t bot = (row1[u0] * w1 + row1[u1] * fx) >> SGL_FIXED_SHIFT;

    return (uint8_t)((top * h1 + bot * fy) >> SGL_FIXED_SHIFT);
}
#endif /* CONFIG_SGL_PIXMAP_BILINEAR_INTERP */

/**
 * @brief draw the rotated text using coverage-preserving inverse mapping
 * @param surf pointer to the target surface (screen slice)
 * @param obj pointer to the label_ext object
 * @param label_ext pointer to the label_ext object
 * @return none
 * @note the text is rasterized into a tight alpha buffer, then mapped to the
 *       screen with an inverse rotation around the text center. Only glyph
 *       coverage is transformed - the background is never touched, so rotated
 *       text blends cleanly over whatever is behind it.
 */
static void sgl_label_ext_draw_rotated(sgl_surf_t *surf, sgl_obj_t *obj, sgl_label_ext_t *label_ext)
{
    const sgl_font_t *font = label_ext->font;
    const int16_t text_w = (int16_t)sgl_font_get_string_width(label_ext->text, font);
    const int16_t text_h = (int16_t)sgl_font_get_height(font);
    const int16_t rotation = sgl_mod360(label_ext->rotation);

    sgl_pos_t align_pos;
    sgl_area_t text_box = SGL_AREA_INVALID, clip = SGL_AREA_INVALID;
    uint8_t *cov_buf;
    int32_t sin_val, cos_val;
    int32_t pivot_x, pivot_y, half_w, half_h;
    int32_t min_x, min_y, max_x, max_y;
    sgl_color_t *dst;
    int32_t ret;

    if (text_w <= 0 || text_h <= 0) {
        return;
    }

    cov_buf = (uint8_t *)sgl_malloc((uint32_t)text_w * text_h);
    if (cov_buf == NULL) {
        SGL_LOG_ERROR("sgl_label_ext: coverage buffer malloc failed");
        return;
    }
    memset(cov_buf, 0, (uint32_t)text_w * text_h);

    /* text box in absolute screen coords, from the LAYOUT rect (not coords,
     * which hold the rotated bbox) so the position stays layout-stable */
    align_pos = sgl_get_text_pos(&label_ext->layout, font, label_ext->text,
                                 0, (sgl_align_type_t)label_ext->align);
    text_box.x1 = align_pos.x + label_ext->offset_x;
    text_box.y1 = align_pos.y + label_ext->offset_y;
    text_box.x2 = text_box.x1 + text_w - 1;
    text_box.y2 = text_box.y1 + text_h - 1;

    ret = sgl_label_ext_fill_coverage(cov_buf, text_w, text_h, label_ext->text, font);
    if (ret != 0) {
        sgl_free(cov_buf);
        sgl_draw_string(surf, &obj->area, text_box.x1, text_box.y1,
                        label_ext->text, label_ext->color, label_ext->alpha, font);
        return;
    }

    /* rotate around the text center */
    pivot_x = ((int32_t)text_box.x1 + text_box.x2) / 2;
    pivot_y = ((int32_t)text_box.y1 + text_box.y2) / 2;
    half_w = text_w / 2;
    half_h = text_h / 2;

    sin_val = sgl_sin(rotation);
    cos_val = sgl_cos(rotation);

    /* exact rotated hull of the 4 half-extent corners.
     * cos/sin are fixed15, half_w/half_h are plain pixels, so >> 15
     * brings the result back to pixels. Exact for 0/90/180/270. */
    const int32_t rx0 = (int32_t)(((int64_t) cos_val * (-half_w) - (int64_t) sin_val * (-half_h)) >> SGL_SIN_SHIFT);
    const int32_t ry0 = (int32_t)(((int64_t) sin_val * (-half_w) + (int64_t) cos_val * (-half_h)) >> SGL_SIN_SHIFT);
    const int32_t rx1 = (int32_t)(((int64_t) cos_val * ( half_w) - (int64_t) sin_val * (-half_h)) >> SGL_SIN_SHIFT);
    const int32_t ry1 = (int32_t)(((int64_t) sin_val * ( half_w) + (int64_t) cos_val * (-half_h)) >> SGL_SIN_SHIFT);
    const int32_t rx2 = (int32_t)(((int64_t) cos_val * ( half_w) - (int64_t) sin_val * ( half_h)) >> SGL_SIN_SHIFT);
    const int32_t ry2 = (int32_t)(((int64_t) sin_val * ( half_w) + (int64_t) cos_val * ( half_h)) >> SGL_SIN_SHIFT);
    const int32_t rx3 = (int32_t)(((int64_t) cos_val * (-half_w) - (int64_t) sin_val * ( half_h)) >> SGL_SIN_SHIFT);
    const int32_t ry3 = (int32_t)(((int64_t) sin_val * (-half_w) + (int64_t) cos_val * ( half_h)) >> SGL_SIN_SHIFT);

    min_x = sgl_min4(rx0, rx1, rx2, rx3);
    max_x = sgl_max4(rx0, rx1, rx2, rx3);
    min_y = sgl_min4(ry0, ry1, ry2, ry3);
    max_y = sgl_max4(ry0, ry1, ry2, ry3);

    /* clip to surface, widget bounds and parent area */
    sgl_area_t draw_box = {
        .x1 = (int16_t)(pivot_x + min_x),
        .y1 = (int16_t)(pivot_y + min_y),
        .x2 = (int16_t)(pivot_x + max_x),
        .y2 = (int16_t)(pivot_y + max_y),
    };

    if (!sgl_surf_clip(surf, &draw_box, &clip)) {
        sgl_free(cov_buf);
        return;
    }
    if (!sgl_area_selfclip(&clip, &obj->parent->area)) {
        sgl_free(cov_buf);
        return;
    }

    dst = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);

    const int32_t pivot_x_fp = pivot_x << SGL_FIXED_SHIFT;
    const int32_t pivot_y_fp = pivot_y << SGL_FIXED_SHIFT;
#if (CONFIG_SGL_PIXMAP_BILINEAR_INTERP)
    const int32_t text_w_fp = (int32_t)text_w << SGL_FIXED_SHIFT;
    const int32_t text_h_fp = (int32_t)text_h << SGL_FIXED_SHIFT;
#endif

    for (int32_t py = clip.y1; py <= clip.y2; py++) {
        sgl_color_t *blend = dst;
        const int32_t rel_y_fp = ((int32_t)py << SGL_FIXED_SHIFT) - pivot_y_fp;
        for (int32_t px = clip.x1; px <= clip.x2; px++, blend++) {
            const int32_t rel_x_fp = ((int32_t)px << SGL_FIXED_SHIFT) - pivot_x_fp;

            /* inverse rotation: cos(fixed15) * rel(fixed15) = fixed30,
             * >> 15 keeps the sub-pixel position in fixed point */
            int32_t u_fp = (int32_t)(((int64_t) cos_val * rel_x_fp + (int64_t) sin_val * rel_y_fp) >> SGL_SIN_SHIFT);
            int32_t v_fp = (int32_t)(((int64_t)-sin_val * rel_x_fp + (int64_t) cos_val * rel_y_fp) >> SGL_SIN_SHIFT);

            u_fp += half_w << SGL_FIXED_SHIFT;
            v_fp += half_h << SGL_FIXED_SHIFT;

#if (CONFIG_SGL_PIXMAP_BILINEAR_INTERP)
            /* bilinear: interpolate the 4 neighbors for smooth edges */
            if (u_fp < 0 || u_fp >= text_w_fp || v_fp < 0 || v_fp >= text_h_fp) {
                continue;
            }
            const uint8_t cov = sgl_label_ext_cov_biln(cov_buf, text_w, text_h, u_fp, v_fp);
#else
            /* nearest: round to the closest texel (1 << 14 rounds fixed15) */
            int32_t u = (u_fp + (1 << (SGL_FIXED_SHIFT - 1))) >> SGL_FIXED_SHIFT;
            int32_t v = (v_fp + (1 << (SGL_FIXED_SHIFT - 1))) >> SGL_FIXED_SHIFT;

            if (u < 0 || u >= text_w || v < 0 || v >= text_h) {
                continue;
            }

            const uint8_t cov = cov_buf[(int32_t)v * text_w + u];
#endif /* CONFIG_SGL_PIXMAP_BILINEAR_INTERP */

            if (cov == 0) {
                continue;
            }

            if (cov >= SGL_ALPHA_MAX && label_ext->alpha == SGL_ALPHA_MAX) {
                *blend = label_ext->color;
            }
            else {
                /* glyph coverage first, then widget alpha */
                sgl_color_t mix = sgl_color_mixer(label_ext->color, *blend, cov);
                if (label_ext->alpha != SGL_ALPHA_MAX) {
                    mix = sgl_color_mixer(mix, *blend, label_ext->alpha);
                }
                *blend = mix;
            }
        }
        dst += surf->w;
    }

    sgl_free(cov_buf);
}

/**
 * @brief construct the label_ext object
 * @param surf pointer to the surface
 * @param obj pointer to the label_ext object
 * @param evt pointer to the event
 * @return none
 */
static void sgl_label_ext_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);

    SGL_ASSERT(label_ext->font != NULL);

    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        if (label_ext->bg_flag) {
            /* clip the background to the LAYOUT rect (full card), not
             * obj->area which is the rotated text bbox when rotated */
            sgl_area_t bg_clip = label_ext->layout;
            sgl_area_selfclip(&bg_clip, &obj->parent->area);
            sgl_draw_fill_rect(surf, &bg_clip, &label_ext->layout, obj->radius, label_ext->bg_color, label_ext->alpha);
        }

        if (sgl_mod360(label_ext->rotation) == 0) {
            sgl_pos_t align_pos = sgl_get_text_pos(&label_ext->layout, label_ext->font, label_ext->text,
                                                   0, (sgl_align_type_t)label_ext->align);
            sgl_draw_string(surf, &obj->area,
                            (int16_t)(align_pos.x + label_ext->offset_x),
                            (int16_t)(align_pos.y + label_ext->offset_y),
                            label_ext->text, label_ext->color, label_ext->alpha, label_ext->font);
        } else {
            sgl_label_ext_draw_rotated(surf, obj, label_ext);
        }
    }
    else if (evt->type == SGL_EVENT_DESTROYED) {
        if (label_ext->dynamic) {
            sgl_free((void*)label_ext->text);
        }
    }
}

/**
 * @brief create a label_ext object
 * @param parent parent of the label_ext
 * @return pointer to the label_ext object
 */
sgl_obj_t* sgl_label_ext_create(sgl_obj_t* parent)
{
    sgl_label_ext_t *label_ext = sgl_malloc(sizeof(sgl_label_ext_t));
    if(label_ext == NULL) {
        SGL_LOG_ERROR("sgl_label_ext_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(label_ext, 0, sizeof(sgl_label_ext_t));

    sgl_obj_t *obj = &label_ext->obj;
    sgl_obj_init(&label_ext->obj, parent);
    obj->construct_fn = sgl_label_ext_construct_cb;

    label_ext->alpha = SGL_ALPHA_MAX;
    label_ext->bg_flag = 0;
    label_ext->color = SGL_THEME_TEXT_COLOR;
    label_ext->text = "";
    label_ext->rotation = 0;
    label_ext->rotated = 0;
    label_ext->font = sgl_get_system_font();
    label_ext->layout = obj->coords;
    label_ext->last_bbox = obj->coords;

    return obj;
}

/**
 * @brief set the text of the label_ext
 * @param obj pointer to the label_ext object
 * @param text pointer to the text
 * @return none
 */
void sgl_label_ext_set_text(sgl_obj_t *obj, const char *text)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);

    if (label_ext->text && strcmp(text, label_ext->text) == 0) {
        return;
    }

    label_ext->text = (char*)text;
    /* the widget no longer owns a writable buffer: fmt updates would write
     * through a possibly read-only pointer, so drop the stale metadata */
    label_ext->dynamic = 0;
    label_ext->text_capacity = 0;

    /* recompute the bounding box, invalidate it and schedule redraw */
    sgl_label_ext_update_coords(label_ext);
    sgl_update_area(&obj->coords);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the text buffer of the label_ext
 * @param obj pointer to the label_ext object
 * @param buf pointer to the text buffer
 * @param buf_size size of the text buffer
 * @return none
 */
void sgl_label_ext_set_text_buffer(sgl_obj_t *obj, char *buf, uint16_t buf_size)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->text = buf;
    label_ext->dynamic = 0;
    label_ext->text_capacity = buf_size;
    buf[0] = '\0';
}

/**
 * @brief set the text of the label_ext with format by manual memory
 * @param obj pointer to the label_ext object
 * @param fmt pointer to the text
 * @return none
 * @note the text buffer must be set by sgl_label_ext_set_text_buffer() before calling this function
 */
void sgl_label_ext_set_text_fmt(sgl_obj_t *obj, const char *fmt, ...)
{
    va_list args;
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);

    /* writing requires a writable buffer: installed via set_text_buffer()
     * or allocated by a previous dynamic fmt call. A pointer installed by
     * set_text() may point to a literal, writing it would fault. */
    if (!label_ext->text || (!label_ext->dynamic && label_ext->text_capacity == 0)) {
        SGL_LOG_WARN("sgl_label_ext_set_text_fmt: no writable text buffer, call sgl_label_ext_set_text_buffer() first");
        return;
    }

    va_start(args, fmt);
    sgl_vsnprintf(label_ext->text, label_ext->text_capacity, fmt, args);
    va_end(args);

    /* recompute the bounding box, invalidate it and schedule redraw */
    sgl_label_ext_update_coords(label_ext);
    sgl_update_area(&obj->coords);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the text of the label_ext with format by dynamic memory
 * @param obj pointer to the label_ext object
 * @param text pointer to the text
 * @return none
 */
void sgl_label_ext_set_text_fmt_dynamic(sgl_obj_t* obj, const char *fmt, ...)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    va_list args;
    va_list args_copy;
    char *text = label_ext->text;
    int len;
    size_t cap;

    va_start(args, fmt);
    va_copy(args_copy, args);
    len = sgl_vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    cap = ((size_t)len + 2) & ~(size_t)1;

    if (label_ext->text_capacity < cap) {
        text = label_ext->dynamic ? sgl_realloc(label_ext->text, cap) : sgl_malloc(cap);
        if (text == NULL) {
            va_end(args);
            SGL_LOG_ERROR("sgl_label_ext_set_text_fmt: alloc failed");
            return;
        }
        text[0] = '\0';
        label_ext->text = text;
        label_ext->dynamic = 1;
        label_ext->text_capacity = cap;
    }

    sgl_vsnprintf(label_ext->text, label_ext->text_capacity, fmt, args);
    va_end(args);

    /* recompute the bounding box, invalidate it and schedule redraw */
    sgl_label_ext_update_coords(label_ext);
    sgl_update_area(&obj->coords);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief update label_ext text area
 * @param obj pointer to the label_ext object
 * @return none
 * @note you can update your label_ext text area when you change the text buffer content
 */
void sgl_label_ext_update_text(sgl_obj_t *obj)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    sgl_label_ext_update_coords(label_ext);
    sgl_update_area(&obj->coords);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the text of the label_ext
 * @param obj pointer to the label_ext object
 * @return pointer to the text
 */
char* sgl_label_ext_get_text(sgl_obj_t *obj)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    return label_ext->text;
}

/**
 * @brief set label_ext font
 * @param obj pointer to the label_ext object
 * @param font pointer to the font
 * @return none
 */
void sgl_label_ext_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->font = font;
    sgl_label_ext_update_coords(label_ext);
    sgl_update_area(&obj->coords);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label_ext text color
 * @param obj pointer to the label_ext object
 * @param color color to be set
 * @return none
 */
void sgl_label_ext_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label_ext background color
 * @param obj pointer to the label_ext object
 * @param color color to be set
 * @return none
 */
void sgl_label_ext_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->bg_color = color;
    label_ext->bg_flag = 1;
    sgl_label_ext_update_coords(label_ext);
    sgl_update_area(&obj->coords);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label_ext radius
 * @param obj pointer to the label_ext object
 * @param radius radius to be set
 * @return none
 */
void sgl_label_ext_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label_ext text align
 * @param obj pointer to the label_ext object
 * @param align align to be set
 * @return none
 */
void sgl_label_ext_set_text_align(sgl_obj_t *obj, sgl_align_type_t align)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->align = align;
    sgl_label_ext_update_coords(label_ext);
    sgl_update_area(&obj->coords);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label_ext alpha
 * @param obj pointer to the label_ext object
 * @param alpha alpha to be set
 * @return none
 */
void sgl_label_ext_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label_ext text offset
 * @param obj pointer to the label_ext object
 * @param offset_x offset_x to be set
 * @param offset_y offset_y to be set
 * @return none
 */
void sgl_label_ext_set_text_offset(sgl_obj_t *obj, int8_t offset_x, int8_t offset_y)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->offset_x = offset_x;
    label_ext->offset_y = offset_y;
    sgl_label_ext_update_coords(label_ext);
    sgl_update_area(&obj->coords);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label_ext text rotation
 * @param obj pointer to the label_ext object
 * @param text_rotation text rotation angle (0-360 degree)
 * @return none
 * @note same idiom as sgl_img_ext_set_rotation: recompute the rotated
 *       bounding box, invalidate it and mark the widget dirty. The dirty
 *       harvest pushes both the previous and the new obj->area, so the old
 *       angle's pixels are cleared automatically.
 */
void sgl_label_ext_set_text_rotation(sgl_obj_t *obj, int16_t text_rotation)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->rotation = sgl_mod360(text_rotation);
    label_ext->rotated = (label_ext->rotation != 0);
    sgl_label_ext_update_coords(label_ext);
    sgl_update_area(&obj->coords);
    sgl_obj_set_dirty(obj);
}
