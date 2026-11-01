/* source/widgets/coverflow/sgl_coverflow.c
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
#include "../rect/sgl_rect.h"
#include "../label/sgl_label.h"
#include "sgl_coverflow.h"

#define COVERFLOW_DEFAULT_SPACING    (170)   /* resting distance between adjacent card centers, px */
#define COVERFLOW_DEFAULT_CARD_W     (170)   /* focused card width, px */
#define COVERFLOW_DEFAULT_CARD_H     (200)   /* focused card height, px */
#define COVERFLOW_DEFAULT_MIN_SCALE  (560)   /* side card scale, permille of the focused card */
#define COVERFLOW_DEFAULT_ANIM_MS    (260)   /* snap animation duration, ms */
#define COVERFLOW_DEFAULT_RADIUS     (24)    /* focused card corner radius, px */
#define COVERFLOW_DEFAULT_BORDER_W   (2)     /* card border width, px */
#define COVERFLOW_CAPTION_GAP        (8)     /* gap between the focused card and its caption, px */

/**
 * @brief place and scale every card from the current camera position
 * @param coverflow coverflow object
 * @return none
 * @note  card scale is derived from its distance to the widget center; the
 *        nearest card is raised above its neighbours
 */
static void coverflow_layout(sgl_coverflow_t *coverflow)
{
    const int16_t center_x = sgl_obj_get_width(&coverflow->obj) / 2;
    const int16_t center_y = sgl_obj_get_height(&coverflow->obj) / 2;
    int16_t top = 0;
    int32_t top_dist = INT32_MAX;

    for (int16_t i = 0; i < coverflow->card_num; i++) {
        const int32_t off = i * coverflow->spacing - coverflow->scroll;
        int32_t dist = sgl_abs(off);
        if (dist > coverflow->spacing) {
            dist = coverflow->spacing;
        }

        /* permille scale: 1000 at the center, min_scale one card away */
        const int32_t scale = 1000 - dist * (1000 - coverflow->min_scale) / coverflow->spacing;
        const int16_t w = coverflow->card_w * scale / 1000;
        const int16_t h = coverflow->card_h * scale / 1000;
        /* side cards are pushed towards the center (x compressed to 3/5),
         * so neighbours partially stack behind the focused card */
        const int16_t cx = center_x + off * 3 / 5;
        const uint16_t radius = coverflow->radius * scale / 1000;

        sgl_obj_set_pos(coverflow->cards[i], cx - w / 2, center_y - h / 2);
        sgl_obj_set_size(coverflow->cards[i], w, h);
        sgl_rect_set_radius(coverflow->cards[i], radius);
        /* the caption is a direct child of the widget. a sgl_label keeps the
         * box it was created with (the parent size) and centers its text in
         * that box, so shrink the box to the text before placing it centered
         * just below the card (widget-local coordinates); the caption is only
         * shown for the focused card (see the visibility pass below) */
        if (coverflow->labels[i] != NULL) {
            sgl_obj_t *label = coverflow->labels[i];
            sgl_label_t *lbl = sgl_container_of(label, sgl_label_t, obj);
            /* measure with the label's current font: sgl_label keeps its
             * text_length from creation and does not refresh it on
             * sgl_label_set_font, so it may be stale / zero here */
            const int16_t tw = sgl_font_get_string_width(lbl->text, lbl->font);
            const int16_t th = sgl_font_get_height(lbl->font);
            sgl_obj_set_size(label, tw, th);
            sgl_obj_set_pos(label, cx - tw / 2,
                            center_y + h / 2 + COVERFLOW_CAPTION_GAP);
        }

        const int32_t true_dist = sgl_abs(off);
        if (true_dist < top_dist) {
            top_dist = true_dist;
            top = i;
        }
    }

    /* only the centered (top) card shows its caption, the side cards hide
     * theirs; toggle the flags only when they really change so a drag does
     * not push redundant dirty areas on every frame */
    for (int16_t i = 0; i < coverflow->card_num; i++) {
        if (coverflow->labels[i] == NULL) {
            continue;
        }
        const bool show = (i == top);
        if (show && sgl_obj_is_hidden(coverflow->labels[i])) {
            sgl_obj_set_visible(coverflow->labels[i]);
        }
        else if (!show && !sgl_obj_is_hidden(coverflow->labels[i])) {
            sgl_obj_set_hidden(coverflow->labels[i]);
        }
    }

    if (top != coverflow->top) {
        coverflow->top = top;
        sgl_obj_move_top(coverflow->cards[top]);
        /* keep the focused caption above its card */
        if (coverflow->labels[top] != NULL) {
            sgl_obj_move_top(coverflow->labels[top]);
        }
    }
}

