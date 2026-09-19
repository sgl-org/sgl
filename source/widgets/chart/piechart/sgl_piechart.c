/* source/widgets/chart/piechart/sgl_piechart.c
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

#include <limits.h>
#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_piechart.h"


#define SGL_PIECHART_DEFAULT_LEGEND_SIZE   (60)
#define SGL_PIECHART_DEFAULT_BOX_SIZE      (10)
#define SGL_PIECHART_DEFAULT_PADDING       (4)
#define SGL_PIECHART_DEFAULT_GAP           (4)

/* default duration of open animation (0->360deg), unit: ms */
#define SGL_PIECHART_OPEN_ANIM_DURATION    (600)


static void sgl_piechart_recalc_total(sgl_piechart_t *pie);
static void sgl_piechart_draw_legend(sgl_surf_t *surf, sgl_obj_t *obj, sgl_piechart_t *pie, sgl_area_t *legend_rect);
static void sgl_piechart_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt);


/**
 * @brief Create a piechart widget
 */
sgl_obj_t* sgl_piechart_create(sgl_obj_t *parent)
{
    sgl_piechart_t *pie = sgl_malloc(sizeof(sgl_piechart_t));
    if (pie == NULL) {
        SGL_LOG_ERROR("sgl_piechart_create: malloc failed");
        return NULL;
    }

    memset(pie, 0, sizeof(sgl_piechart_t));

    sgl_obj_t *obj = &pie->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_piechart_construct_cb;

    /* default configuration */
    pie->alpha             = SGL_ALPHA_MAX;
    pie->option_bits.legend_enable = 1U;
    pie->option_bits.smooth = 0U; /* 默认关闭平滑，需要时由用户调用 sgl_piechart_set_smooth 开启 */
    pie->option_bits.legend_bg_enable = 0U;
    pie->option_bits.legend_dir = 0U;
    pie->inner_radius_rate = 0;   /* full pie */
    pie->legend_box_size   = SGL_PIECHART_DEFAULT_BOX_SIZE;
    pie->legend_item_gap   = SGL_PIECHART_DEFAULT_GAP;
    pie->legend_padding    = SGL_PIECHART_DEFAULT_PADDING;
    pie->slice_count       = 0;
    pie->start_angle       = 0;
    pie->legend_area_size  = SGL_PIECHART_DEFAULT_LEGEND_SIZE;
    pie->layout_bits.legend_pos = SGL_PIECHART_LEGEND_POS_RIGHT;
    pie->legend_font       = sgl_get_system_font();
    pie->legend_text_color = SGL_THEME_TEXT_COLOR;
    pie->legend_bg_color   = SGL_THEME_BG_COLOR;
    pie->legend_border_color = SGL_THEME_BORDER_COLOR;
    pie->legend_alpha      = SGL_ALPHA_MAX;
    pie->slices            = NULL;
    pie->total_value       = 0;

    return obj;
}


/**
 * @brief Set number of slices
 */
void sgl_piechart_set_slice_count(sgl_obj_t *obj, uint8_t count)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);

    if (pie->slice_count == count) {
        return;
    }

    /* free old slices */
    if (pie->slices) {
        sgl_free(pie->slices);
        pie->slices = NULL;
    }

    pie->slice_count = 0;
    pie->total_value = 0;

    if (count == 0) {
        sgl_obj_set_dirty(obj);
        return;
    }

    sgl_piechart_slice_t *slices = sgl_malloc(sizeof(sgl_piechart_slice_t) * count);
    if (slices == NULL) {
        SGL_LOG_ERROR("sgl_piechart_set_slice_count: malloc failed");
        sgl_obj_set_dirty(obj);
        return;
    }

    memset(slices, 0, sizeof(sgl_piechart_slice_t) * count);

    for (uint8_t i = 0; i < count; i++) {
        slices[i].alpha = SGL_ALPHA_MAX;
        slices[i].color = SGL_THEME_COLOR;
        slices[i].value = 0;
        slices[i].label = NULL;
    }

    pie->slices = slices;
    pie->slice_count = count;
    pie->total_value = 0;

    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set slice value
 */
void sgl_piechart_set_slice_value(sgl_obj_t *obj, uint8_t index, int32_t value)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);

    if (pie->slices == NULL || index >= pie->slice_count) {
        SGL_LOG_ERROR("sgl_piechart_set_slice_value: invalid index %d", index);
        return;
    }

    pie->slices[index].value = value;
    sgl_piechart_recalc_total(pie);
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set slice color
 */
void sgl_piechart_set_slice_color(sgl_obj_t *obj, uint8_t index, sgl_color_t color)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);

    if (pie->slices == NULL || index >= pie->slice_count) {
        SGL_LOG_ERROR("sgl_piechart_set_slice_color: invalid index %d", index);
        return;
    }

    pie->slices[index].color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set slice alpha
 */
