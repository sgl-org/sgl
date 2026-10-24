/* source/widgets/digitalclock/sgl_digitalclock.c
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

#include <sgl_theme.h>
#include <string.h>
#include "sgl_digitalclock.h"

#define SGL_DIGITALCLOCK_DEFAULT_FLIP_MS   300
#define SGL_DIGITALCLOCK_DIGIT_COUNT       6
#define SGL_DIGITALCLOCK_PROGRESS_MAX      1024
#define SGL_DIGITALCLOCK_HALF_PROGRESS     (SGL_DIGITALCLOCK_PROGRESS_MAX / 2)
#define SGL_DIGITALCLOCK_DEFAULT_CONTRAST  255

typedef struct sgl_digitalclock_layout {
    int16_t card_w;
    int16_t card_h;
    int16_t gap;
    int16_t colon_w;
    int16_t radius;
    int16_t x;
    int16_t y;
} sgl_digitalclock_layout_t;

static void sgl_digitalclock_get_layout(sgl_obj_t *obj, sgl_digitalclock_layout_t *layout)
{
    sgl_digitalclock_t *clock;
    int16_t width;
    int16_t used_w;
    int16_t max_radius;

    width = sgl_obj_get_width(obj);
    layout->card_h = sgl_obj_get_height(obj);
    layout->gap = sgl_max(2, layout->card_h / 18);
    layout->colon_w = sgl_max(layout->gap * 3, layout->card_h / 5);
    layout->card_w = (width - 2 * layout->colon_w - 7 * layout->gap) / 6;

    if (layout->card_w < 4) {
        layout->gap = 1;
        layout->colon_w = 3;
        layout->card_w = (width - 13) / 6;
    }

    clock = sgl_container_of(obj, sgl_digitalclock_t, obj);
    max_radius = sgl_min(layout->card_w, layout->card_h) / 2;
    if (clock->radius == SGL_DIGITALCLOCK_RADIUS_AUTO)
        layout->radius = sgl_max(1, sgl_min(layout->card_w, layout->card_h) / 14);
    else
        layout->radius = sgl_max(0, sgl_min(clock->radius, max_radius));
    used_w = 6 * layout->card_w + 2 * layout->colon_w + 7 * layout->gap;
    layout->x = obj->coords.x1 + (width - used_w) / 2;
    layout->y = obj->coords.y1;
}

static void sgl_digitalclock_card_area(const sgl_digitalclock_layout_t *layout,
                                       uint8_t index, sgl_area_t *card)
{
    int16_t x;

    x = layout->x + index * (layout->card_w + layout->gap);
    if (index >= 2)
        x += layout->colon_w;
    if (index >= 4)
        x += layout->colon_w;

    card->x1 = x;
    card->y1 = layout->y;
    card->x2 = x + layout->card_w - 1;
    card->y2 = layout->y + layout->card_h - 1;
}

static void sgl_digitalclock_draw_digit(const sgl_digitalclock_t *clock,
                                        sgl_surf_t *surf, sgl_area_t *clip,
                                        const sgl_area_t *card, uint8_t digit,
                                        int16_t card_w, sgl_color_t color)
{
    char text[2];
    int16_t text_w;
    int16_t text_h;
    int16_t x;
    int16_t y;

    text[0] = (char)('0' + digit);
    text[1] = '\0';
    text_w = (int16_t)sgl_font_get_string_width(text, clock->font);
    text_h = sgl_font_get_height(clock->font);
    x = card->x1 + (card_w - text_w) / 2;
    y = card->y1 + ((card->y2 - card->y1 + 1) - text_h) / 2;
    sgl_draw_string(surf, clip, x, y, text, color, clock->alpha, clock->font);
}

static void sgl_digitalclock_draw_scaled_half(const sgl_digitalclock_t *clock,
                                               sgl_surf_t *surf, const sgl_area_t *card,
                                               uint8_t digit, int16_t card_w,
                                               int16_t split, bool top,
                                               int16_t dest_y1, int16_t dest_y2,
                                               sgl_color_t color)
{
    char text[2];
    sgl_area_t row;
    int16_t text_w;
    int16_t text_h;
    int16_t text_x;
    int16_t text_y;
    int16_t source_h;
    int16_t dest_h;
    int16_t source_y;
    int16_t y;

    dest_h = dest_y2 - dest_y1 + 1;
    if (dest_h <= 0)
        return;

    source_h = top ? split - card->y1 : card->y2 - split + 1;
    if (source_h <= 0)
        return;

    text[0] = (char)('0' + digit);
    text[1] = '\0';
    text_w = (int16_t)sgl_font_get_string_width(text, clock->font);
    text_h = sgl_font_get_height(clock->font);
    text_x = card->x1 + (card_w - text_w) / 2;
    text_y = card->y1 + ((card->y2 - card->y1 + 1) - text_h) / 2;
    row.x1 = card->x1 + 1;
    row.x2 = card->x2 - 1;

    for (y = dest_y1; y <= dest_y2; y++) {
        if (top)
            source_y = card->y1 + (y - dest_y1) * source_h / dest_h;
        else
            source_y = split + (y - dest_y1) * source_h / dest_h;

        row.y1 = y;
        row.y2 = y;
        sgl_draw_string(surf, &row, text_x, text_y + y - source_y, text, color, clock->alpha, clock->font);
    }
}

/* Blend a settled color towards the flip color. level 0 keeps the settled
 * color, level 255 uses the flip color. */