/**
 * @brief animation frame callback, value is the new camera position
 */
static void coverflow_anim_path_cb(sgl_anim_t *anim, int32_t value)
{
    sgl_coverflow_t *coverflow = (sgl_coverflow_t *)sgl_anim_get_data(anim);
    coverflow->scroll = value;
    coverflow_layout(coverflow);
}

/**
 * @brief animation finished callback
 * @note  the animation frees itself (auto free), just drop the reference
 */
static void coverflow_anim_finish_cb(sgl_anim_t *anim)
{
    sgl_coverflow_t *coverflow = (sgl_coverflow_t *)sgl_anim_get_data(anim);
    coverflow->anim = NULL;
}

/**
 * @brief scroll the menu with an ease-out scale animation
 * @param coverflow coverflow object
 * @param target camera position in pixels, clamped to the card range
 * @return none
 */
static void coverflow_scroll_to(sgl_coverflow_t *coverflow, int32_t target)
{
    target = sgl_clamp(target, 0, (coverflow->card_num - 1) * coverflow->spacing);
    coverflow->index = target / coverflow->spacing;

    if (coverflow->anim != NULL) {
        sgl_anim_delete(coverflow->anim);
        coverflow->anim = NULL;
    }
    if (target == coverflow->scroll) {
        /* already at the target camera position, but the widget geometry may
         * have changed since the last layout (the caller resized / moved it
         * after creation): re-run the layout so the cards stay centered */
        coverflow_layout(coverflow);
        return;
    }

    coverflow->anim = sgl_anim_create();
    if (coverflow->anim == NULL) {
        coverflow->scroll = target;
        coverflow_layout(coverflow);
        return;
    }

    sgl_anim_set_data(coverflow->anim, coverflow);
    sgl_anim_set_start_value(coverflow->anim, coverflow->scroll);
    sgl_anim_set_end_value(coverflow->anim, target);
    sgl_anim_set_act_duration(coverflow->anim, coverflow->anim_ms);
    sgl_anim_set_path(coverflow->anim, coverflow_anim_path_cb, SGL_ANIM_PATH_EASE_OUT);
    sgl_anim_set_finish_cb(coverflow->anim, coverflow_anim_finish_cb);
    sgl_anim_set_auto_free(coverflow->anim);
    sgl_anim_start(coverflow->anim, SGL_ANIM_REPEAT_ONCE);
}

/**
 * @brief scroll engine commit callback, mirrors sc->offset into the camera
 * @param sc scroll state owned by this widget
 * @return none
 * @note called on every inertia/rebound frame; once the glide settles
 *       (coasting cleared) the menu snaps to the nearest card
 */
static void coverflow_scroll_commit(sgl_scroll_t *sc)
{
    sgl_coverflow_t *coverflow = sgl_container_of(sc, sgl_coverflow_t, sc);
    coverflow->scroll = sc->offset;
    coverflow_layout(coverflow);

    if (!sc->coasting) {
        coverflow_scroll_to(coverflow, ((coverflow->scroll + coverflow->spacing / 2) / coverflow->spacing) * coverflow->spacing);
    }
}

/**
 * @brief find the card under the given screen position
 * @param coverflow coverflow object
 * @param pos event position in screen coordinates
 * @return card index, -1 if the position hits no card
 * @note  cards overlap near the focused one, so among all hit cards the
 *        one whose center is closest to the position wins
 */
