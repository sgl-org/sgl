/* source/widgets/winslate/sgl_winslate.c
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

#include <string.h>
#include "sgl_winslate.h"

static void winslate_sync_btn_metrics(sgl_winslate_t *ws)
{
    int16_t th = ws->chrome.title_h;
    ws->btn_w   = th;
    ws->btn_h   = (int16_t)(th - 4);
    ws->btn_gap = 1;
}

/* Fill the three caption button rectangles (minimize + maximize + close) for
 * the current geometry.  The close button sits flush against the right edge,
 * the maximize button sits to its left separated by btn_gap, and the minimize
 * button to the left of that.  All are vertically centered in the caption
 * bar.  The minimize button is display-only (never hit-tested). */
static void winslate_btn_rects(sgl_winslate_t *ws, sgl_area_t *min_r, sgl_area_t *max_r, sgl_area_t *close_r)
{
    sgl_obj_t *obj = &ws->obj;
    int16_t title_h = ws->chrome.title_h;
    int16_t right   = (int16_t)(obj->coords.x2 - obj->border);
    int16_t top     = (int16_t)(obj->coords.y1 + (title_h - ws->btn_h) / 2);

    close_r->x2 = (int16_t)(right - 1);
    close_r->x1 = (int16_t)(close_r->x2 - ws->btn_w + 1);
    close_r->y1 = top;
    close_r->y2 = (int16_t)(top + ws->btn_h - 1);

    max_r->x2 = (int16_t)(close_r->x1 - ws->btn_gap - 1);
    max_r->x1 = (int16_t)(max_r->x2 - ws->btn_w + 1);
    max_r->y1 = top;
    max_r->y2 = close_r->y2;

    min_r->x2 = (int16_t)(max_r->x1 - ws->btn_gap - 1);
    min_r->x1 = (int16_t)(min_r->x2 - ws->btn_w + 1);
    min_r->y1 = top;
    min_r->y2 = close_r->y2;
}

static uint8_t winslate_hit(const sgl_area_t *r, int16_t px, int16_t py)
{
    return (uint8_t)(px >= r->x1 && px <= r->x2 && py >= r->y1 && py <= r->y2);
}

static void winslate_toggle_max(sgl_winslate_t *ws)
{
    sgl_obj_t *obj    = &ws->obj;
    sgl_obj_t *parent = obj->parent;

    if (!ws->maximized) {
        sgl_pos_t p = sgl_obj_get_pos(obj);
        ws->res_x = p.x;
        ws->res_y = p.y;
        ws->res_w = sgl_obj_get_width(obj);
        ws->res_h = sgl_obj_get_height(obj);
        sgl_obj_set_pos(obj, 0, 0);
        sgl_obj_set_size(obj, sgl_obj_get_width(parent), sgl_obj_get_height(parent));
        ws->maximized = 1;
    }
    else {
        sgl_obj_set_pos(obj, ws->res_x, ws->res_y);
        sgl_obj_set_size(obj, ws->res_w, ws->res_h);
        ws->maximized = 0;
    }
    sgl_obj_set_dirty(obj);
}

