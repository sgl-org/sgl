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
 * @color: card body color
 * @pixmap: card background image, NULL for a plain color card
 * @text: card caption text, NULL for no caption
 */
typedef struct sgl_coverflow_card {
    sgl_color_t        color;
    const sgl_pixmap_t *pixmap;
    const char         *text;
} sgl_coverflow_card_t;

/**
 * @brief sgl coverflow struct
 * @desc: stacked horizontal sliding card menu; the centered card stays on
 *        the top layer and is drawn largest, the cards beside it shrink
 *        progressively. Drag left / right and the cards follow the finger
 *        with a live scale effect; on release the menu snaps to the nearest
 *        card with an ease-out animation. Tapping a side card slides it to
 *        the center; tapping the centered card fires the card's own event
 *        callback (set with sgl_obj_set_event_cb on the card object). The
 *        widget itself lives in a key group: press ENTER once to enter
 *        scroll mode, then LEFT / RIGHT scroll one card per key press and
 *        ESC leaves scroll mode again.
 */
typedef struct sgl_coverflow {
    sgl_obj_t        obj;
    sgl_obj_t        **cards;          /**< card rect objects */
    sgl_obj_t        **labels;         /**< card caption labels (NULL slots allowed) */
    sgl_anim_t       *anim;            /**< snap animation, NULL when idle */
    sgl_key_group_t  *group;           /**< key group for arrow key scrolling */
    sgl_scroll_t     sc;               /**< shared scroll physics: inertia + rubber band */
    const sgl_font_t *font;            /**< caption font */
    sgl_color_t      text_color;       /**< caption text color */
    sgl_color_t      border_color;     /**< card border color */
    int32_t          scroll;           /**< camera position: card i rests at i * spacing */
    uint32_t         anim_ms;          /**< snap animation duration, ms */
    int16_t          index;            /**< selected (or animating towards) card */
    int16_t          top;              /**< card currently on the top layer */
    int16_t          press_card;       /**< card the press started on, -1 when idle */
    int16_t          card_num;         /**< number of cards */
    int16_t          spacing;          /**< resting distance between adjacent card centers, px */
    int16_t          card_w;           /**< focused card width, px */
    int16_t          card_h;           /**< focused card height, px */
    int16_t          min_scale;        /**< side card scale, permille of the focused card */
    int16_t          radius;           /**< focused card corner radius, px */
} sgl_coverflow_t;

/**
 * @brief create a coverflow object
 * @param parent parent object, NULL creates the menu on the active screen
 * @param cards card descriptor array (copied into the created cards)
 * @param card_num number of cards
 * @return coverflow object, NULL on failure
 * @note the widget is created with the parent size; the focused card rests
 *       at the widget center. The widget itself is the touch / key event
 *       target and joins a key group so arrow key scrolling works out of
 *       the box; the cards stay unclickable and their event callbacks are
 *       left to the user
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
 * @brief get a card rect object
 * @param obj coverflow object
 * @param index card index
 * @return card object, NULL if index is out of range
 */
sgl_obj_t* sgl_coverflow_get_card(sgl_obj_t *obj, int16_t index);

/**
 * @brief set the resting distance between adjacent card centers
 * @param obj coverflow object
 * @param spacing distance in pixels
 * @return none
 */
void sgl_coverflow_set_spacing(sgl_obj_t *obj, int16_t spacing);

/**
 * @brief set the focused card size
 * @param obj coverflow object
 * @param width focused card width, px
 * @param height focused card height, px
 * @return none
 */
void sgl_coverflow_set_card_size(sgl_obj_t *obj, int16_t width, int16_t height);

/**
 * @brief set the side card scale
 * @param obj coverflow object
 * @param scale permille of the focused card size (e.g. 560 means 56%)
 * @return none
 */
void sgl_coverflow_set_min_scale(sgl_obj_t *obj, int16_t scale);

/**
 * @brief set the snap animation duration
 * @param obj coverflow object
 * @param ms duration in milliseconds
 * @return none
 */
void sgl_coverflow_set_anim_time(sgl_obj_t *obj, uint32_t ms);

/**
 * @brief set the focused card corner radius (side cards scale it down)
 * @param obj coverflow object
 * @param radius corner radius, px
 * @return none
 */
void sgl_coverflow_set_radius(sgl_obj_t *obj, int16_t radius);

/**
 * @brief set the card border
 * @param obj coverflow object
 * @param color border color
 * @param width border width, px
 * @return none
 */
void sgl_coverflow_set_border(sgl_obj_t *obj, sgl_color_t color, uint8_t width);

/**
 * @brief set the caption text color
 * @param obj coverflow object
 * @param color text color
 * @return none
 */
void sgl_coverflow_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the caption font
 * @param obj coverflow object
 * @param font font to use
 * @return none
 */
void sgl_coverflow_set_font(sgl_obj_t *obj, const sgl_font_t *font);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_COVERFLOW_H__