static int16_t coverflow_hit_card(sgl_coverflow_t *coverflow, sgl_event_pos_t pos)
{
    int16_t hit = -1;
    int32_t hit_dist = INT32_MAX;

    for (int16_t i = 0; i < coverflow->card_num; i++) {
        const sgl_area_t *coords = &coverflow->cards[i]->coords;
        if (pos.x < coords->x1 || pos.x > coords->x2 ||
            pos.y < coords->y1 || pos.y > coords->y2) {
            continue;
        }

        const int32_t dist = sgl_abs(pos.x - (coords->x1 + coords->x2) / 2);
        if (dist < hit_dist) {
            hit_dist = dist;
            hit = i;
        }
    }

    return hit;
}

/**
 * @brief forward an event to a card: its construct callback produces the
 *        press visual feedback, then the user event callback (if any) is
 *        invoked with the card as the event object
 * @param coverflow coverflow object
 * @param index target card index
 * @param evt event to forward
 * @return none
 */
static void coverflow_forward_event(sgl_coverflow_t *coverflow, int16_t index, sgl_event_t *evt)
{
    sgl_obj_t *card = coverflow->cards[index];
    sgl_event_t ce = *evt;
    ce.obj = card;
    ce.event_data = card->event_data;

    card->construct_fn(NULL, card, &ce);
    if (card->event_fn != NULL) {
        card->event_fn(&ce);
    }
}

/**
 * @brief coverflow construct callback, the widget itself draws nothing;
 *        it handles the touch / key scrolling and releases its resources
 *        on destroy
 * @note  tapping a side card scrolls it to the center; tapping the
 *        centered card forwards the click to the card's own event
 *        callback so the user can handle it
 */
