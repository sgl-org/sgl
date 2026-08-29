/* source/widgets/menu/sgl_menu.c
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

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_menu.h"

/* Symbian S60 style palette */
#define  SGL_MENU_TITLE_COLOR      sgl_rgb(21, 94, 160)   /* title / softkey bar blue */
#define  SGL_MENU_SEL_COLOR        sgl_rgb(39, 119, 196)  /* selection highlight blue */
#define  SGL_MENU_BAR_TEXT_COLOR   sgl_rgb(255, 255, 255)

/* layout paddings (px) */
#define  SGL_MENU_TITLE_PAD        (3)   /* title bar vertical padding   */
#define  SGL_MENU_SOFT_PAD         (2)   /* softkey bar vertical padding */
#define  SGL_MENU_ITEM_PAD         (4)   /* item row vertical padding    */
#define  SGL_MENU_TEXT_PAD         (6)   /* horizontal text margin       */

static void sgl_menu_slide(sgl_menu_t *menu, uint8_t dir);
static void sgl_menu_activate(sgl_menu_t *menu);

/**
 * @brief compute the menu layout metrics from the current font
 * @param menu menu widget
 * @param title_h output title bar height
 * @param soft_h output softkey bar height
 * @param item_h output item row height
 * @return none
 */
static void sgl_menu_metrics(const sgl_menu_t *menu, int *title_h, int *soft_h, int *item_h)
{
    const int font_h = sgl_font_get_height(menu->font);
    *title_h = font_h + 2 * SGL_MENU_TITLE_PAD;
    *soft_h  = font_h + 2 * SGL_MENU_SOFT_PAD;
    *item_h  = font_h + 2 * SGL_MENU_ITEM_PAD;
}

/**
 * @brief maximum scroll offset of the top page list
 * @param menu menu widget
 * @return scroll upper limit in pixels, 0 when all items fit
 */
static int32_t sgl_menu_max_scroll(const sgl_menu_t *menu)
{
    const sgl_menu_frame_t *frame = &menu->stack[menu->depth - 1];
    int title_h, soft_h, item_h;
    int list_h, content_h;

    sgl_menu_metrics(menu, &title_h, &soft_h, &item_h);
    list_h = (menu->obj.coords.y2 - menu->obj.coords.y1 + 1) - title_h - soft_h;
    content_h = (int)frame->def->item_num * item_h;
    return sgl_max(0, content_h - list_h);
}

/**
 * @brief scroll the selected item of the top page into the viewport
 * @param menu menu widget
 * @return none
 */
static void sgl_menu_ensure_visible(sgl_menu_t *menu)
{
    const sgl_menu_frame_t *frame = &menu->stack[menu->depth - 1];
    int title_h, soft_h, item_h;
    int list_h, selected_y, view_top;
    const int32_t max_scroll = sgl_menu_max_scroll(menu);

    sgl_menu_metrics(menu, &title_h, &soft_h, &item_h);
    list_h = (menu->obj.coords.y2 - menu->obj.coords.y1 + 1) - title_h - soft_h;
    selected_y = frame->selected * item_h;
    view_top = (int)menu->sc.offset;

    if (selected_y < view_top) {
        menu->sc.offset = selected_y;
    }
    else if (selected_y + item_h > view_top + list_h) {
        menu->sc.offset = selected_y + item_h - list_h;
    }

    if (menu->sc.offset < 0)
        menu->sc.offset = 0;
    if (menu->sc.offset > max_scroll)
        menu->sc.offset = max_scroll;
}

/**
 * @brief compute the screen area of one item row of the top page
 * @param menu menu widget
 * @param index item index
 * @param row output area
 * @return none
 */
static void sgl_menu_row_area(const sgl_menu_t *menu, int16_t index, sgl_area_t *row)
{
    const sgl_obj_t *obj = &menu->obj;
    int title_h, soft_h, item_h;

    sgl_menu_metrics(menu, &title_h, &soft_h, &item_h);

    row->x1 = obj->coords.x1 + obj->border;
    row->x2 = obj->coords.x2 - obj->border;
    row->y1 = obj->coords.y1 + title_h + index * item_h - (int16_t)menu->sc.offset;
    row->y2 = row->y1 + item_h;
}

