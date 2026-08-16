/* source/widgets/stepper/sgl_stepper.c
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
#include "sgl_stepper.h"

/* 10^n lookup table */
static const int32_t _sp_pow10[] = { 1, 10, 100, 1000, 10000 };

/**
 * @brief  get 10^decimals
 * @param  decimals: decimal places
 * @return 10^decimals, or 1 if out of range
 */
static int32_t sgl_stepper_divisor(uint8_t decimals)
{
    if (decimals > 4) return 1;
    return _sp_pow10[decimals];
}

/**
 * @brief  format value into display string (with sign and decimal point)
 * @param  s: stepper pointer (result written to s->buf)
 * @return none
 */
static void sgl_stepper_format(sgl_stepper_t *s)
{
    int32_t divisor = sgl_stepper_divisor(s->decimals);
    int32_t v = s->value;
    uint8_t neg = (v < 0) ? 1 : 0;
    uint32_t av = (uint32_t)(neg ? -v : v);
    uint32_t ip = av / (uint32_t)divisor;
    uint32_t fp = av % (uint32_t)divisor;
    uint8_t pos = 0;
    uint8_t cap = (uint8_t)sizeof(s->buf);
    char tmp[12];
    uint8_t n;

    if (neg) s->buf[pos++] = '-';

    /* integer part */
    n = 0;
    do { tmp[n++] = (char)('0' + (ip % 10)); ip /= 10; } while (ip != 0 && n < sizeof(tmp));
    while (n > 0 && pos < (uint8_t)(cap - 1)) s->buf[pos++] = tmp[--n];

    /* decimal part */
    if (s->decimals > 0) {
        if (pos < (uint8_t)(cap - 1)) s->buf[pos++] = '.';
        n = 0;
        do { tmp[n] = (char)('0' + (fp % 10)); fp /= 10; n++; } while (n < s->decimals && n < sizeof(tmp));
        while (n < s->decimals && n < sizeof(tmp)) tmp[n++] = '0';
        while (n > 0 && pos < (uint8_t)(cap - 1)) s->buf[pos++] = tmp[--n];
    }
    s->buf[pos] = '\0';
}

/**
 * @brief  get button area width (square: min of h and w/3)
 * @param  s: stepper pointer
 * @return button width in pixels
 */
static int16_t sgl_stepper_btn_w(const sgl_stepper_t *s)
{
    int16_t w = (int16_t)(s->obj.coords.y2 - s->obj.coords.y1 + 1);
    int16_t third = (int16_t)((s->obj.coords.x2 - s->obj.coords.x1 + 1) / 3);
    if (w > third) w = third;
    if (w < 1) w = 1;
    return w;
}

/**
 * @brief  check if decrement button is disabled
 * @param  s: stepper pointer
 * @return 1 if disabled
 */
static uint8_t sgl_stepper_dec_disabled(const sgl_stepper_t *s)
{
    return (!s->wrap && s->value <= s->min_value) ? 1 : 0;
}

/**
 * @brief  check if increment button is disabled
 * @param  s: stepper pointer
 * @return 1 if disabled
 */
static uint8_t sgl_stepper_inc_disabled(const sgl_stepper_t *s)
{
    return (!s->wrap && s->value >= s->max_value) ? 1 : 0;
}

/**
 * @brief  apply step in given direction
 * @param  s: stepper pointer
 * @param  dir: -1 for decrement, +1 for increment
 * @return 1 if value changed, 0 if at boundary
 */
static uint8_t sgl_stepper_apply(sgl_stepper_t *s, int8_t dir)
{
    int32_t nv = s->value + (int32_t)dir * s->step;

    if (s->wrap && s->max_value > s->min_value) {
        int32_t span = s->max_value - s->min_value + 1;
        if (nv > s->max_value)
            nv = s->min_value + ((nv - s->min_value) % span);
        else if (nv < s->min_value)
            nv = s->max_value - ((s->max_value - nv) % span);
    } else {
        nv = sgl_clamp(nv, s->min_value, s->max_value);
    }

    if (nv == s->value) return 0;
    s->value = nv;
    sgl_obj_set_dirty(&s->obj);
    return 1;
}