void sgl_piechart_set_slice_alpha(sgl_obj_t *obj, uint8_t index, uint8_t alpha)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);

    if (pie->slices == NULL || index >= pie->slice_count) {
        SGL_LOG_ERROR("sgl_piechart_set_slice_alpha: invalid index %d", index);
        return;
    }

    pie->slices[index].alpha = alpha;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set slice label
 */
void sgl_piechart_set_slice_label(sgl_obj_t *obj, uint8_t index, const char *label)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);

    if (pie->slices == NULL || index >= pie->slice_count) {
        SGL_LOG_ERROR("sgl_piechart_set_slice_label: invalid index %d", index);
        return;
    }

    pie->slices[index].label = label;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Recalculate total value of slices
 */
static void sgl_piechart_recalc_total(sgl_piechart_t *pie)
{
    int64_t sum = 0;

    for (uint8_t i = 0; i < pie->slice_count; i++) {
        if (pie->slices[i].value > 0) {
            sum += pie->slices[i].value;
        }
    }

    if (sum > INT32_MAX) {
        pie->total_value = INT32_MAX;
    } else if (sum < 0) {
        pie->total_value = 0;
    } else {
        pie->total_value = (int32_t)sum;
    }
}


/**
 * @brief Draw legend
 */
static void sgl_piechart_draw_legend(sgl_surf_t *surf, sgl_obj_t *obj, sgl_piechart_t *pie, sgl_area_t *legend_rect)
{
    if (!pie->option_bits.legend_enable || pie->slice_count == 0 || pie->legend_font == NULL || pie->legend_alpha == 0) {
        return;
    }

    sgl_area_t clip = *legend_rect;
    if (!sgl_area_selfclip(&clip, &obj->area)) {
        return;
    }

    /* draw legend background & border */
    if (pie->option_bits.legend_bg_enable) {
        sgl_draw_fill_rect(surf, &obj->area, &clip, 0, pie->legend_bg_color, pie->legend_alpha);
        sgl_draw_wireframe(surf, &obj->area, &clip, 0, 1, pie->legend_border_color, pie->legend_alpha);
    }

    const int16_t rect_w = clip.x2 - clip.x1 + 1;
    const int16_t rect_h = clip.y2 - clip.y1 + 1;

    const uint8_t box_size = pie->legend_box_size ? pie->legend_box_size : SGL_PIECHART_DEFAULT_BOX_SIZE;
    const uint8_t padding  = pie->legend_padding;
    const uint8_t gap      = pie->legend_item_gap;
    const uint8_t font_h   = pie->legend_font->font_height;

    if (rect_w <= 0 || rect_h <= 0) {
        return;
    }

    if (pie->option_bits.legend_dir == SGL_PIECHART_LEGEND_DIR_VERTICAL) {
        /* top to bottom layout */
        int16_t y = clip.y1 + padding;

        for (uint8_t i = 0; i < pie->slice_count; i++) {
            sgl_piechart_slice_t *s = &pie->slices[i];

            sgl_area_t box = {
                .x1 = clip.x1 + padding,
                .y1 = y,
                .x2 = clip.x1 + padding + box_size - 1,
                .y2 = y + box_size - 1,
            };

            if (sgl_area_selfclip(&box, &clip)) {
                sgl_draw_fill_rect(surf, &obj->area, &box, 0, s->color, pie->legend_alpha);
            }

            if (s->label) {
                int16_t text_x = clip.x1 + padding + box_size + 4;
                int16_t text_y = y + (int16_t)(box_size - font_h) / 2;

                if (text_y < clip.y1) {
                    text_y = clip.y1;
                }

                sgl_draw_string(surf, &clip, text_x, text_y, s->label,
                                pie->legend_text_color, pie->legend_alpha, pie->legend_font);
            }

            /* advance to next row */
            int16_t step = box_size;
            if (font_h > box_size) {
                step = font_h;
            }
            y += step + gap;

            if (y > clip.y2) {
                break;
            }
        }
    }
    else {
        /* left to right layout */
        int16_t content_w = rect_w - (int16_t)padding * 2;
        if (content_w <= 0) {
            return;
        }

        uint8_t visible_cnt = pie->slice_count ? pie->slice_count : 1;
        int16_t item_w = content_w / visible_cnt;
        if (item_w < (int16_t)(box_size + 4)) {
            item_w = box_size + 4;
        }

        for (uint8_t i = 0; i < pie->slice_count; i++) {
            sgl_piechart_slice_t *s = &pie->slices[i];

            int16_t base_x = clip.x1 + padding + (int16_t)i * item_w;
            int16_t box_y1 = clip.y1 + padding;

            sgl_area_t box = {
                .x1 = base_x,
                .y1 = box_y1,
                .x2 = base_x + box_size - 1,
                .y2 = box_y1 + box_size - 1,
            };

            if (sgl_area_selfclip(&box, &clip)) {
                sgl_draw_fill_rect(surf, &obj->area, &box, 0, s->color, pie->legend_alpha);
            }

            if (s->label) {
                int16_t text_x = base_x + box_size + 4;
                int16_t text_y = box_y1 + (int16_t)(box_size - font_h) / 2;
                if (text_y < clip.y1) {
                    text_y = clip.y1;
                }

                sgl_draw_string(surf, &clip, text_x, text_y, s->label,
                                pie->legend_text_color, pie->legend_alpha, pie->legend_font);
            }
        }
    }
}