/**
 * @brief change the selection of the top page with a minimal redraw
 * @param menu menu widget
 * @param index new selection index
 * @return none
 */
static void sgl_menu_change_item(sgl_menu_t *menu, int16_t index)
{
    sgl_obj_t *obj = &menu->obj;
    sgl_menu_frame_t *frame = &menu->stack[menu->depth - 1];
    sgl_area_t row;
    const int32_t offset_last = menu->sc.offset;
    int title_h, soft_h, item_h;
    sgl_area_t title_box;

    if (menu->depth == 0)
        return;
    if (index < 0 || index >= (int16_t)frame->def->item_num || index == frame->selected)
        return;

    /* invalidate the old selection row */
    sgl_menu_row_area(menu, frame->selected, &row);
    sgl_obj_update_area(&row);

    frame->selected = index;

    /* stop coasting and scroll the new selection into the viewport */
    sgl_scroll_anim_stop(&menu->sc);
    sgl_menu_ensure_visible(menu);

    if (menu->sc.offset != offset_last) {
        /* the whole list shifted, full redraw */
        sgl_scroll_bar_wake(&menu->sc);
        sgl_obj_set_dirty(obj);
        return;
    }

    sgl_menu_row_area(menu, index, &row);
    sgl_obj_update_area(&row);

    sgl_menu_metrics(menu, &title_h, &soft_h, &item_h);
    title_box.x1 = obj->coords.x1 + obj->border;
    title_box.y1 = obj->coords.y1 + obj->border;
    title_box.x2 = obj->coords.x2 - obj->border;
    title_box.y2 = obj->coords.y1 + title_h - 1;
    sgl_obj_update_area(&title_box);
}

/**
 * @brief scroll change commit callback of the shared scroll state
 * @param sc scroll state embedded in the menu widget
 * @return none
 */
static void sgl_menu_scroll_commit(sgl_scroll_t *sc)
{
    sgl_menu_t *menu = sgl_container_of(sc, sgl_menu_t, sc);
    sgl_scroll_bar_wake(sc);
    sgl_obj_set_dirty(&menu->obj);
}

/**
 * @brief draw one menu page (title bar, item list, softkey bar)
 * @param menu menu widget
 * @param surf drawing surface
 * @param frame stack frame describing the page
 * @param offset list scroll offset of the page
 * @param x_ofs horizontal slide offset used by the transition animation
 * @param is_top non zero for the topmost page (draws scrollbar, real softkeys)
 * @return none
 * @note items are drawn before the two bars so that the bars always
 *       cover list pixels overflowing at the viewport edges
 */
