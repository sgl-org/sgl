/* source/widgets/toast/sgl_toast.c
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
#include <sgl_theme.h>
#include <sgl_anim.h>
#include "sgl_toast.h"

/* default time the toast stays at full alpha, ms */
#define SGL_TOAST_DEFAULT_HOLD                    (2000)

/* default fade out time, ms, alpha ramps down slowly during this phase */
#define SGL_TOAST_DEFAULT_FADE                    (500)

/* default corner radius of the toast box, px */
#define SGL_TOAST_DEFAULT_RADIUS                  (8)

/**
 * @brief toast construct callback
 * @param surf surface to draw on
 * @param obj toast object
 * @param evt event pointer
 * @return none
 * @note draws the rounded background and the centered text with alpha_cur,
 *       nothing is drawn while the toast is invisible (alpha_cur == 0)
 */
static void sgl_toast_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_toast_t *toast = sgl_container_of(obj, sgl_toast_t, obj);
    sgl_pos_t align_pos;
    sgl_area_t text_coords;
    sgl_draw_rect_t body_desc = {
        .alpha = toast->alpha_cur,
        .border = 0,
        .border_alpha = 0,
        .border_color = toast->bg_color,
        .border_mask = 0,
        .color = toast->bg_color,
        .radius = obj->radius,
        .pixmap = NULL,
    };

    if (evt->type == SGL_EVENT_DESTROYED) {
        sgl_anim_delete_by_obj(obj);
        return;
    }

    if (evt->type != SGL_EVENT_DRAW_MAIN || toast->alpha_cur == 0) {
        return;
    }

    SGL_ASSERT(toast->font != NULL);

    sgl_draw_rect(surf, &obj->area, &obj->coords, &body_desc);

    text_coords = obj->coords;
    align_pos = sgl_get_text_pos(&text_coords, toast->font, toast->text, 0, SGL_ALIGN_CENTER);
    sgl_draw_string(surf, &obj->area, align_pos.x, align_pos.y, toast->text, toast->text_color, toast->alpha_cur, toast->font);
}

/**
 * @brief animation path callback, drives the fade out alpha ramp
 * @param anim animation object
 * @param value current alpha, linear from alpha_max down to 0
 * @return none
 */
static void sgl_toast_anim_cb(sgl_anim_t *anim, int32_t value)
{
    sgl_obj_t *obj = (sgl_obj_t*)anim->data;
    sgl_toast_t *toast = sgl_container_of(obj, sgl_toast_t, obj);
    uint8_t alpha = (uint8_t)value;

    if (alpha != toast->alpha_cur) {
        toast->alpha_cur = alpha;
        sgl_obj_set_dirty(obj);
    }
}

/**
 * @brief animation finish callback, schedules the toast removal
 * @param anim animation object
 * @return none
 * @note only marks the destroyed flag, the object is removed and freed by the
 *       core on the next draw cycle, so it is safe from the animation task
 */
static void sgl_toast_finished_cb(sgl_anim_t *anim)
{
    sgl_obj_t *obj = (sgl_obj_t*)anim->data;
    if (obj != NULL) {
        sgl_obj_set_destroyed(obj);
    }
}

/**
 * @brief create a toast object
 * @param parent parent object
 * @return toast object
 */
sgl_obj_t* sgl_toast_create(sgl_obj_t* parent)
{
    sgl_toast_t *toast = sgl_malloc(sizeof(sgl_toast_t));
    sgl_obj_t *obj;

    if(toast == NULL) {
        SGL_LOG_ERROR("sgl_toast_create: malloc failed");
        return NULL;
    }
    memset(toast, 0, sizeof(sgl_toast_t));

    obj = &toast->obj;
    sgl_obj_init(&toast->obj, parent);
    obj->clickable = 0;
    obj->construct_fn = sgl_toast_construct_cb;
    sgl_obj_set_radius(obj, SGL_TOAST_DEFAULT_RADIUS);

    toast->font = sgl_get_system_font();
    toast->text = "Toast";
    toast->bg_color = SGL_THEME_COLOR;
    toast->text_color = SGL_THEME_TEXT_COLOR;
    toast->alpha_max = SGL_THEME_ALPHA;
    toast->alpha_cur = 0;
    toast->hold_ms = SGL_TOAST_DEFAULT_HOLD;
    toast->fade_ms = SGL_TOAST_DEFAULT_FADE;

    return obj;
}