static void sgl_winslate_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_winslate_t *ws = sgl_container_of(obj, sgl_winslate_t, obj);
    sgl_area_t min_r, max_r, close_r;
    sgl_area_t rect  = obj->coords;
    sgl_area_t title = obj->coords;

    winslate_btn_rects(ws, &min_r, &max_r, &close_r);

    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        sgl_pos_t pos;
        sgl_slate_state_t max_state, close_state;

        sgl_draw_slate_window(surf, &obj->area, &rect, &ws->chrome);

        title.y2 = (int16_t)(title.y1 + ws->chrome.title_h);
        pos = sgl_get_text_pos(&title, ws->title_font, ws->title_text, 0, SGL_ALIGN_LEFT_MID);
        sgl_draw_string(surf, &obj->area, (int16_t)(pos.x + 8), pos.y, ws->title_text,
                        ws->title_text_color, ws->chrome.alpha, ws->title_font);

        max_state   = (ws->pressed_btn == SGL_WINSLATE_BTN_MAXIMIZE) ? SGL_SLATE_STATE_PRESSED : SGL_SLATE_STATE_NORMAL;
        close_state = (ws->pressed_btn == SGL_WINSLATE_BTN_CLOSE)    ? SGL_SLATE_STATE_PRESSED : SGL_SLATE_STATE_HOVER;

        sgl_draw_slate_caption_btn(surf, &obj->area, &min_r,
                                   SGL_SLATE_BTN_MIN, SGL_SLATE_STATE_NORMAL, ws->chrome.alpha);
        sgl_draw_slate_caption_btn(surf, &obj->area, &max_r,
                                   ws->maximized ? SGL_SLATE_BTN_RESTORE : SGL_SLATE_BTN_MAX,
                                   max_state, ws->chrome.alpha);
        sgl_draw_slate_caption_btn(surf, &obj->area, &close_r,
                                   SGL_SLATE_BTN_CLOSE, close_state, ws->chrome.alpha);
    }
    else if (evt->type == SGL_EVENT_PRESSED) {
        if (winslate_hit(&close_r, evt->pos.x, evt->pos.y)) {
            ws->pressed_btn = SGL_WINSLATE_BTN_CLOSE;
        }
        else if (winslate_hit(&max_r, evt->pos.x, evt->pos.y)) {
            ws->pressed_btn = SGL_WINSLATE_BTN_MAXIMIZE;
        }
        else {
            ws->pressed_btn = SGL_WINSLATE_BTN_NONE;
            if (!ws->maximized && evt->pos.y < obj->coords.y1 + ws->chrome.title_h) {
                ws->dragging = 1;
                ws->grab_dx  = (int16_t)(obj->coords.x1 - evt->pos.x);
                ws->grab_dy  = (int16_t)(obj->coords.y1 - evt->pos.y);
            }
        }
        if (ws->pressed_btn != SGL_WINSLATE_BTN_NONE) {
            sgl_obj_set_dirty(obj);
        }
    }
    else if (evt->type == SGL_EVENT_MOVE_LEFT || evt->type == SGL_EVENT_MOVE_RIGHT ||
             evt->type == SGL_EVENT_MOVE_UP   || evt->type == SGL_EVENT_MOVE_DOWN) {
        if (ws->dragging) {
            sgl_obj_t *parent = obj->parent;
            sgl_obj_set_pos(obj, (int16_t)(evt->pos.x + ws->grab_dx - parent->coords.x1),
                                 (int16_t)(evt->pos.y + ws->grab_dy - parent->coords.y1));
        }
    }
    else if (evt->type == SGL_EVENT_RELEASED) {
        ws->dragging = 0;
        if (ws->pressed_btn != SGL_WINSLATE_BTN_NONE) {
            ws->pressed_btn = SGL_WINSLATE_BTN_NONE;
            sgl_obj_set_dirty(obj);
        }
    }
    else if (evt->type == SGL_EVENT_CLICKED) {
        if (winslate_hit(&close_r, evt->pos.x, evt->pos.y)) {
            ws->pressed_btn = SGL_WINSLATE_BTN_NONE;
            sgl_obj_set_destroyed(obj);
        }
        else if (winslate_hit(&max_r, evt->pos.x, evt->pos.y)) {
            ws->pressed_btn = SGL_WINSLATE_BTN_NONE;
            winslate_toggle_max(ws);
        }
    }
}

/**
 * @brief create a Slate window object
 * @param parent parent of the window, NULL creates it on the active screen
 * @return window object
 */