static void sgl_menu_draw_page(sgl_menu_t *menu, sgl_surf_t *surf,
                               const sgl_menu_frame_t *frame, int32_t offset,
                               int16_t x_ofs, uint8_t is_top)
{
    sgl_obj_t *obj = &menu->obj;
    const sgl_menu_def_t *def = frame->def;
    int title_h, soft_h, item_h;
    const int16_t x1 = obj->coords.x1 + x_ofs;
    const int16_t x2 = obj->coords.x2 + x_ofs;
    const int16_t y1 = obj->coords.y1;
    const int16_t y2 = obj->coords.y2;
    int16_t item_y;
    uint16_t i;
    sgl_area_t box;
    char buf[16];

    sgl_menu_metrics(menu, &title_h, &soft_h, &item_h);

    /* list background of the whole page */
    box.x1 = x1;
    box.x2 = x2;
    box.y1 = y1;
    box.y2 = y2;
    sgl_draw_fill_rect(surf, &obj->area, &box, 0, menu->bg_color, menu->alpha);

    /* item list */
    item_y = y1 + title_h - (int16_t)offset;
    for (i = 0; i < def->item_num; i++, item_y += item_h) {
        const sgl_menu_item_t *item = &def->items[i];
        const uint8_t selected = ((int16_t)i == frame->selected);

        if (item_y >= y2 - soft_h + 1) {
            break;                              /* below the list viewport */
        }
        if (item_y + item_h <= y1 + title_h) {
            continue;                           /* above the list viewport */
        }

        if (selected) {
            box.x1 = x1;
            box.x2 = x2;
            box.y1 = item_y;
            box.y2 = item_y + item_h - 1;
            sgl_draw_fill_rect(surf, &obj->area, &box, 0, menu->sel_color, menu->alpha);
        }

        sgl_draw_string(surf, &obj->area, x1 + SGL_MENU_TEXT_PAD, item_y + SGL_MENU_ITEM_PAD,
                        item->text, selected ? menu->sel_text_color : menu->text_color,
                        menu->alpha, menu->font);

        /* cascade arrow of submenu items: right facing chevron */
        if (item->type == SGL_MENU_TYPE_SUBMENU) {
            const int16_t aw = (int16_t)(item_h / 3);
            const int16_t ax = x2 - SGL_MENU_TEXT_PAD - aw;
            const int16_t ay = item_y + (int16_t)(item_h - aw) / 2;
            const sgl_color_t color = selected ? menu->sel_text_color : menu->text_color;

            sgl_draw_line_noaa(surf, &obj->area, ax, ay, ax + aw, ay + aw / 2, color, 1, menu->alpha);
            sgl_draw_line_noaa(surf, &obj->area, ax + aw, ay + aw / 2, ax, ay + aw, color, 1, menu->alpha);
        }
    }

    /* scrollbar only on the settled top page */
    if (is_top) {
        const int32_t max_scroll = sgl_menu_max_scroll(menu);
        sgl_area_t viewport = {
            .x1 = x1, .x2 = x2,
            .y1 = y1 + title_h, .y2 = y2 - soft_h
        };
        sgl_scroll_draw_bar(surf, obj, &menu->sc, max_scroll, &viewport, menu->title_bg_color);
    }

    /* title bar with the page title and the Symbian "n/m" indicator */
    box.x1 = x1;
    box.x2 = x2;
    box.y1 = y1;
    box.y2 = y1 + title_h - 1;
    sgl_draw_fill_rect(surf, &obj->area, &box, 0, menu->title_bg_color, menu->alpha);

    if (def->title) {
        sgl_draw_string(surf, &obj->area, x1 + SGL_MENU_TEXT_PAD, y1 + SGL_MENU_TITLE_PAD,
                        def->title, menu->title_text_color, menu->alpha, menu->font);
    }

    if (def->item_num > 0) {
        sgl_snprintf(buf, sizeof(buf), "%d/%d", (int)frame->selected + 1, (int)def->item_num);
        sgl_draw_string(surf, &obj->area,
                        x2 - SGL_MENU_TEXT_PAD - (int16_t)sgl_font_get_string_width(buf, menu->font),
                        y1 + SGL_MENU_TITLE_PAD, buf, menu->title_text_color, menu->alpha, menu->font);
    }

    /* softkey bar: left [Select] / right [Back|Exit] */
    box.y1 = y2 - soft_h + 1;
    box.y2 = y2;
    sgl_draw_fill_rect(surf, &obj->area, &box, 0, menu->title_bg_color, menu->alpha);

    sgl_draw_string(surf, &obj->area, x1 + SGL_MENU_TEXT_PAD, y2 - soft_h + SGL_MENU_SOFT_PAD,
                    "Select", menu->title_text_color, menu->alpha, menu->font);

    {
        const char *right = is_top ? ((menu->depth > 1) ? "Back" : "Exit") : "Back";
        sgl_draw_string(surf, &obj->area,
                        x2 - SGL_MENU_TEXT_PAD - (int16_t)sgl_font_get_string_width(right, menu->font),
                        y2 - soft_h + SGL_MENU_SOFT_PAD, right, menu->title_text_color, menu->alpha, menu->font);
    }
}

/**
 * @brief transition animation path callback, moves the top page horizontally
 * @param anim animation object
 * @param value current slide offset in pixels
 * @return none
 */
