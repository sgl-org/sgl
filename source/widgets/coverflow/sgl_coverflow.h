/* source/widgets/coverflow/sgl_coverflow.h
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

#ifndef __SGL_COVERFLOW_H__
#define __SGL_COVERFLOW_H__

#include <sgl_core.h>
#include <sgl_anim.h>
#include <sgl_mm.h>
#include <sgl_log.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief coverflow card descriptor
 * @pixmap: card image, mandatory: its width and height define the card size.
 *          ARGB8565 (RGB565 + 8-bit alpha) images are scaled with
 *          nearest-neighbour sampling and per-pixel alpha blending, other
 *          formats are drawn by sgl_draw_rect
 * @text: card text, not drawn by the widget; it is kept for application use
 *        (e.g. the example shows it in a msgbox)
 */
typedef struct sgl_coverflow_card {
    const sgl_pixmap_t *pixmap;
    const char         *text;
} sgl_coverflow_card_t;

/**
 * @brief coverflow card geometry
 * @coords: card rect, relative to the coverflow widget origin
 * @radius: card corner radius, px
 */
typedef struct sgl_coverflow_item {
    sgl_area_t       coords;
    int16_t          radius;
} sgl_coverflow_item_t;

/**
 * @brief coverflow card activation callback
 * @param evt event forwarded from the widget, evt->obj is the coverflow object
 * @param index index of the activated card
 */
typedef void (*sgl_coverflow_card_event_cb_t)(sgl_event_t *evt, int16_t index);

/**
 * @brief sgl coverflow struct
 * @desc: stacked horizontal sliding card menu; the centered card stays on
 *        the top layer and is drawn largest, the cards beside it shrink
 *        progressively. Drag left / right and the cards follow the finger
 *        with a live scale effect; on release the menu snaps to the nearest
 *        card with an ease-out animation. Tapping a side card slides it to
 *        the center; tapping the centered card fires the card activation
 *        callback (set with sgl_coverflow_set_card_event_cb). The cards are
 *        drawn by the widget itself, no child objects are created. The
 *        widget is designed as a main menu: it is the only
 *        member of its key group and is focused with the group switched into
 *        the editing state at creation, so LEFT / RIGHT / UP / DOWN scroll
 *        one card per key press immediately (no focus / arming ENTER needed)
 *        and ENTER fires the centered card's activation callback.
 */
typedef struct sgl_coverflow {
    sgl_obj_t        obj;
    sgl_coverflow_card_t *cards;       /**< copied card descriptors */
    sgl_coverflow_item_t *items;       /**< per-card geometry */
    sgl_coverflow_card_event_cb_t card_event_cb; /**< card activation callback */
    void             *card_event_data; /**< user data for card_event_cb */
    sgl_key_group_t  *group;           /**< key group for arrow key scrolling */
    sgl_scroll_t     sc;               /**< shared scroll physics: inertia + rubber band */
    int32_t          scroll;           /**< camera position: card i rests at i * spacing */
    uint32_t         anim_ms;          /**< snap animation duration, ms */
    int16_t          index;            /**< selected (or animating towards) card */
    int16_t          top;              /**< card currently on the top layer */
    int16_t          press_card;       /**< card the press started on, -1 when idle */
    int16_t          card_num;         /**< number of cards */
    int16_t          spacing;          /**< resting distance between adjacent card centers, px (derived) */
    int16_t          radius;           /**< focused card corner radius, px */
} sgl_coverflow_t;

/**
 * @brief create a coverflow object
 * @param parent parent object, NULL creates the menu on the active screen
 * @param cards card descriptor array (copied into the created cards)
 * @param card_num number of cards
 * @return coverflow object, NULL on failure (e.g. a card without a pixmap)
 * @note the widget is created with the parent size; the focused card rests
 *       at the widget center. The pixmap of every card is mandatory; the
 *       widget itself is the touch / key event target and joins a key group
 *       so arrow key scrolling works out of the box; the cards are drawn
 *       directly by the widget
 */
sgl_obj_t* sgl_coverflow_create(sgl_obj_t *parent, const sgl_coverflow_card_t *cards, int16_t card_num);

/**
 * @brief scroll to the given card with an ease-out animation
 * @param obj coverflow object
 * @param index target card index, clamped to the card range
 * @return none
 */
void sgl_coverflow_set_index(sgl_obj_t *obj, int16_t index);

/**
 * @brief get the selected card index
 * @param obj coverflow object
 * @return selected card index
 */
int16_t sgl_coverflow_get_index(sgl_obj_t *obj);

/**
 * @brief set the card activation callback
 * @param obj coverflow object
 * @param cb callback fired when the centered card is activated (tapped or
 *           ENTER); it also receives the press / release events of the
 *           touched card
 * @param data user data passed as evt->event_data
 * @return none
 */
void sgl_coverflow_set_card_event_cb(sgl_obj_t *obj, sgl_coverflow_card_event_cb_t cb, void *data);

/**
 * @brief set the snap animation duration
 * @param obj coverflow object
 * @param ms duration in milliseconds
 * @return none
 */
void sgl_coverflow_set_anim_time(sgl_obj_t *obj, uint32_t ms);

/**
 * @brief set the focused card corner radius (other cards scale it with their size)
 * @param obj coverflow object
 * @param radius corner radius, px
 * @return none
 */
void sgl_coverflow_set_radius(sgl_obj_t *obj, int16_t radius);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_COVERFLOW_H__