static sgl_color_t sgl_digitalclock_flip_mix(sgl_color_t base, sgl_color_t flip, uint8_t level)
{
    return sgl_color_mixer(flip, base, level);
}

static void sgl_digitalclock_draw_card(sgl_digitalclock_t *clock, sgl_surf_t *surf,
                                       const sgl_digitalclock_layout_t *layout, uint8_t index)
{
    sgl_area_t card;
    sgl_area_t clip;
    sgl_area_t flap;
    sgl_color_t flap_card_color;
    sgl_color_t flap_text_color;
    sgl_color_t seam_color;
    uint8_t old_digit;
    uint8_t new_digit;
    uint16_t phase;
    uint16_t progress;
    int16_t split;
    int16_t top_h;
    int16_t bottom_h;
    int16_t flap_h;
    uint8_t tint;
    bool flipping;

    sgl_digitalclock_card_area(layout, index, &card);
    old_digit = clock->old_digits[index];
    new_digit = clock->digits[index];
    progress = clock->progress;
    flipping = old_digit != new_digit && progress < SGL_DIGITALCLOCK_PROGRESS_MAX;
    split = card.y1 + layout->card_h / 2;
    top_h = split - card.y1;
    bottom_h = card.y2 - split + 1;

    sgl_draw_fill_rect(surf, &clock->obj.area, &card, layout->radius,
                       clock->card_color, clock->alpha);

    clip.x1 = card.x1 + 1;
    clip.x2 = card.x2 - 1;
    if (!flipping) {
        clip.y1 = card.y1;
        clip.y2 = card.y2;
        sgl_digitalclock_draw_digit(clock, surf, &clip, &card, new_digit,
                                    layout->card_w, clock->text_color);
    }
    else {
        /* The incoming upper half is uncovered as the old leaf falls. The
         * outgoing lower half stays in place until the new leaf opens. */
        clip.y1 = card.y1;
        clip.y2 = split - 1;
        sgl_digitalclock_draw_digit(clock, surf, &clip, &card, new_digit,
                                    layout->card_w, clock->text_color);
        clip.y1 = split;
        clip.y2 = card.y2;
        sgl_digitalclock_draw_digit(clock, surf, &clip, &card, old_digit,
                                    layout->card_w, clock->text_color);

        if (progress < SGL_DIGITALCLOCK_HALF_PROGRESS) {
            phase = progress;
            flap_h = top_h - top_h * phase / SGL_DIGITALCLOCK_HALF_PROGRESS;
            tint = (uint8_t)((uint16_t)clock->flip_contrast *
                    (SGL_DIGITALCLOCK_HALF_PROGRESS - phase) /
                    SGL_DIGITALCLOCK_HALF_PROGRESS);
            flap.x1 = card.x1;
            flap.x2 = card.x2;
            flap.y1 = split - flap_h;
            flap.y2 = split - 1;
            flap_card_color = sgl_digitalclock_flip_mix(clock->card_color,
                                                       clock->flip_color, tint);
            flap_text_color = sgl_digitalclock_flip_mix(clock->text_color,
                                                       clock->flip_text_color, tint);
            /* Clip the upper half of a complete rounded card. */
            sgl_draw_fill_rect(surf, &flap, &card, layout->radius,
                               flap_card_color, clock->alpha);
            sgl_digitalclock_draw_scaled_half(clock, surf, &card, old_digit,
                                               layout->card_w, split, true,
                                               flap.y1, flap.y2, flap_text_color);
        }
        else {
            phase = progress - SGL_DIGITALCLOCK_HALF_PROGRESS;
            flap_h = 1 + (bottom_h - 1) * phase / SGL_DIGITALCLOCK_HALF_PROGRESS;
            tint = (uint8_t)((uint16_t)clock->flip_contrast *
                    (SGL_DIGITALCLOCK_HALF_PROGRESS - phase) /
                    SGL_DIGITALCLOCK_HALF_PROGRESS);
            flap.x1 = card.x1;
            flap.x2 = card.x2;
            flap.y1 = split;
            flap.y2 = split + flap_h - 1;
            flap_card_color = sgl_digitalclock_flip_mix(clock->card_color,
                                                       clock->flip_color, tint);
            flap_text_color = sgl_digitalclock_flip_mix(clock->text_color,
                                                       clock->flip_text_color, tint);

            sgl_draw_fill_rect(surf, &flap, &card, layout->radius,
                               flap_card_color, clock->alpha);
            sgl_digitalclock_draw_scaled_half(clock, surf, &card, new_digit,
                                               layout->card_w, split, false,
                                               flap.y1, flap.y2, flap_text_color);
        }
    }

    seam_color = sgl_color_mixer(sgl_rgb(72, 72, 74), clock->card_color, 112);
    sgl_draw_fill_hline(surf, &clock->obj.area, split - 1,
                        card.x1 + 1, card.x2 - 1, 1, seam_color, clock->alpha);
}