static void sgl_coverflow_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    SGL_UNUSED(surf);
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    const int32_t range = (coverflow->card_num - 1) * coverflow->spacing;

    switch (evt->type) {
    case SGL_EVENT_PRESSED:
        /* grab the menu: stop the snap animation and any coasting inertia,
         * sync the engine with the current camera, then start tracking */
        if (coverflow->anim != NULL) {
            sgl_anim_delete(coverflow->anim);
            coverflow->anim = NULL;
        }
        sgl_scroll_anim_stop(&coverflow->sc);
        coverflow->sc.offset = coverflow->scroll;
        sgl_scroll_press(&coverflow->sc, evt->pos.x);

        /* press feedback on the touched card (flexible zoom) */
        coverflow->press_card = coverflow_hit_card(coverflow, evt->pos);
        if (coverflow->press_card >= 0) {
            coverflow_forward_event(coverflow, coverflow->press_card, evt);
        }
        break;

    case SGL_EVENT_MOVE_LEFT:
    case SGL_EVENT_MOVE_RIGHT:
        /* cards follow the finger; the engine samples the flick speed on
         * the way and rubber-bands beyond the first / last card */
        if (sgl_scroll_stay(&coverflow->sc, evt->pos.x, range)) {
            coverflow->scroll = coverflow->sc.offset;
            coverflow_layout(coverflow);
        }
        break;

    case SGL_EVENT_RELEASED:
        /* flick: coast with inertia, snap when the glide settles (commit);
         * dragged release without inertia: snap straight to the nearest
         * card. A plain tap (no drag) is left alone: the CLICKED event,
         * which is queued right before this release, may have started a
         * scroll animation of its own and snapping here would cancel it */
        coverflow->sc.range  = range;
        coverflow->sc.commit = coverflow_scroll_commit;
        if (sgl_scroll_release(&coverflow->sc, range)) {
            sgl_scroll_anim_start(&coverflow->sc);
        }
        else if (coverflow->sc.dragged) {
            coverflow->sc.dragged = 0;
            coverflow_scroll_to(coverflow, ((coverflow->scroll + coverflow->spacing / 2) / coverflow->spacing) * coverflow->spacing);
        }

        /* release feedback on the card the press started on */
        if (coverflow->press_card >= 0) {
            coverflow_forward_event(coverflow, coverflow->press_card, evt);
            coverflow->press_card = -1;
        }
        break;

    case SGL_EVENT_CLICKED:
    case SGL_EVENT_LONG_CLICKED: {
        /* ENTER (quick tap or hold) activates the centered card, so both
         * click kinds are handled the same way here.
         * a drag ends with no activation, snap back to the nearest card */
        if (coverflow->sc.dragged) {
            coverflow->sc.dragged = 0;
            coverflow_scroll_to(coverflow, ((coverflow->scroll + coverflow->spacing / 2) / coverflow->spacing) * coverflow->spacing);
            break;
        }

        /* key driven click (Enter) activates the centered card */
        int16_t hit = (evt->pos.x == SGL_POS_MIN && evt->pos.y == SGL_POS_MIN) ?
                      coverflow->index : coverflow_hit_card(coverflow, evt->pos);
        if (hit < 0) {
            break;
        }
        SGL_LOG_INFO("coverflow CLICKED: pos %d,%d hit %d index %d", evt->pos.x, evt->pos.y, hit, coverflow->index);

        if (hit != coverflow->index) {
            /* side card tapped: slide it to the center */
            coverflow_scroll_to(coverflow, hit * coverflow->spacing);
        }
        else {
            /* centered card tapped: fire the user event callback */
            SGL_LOG_INFO("coverflow: center card %d clicked, forward to user cb", hit);
            coverflow_forward_event(coverflow, hit, evt);
        }
        break;
    }

    case SGL_EVENT_KEY_LEFT:
    case SGL_EVENT_KEY_UP:
        sgl_scroll_anim_stop(&coverflow->sc);
        coverflow_scroll_to(coverflow, (coverflow->index - 1) * coverflow->spacing);
        break;

    case SGL_EVENT_KEY_RIGHT:
    case SGL_EVENT_KEY_DOWN:
        sgl_scroll_anim_stop(&coverflow->sc);
        coverflow_scroll_to(coverflow, (coverflow->index + 1) * coverflow->spacing);
        break;

    case SGL_EVENT_KEY_ESC:
        sgl_key_send_signal(evt, SGL_EVENT_KEY_CANCEL);
        break;

    case SGL_EVENT_DESTROYED:
        if (coverflow->anim != NULL) {
            sgl_anim_delete(coverflow->anim);
            coverflow->anim = NULL;
        }
        sgl_scroll_anim_stop(&coverflow->sc);
        if (coverflow->group != NULL) {
            sgl_key_group_delete(coverflow->group);
            coverflow->group = NULL;
        }
        sgl_free(coverflow->cards);
        sgl_free(coverflow->labels);
        coverflow->cards = NULL;
        coverflow->labels = NULL;
        break;

    default:
        break;
    }
}

/**
 * @brief create a coverflow object
 * @param parent parent object, NULL creates the menu on the active screen
 * @param cards card descriptor array (copied into the created cards)
 * @param card_num number of cards
 * @return coverflow object, NULL on failure
 */