/**
 * @brief  draw +/- sign using horizontal and vertical lines
 * @param  surf: surface pointer
 * @param  area: clip area
 * @param  cx: center X
 * @param  cy: center Y
 * @param  half: half length of each bar
 * @param  thick: line thickness
 * @param  is_plus: 1 for '+', 0 for '-'
 * @param  color: sign color
 * @param  alpha: transparency
 */
static void sgl_stepper_draw_sign(sgl_surf_t *surf, sgl_area_t *area,
                                  int16_t cx, int16_t cy, int16_t half,
                                  int16_t thick, uint8_t is_plus,
                                  sgl_color_t color, uint8_t alpha)
{
    /* horizontal bar */
    sgl_draw_fill_hline(surf, area, cy, (int16_t)(cx - half), (int16_t)(cx + half),
                        (uint8_t)thick, color, alpha);
    /* vertical bar (only for '+') */
    if (is_plus) {
        sgl_draw_fill_vline(surf, area, cx, (int16_t)(cy - half), (int16_t)(cy + half),
                            (uint8_t)thick, color, alpha);
    }
}

/**
 * @brief  determine which side was clicked
 * @param  s: stepper pointer
 * @param  px: touch X position
 * @return -1=left (decrement), 0=center (none), +1=right (increment)
 */
static int8_t sgl_stepper_hit_side(const sgl_stepper_t *s, int16_t px)
{
    int16_t bw = sgl_stepper_btn_w(s);
    int16_t x1 = s->obj.coords.x1;
    int16_t x2 = s->obj.coords.x2;
    if (px < x1 + bw) return -1;
    if (px >= x2 - bw + 1) return 1;
    return 0;
}

/**
 * @brief  stepper construct callback
 * @param  surf: surface pointer
 * @param  obj: object pointer
 * @param  evt: event pointer
 * @return none
 * @note   this function is called when the object is created or redraw
 */
