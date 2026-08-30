/* source/widgets/sgl_win.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL  
 * Document reference link: docs directory
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

#include <sgl_theme.h>
#include <string.h>
#include "sgl_win.h"

static void sgl_win_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    sgl_area_t title_area = obj->coords;
    sgl_area_t body_area = obj->area;
    sgl_draw_rect_t desc = win->bg;
    sgl_pos_t align_pos;
    int16_t close_cx, close_cy, close_r, title_h;

    title_h = sgl_max3(obj->radius, win->title_h, sgl_font_get_height(win->title_font));
    close_r  = title_h / 3;
    close_cx = (obj->coords.x2 - obj->border - title_h / 2);
    close_cy = obj->coords.y1 + title_h / 2 + obj->border / 2;

    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        title_area.y2 = title_area.y1 + title_h;
        body_area.y1 = title_area.y2;
        desc.color = win->bg.color;
        desc.border_mask = obj->focus;
        sgl_draw_rect(surf, &body_area, &obj->coords, &desc);

        desc.color = win->title_bg_color;
        desc.alpha = win->bg.alpha;
        desc.pixmap = NULL;
        sgl_draw_rect(surf, &title_area, &obj->coords, &desc);

        title_area.x1 += obj->border;
        title_area.x2 -= obj->border;
        align_pos = sgl_get_text_pos(&title_area, win->title_font, win->title_text, 0, (sgl_align_type_t)win->title_align);
        if (win->title_align == SGL_ALIGN_LEFT_MID) {
            align_pos.x += obj->radius;
        }
        sgl_draw_string(surf, &obj->area, align_pos.x + 2, align_pos.y + obj->border, win->title_text, 
                          win->title_text_color, win->bg.alpha, win->title_font
                        );
        sgl_draw_fill_circle(surf, &title_area, close_cx, close_cy, close_r, win->close_color, win->bg.alpha);
    }
    else if (evt->type == SGL_EVENT_PRESSED || evt->type == SGL_EVENT_CLICKED) {
        if (evt->pos.x >= (close_cx - close_r) && evt->pos.x <= (close_cx + close_r) 
             && evt->pos.y >= (close_cy - close_r) && evt->pos.y <= (close_cy + close_r)) {
            sgl_obj_set_destroyed(obj);
        }
    }
}

/**
 * @brief create a window object
 * @param parent parent of the window
 * @return window object
 */
sgl_obj_t* sgl_win_create(sgl_obj_t* parent)
{
    sgl_win_t *win = sgl_malloc(sizeof(sgl_win_t));
    if(win == NULL) {
        SGL_LOG_ERROR("sgl_win_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(win, 0, sizeof(sgl_win_t));

    sgl_obj_t *obj = &win->obj;
    sgl_obj_init(&win->obj, parent);
    obj->construct_fn = sgl_win_construct_cb;
    sgl_obj_set_clickable(obj);

    win->bg.alpha = SGL_THEME_ALPHA;
    win->bg.color = SGL_THEME_COLOR;
    win->bg.radius = SGL_THEME_RADIUS;
    win->bg.border = 0;
    win->bg.border_color = SGL_THEME_BORDER_COLOR;
    win->bg.border_alpha = SGL_THEME_ALPHA;

    win->title_align = SGL_ALIGN_LEFT_MID;
    win->title_bg_color = sgl_color_mixer(SGL_THEME_COLOR, SGL_THEME_BG_COLOR, 128);
    win->title_font = sgl_get_system_font();
    win->close_color = sgl_rgb(255, 90, 80);

    return obj;
}

/**
 * @brief set window color
 * @param obj window object
 * @param color color
 * @return none
 */
void sgl_win_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    win->bg.color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set window radius
 * @param obj window object
 * @param radius radius
 * @return none
 */
void sgl_win_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    win->bg.radius = radius;
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set window alpha
 * @param obj window object
 * @param alpha alpha
 * @return none
 */
void sgl_win_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    win->bg.alpha = alpha;
    win->bg.border_alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set window border width
 * @param obj window object
 * @param width border width
 * @return none
 */
void sgl_win_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    win->bg.border = width;
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief set window border color
 * @param obj window object
 * @param color border color
 * @return none
 */
void sgl_win_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    win->bg.border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set window pixmap
 * @param obj window object
 * @param pixmap pixmap
 * @return none
 */
void sgl_win_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    win->bg.pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set window title text
 * @param obj window object
 * @param text title text
 * @return none
 */
void sgl_win_set_title_text(sgl_obj_t *obj, const char *text)
{
    sgl_area_t area = obj->area;
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    win->title_text = text;
    area.y2 = area.y1 + win->title_h;
    sgl_obj_update_area(&area);
}

/**
 * @brief set window title text color
 * @param obj window object
 * @param color text color
 * @return none
 */
void sgl_win_set_title_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    win->title_text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set window title font
 * @param obj window object
 * @param font title font
 * @return none
 */
void sgl_win_set_title_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    win->title_font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set window title height
 * @param obj window object
 * @param height title height
 * @return none
 */
void sgl_win_set_title_height(sgl_obj_t *obj, uint16_t height)
{
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    win->title_h = height;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set window title text align
 * @param obj window object
 * @param align text align
 * @return none
 */
void sgl_win_set_title_text_align(sgl_obj_t *obj, uint8_t align)
{
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    win->title_align = align;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set window title background color
 * @param obj window object
 * @param color title background color
 * @return none
 */
void sgl_win_set_title_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    win->title_bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set window close button color
 * @param obj window object
 * @param color close button color
 * @return none
 */
void sgl_win_set_close_btn_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    win->close_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get window title height
 * @param obj window object
 * @return title height
 */
int16_t sgl_win_title_height(sgl_obj_t *obj)
{
    sgl_win_t *win = sgl_container_of(obj, sgl_win_t, obj);
    return sgl_max3(obj->radius, win->title_h, sgl_font_get_height(win->title_font));
}