static void sgl_menu_anim_path_cb(sgl_anim_t *anim, int32_t value)
{
    sgl_menu_t *menu = (sgl_menu_t *)anim->data;
    menu->slide_ofs = (int16_t)value;
    sgl_obj_set_dirty(&menu->obj);
}

/**
 * @brief transition animation finish callback, commits the stack change
 * @param anim animation object
 * @return none
 */
static void sgl_menu_anim_finish_cb(sgl_anim_t *anim)
{
    sgl_menu_t *menu = (sgl_menu_t *)anim->data;
    sgl_obj_t *obj = &menu->obj;

    switch (menu->anim_dir) {
    case SGL_MENU_ANIM_ENTER:
        menu->slide_ofs = 0;
        menu->anim_dir = SGL_MENU_ANIM_NONE;
        sgl_obj_set_dirty(obj);
        break;

    case SGL_MENU_ANIM_EXIT: {
        /* the page slid out, drop it and restore the parent page state */
        sgl_menu_frame_t *frame;
        menu->depth--;
        frame = &menu->stack[menu->depth - 1];
        sgl_scroll_reset(&menu->sc);
        menu->sc.offset = frame->offset;
        menu->slide_ofs = 0;
        menu->anim_dir = SGL_MENU_ANIM_NONE;
        sgl_obj_set_dirty(obj);
    }
    break;

    case SGL_MENU_ANIM_CLOSE:
        menu->slide_ofs = 0;
        menu->anim_dir = SGL_MENU_ANIM_NONE;
        if (menu->close_cb) {
            menu->close_cb(obj);
        }
        else {
            sgl_obj_delete(obj);
        }
        break;

    default:
        break;
    }
}

/**
 * @brief start the horizontal slide transition of the top page
 * @param menu menu widget
 * @param dir SGL_MENU_ANIM_ENTER / SGL_MENU_ANIM_EXIT / SGL_MENU_ANIM_CLOSE
 * @return none
 */
static void sgl_menu_slide(sgl_menu_t *menu, uint8_t dir)
{
    const int16_t width = sgl_obj_get_width(&menu->obj);

    menu->anim_dir = dir;
    /* apply the first frame right away: sgl_anim_task skips the path
     * callback while value == last_value, so without this the page would
     * flash one frame in its settled position before the slide starts */
    menu->slide_ofs = (dir == SGL_MENU_ANIM_ENTER) ? width : 0;
    sgl_anim_stop(&menu->anim);
    sgl_anim_set_start_value(&menu->anim, (dir == SGL_MENU_ANIM_ENTER) ? width : 0);
    sgl_anim_set_end_value(&menu->anim, (dir == SGL_MENU_ANIM_ENTER) ? 0 : width);
    sgl_anim_start(&menu->anim, SGL_ANIM_REPEAT_ONCE);
}

/**
 * @brief activate the selected item of the top page
 * @param menu menu widget
 * @return none
 * @note submenu items push their child page, action items invoke the
 *       user callback from the declarative item table
 */
static void sgl_menu_activate(sgl_menu_t *menu)
{
    sgl_menu_frame_t *frame;
    const sgl_menu_item_t *item;

    if (menu->depth == 0 || menu->anim_dir != SGL_MENU_ANIM_NONE)
        return;

    frame = &menu->stack[menu->depth - 1];
    if (frame->def->item_num == 0)
        return;
    if (frame->selected < 0 || frame->selected >= (int16_t)frame->def->item_num)
        return;

    item = &frame->def->items[frame->selected];
    if (item->type == SGL_MENU_TYPE_SUBMENU) {
        if (item->submenu) {
            sgl_menu_push(&menu->obj, item->submenu);
        }
    }
    else if (item->action) {
        item->action(&menu->obj, frame->selected);
    }
}