/**
 * @brief set toast message text
 * @param obj toast object
 * @param text message text, the pointer must stay valid while shown
 * @return none
 */
void sgl_toast_set_text(sgl_obj_t *obj, const char *text)
{
    sgl_toast_t *toast = sgl_container_of(obj, sgl_toast_t, obj);
    toast->text = text;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set toast font
 * @param obj toast object
 * @param font font used to draw the text
 * @return none
 */
void sgl_toast_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_toast_t *toast = sgl_container_of(obj, sgl_toast_t, obj);
    toast->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set toast background color
 * @param obj toast object
 * @param color background color
 * @return none
 */
void sgl_toast_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_toast_t *toast = sgl_container_of(obj, sgl_toast_t, obj);
    toast->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set toast text color
 * @param obj toast object
 * @param color text color
 * @return none
 */
void sgl_toast_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_toast_t *toast = sgl_container_of(obj, sgl_toast_t, obj);
    toast->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set toast peak alpha
 * @param obj toast object
 * @param alpha peak alpha of both background and text [0, 255]
 * @return none
 */
void sgl_toast_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_toast_t *toast = sgl_container_of(obj, sgl_toast_t, obj);
    toast->alpha_max = alpha;
    if (toast->alpha_cur > alpha) {
        toast->alpha_cur = alpha;
        sgl_obj_set_dirty(obj);
    }
}

/**
 * @brief set toast hold time
 * @param obj toast object
 * @param hold_ms time at full alpha before fading out, ms
 * @return none
 */
void sgl_toast_set_duration(sgl_obj_t *obj, uint16_t hold_ms)
{
    sgl_toast_t *toast = sgl_container_of(obj, sgl_toast_t, obj);
    toast->hold_ms = hold_ms;
}

/**
 * @brief set toast fade time
 * @param obj toast object
 * @param fade_ms fade in time in ms, the fade out uses the same value
 * @return none
 */
void sgl_toast_set_fade_time(sgl_obj_t *obj, uint16_t fade_ms)
{
    sgl_toast_t *toast = sgl_container_of(obj, sgl_toast_t, obj);
    toast->fade_ms = fade_ms;
}

/**
 * @brief show the toast and start its life cycle
 * @param obj toast object
 * @return none
 */
void sgl_toast_show(sgl_obj_t *obj)
{
    sgl_toast_t *toast = sgl_container_of(obj, sgl_toast_t, obj);
    sgl_anim_t *anim;

    sgl_anim_delete_by_obj(obj);

    /* appear at once, hold, then fade out: one delayed linear animation */
    toast->alpha_cur = toast->alpha_max;
    sgl_obj_set_dirty(obj);

    anim = sgl_anim_create();
    if (anim == NULL) {
        SGL_LOG_ERROR("sgl_toast_show: anim create failed");
        return;
    }
    sgl_anim_set_data(anim, obj);
    sgl_anim_set_auto_free(anim);
    sgl_anim_set_start_value(anim, toast->alpha_max);
    sgl_anim_set_end_value(anim, 0);
    sgl_anim_set_act_delay(anim, toast->hold_ms);
    sgl_anim_set_act_duration(anim, toast->fade_ms ? toast->fade_ms : 1);
    sgl_anim_set_path(anim, sgl_toast_anim_cb, SGL_ANIM_PATH_LINEAR);
    sgl_anim_set_finish_cb(anim, sgl_toast_finished_cb);
    sgl_anim_start(anim, SGL_ANIM_REPEAT_ONCE);
}

/**
 * @brief check whether the toast life cycle is running
 * @param obj toast object
 * @return true if the toast is fading or holding
 */
bool sgl_toast_is_showing(sgl_obj_t *obj)
{
    return (sgl_anim_get_by_obj(obj) != NULL);
}