sgl_obj_t* sgl_coverflow_create(sgl_obj_t *parent, const sgl_coverflow_card_t *cards, int16_t card_num)
{
    if (cards == NULL || card_num <= 0) {
        SGL_LOG_ERROR("sgl_coverflow_create: invalid cards");
        return NULL;
    }

    sgl_coverflow_t *coverflow = sgl_malloc(sizeof(sgl_coverflow_t));
    if (coverflow == NULL) {
        SGL_LOG_ERROR("sgl_coverflow_create: malloc failed");
        return NULL;
    }
    memset(coverflow, 0, sizeof(sgl_coverflow_t));

    coverflow->cards = sgl_malloc(card_num * sizeof(sgl_obj_t*));
    coverflow->labels = sgl_malloc(card_num * sizeof(sgl_obj_t*));
    if (coverflow->cards == NULL || coverflow->labels == NULL) {
        SGL_LOG_ERROR("sgl_coverflow_create: malloc failed");
        sgl_free(coverflow->cards);
        sgl_free(coverflow->labels);
        sgl_free(coverflow);
        return NULL;
    }
    memset(coverflow->cards, 0, card_num * sizeof(sgl_obj_t*));
    memset(coverflow->labels, 0, card_num * sizeof(sgl_obj_t*));

    sgl_obj_t *obj = &coverflow->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_coverflow_construct_cb;
    /* the widget itself is the event target: cards stay unclickable so the
     * click detection falls through to this object; movable enables drag
     * tracking, editable lets ENTER activate the centered card while the
     * group is in the (auto-enabled) key scroll mode */
    sgl_obj_set_movable(obj);
    sgl_obj_set_editable(obj);
    /* default to the parent size, the focused card rests at the center */
    sgl_obj_set_size(obj, sgl_obj_get_width(obj->parent), sgl_obj_get_height(obj->parent));

    obj->border = COVERFLOW_DEFAULT_BORDER_W;
    coverflow->card_num = card_num;
    coverflow->spacing = COVERFLOW_DEFAULT_SPACING;
    coverflow->card_w = COVERFLOW_DEFAULT_CARD_W;
    coverflow->card_h = COVERFLOW_DEFAULT_CARD_H;
    coverflow->min_scale = COVERFLOW_DEFAULT_MIN_SCALE;
    coverflow->anim_ms = COVERFLOW_DEFAULT_ANIM_MS;
    coverflow->radius = COVERFLOW_DEFAULT_RADIUS;
    coverflow->border_color = SGL_COLOR_WHITE;
    coverflow->text_color = SGL_COLOR_WHITE;
    coverflow->font = sgl_get_system_font();
    coverflow->top = -1;
    coverflow->press_card = -1;
    sgl_scroll_reset(&coverflow->sc);

    /* key group for arrow key scrolling: the widget is the only member and,
     * being a main menu, is focused and switched into the editing state at
     * load time, so LEFT / RIGHT / UP / DOWN scroll one card per press right
     * away and ENTER activates the centered card (no arming press needed) */
    coverflow->group = sgl_key_group_create(1);
    if (coverflow->group == NULL) {
        SGL_LOG_ERROR("sgl_coverflow_create: create key group failed");
    }
    else {
        sgl_key_group_add_obj(coverflow->group, obj);
    }

    for (int16_t i = 0; i < card_num; i++) {
        sgl_obj_t *card = sgl_rect_create(obj);
        coverflow->cards[i] = card;
        sgl_obj_set_flexible(card);
        sgl_rect_set_color(card, cards[i].color);
        sgl_rect_set_border_width(card, obj->border);
        sgl_rect_set_border_color(card, coverflow->border_color);
        sgl_rect_set_pixmap(card, cards[i].pixmap);

        /* card caption: a direct child of the widget (not of the card) so
         * the click detection always resolves to the widget itself; it is
         * kept centered on its card by coverflow_layout() */
        if (cards[i].text != NULL) {
            sgl_obj_t *label = sgl_label_create(obj);
            sgl_label_set_font(label, coverflow->font);
            sgl_label_set_text(label, cards[i].text);
            sgl_label_set_text_color(label, coverflow->text_color);
            coverflow->labels[i] = label;
        }
    }

    /* start with the middle card focused */
    coverflow->index = card_num / 2;
    coverflow->scroll = coverflow->index * coverflow->spacing;
    coverflow_layout(coverflow);

    if (coverflow->group != NULL) {
        /* focus the coverflow and switch the group into the editing state
         * right away: this is a main menu, so the arrow keys must scroll card
         * by card without a first ENTER, and ENTER activates the center card.
         * the group is dedicated to this widget, so forcing editing here is
         * safe and does not affect the focus navigation of other widgets */
        coverflow->group->focused = 0;
        sgl_key_group_load(coverflow->group);
        coverflow->group->editing = 1;
        /* key dispatch only needs the group focus index, not the obj focus
         * flag that makes the core draw a wireframe around the widget; as a
         * full-screen main menu the coverflow must not show that box */
        sgl_obj_clear_focused(obj);
    }

    return obj;
}