/**
 * @brief Construct callback of piechart
 */
static void sgl_piechart_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);

    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        /* quick reject if completely outside screen */
        if (obj->area.x2 < surf->x1 || obj->area.x1 > surf->x2 ||
            obj->area.y2 < surf->y1 || obj->area.y1 > surf->y2) {
            return;
        }

        sgl_area_t pie_rect = obj->coords;
        sgl_area_t legend_rect;
        bool legend_has_area = false;

        /* split area into pie and legend */
        if (pie->option_bits.legend_enable && pie->slice_count > 0 && pie->legend_font != NULL && pie->legend_alpha > 0) {
            int16_t width  = obj->coords.x2 - obj->coords.x1 + 1;
            int16_t height = obj->coords.y2 - obj->coords.y1 + 1;
            int16_t legend_size = pie->legend_area_size ? (int16_t)pie->legend_area_size : SGL_PIECHART_DEFAULT_LEGEND_SIZE;

            switch ((sgl_piechart_legend_pos_t)pie->layout_bits.legend_pos) {
            case SGL_PIECHART_LEGEND_POS_LEFT:
                if (legend_size >= width) legend_size = width / 3;
                legend_rect.x1 = obj->coords.x1;
                legend_rect.x2 = obj->coords.x1 + legend_size - 1;
                legend_rect.y1 = obj->coords.y1;
                legend_rect.y2 = obj->coords.y2;
                pie_rect.x1 = legend_rect.x2 + 1;
                legend_has_area = true;
                break;

            case SGL_PIECHART_LEGEND_POS_RIGHT:
                if (legend_size >= width) legend_size = width / 3;
                legend_rect.x1 = obj->coords.x2 - legend_size + 1;
                legend_rect.x2 = obj->coords.x2;
                legend_rect.y1 = obj->coords.y1;
                legend_rect.y2 = obj->coords.y2;
                pie_rect.x2 = legend_rect.x1 - 1;
                legend_has_area = true;
                break;

            case SGL_PIECHART_LEGEND_POS_TOP:
                if (legend_size >= height) legend_size = height / 3;
                legend_rect.x1 = obj->coords.x1;
                legend_rect.x2 = obj->coords.x2;
                legend_rect.y1 = obj->coords.y1;
                legend_rect.y2 = obj->coords.y1 + legend_size - 1;
                pie_rect.y1 = legend_rect.y2 + 1;
                legend_has_area = true;
                break;

            case SGL_PIECHART_LEGEND_POS_BOTTOM:
                if (legend_size >= height) legend_size = height / 3;
                legend_rect.x1 = obj->coords.x1;
                legend_rect.x2 = obj->coords.x2;
                legend_rect.y1 = obj->coords.y2 - legend_size + 1;
                legend_rect.y2 = obj->coords.y2;
                pie_rect.y2 = legend_rect.y1 - 1;
                legend_has_area = true;
                break;

            default:
                break;
            }
        }

        /* clip pie area to object area */
        if (!sgl_area_selfclip(&pie_rect, &obj->area)) {
            /* nothing to draw */
        }

        /* draw pie slices */
        if (pie->slice_count > 0 && pie->total_value > 0) {
            int16_t pie_w = pie_rect.x2 - pie_rect.x1 + 1;
            int16_t pie_h = pie_rect.y2 - pie_rect.y1 + 1;
            int16_t radius;

            /* 如果用户通过 API 设置了半径（>0），则优先使用用户设定值；
             * 否则从控件尺寸自动计算半径 */
            if (pie->radius_out > 0) {
                radius = pie->radius_out;
            }
            else {
                radius = (pie_w < pie_h ? pie_w : pie_h) / 2;
            }

            if (radius > 0) {
                int16_t cx = (pie_rect.x1 + pie_rect.x2) / 2;
                int16_t cy = (pie_rect.y1 + pie_rect.y2) / 2;

                int16_t radius_in = (int16_t)((int32_t)radius * pie->inner_radius_rate / 100);
                if (radius_in < 0) radius_in = 0;
                if (radius_in >= radius) radius_in = radius - 1;

                uint8_t global_alpha = pie->alpha;
                if (global_alpha == 0) {
                    global_alpha = SGL_ALPHA_MAX;
                }

                int32_t base_angle = pie->start_angle;
                /* normalize base angle to [0, 360) */
                while (base_angle < 0) base_angle += 360;
                while (base_angle >= 360) base_angle -= 360;

                /* handle open animation (0->360deg reveal) */
                bool anim_active = false;
                int32_t reveal_end_angle = base_angle + 360;

                if (pie->option_bits.open_anim_enable) {
                    anim_active = true;

                    if (!pie->option_bits.open_anim_playing) {
                        pie->option_bits.open_anim_playing = 1U;
                        pie->open_anim_start_tick = sgl_tick_get();
                    }

                    uint32_t now   = sgl_tick_get();
                    uint32_t elaps = now - pie->open_anim_start_tick;

                    if (elaps >= SGL_PIECHART_OPEN_ANIM_DURATION) {
                        /* animation finished: draw full pie from now on */
                        pie->option_bits.open_anim_enable = 0U;
                        pie->option_bits.open_anim_playing = 0U;
                        anim_active            = false;
                        reveal_end_angle       = base_angle + 360;
                    }
                    else {
                        int32_t angle_step;
                        if (pie->open_anim_path) {
                            /* 使用外部指定的缓动算法（如 SGL_ANIM_PATH_EASE_IN 等） */
                            angle_step = pie->open_anim_path(elaps, SGL_PIECHART_OPEN_ANIM_DURATION, 0, 360);
                        }
                        else {
                            /* 默认线性展开 */
                            angle_step = (int32_t)((360 * (int32_t)elaps) / (int32_t)SGL_PIECHART_OPEN_ANIM_DURATION);
                        }
                        reveal_end_angle = base_angle + angle_step;
                        /* request next frame: 标记整个饼图对象为脏 */
                        sgl_obj_set_dirty(obj);
                    }
                }

                int8_t last_positive = -1;
                for (uint8_t i = 0; i < pie->slice_count; i++) {
                    if (pie->slices[i].value > 0) {
                        last_positive = (int8_t)i;
                    }
                }

                int32_t current_angle = base_angle;
                for (uint8_t i = 0; i < pie->slice_count; i++) {
                    sgl_piechart_slice_t *s = &pie->slices[i];
                    if (s->value <= 0) {
                        continue;
                    }

                    int32_t end_angle;
                    if (i == (uint8_t)last_positive) {
                        end_angle = base_angle + 360;
                    } else {
                        int64_t delta = (int64_t)360 * (int64_t)s->value / (int64_t)pie->total_value;
                        if (delta <= 0) {
                            delta = 1; /* at least 1 degree */
                        }
                        end_angle = current_angle + (int32_t)delta;
                    }

                    if (end_angle > base_angle + 360) {
                        end_angle = base_angle + 360;
                    }

                    /* apply open animation clipping */
                    if (anim_active) {
                        if (current_angle >= reveal_end_angle) {
                            break; /* remaining slices are not yet visible */
                        }
                        if (end_angle > reveal_end_angle) {
                            end_angle = reveal_end_angle;
                        }
                    }

                    uint8_t slice_alpha = s->alpha ? s->alpha : SGL_ALPHA_MAX;
                    uint16_t mix_alpha = (uint16_t)slice_alpha * (uint16_t)global_alpha / 255;
                    if (mix_alpha == 0) {
                        mix_alpha = slice_alpha;
                    }

                    sgl_draw_arc_t desc = {
                        .alpha      = (uint8_t)mix_alpha,
                        .color      = s->color,
                        .cx         = cx,
                        .cy         = cy,
                        .radius_in  = radius_in,
                        .radius_out = radius,
                        .start_angle = (uint32_t)(current_angle & 0x1FF),
                        .end_angle   = (uint32_t)(end_angle   & 0x1FF),
                        .mode       = pie->option_bits.smooth ? SGL_ARC_MODE_NORMAL_SMOOTH : SGL_ARC_MODE_NORMAL,
                        .bg_color   = SGL_THEME_BG_COLOR,
                    };

                    sgl_draw_fill_arc(surf, &pie_rect, &desc);

                    current_angle = end_angle;
                }
            }
        }

        /* draw legend */
        if (legend_has_area) {
            sgl_piechart_draw_legend(surf, obj, pie, &legend_rect);
        }
    }
}
