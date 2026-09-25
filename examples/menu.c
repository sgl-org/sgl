/* examples/menu_theme.c
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
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sgl.h>

/**
 * Menu widget examples: rounded card item style, one per theme
 *  - every item is drawn as a rounded rectangle card
 *  - the selected card changes its border color (accent highlight)
 *  - one white (light) theme menu and one dark theme menu are shown
 *    side by side, sharing the same declarative page tables
 */

/* ------------------------------------------------------------------ */
/* shared declarative menu tree                                        */
/* ------------------------------------------------------------------ */

/* answer of the last msgbox button press (left/right button text) */
static const char *menu_theme_msgbox_answer = NULL;

/**
 * @brief get the text of the currently selected item on the top page
 * @param menu menu object
 * @return item text, NULL when the menu has no valid selection
 * @note reads the public sgl_menu_t layout: stack frame -> page def -> items
 */
static const char* menu_theme_selected_text(sgl_obj_t *menu)
{
    sgl_menu_t *m = sgl_container_of(menu, sgl_menu_t, obj);
    const sgl_menu_frame_t *frame;

    if (m->depth == 0) {
        return NULL;
    }

    frame = &m->stack[m->depth - 1];
    if (frame->selected < 0 || frame->selected >= (int16_t)frame->def->item_num) {
        return NULL;
    }

    return frame->def->items[frame->selected].text;
}

/**
 * @brief show a msgbox dialog with the text of the activated item
 * @param menu menu object
 * @param index index of the activated item
 * @return none
 * @note the msgbox destroys itself when either button is pressed; the
 *       pressed button text is stored in menu_theme_msgbox_answer
 */
static void menu_theme_show_msgbox(sgl_obj_t *menu, int16_t index)
{
    sgl_obj_t *msgbox = sgl_msgbox_create(NULL);
    if (msgbox == NULL) {
        return;
    }

    /* centered dialog, roughly one third of the screen */
    sgl_obj_set_pos(msgbox, 200, 150);
    sgl_obj_set_size(msgbox, 400, 180);

    sgl_msgbox_set_color(msgbox, sgl_rgb(250, 250, 250));
    sgl_msgbox_set_border_color(msgbox, sgl_rgb(70, 74, 84));
    sgl_msgbox_set_border_width(msgbox, 2);
    sgl_msgbox_set_radius(msgbox, 8);
    sgl_msgbox_set_alpha(msgbox, 255);
    sgl_msgbox_set_main_alpha(msgbox, 255);
    sgl_msgbox_set_border_alpha(msgbox, 255);
    sgl_msgbox_set_font(msgbox, &consolas14);

    /* title bar + item text as message body */
    sgl_msgbox_set_title_text(msgbox, "Menu Item");
    sgl_msgbox_set_title_text_color(msgbox, sgl_rgb(40, 44, 52));
    sgl_msgbox_set_title_height(msgbox, 32);
    sgl_msgbox_set_msg_text(msgbox, menu_theme_selected_text(menu));
    sgl_msgbox_set_msg_text_color(msgbox, sgl_rgb(40, 44, 52));
    sgl_msgbox_set_msg_line_margin(msgbox, 4);
    sgl_msgbox_set_msg_x_offset(msgbox, 16);
    sgl_msgbox_set_msg_y_offset(msgbox, 12);

    /* OK button on the right, cancel on the left */
    sgl_msgbox_set_left_btn_text(msgbox, "Close");
    sgl_msgbox_set_left_btn_text_color(msgbox, SGL_COLOR_WHITE);
    sgl_msgbox_set_left_btn_color(msgbox, sgl_rgb(120, 120, 120));
    sgl_msgbox_set_right_btn_text(msgbox, "OK");
    sgl_msgbox_set_right_btn_text_color(msgbox, SGL_COLOR_WHITE);
    sgl_msgbox_set_right_btn_color(msgbox, sgl_rgb(0, 122, 255));

    /* record which button closed the dialog */
    sgl_msgbox_set_exit_answer(msgbox, &menu_theme_msgbox_answer);

    SGL_LOG_INFO("menu theme demo: item %d activated on depth %d",
                 (int)index, (int)sgl_menu_get_depth(menu));
}

static void menu_theme_action(sgl_obj_t *menu, int16_t index)
{
    /* popup a msgbox showing the clicked item text */
    menu_theme_show_msgbox(menu, index);
}

/* child pages are defined before the parent page referencing them */
static const sgl_menu_item_t menu_theme_network_items[] = {
    SGL_MENU_ITEM("Wi-Fi",         menu_theme_action),
    SGL_MENU_ITEM("Bluetooth",     menu_theme_action),
    SGL_MENU_ITEM("Mobile data",   menu_theme_action),
};
SGL_MENU_DEF(menu_theme_network, "Network", menu_theme_network_items);