/**
 * @brief scroll to the given card with an ease-out animation
 * @param obj coverflow object
 * @param index target card index, clamped to the card range
 * @return none
 */
void sgl_coverflow_set_index(sgl_obj_t *obj, int16_t index)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    coverflow_scroll_to(coverflow, (int32_t)index * coverflow->spacing);
}

/**
 * @brief get the selected card index
 * @param obj coverflow object
 * @return selected card index
 */
int16_t sgl_coverflow_get_index(sgl_obj_t *obj)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    return coverflow->index;
}

/**
 * @brief get a card rect object
 * @param obj coverflow object
 * @param index card index
 * @return card object, NULL if index is out of range
 */
sgl_obj_t* sgl_coverflow_get_card(sgl_obj_t *obj, int16_t index)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    if (index < 0 || index >= coverflow->card_num) {
        return NULL;
    }
    return coverflow->cards[index];
}

/**
 * @brief set the resting distance between adjacent card centers
 * @param obj coverflow object
 * @param spacing distance in pixels
 * @return none
 */
void sgl_coverflow_set_spacing(sgl_obj_t *obj, int16_t spacing)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    if (spacing <= 0 || coverflow->spacing == spacing) return;
    coverflow->spacing = spacing;
    coverflow->scroll = coverflow->index * spacing;
    coverflow_layout(coverflow);
}

/**
 * @brief set the focused card size
 * @param obj coverflow object
 * @param width focused card width, px
 * @param height focused card height, px
 * @return none
 */
void sgl_coverflow_set_card_size(sgl_obj_t *obj, int16_t width, int16_t height)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    coverflow->card_w = width;
    coverflow->card_h = height;
    coverflow_layout(coverflow);
}

/**
 * @brief set the side card scale
 * @param obj coverflow object
 * @param scale permille of the focused card size
 * @return none
 */
void sgl_coverflow_set_min_scale(sgl_obj_t *obj, int16_t scale)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    coverflow->min_scale = sgl_clamp(scale, 0, 1000);
    coverflow_layout(coverflow);
}

/**
 * @brief set the snap animation duration
 * @param obj coverflow object
 * @param ms duration in milliseconds
 * @return none
 */
void sgl_coverflow_set_anim_time(sgl_obj_t *obj, uint32_t ms)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    coverflow->anim_ms = ms;
}

/**
 * @brief set the focused card corner radius
 * @param obj coverflow object
 * @param radius corner radius, px
 * @return none
 */
void sgl_coverflow_set_radius(sgl_obj_t *obj, int16_t radius)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    coverflow->radius = radius;
    coverflow_layout(coverflow);
}

/**
 * @brief set the card border
 * @param obj coverflow object
 * @param color border color
 * @param width border width, px
 * @return none
 */
void sgl_coverflow_set_border(sgl_obj_t *obj, sgl_color_t color, uint8_t width)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    coverflow->border_color = color;
    obj->border = width;
    for (int16_t i = 0; i < coverflow->card_num; i++) {
        sgl_rect_set_border_color(coverflow->cards[i], color);
        sgl_rect_set_border_width(coverflow->cards[i], width);
    }
}

/**
 * @brief set the caption text color
 * @param obj coverflow object
 * @param color text color
 * @return none
 */
void sgl_coverflow_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    coverflow->text_color = color;
    for (int16_t i = 0; i < coverflow->card_num; i++) {
        if (coverflow->labels[i] != NULL) {
            sgl_label_set_text_color(coverflow->labels[i], color);
        }
    }
}

/**
 * @brief set the caption font
 * @param obj coverflow object
 * @param font font to use
 * @return none
 */
void sgl_coverflow_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    coverflow->font = font;
    for (int16_t i = 0; i < coverflow->card_num; i++) {
        if (coverflow->labels[i] != NULL) {
            sgl_label_set_font(coverflow->labels[i], font);
        }
    }
    /* the caption box is sized from the font metrics, so re-layout */
    coverflow_layout(coverflow);
}