static void sgl_digitalclock_draw_colon(sgl_digitalclock_t *clock, sgl_surf_t *surf,
                                        const sgl_digitalclock_layout_t *layout,
                                        int16_t x)
{
    int16_t cx;
    int16_t cy1;
    int16_t cy2;
    int16_t radius;

    cx = x + layout->colon_w / 2;
    cy1 = layout->y + layout->card_h * 35 / 100;
    cy2 = layout->y + layout->card_h * 65 / 100;
    radius = sgl_max(1, layout->colon_w / 9);
    sgl_draw_fill_circle(surf, &clock->obj.area, cx, cy1, radius,
                         clock->colon_color, clock->alpha);
    sgl_draw_fill_circle(surf, &clock->obj.area, cx, cy2, radius,
                         clock->colon_color, clock->alpha);
}

static void sgl_digitalclock_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_digitalclock_t *clock;
    sgl_digitalclock_layout_t layout;
    int16_t colon_x;
    uint8_t i;

    clock = sgl_container_of(obj, sgl_digitalclock_t, obj);
    if (evt->type == SGL_EVENT_DESTROYED) {
        sgl_anim_delete_by_obj(obj);
        return;
    }
    if (evt->type != SGL_EVENT_DRAW_MAIN || clock->font == NULL)
        return;

    sgl_digitalclock_get_layout(obj, &layout);
    if (layout.card_w < 3 || layout.card_h < 4)
        return;

    for (i = 0; i < SGL_DIGITALCLOCK_DIGIT_COUNT; i++) {
        sgl_digitalclock_draw_card(clock, surf, &layout, i);
        if (i == 1 || i == 3) {
            colon_x = layout.x + (i + 1) * layout.card_w + i * layout.gap;
            if (i >= 2)
                colon_x += layout.colon_w;
            colon_x += layout.gap;
            sgl_digitalclock_draw_colon(clock, surf, &layout, colon_x);
        }
    }
}

static void sgl_digitalclock_anim_cb(sgl_anim_t *anim, int32_t value)
{
    sgl_digitalclock_t *clock;

    clock = sgl_container_of(anim->data, sgl_digitalclock_t, obj);
    clock->progress = (uint16_t)value;
    sgl_obj_set_dirty(&clock->obj);
}

static void sgl_digitalclock_anim_finish_cb(sgl_anim_t *anim)
{
    sgl_digitalclock_t *clock;

    clock = sgl_container_of(anim->data, sgl_digitalclock_t, obj);
    memcpy(clock->old_digits, clock->digits, sizeof(clock->digits));
    clock->progress = SGL_DIGITALCLOCK_PROGRESS_MAX;
    sgl_obj_set_dirty(&clock->obj);
}