static void sgl_stepper_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_stepper_t *s = sgl_container_of(obj, sgl_stepper_t, obj);
    int16_t x1 = obj->coords.x1;
    int16_t y1 = obj->coords.y1;
    int16_t x2 = obj->coords.x2;
    int16_t y2 = obj->coords.y2;
    int16_t w = x2 - x1 + 1;
    int16_t h = y2 - y1 + 1;
    int16_t bw = sgl_stepper_btn_w(s);
    int16_t r = (int16_t)obj->radius;
    uint8_t dec_dis = sgl_stepper_dec_disabled(s);
    uint8_t inc_dis = sgl_stepper_inc_disabled(s);
    int16_t sign_half = sgl_max(h / 6, 4);
    int16_t sign_thick = sgl_max(h / 10, 2);

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN: {
        sgl_area_t clip = { x1, y1, x2, y2 };

        /* Background */
        sgl_draw_fill_rect(surf, &obj->area, &clip, r, s->bg_color, SGL_ALPHA_MAX);

        /* Left button area */
        {
            sgl_color_t lbg = (s->pressed && s->active_side < 0) ? s->btn_press_color : s->btn_color;
            sgl_area_t lb = { x1, y1, x1 + bw - 1, y2 };
            sgl_draw_fill_rect(surf, &obj->area, &lb, r, lbg, SGL_ALPHA_MAX);
        }

        /* Right button area */
        {
            sgl_color_t rbg = (s->pressed && s->active_side > 0) ? s->btn_press_color : s->btn_color;
            sgl_area_t rb = { x2 - bw + 1, y1, x2, y2 };
            sgl_draw_fill_rect(surf, &obj->area, &rb, r, rbg, SGL_ALPHA_MAX);
        }

        /* '-' sign */
        {
            sgl_color_t sc = dec_dis ? s->sign_dis_color
                           : (s->pressed && s->active_side < 0) ? s->sign_press_color : s->sign_color;
            sgl_stepper_draw_sign(surf, &obj->area, (int16_t)(x1 + bw / 2),
                                  (int16_t)(y1 + h / 2), sign_half, sign_thick, 0, sc, SGL_ALPHA_MAX);
        }

        /* '+' sign */
        {
            sgl_color_t sc = inc_dis ? s->sign_dis_color
                           : (s->pressed && s->active_side > 0) ? s->sign_press_color : s->sign_color;
            sgl_stepper_draw_sign(surf, &obj->area, (int16_t)(x2 - bw / 2 + 1),
                                  (int16_t)(y1 + h / 2), sign_half, sign_thick, 1, sc, SGL_ALPHA_MAX);
        }

        /* Value text */
        sgl_stepper_format(s);
        if (s->font && s->buf[0] != '\0') {
            int16_t tw = sgl_font_get_string_width(s->buf, s->font);
            int16_t th = sgl_font_get_height(s->font);
            int16_t tx = x1 + bw + (w - 2 * bw - tw) / 2;
            int16_t ty = y1 + (h - th) / 2;
            sgl_area_t ta = { (int16_t)(x1 + bw), y1, (int16_t)(x2 - bw + 1), y2 };
            sgl_draw_string(surf, &ta, tx, ty, s->buf, s->text_color, SGL_ALPHA_MAX, s->font);
        }

        /* Border */
        sgl_draw_fill_rect_border(surf, &obj->area, &clip, r,
                                  s->border_color, obj->border, SGL_ALPHA_MAX);
        break;
    }

    case SGL_EVENT_PRESSED: {
        int8_t side = (evt->pos.x != SGL_POS_MIN) ? sgl_stepper_hit_side(s, evt->pos.x) : 0;
        s->active_side = side;
        if (side != 0) {
            s->pressed = 1;
            sgl_stepper_apply(s, side);
        }
        break;
    }

    case SGL_EVENT_KEY_UP:
        sgl_stepper_apply(s, 1);
        break;
    case SGL_EVENT_KEY_DOWN: 
        sgl_stepper_apply(s, -1);
        break;

    case SGL_EVENT_RELEASED:
    case SGL_EVENT_DESTROYED:
        if (s->pressed || s->active_side != 0) {
            s->pressed = 0;
            s->active_side = 0;
            sgl_obj_set_dirty(obj);
        }
        break;
    default:
        break;
    }
}

/**
 * @brief  create a stepper object
 * @param  parent: parent object pointer
 * @return stepper object pointer, NULL on failure
 */
sgl_obj_t* sgl_stepper_create(sgl_obj_t *parent)
{
    sgl_stepper_t *s = sgl_malloc(sizeof(sgl_stepper_t));
    if (!s) {
        SGL_LOG_ERROR("sgl_stepper_create: malloc failed");
        return NULL;
    }
    memset(s, 0, sizeof(*s));

    sgl_obj_init(&s->obj, parent);
    sgl_obj_set_clickable(&s->obj);
    sgl_obj_set_editable(&s->obj);
    s->obj.construct_fn = sgl_stepper_construct_cb;
    s->obj.border       = 1;
    s->obj.radius       = 6;
    s->value          = 0;
    s->min_value      = 0;
    s->max_value      = 100;
    s->step           = 1;
    s->decimals       = 0;
    s->font           = sgl_get_system_font();
    s->active_side    = 0;
    s->pressed        = 0;
    s->wrap           = 0;
    s->buf[0]         = '\0';

    s->bg_color         = sgl_rgb(46, 52, 64);
    s->btn_color        = sgl_rgb(58, 66, 82);
    s->btn_press_color  = sgl_rgb(64, 152, 231);
    s->text_color       = sgl_rgb(229, 233, 240);
    s->sign_color       = sgl_rgb(210, 218, 232);
    s->sign_press_color = sgl_rgb(255, 255, 255);
    s->sign_dis_color   = sgl_rgb(90, 98, 114);
    s->border_color     = SGL_THEME_BORDER_COLOR;

    return &s->obj;
}

/**
 * @brief set stepper value
 * @param  obj: stepper object pointer
 * @param  value: new value (fixed-point)
 * @return none
 */