static void sgl_menu_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_menu_t *menu = sgl_container_of(obj, sgl_menu_t, obj);

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN: {
        if (menu->depth == 0) {
            break;
        }

        /* the size is only known after the user laid out the object,
         * kick off the enter animation on the first real draw */
        if (menu->anim_dir == SGL_MENU_ANIM_PENDING) {
            sgl_menu_slide(menu, SGL_MENU_ANIM_ENTER);
        }

        /* during a transition the parent page stays static underneath */
        if (menu->slide_ofs != 0 && menu->depth >= 2) {
            const sgl_menu_frame_t *parent = &menu->stack[menu->depth - 2];
            sgl_menu_draw_page(menu, surf, parent, parent->offset, 0, 0);
        }

        sgl_menu_draw_page(menu, surf, &menu->stack[menu->depth - 1], menu->sc.offset,
                           menu->slide_ofs, (uint8_t)(menu->slide_ofs == 0));
    }
    break;

    case SGL_EVENT_PRESSED:
        if (menu->depth == 0 || menu->anim_dir != SGL_MENU_ANIM_NONE)
            break;
        sgl_scroll_press(&menu->sc, evt->pos.y);
        break;

    case SGL_EVENT_MOVE_UP:
    case SGL_EVENT_MOVE_DOWN: {
        uint8_t r;
        if (menu->depth == 0 || menu->anim_dir != SGL_MENU_ANIM_NONE)
            break;
        r = sgl_scroll_stay(&menu->sc, evt->pos.y, sgl_menu_max_scroll(menu));
        if (r) {
            sgl_obj_set_dirty(obj);
        }
    }
    break;

    case SGL_EVENT_RELEASED: {
        int32_t max_scroll;
        if (menu->depth == 0 || menu->anim_dir != SGL_MENU_ANIM_NONE)
            break;
        max_scroll = sgl_menu_max_scroll(menu);
        menu->sc.range = max_scroll;
        menu->sc.commit = sgl_menu_scroll_commit;
        if (sgl_scroll_release(&menu->sc, max_scroll)) {
            sgl_scroll_anim_start(&menu->sc);
        }
        sgl_obj_set_dirty(obj);
    }
    break;

    case SGL_EVENT_CLICKED: {
        sgl_menu_frame_t *frame = &menu->stack[menu->depth - 1];
        int title_h, soft_h, item_h;
        int16_t local_y;
        int16_t index;

        if (menu->depth == 0 || menu->anim_dir != SGL_MENU_ANIM_NONE)
            break;

        /* a drag ends with no activation */
        if (menu->sc.dragged) {
            menu->sc.dragged = 0;
            break;
        }

        /* key driven click (Enter) carries a zero position */
        if (evt->pos.x == 0 && evt->pos.y == 0) {
            sgl_menu_activate(menu);
            break;
        }

        if (evt->pos.x < obj->coords.x1 || evt->pos.x > obj->coords.x2 ||
            evt->pos.y < obj->coords.y1 || evt->pos.y > obj->coords.y2) {
            break;
        }

        sgl_menu_metrics(menu, &title_h, &soft_h, &item_h);

        if (evt->pos.y >= obj->coords.y2 - soft_h + 1) {
            if (evt->pos.x < (obj->coords.x1 + obj->coords.x2) / 2) {
                sgl_menu_activate(menu);
            }
            else if (menu->depth > 1) {
                sgl_menu_pop(obj);
            }
            break;
        }

        if (evt->pos.y < obj->coords.y1 + title_h) {
            break;                              /* title bar: nothing to do */
        }

        local_y = evt->pos.y - (obj->coords.y1 + title_h);
        index = (int16_t)((local_y + (int16_t)menu->sc.offset) / item_h);
        if (index >= 0 && index < (int16_t)frame->def->item_num) {
            if (index == frame->selected) {
                sgl_menu_activate(menu);        /* second tap activates */
            }
            else {
                sgl_menu_change_item(menu, index);
            }
        }
    }
    break;

    case SGL_EVENT_KEY_DOWN:
        if (menu->depth != 0 && menu->anim_dir == SGL_MENU_ANIM_NONE) {
            sgl_menu_change_item(menu, menu->stack[menu->depth - 1].selected + 1);
        }
        break;

    case SGL_EVENT_KEY_UP:
        if (menu->depth != 0 && menu->anim_dir == SGL_MENU_ANIM_NONE) {
            sgl_menu_change_item(menu, menu->stack[menu->depth - 1].selected - 1);
        }
        break;

    case SGL_EVENT_KEY_RIGHT:
        sgl_menu_activate(menu);
        break;

    case SGL_EVENT_KEY_LEFT:
    case SGL_EVENT_KEY_ESC:
        sgl_menu_pop(obj);
        break;

    case SGL_EVENT_DESTROYED:
        sgl_anim_stop(&menu->anim);
        sgl_scroll_anim_stop(&menu->sc);
        break;

    default:
        break;
    }
}