/**
 * @brief Create a split-flap digital clock object with default appearance.
 * @param parent Parent object; NULL attaches to the active screen.
 * @return Clock object pointer, or NULL when memory allocation fails.
 * @note Initial time is 00:00:00. Default colors, font, radius and flip
 *       duration are applied automatically.
 */
sgl_obj_t* sgl_digitalclock_create(sgl_obj_t *parent)
{
    sgl_digitalclock_t *clock;
    sgl_obj_t *obj;

    clock = sgl_malloc(sizeof(sgl_digitalclock_t));
    if (clock == NULL) {
        SGL_LOG_ERROR("sgl_digitalclock_create: malloc failed");
        return NULL;
    }
    memset(clock, 0, sizeof(sgl_digitalclock_t));

    obj = &clock->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_digitalclock_construct_cb;
    obj->clickable = 0;
    clock->font = sgl_get_system_font();
    clock->card_color = sgl_rgb(24, 24, 26);
    clock->text_color = SGL_COLOR_WHITE;
    clock->colon_color = SGL_COLOR_WHITE;
    clock->flip_color = sgl_rgb(136, 140, 150);
    clock->flip_text_color = sgl_rgb(28, 28, 32);
    clock->flip_contrast = SGL_DIGITALCLOCK_DEFAULT_CONTRAST;
    clock->radius = SGL_DIGITALCLOCK_RADIUS_AUTO;
    clock->flip_ms = SGL_DIGITALCLOCK_DEFAULT_FLIP_MS;
    clock->alpha = SGL_THEME_ALPHA;
    clock->progress = SGL_DIGITALCLOCK_PROGRESS_MAX;

    return obj;
}

/**
 * @brief Set the displayed time and trigger a flip animation for changed digits.
 * @param obj Clock object.
 * @param hour Hour value; taken modulo 24 (0-23 effective).
 * @param min Minute value; clamped to 0-59.
 * @param sec Second value; clamped to 0-59.
 * @return none
 * @note If a flip animation is in progress it is completed synchronously
 *       before the new transition starts. When flip_ms is 0 the update is
 *       immediate with no animation.
 */
void sgl_digitalclock_set_time(sgl_obj_t *obj, uint8_t hour, uint8_t min, uint8_t sec)
{
    sgl_digitalclock_t *clock;
    sgl_anim_t *anim;
    uint8_t target[SGL_DIGITALCLOCK_DIGIT_COUNT];
    bool changed;
    uint8_t i;

    clock = sgl_container_of(obj, sgl_digitalclock_t, obj);
    hour %= 24;
    min = sgl_min(min, 59);
    sec = sgl_min(sec, 59);
    target[0] = hour / 10;
    target[1] = hour % 10;
    target[2] = min / 10;
    target[3] = min % 10;
    target[4] = sec / 10;
    target[5] = sec % 10;

    changed = false;
    for (i = 0; i < SGL_DIGITALCLOCK_DIGIT_COUNT; i++) {
        if (target[i] != clock->digits[i]) {
            changed = true;
            break;
        }
    }
    if (!changed)
        return;

    anim = sgl_anim_get_by_obj(obj);
    if (anim != NULL) {
        /* Never mix a partially flipped old value into a new transition. */
        sgl_digitalclock_anim_finish_cb(anim);
        sgl_anim_delete(anim);
    }

    memcpy(clock->old_digits, clock->digits, sizeof(clock->digits));
    memcpy(clock->digits, target, sizeof(clock->digits));

    if (clock->flip_ms == 0) {
        memcpy(clock->old_digits, clock->digits, sizeof(clock->digits));
        clock->progress = SGL_DIGITALCLOCK_PROGRESS_MAX;
        sgl_obj_set_dirty(obj);
        return;
    }

    clock->progress = 0;
    sgl_obj_set_dirty(obj);
    sgl_anim_setup_obj_action(obj, 0, SGL_DIGITALCLOCK_PROGRESS_MAX, clock->flip_ms,
                              sgl_digitalclock_anim_cb, SGL_ANIM_PATH_EASE_IN_OUT,
                              sgl_digitalclock_anim_finish_cb);
    if (sgl_anim_get_by_obj(obj) == NULL) {
        /* Allocation failed: keep the new time usable without animation. */
        memcpy(clock->old_digits, clock->digits, sizeof(clock->digits));
        clock->progress = SGL_DIGITALCLOCK_PROGRESS_MAX;
        sgl_obj_set_dirty(obj);
    }
}