sgl_obj_t* sgl_winslate_create(sgl_obj_t* parent)
{
    sgl_winslate_t *ws = sgl_malloc(sizeof(sgl_winslate_t));
    if (ws == NULL) {
        SGL_LOG_ERROR("sgl_winslate_create: malloc failed");
        return NULL;
    }

    memset(ws, 0, sizeof(sgl_winslate_t));

    sgl_obj_t *obj = &ws->obj;
    if (sgl_obj_init(obj, parent) != 0) {
        sgl_free(ws);
        return NULL;
    }
    obj->construct_fn = sgl_winslate_construct_cb;
    sgl_obj_set_movable(obj);

    ws->chrome.title_color  = SGL_COLOR_WHITE;
    ws->chrome.body_color   = SGL_WINSLATE_BODY_COLOR;
    ws->chrome.border_color = SGL_WINSLATE_BORDER_COLOR;
    ws->chrome.title_h      = 20;
    ws->chrome.border_width = 1;
    ws->chrome.alpha        = SGL_ALPHA_MAX;
    sgl_obj_set_border_width(obj, ws->chrome.border_width);

    ws->title_text       = "Window";
    ws->title_font       = sgl_get_system_font();
    ws->title_text_color = SGL_WINSLATE_TITLE_TEXT_COLOR;

    winslate_sync_btn_metrics(ws);

    return obj;
}

/**
 * @brief set the window title text
 * @param obj window object
 * @param text title text
 * @return none
 */
void sgl_winslate_set_title_text(sgl_obj_t *obj, const char *text)
{
    sgl_winslate_t *ws = sgl_container_of(obj, sgl_winslate_t, obj);
    ws->title_text = text;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the window title font
 * @param obj window object
 * @param font title font
 * @return none
 */
void sgl_winslate_set_title_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_winslate_t *ws = sgl_container_of(obj, sgl_winslate_t, obj);
    ws->title_font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the window title text color
 * @param obj window object
 * @param color text color
 * @return none
 */
void sgl_winslate_set_title_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_winslate_t *ws = sgl_container_of(obj, sgl_winslate_t, obj);
    ws->title_text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the window caption bar color
 * @param obj window object
 * @param color caption bar color
 * @return none
 */
void sgl_winslate_set_title_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_winslate_t *ws = sgl_container_of(obj, sgl_winslate_t, obj);
    ws->chrome.title_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the window client body color
 * @param obj window object
 * @param color body color
 * @return none
 */
void sgl_winslate_set_body_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_winslate_t *ws = sgl_container_of(obj, sgl_winslate_t, obj);
    ws->chrome.body_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the window outline color
 * @param obj window object
 * @param color outline color
 * @return none
 */
void sgl_winslate_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_winslate_t *ws = sgl_container_of(obj, sgl_winslate_t, obj);
    ws->chrome.border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the window caption bar height
 * @param obj window object
 * @param height caption bar height
 * @return none
 */
void sgl_winslate_set_title_height(sgl_obj_t *obj, int16_t height)
{
    sgl_winslate_t *ws = sgl_container_of(obj, sgl_winslate_t, obj);
    ws->chrome.title_h = height;
    winslate_sync_btn_metrics(ws);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the window alpha
 * @param obj window object
 * @param alpha alpha
 * @return none
 */
void sgl_winslate_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_winslate_t *ws = sgl_container_of(obj, sgl_winslate_t, obj);
    ws->chrome.alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the window caption bar height
 * @param obj window object
 * @return caption bar height
 */
int16_t sgl_winslate_title_height(sgl_obj_t *obj)
{
    sgl_winslate_t *ws = sgl_container_of(obj, sgl_winslate_t, obj);
    return ws->chrome.title_h;
}

/**
 * @brief query whether the window is currently maximized
 * @param obj window object
 * @return 1 when maximized, 0 otherwise
 */
uint8_t sgl_winslate_is_maximized(sgl_obj_t *obj)
{
    sgl_winslate_t *ws = sgl_container_of(obj, sgl_winslate_t, obj);
    return ws->maximized;
}
