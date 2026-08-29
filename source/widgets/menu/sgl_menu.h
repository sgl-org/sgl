/* source/widgets/menu/sgl_menu.h
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

#ifndef __SGL_MENU_H__
#define __SGL_MENU_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <sgl_misc.h>
#include <sgl_anim.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* forward declaration of the menu descriptor */
typedef struct sgl_menu_def sgl_menu_def_t;

/**
 * @brief menu item action callback
 * @param menu menu widget object
 * @param index zero based index of the activated item in the current page
 */
typedef void (*sgl_menu_action_t)(sgl_obj_t *menu, int16_t index);

/**
 * @brief menu item type
 */
typedef enum sgl_menu_item_type {
    SGL_MENU_TYPE_ACTION  = 0,   /* leaf item, invokes the action callback */
    SGL_MENU_TYPE_SUBMENU = 1,   /* cascade item, pushes a child menu page */
} sgl_menu_item_type_t;

/**
 * @brief one menu entry of a menu page
 * @text:   display text of the item
 * @type:   SGL_MENU_TYPE_ACTION or SGL_MENU_TYPE_SUBMENU
 * @action: callback for action items, NULL for submenus
 * @submenu: descriptor of the child page for submenu items, NULL for actions
 */
typedef struct sgl_menu_item {
    const char             *text;
    uint8_t                 type;
    sgl_menu_action_t       action;
    const sgl_menu_def_t   *submenu;
} sgl_menu_item_t;

/**
 * @brief declarative descriptor of one menu page
 * @title:    text shown in the title bar
 * @items:    constant item table
 * @item_num: number of items in the table
 */
struct sgl_menu_def {
    const char             *title;
    const sgl_menu_item_t  *items;
    uint16_t                item_num;
};

/**
 * @brief declare an action item
 */
#define  SGL_MENU_ITEM(_text, _action) \
        { (_text), SGL_MENU_TYPE_ACTION, (_action), NULL }

/**
 * @brief declare a submenu (cascade) item
 */
#define  SGL_MENU_SUBMENU(_text, _submenu) \
        { (_text), SGL_MENU_TYPE_SUBMENU, NULL, (_submenu) }

/**
 * @brief declare a menu page descriptor from a constant item table
 * @param _name   name of the generated sgl_menu_def_t variable
 * @param _title  title bar text of the page
 * @param _items  constant array of sgl_menu_item_t
 */
#define  SGL_MENU_DEF(_name, _title, _items) \
        const sgl_menu_def_t _name = { (_title), (_items), \
        (uint16_t)(sizeof(_items) / sizeof((_items)[0])) }

/* maximum submenu nesting depth */
#define  SGL_MENU_STACK_MAX              (8)

/* milliseconds of the push / pop slide animation */
#define  SGL_MENU_ANIM_MS                (220)

/* internal animation direction states */
#define  SGL_MENU_ANIM_NONE              (0)
#define  SGL_MENU_ANIM_ENTER             (1)   /* page slides in from the right */
#define  SGL_MENU_ANIM_EXIT              (2)   /* page slides out to the right  */
#define  SGL_MENU_ANIM_CLOSE             (3)   /* root page slides out, then close */
#define  SGL_MENU_ANIM_PENDING           (4)   /* waiting for the first draw   */

/**
 * @brief one stack frame of the menu navigation
 * @def:      menu page descriptor shown by this frame
 * @selected: remembered selection index of the page
 * @offset:   remembered scroll offset of the page
 */
typedef struct sgl_menu_frame {
    const sgl_menu_def_t *def;
    int16_t               selected;
    int32_t               offset;
} sgl_menu_frame_t;

/**
 * @brief sgl menu struct
 * @obj: sgl general object
 */
typedef struct sgl_menu {
    sgl_obj_t        obj;
    sgl_menu_frame_t stack[SGL_MENU_STACK_MAX];
    uint8_t          depth;               /* number of frames on the stack   */
    const sgl_font_t *font;
    sgl_color_t      bg_color;           /* list background               */
    sgl_color_t      text_color;         /* item text                   */
    sgl_color_t      title_bg_color;     /* title / softkey bar background */
    sgl_color_t      title_text_color;   /* title / softkey bar text    */
    sgl_color_t      sel_color;          /* highlighted selection bar     */
    sgl_color_t      sel_text_color;     /* text on the selection bar   */
    uint8_t          alpha;
    sgl_scroll_t     sc;                 /* list scroll physics         */
    /* transition animation state */
    sgl_anim_t       anim;
    int16_t          slide_ofs;          /* horizontal offset of the top page */
    uint8_t          anim_dir;           /* SGL_MENU_ANIM_xxx           */
    void           (*close_cb)(sgl_obj_t *menu);
} sgl_menu_t;

/**
 * @brief create a menu object and push the root page with an enter animation
 * @param parent parent of the menu, NULL creates it on the active screen
 * @param root descriptor of the root menu page
 * @return menu object, NULL on failure
 */
sgl_obj_t* sgl_menu_create(sgl_obj_t *parent, const sgl_menu_def_t *root);

/**
 * @brief push a submenu page onto the stack with a slide-in animation
 * @param obj menu object
 * @param def descriptor of the page to push
 * @return none
 * @note ignored while a transition animation is running or the stack is full
 */
void sgl_menu_push(sgl_obj_t *obj, const sgl_menu_def_t *def);

/**
 * @brief pop the top page with a slide-out animation
 * @param obj menu object
 * @return none
 * @note when only the root page remains this behaves like sgl_menu_close
 */
void sgl_menu_pop(sgl_obj_t *obj);

/**
 * @brief close the whole menu with a slide-out animation
 * @param obj menu object
 * @return none
 * @note when the animation finishes the close callback is invoked, or the
 *       menu object is deleted when no callback is registered
 */
void sgl_menu_close(sgl_obj_t *obj);

/**
 * @brief get current stack depth
 * @param obj menu object
 * @return number of pages on the stack (1 = root page)
 */
uint8_t sgl_menu_get_depth(sgl_obj_t *obj);

/**
 * @brief get the selected index of the top page
 * @param obj menu object
 * @return zero based selected index, -1 when the menu is empty
 */
int16_t sgl_menu_get_selected(sgl_obj_t *obj);

/**
 * @brief set the close callback invoked after the close animation
 * @param obj menu object
 * @param cb callback, the callback owns destroying the menu object
 * @return none
 */
void sgl_menu_set_close_cb(sgl_obj_t *obj, void (*cb)(sgl_obj_t *menu));

/**
 * @brief set the text font of the menu
 * @param obj menu object
 * @param font font of title bar, items and softkey bar
 * @return none
 */
void sgl_menu_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set the list background color of the menu
 * @param obj menu object
 * @param color background color
 * @return none
 */
void sgl_menu_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the item text color of the menu
 * @param obj menu object
 * @param color item text color
 * @return none
 */
void sgl_menu_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the title bar and softkey bar color of the menu
 * @param obj menu object
 * @param color bar background color
 * @param text_color bar text color
 * @return none
 */
void sgl_menu_set_title_color(sgl_obj_t *obj, sgl_color_t color, sgl_color_t text_color);

/**
 * @brief set the selection bar color of the menu
 * @param obj menu object
 * @param color selection bar background color
 * @param text_color text color on the selection bar
 * @return none
 */
void sgl_menu_set_selected_color(sgl_obj_t *obj, sgl_color_t color, sgl_color_t text_color);

/**
 * @brief set the alpha of the menu
 * @param obj menu object
 * @param alpha alpha value
 * @return none
 */
void sgl_menu_set_alpha(sgl_obj_t *obj, uint8_t alpha);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_MENU_H__