void sgl_stepper_set_value(sgl_obj_t *obj, int32_t value)
{
    sgl_stepper_t *s = sgl_container_of(obj, sgl_stepper_t, obj);
    int32_t nv = sgl_clamp(value, s->min_value, s->max_value);
    if (nv == s->value) return;
    s->value = nv;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get stepper value
 * @param  obj: stepper object pointer
 * @return current value (fixed-point)
 */
int32_t sgl_stepper_get_value(sgl_obj_t *obj)
{
    sgl_stepper_t *s = sgl_container_of(obj, sgl_stepper_t, obj);
    return s->value;
}

/**
 * @brief set step increment
 * @param  obj: stepper object pointer
 * @param  step: step value (>0)
 * @return none
 */
void sgl_stepper_set_step(sgl_obj_t *obj, int32_t step)
{
    sgl_stepper_t *s = sgl_container_of(obj, sgl_stepper_t, obj);
    s->step = (step > 0) ? step : 1;
}

/**
 * @brief set value range
 * @param  obj: stepper object pointer
 * @param  min_value: minimum value
 * @param  max_value: maximum value
 * @return none
 */
void sgl_stepper_set_range(sgl_obj_t *obj, int32_t min_value, int32_t max_value)
{
    sgl_stepper_t *s = sgl_container_of(obj, sgl_stepper_t, obj);
    if (min_value > max_value) { int32_t t = min_value; min_value = max_value; max_value = t; }
    s->min_value = min_value;
    s->max_value = max_value;
    int32_t nv = sgl_clamp(s->value, s->min_value, s->max_value);
    if (nv != s->value) {
        s->value = nv;
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set decimal places
 * @param  obj: stepper object pointer
 * @param  decimals: decimal places (0-4)
 * @return none
 */
void sgl_stepper_set_decimals(sgl_obj_t *obj, uint8_t decimals)
{
    sgl_stepper_t *s = sgl_container_of(obj, sgl_stepper_t, obj);
    s->decimals = (decimals > 4) ? 4 : decimals;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set wrap mode
 * @param  obj: stepper object pointer
 * @param  wrap: 1=wrap at boundary, 0=disable button
 * @return none
 */
void sgl_stepper_set_wrap(sgl_obj_t *obj, uint8_t wrap)
{
    sgl_stepper_t *s = sgl_container_of(obj, sgl_stepper_t, obj);
    s->wrap = wrap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set font for value display
 * @param  obj: stepper object pointer
 * @param  font: font pointer
 * @return none
 */
void sgl_stepper_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_stepper_t *s = sgl_container_of(obj, sgl_stepper_t, obj);
    s->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set background color
 * @param  obj: stepper object pointer
 * @param  color: background color
 * @return none
 */
void sgl_stepper_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_stepper_t *s = sgl_container_of(obj, sgl_stepper_t, obj);
    s->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set button color
 * @param  obj: stepper object pointer
 * @param  color: button color
 * @return none
 */
void sgl_stepper_set_btn_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_stepper_t *s = sgl_container_of(obj, sgl_stepper_t, obj);
    s->btn_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set text color
 * @param  obj: stepper object pointer
 * @param  color: text color
 * @return none
 */
void sgl_stepper_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_stepper_t *s = sgl_container_of(obj, sgl_stepper_t, obj);
    s->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set sign color
 * @param  obj: stepper object pointer
 * @param  color: sign color
 * @return none
 */
void sgl_stepper_set_sign_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_stepper_t *s = sgl_container_of(obj, sgl_stepper_t, obj);
    s->sign_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set border color
 * @param  obj: stepper object pointer
 * @param  color: border color
 * @return none
 */
void sgl_stepper_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_stepper_t *s = sgl_container_of(obj, sgl_stepper_t, obj);
    s->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set corner radius
 * @param  obj: stepper object pointer
 * @param  radius: corner radius
 * @return none
 */
void sgl_stepper_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    obj->radius = radius;
    sgl_obj_set_dirty(obj);
}