/**
 * @brief create a menu object and push the root page with an enter animation
 * @param parent parent of the menu, NULL creates it on the active screen
 * @param root descriptor of the root menu page
 * @return menu object, NULL on failure
 */
sgl_obj_t* sgl_menu_create(sgl_obj_t *parent, const sgl_menu_def_t *root)
{
    sgl_menu_t *menu = sgl_malloc(sizeof(sgl_menu_t));
    if (menu == NULL) {
        SGL_LOG_ERROR("sgl_menu_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(menu, 0, sizeof(sgl_menu_t));

    if (root == NULL) {
        sgl_free(menu);
        return NULL;
    }

    sgl_obj_init(&menu->obj, parent);
    menu->obj.construct_fn = sgl_menu_construct_cb;
    sgl_obj_set_clickable(&menu->obj);
    sgl_obj_set_movable(&menu->obj);
    sgl_obj_set_editable(&menu->obj);
    sgl_obj_set_keypress_click(&menu->obj);

    menu->font = sgl_get_system_font();
    menu->alpha = SGL_THEME_ALPHA;
    menu->bg_color = SGL_THEME_COLOR;
    menu->text_color = SGL_THEME_TEXT_COLOR;
    menu->title_bg_color = SGL_MENU_TITLE_COLOR;
    menu->title_text_color = SGL_MENU_BAR_TEXT_COLOR;
    menu->sel_color = SGL_MENU_SEL_COLOR;
    menu->sel_text_color = SGL_MENU_BAR_TEXT_COLOR;

    menu->stack[0].def = root;
    menu->stack[0].selected = 0;
    menu->stack[0].offset = 0;
    menu->depth = 1;
    sgl_scroll_reset(&menu->sc);

    sgl_anim_init(&menu->anim);
    menu->anim.data = menu;
    sgl_anim_set_act_duration(&menu->anim, SGL_MENU_ANIM_MS);
    sgl_anim_set_path(&menu->anim, sgl_menu_anim_path_cb, SGL_ANIM_PATH_EASE_IN_OUT_SINE);
    sgl_anim_set_finish_cb(&menu->anim, sgl_menu_anim_finish_cb);

    /* the enter animation is kicked off by the first DRAW_MAIN, once the
     * caller had the chance to size the object */
    menu->anim_dir = SGL_MENU_ANIM_PENDING;

    return &menu->obj;
}

/**
 * @brief push a submenu page onto the stack with a slide-in animation
 * @param obj menu object
 * @param def descriptor of the page to push
 * @return none
 * @note ignored while a transition animation is running or the stack is full
 */
void sgl_menu_push(sgl_obj_t *obj, const sgl_menu_def_t *def)
{
    sgl_menu_t *menu = (sgl_menu_t *)obj;
    sgl_menu_frame_t *frame;

    if (menu == NULL || def == NULL)
        return;
    if (menu->anim_dir != SGL_MENU_ANIM_NONE || menu->depth == 0)
        return;
    if (menu->depth >= SGL_MENU_STACK_MAX)
        return;

    /* remember the current page's scroll position for the return trip */
    menu->stack[menu->depth - 1].offset = menu->sc.offset;

    frame = &menu->stack[menu->depth];
    frame->def = def;
    frame->selected = 0;
    frame->offset = 0;
    menu->depth++;

    sgl_scroll_reset(&menu->sc);
    sgl_menu_slide(menu, SGL_MENU_ANIM_ENTER);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief pop the top page with a slide-out animation
 * @param obj menu object
 * @return none
 * @note when only the root page remains this behaves like sgl_menu_close
 */
void sgl_menu_pop(sgl_obj_t *obj)
{
    sgl_menu_t *menu = (sgl_menu_t *)obj;

    if (menu == NULL || menu->depth == 0)
        return;
    if (menu->anim_dir != SGL_MENU_ANIM_NONE)
        return;

    if (menu->depth == 1) {
        sgl_menu_close(obj);
        return;
    }

    sgl_menu_slide(menu, SGL_MENU_ANIM_EXIT);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief close the whole menu with a slide-out animation
 * @param obj menu object
 * @return none
 * @note when the animation finishes the close callback is invoked, or the
 *       menu object is deleted when no callback is registered
 */
void sgl_menu_close(sgl_obj_t *obj)
{
    sgl_menu_t *menu = (sgl_menu_t *)obj;

    if (menu == NULL || menu->depth == 0)
        return;
    if (menu->anim_dir != SGL_MENU_ANIM_NONE)
        return;

    sgl_menu_slide(menu, SGL_MENU_ANIM_CLOSE);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get current stack depth
 * @param obj menu object
 * @return number of pages on the stack (1 = root page)
 */
uint8_t sgl_menu_get_depth(sgl_obj_t *obj)
{
    sgl_menu_t *menu = (sgl_menu_t *)obj;
    return menu->depth;
}

/**
 * @brief get the selected index of the top page
 * @param obj menu object
 * @return zero based selected index, -1 when the menu is empty
 */
int16_t sgl_menu_get_selected(sgl_obj_t *obj)
{
    sgl_menu_t *menu = (sgl_menu_t *)obj;

    if (menu->depth == 0)
        return -1;
    return menu->stack[menu->depth - 1].selected;
}

/**
 * @brief set the close callback invoked after the close animation
 * @param obj menu object
 * @param cb callback, the callback owns destroying the menu object
 * @return none
 */
void sgl_menu_set_close_cb(sgl_obj_t *obj, void (*cb)(sgl_obj_t *menu))
{
    sgl_menu_t *menu = (sgl_menu_t *)obj;
    menu->close_cb = cb;
}

/**
 * @brief set the text font of the menu
 * @param obj menu object
 * @param font font of title bar, items and softkey bar
 * @return none
 */
void sgl_menu_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_menu_t *menu = (sgl_menu_t *)obj;
    menu->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the list background color of the menu
 * @param obj menu object
 * @param color background color
 * @return none
 */
void sgl_menu_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_menu_t *menu = (sgl_menu_t *)obj;
    menu->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the item text color of the menu
 * @param obj menu object
 * @param color item text color
 * @return none
 */
void sgl_menu_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_menu_t *menu = (sgl_menu_t *)obj;
    menu->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the title bar and softkey bar color of the menu
 * @param obj menu object
 * @param color bar background color
 * @param text_color bar text color
 * @return none
 */
void sgl_menu_set_title_color(sgl_obj_t *obj, sgl_color_t color, sgl_color_t text_color)
{
    sgl_menu_t *menu = (sgl_menu_t *)obj;
    menu->title_bg_color = color;
    menu->title_text_color = text_color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the selection bar color of the menu
 * @param obj menu object
 * @param color selection bar background color
 * @param text_color text color on the selection bar
 * @return none
 */
void sgl_menu_set_selected_color(sgl_obj_t *obj, sgl_color_t color, sgl_color_t text_color)
{
    sgl_menu_t *menu = (sgl_menu_t *)obj;
    menu->sel_color = color;
    menu->sel_text_color = text_color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the alpha of the menu
 * @param obj menu object
 * @param alpha alpha value
 * @return none
 */
void sgl_menu_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_menu_t *menu = (sgl_menu_t *)obj;
    menu->alpha = alpha;
    sgl_obj_set_dirty(obj);
}