static const sgl_menu_item_t menu_theme_settings_items[] = {
    SGL_MENU_ITEM("Display",       menu_theme_action),
    SGL_MENU_ITEM("Sound",         menu_theme_action),
    SGL_MENU_SUBMENU("Network",    &menu_theme_network),
    SGL_MENU_ITEM("Theme",         menu_theme_action),
    SGL_MENU_ITEM("Language",      menu_theme_action),
};
SGL_MENU_DEF(menu_theme_settings, "Settings", menu_theme_settings_items);

static const sgl_menu_item_t menu_theme_media_items[] = {
    SGL_MENU_ITEM("Music",         menu_theme_action),
    SGL_MENU_ITEM("Photos",        menu_theme_action),
    SGL_MENU_ITEM("Videos",        menu_theme_action),
    SGL_MENU_ITEM("Radio",         menu_theme_action),
};
SGL_MENU_DEF(menu_theme_media, "Media", menu_theme_media_items);

static const sgl_menu_item_t menu_theme_main_items[] = {
    SGL_MENU_ITEM("Messages",      menu_theme_action),
    SGL_MENU_ITEM("Contacts",      menu_theme_action),
    SGL_MENU_SUBMENU("Settings",   &menu_theme_settings),
    SGL_MENU_SUBMENU("Media",      &menu_theme_media),
    SGL_MENU_ITEM("Calendar",      menu_theme_action),
    SGL_MENU_ITEM("Clock",         menu_theme_action),
    SGL_MENU_ITEM("Notes",         menu_theme_action),
    SGL_MENU_ITEM("About",         menu_theme_action),
};
SGL_MENU_DEF(menu_theme_main, "Main Menu", menu_theme_main_items);

/* ------------------------------------------------------------------ */
/* theme palettes                                                      */
/* ------------------------------------------------------------------ */

typedef struct menu_theme_palette {
    sgl_color_t bg;               /* menu page background             */
    sgl_color_t bar_bg;           /* title / softkey bar background   */
    sgl_color_t bar_text;         /* title / softkey bar text         */
    sgl_color_t card_bg;          /* idle item card background        */
    sgl_color_t card_border;      /* idle item card border            */
    sgl_color_t text;             /* item text                        */
    sgl_color_t sel_bg;           /* selected card background         */
    sgl_color_t sel_border;       /* selected card border (accent)    */
    sgl_color_t sel_text;         /* text on the selected card        */
} menu_theme_palette_t;

static const menu_theme_palette_t menu_theme_light = {
    /* white theme: soft gray cards, blue accent selection */
    .bg          = sgl_rgb(245, 246, 248),
    .bar_bg      = sgl_rgb(255, 255, 255),
    .bar_text    = sgl_rgb(40, 44, 52),
    .card_bg     = sgl_rgb(255, 255, 255),
    .card_border = sgl_rgb(224, 227, 231),
    .text        = sgl_rgb(40, 44, 52),
    .sel_bg      = sgl_rgb(232, 242, 255),
    .sel_border  = sgl_rgb(0, 122, 255),
    .sel_text    = sgl_rgb(0, 86, 201),
};

static const menu_theme_palette_t menu_theme_dark = {
    /* dark theme: deep gray cards, cyan accent selection */
    .bg          = sgl_rgb(24, 26, 32),
    .bar_bg      = sgl_rgb(16, 17, 22),
    .bar_text    = sgl_rgb(220, 223, 228),
    .card_bg     = sgl_rgb(44, 47, 56),
    .card_border = sgl_rgb(60, 64, 74),
    .text        = sgl_rgb(220, 223, 228),
    .sel_bg      = sgl_rgb(38, 58, 84),
    .sel_border  = sgl_rgb(0, 170, 255),
    .sel_text    = sgl_rgb(130, 215, 255),
};

/* ------------------------------------------------------------------ */
/* menu builders                                                       */
/* ------------------------------------------------------------------ */

static sgl_obj_t* menu_theme_create(sgl_obj_t *parent,
                                    const menu_theme_palette_t *pal,
                                    int16_t x, int16_t y, int16_t w, int16_t h)
{
    sgl_obj_t *menu = sgl_menu_create(parent, &menu_theme_main);
    if (menu == NULL) {
        return NULL;
    }

    sgl_obj_set_pos(menu, x, y);
    sgl_obj_set_size(menu, w, h);

    sgl_menu_set_font(menu, &consolas23);
    sgl_menu_set_bg_color(menu, pal->bg);
    sgl_menu_set_title_color(menu, pal->bar_bg, pal->bar_text);
    sgl_menu_set_text_color(menu, pal->text);
    sgl_menu_set_card_style(menu, pal->card_bg, pal->card_border, 4);
    sgl_menu_set_sel_style(menu, pal->sel_bg, pal->sel_border, pal->sel_text, 1);

    return menu;
}

/**
 * @brief create the menu theme examples: a white (light) theme menu on
 *        the left and a dark theme menu on the right
 * @param parent parent object, NULL creates the menus on the active screen
 * @return none
 */
void sgl_menu_examples(sgl_obj_t *parent)
{
    menu_theme_create(parent, &menu_theme_light, 40, 60, 340, 360);
    menu_theme_create(parent, &menu_theme_dark, 420, 60, 340, 360);
}