/**
 * @brief Set the font used to draw digit glyphs on the cards.
 * @param obj Clock object.
 * @param font Font pointer; NULL restores the system default font.
 * @return none
 */
void sgl_digitalclock_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_digitalclock_t *clock;

    clock = sgl_container_of(obj, sgl_digitalclock_t, obj);
    clock->font = font ? font : sgl_get_system_font();
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the duration of a complete two-stage flip animation.
 * @param obj Clock object.
 * @param duration_ms Flip duration in milliseconds; 0 disables animation
 *        so digits update immediately.
 * @return none
 * @note Applies to the next sgl_digitalclock_set_time call; does not affect
 *       a currently running animation.
 */
void sgl_digitalclock_set_flip_time(sgl_obj_t *obj, uint16_t duration_ms)
{
    sgl_digitalclock_t *clock;

    clock = sgl_container_of(obj, sgl_digitalclock_t, obj);
    clock->flip_ms = duration_ms;
}

/**
 * @brief Set the background color of the digit cards.
 * @param obj Clock object.
 * @param color Card fill color.
 * @return none
 */
void sgl_digitalclock_set_card_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_digitalclock_t *clock;

    clock = sgl_container_of(obj, sgl_digitalclock_t, obj);
    clock->card_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the color of the static digit text on settled cards.
 * @param obj Clock object.
 * @param color Digit text color.
 * @return none
 */
void sgl_digitalclock_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_digitalclock_t *clock;

    clock = sgl_container_of(obj, sgl_digitalclock_t, obj);
    clock->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the color of the two colon separators between hour/minute/second pairs.
 * @param obj Clock object.
 * @param color Colon dot color.
 * @return none
 */
void sgl_digitalclock_set_colon_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_digitalclock_t *clock;

    clock = sgl_container_of(obj, sgl_digitalclock_t, obj);
    clock->colon_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the highlight color shown on the moving leaf during a flip.
 * @param obj Clock object.
 * @param color Leaf card color at peak contrast; it blends back to
 *        card_color as the leaf settles.
 * @return none
 */
void sgl_digitalclock_set_flip_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_digitalclock_t *clock;

    clock = sgl_container_of(obj, sgl_digitalclock_t, obj);
    clock->flip_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the digit text color shown on the moving leaf during a flip.
 * @param obj Clock object.
 * @param color Leaf text color at peak contrast; it blends back to
 *        text_color as the leaf settles.
 * @return none
 */
void sgl_digitalclock_set_flip_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_digitalclock_t *clock;

    clock = sgl_container_of(obj, sgl_digitalclock_t, obj);
    clock->flip_text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set how strongly the moving leaf uses the flip highlight colors.
 * @param obj Clock object.
 * @param contrast Peak blend level 0-255; 0 keeps settled colors (no visual
 *        highlight), 255 uses flip_color/flip_text_color at full strength.
 * @return none
 */
void sgl_digitalclock_set_flip_contrast(sgl_obj_t *obj, uint8_t contrast)
{
    sgl_digitalclock_t *clock;

    clock = sgl_container_of(obj, sgl_digitalclock_t, obj);
    clock->flip_contrast = contrast;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the corner radius of the digit cards.
 * @param obj Clock object.
 * @param radius Corner radius in pixels. SGL_DIGITALCLOCK_RADIUS_AUTO (-1)
 *        restores the size-dependent auto default. Positive values are
 *        clamped to half the shorter card dimension.
 * @return none
 */
void sgl_digitalclock_set_radius(sgl_obj_t *obj, int16_t radius)
{
    sgl_digitalclock_t *clock;

    clock = sgl_container_of(obj, sgl_digitalclock_t, obj);
    clock->radius = (radius < 0) ? SGL_DIGITALCLOCK_RADIUS_AUTO : radius;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the overall opacity applied to all card, text and colon drawing.
 * @param obj Clock object.
 * @param alpha Opacity value 0 (transparent) to 255 (fully opaque).
 * @return none
 */
void sgl_digitalclock_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_digitalclock_t *clock;

    clock = sgl_container_of(obj, sgl_digitalclock_t, obj);
    clock->alpha = alpha;
    sgl_obj_set_dirty(obj);
}
