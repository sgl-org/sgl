/* examples/menu.c
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

#include <sgl.h>

/**
 * Menu widget example: Symbian (S60) style declarative menu
 *  - the menu tree is described with constant tables (declarative)
 *  - stack based multi level navigation with slide animations
 *  - touch (drag to scroll, tap to select / activate) and keys
 *  - requires CONFIG_SGL_ANIMATION enabled for the transition effect
 */

static sgl_obj_t       *g_menu_demo_obj   = NULL;
static sgl_key_group_t *g_menu_demo_group = NULL;

static void sgl_menu_action(sgl_obj_t *menu, int16_t index)
{
    SGL_LOG_INFO("menu demo: item %d activated on depth %d",
                 (int)index, (int)sgl_menu_get_depth(menu));
}

/* child pages are defined before the parent page referencing them */
static const sgl_menu_item_t g_menu_demo_network_items[] = {
    SGL_MENU_ITEM("Wi-Fi",          sgl_menu_action),
    SGL_MENU_ITEM("Bluetooth",      sgl_menu_action),
    SGL_MENU_ITEM("Mobile data",    sgl_menu_action),
};
SGL_MENU_DEF(g_menu_demo_network, "Network", g_menu_demo_network_items);

static const sgl_menu_item_t g_menu_demo_settings_items[] = {
    SGL_MENU_ITEM("Display",        sgl_menu_action),
    SGL_MENU_ITEM("Sound",          sgl_menu_action),
    SGL_MENU_SUBMENU("Network",     &g_menu_demo_network),
    SGL_MENU_ITEM("Factory reset",  sgl_menu_action),
    SGL_MENU_ITEM("Theme",          sgl_menu_action),
    SGL_MENU_ITEM("Language",       sgl_menu_action),
    SGL_MENU_ITEM("Time and date",  sgl_menu_action),
};
SGL_MENU_DEF(g_menu_demo_settings, "Settings", g_menu_demo_settings_items);

static const sgl_menu_item_t g_menu_demo_media_items[] = {
    SGL_MENU_ITEM("Music",         sgl_menu_action),
    SGL_MENU_ITEM("Photos",        sgl_menu_action),
    SGL_MENU_ITEM("Videos",        sgl_menu_action),
    SGL_MENU_ITEM("Radio",         sgl_menu_action),
    SGL_MENU_ITEM("Recorder",      sgl_menu_action),
};
SGL_MENU_DEF(g_menu_demo_media, "Media", g_menu_demo_media_items);

static const sgl_menu_item_t g_menu_demo_main_items[] = {
    SGL_MENU_ITEM("Messages",      sgl_menu_action),
    SGL_MENU_ITEM("Contacts",      sgl_menu_action),
    SGL_MENU_SUBMENU("Settings",   &g_menu_demo_settings),
    SGL_MENU_SUBMENU("Media",      &g_menu_demo_media),
    SGL_MENU_ITEM("Calendar",      sgl_menu_action),
    SGL_MENU_ITEM("Clock",         sgl_menu_action),
    SGL_MENU_ITEM("Notes",         sgl_menu_action),
    SGL_MENU_ITEM("About",         sgl_menu_action),
    SGL_MENU_ITEM("About1",         sgl_menu_action),
    SGL_MENU_ITEM("About2",         sgl_menu_action),
    SGL_MENU_ITEM("About3",         sgl_menu_action),
    SGL_MENU_ITEM("About4",         sgl_menu_action),
    SGL_MENU_ITEM("About5",         sgl_menu_action),
    SGL_MENU_ITEM("About6",         sgl_menu_action),
    SGL_MENU_ITEM("About7",         sgl_menu_action),
    SGL_MENU_ITEM("About8",         sgl_menu_action),
};
SGL_MENU_DEF(g_menu_demo_main, "Main Menu", g_menu_demo_main_items);

/* close callback: leave the key group and destroy the menu object */
static void sgl_menu_demo_close_cb(sgl_obj_t *menu)
{
    if (g_menu_demo_group != NULL) {
        sgl_key_group_remove_obj(g_menu_demo_group, menu);
    }
    sgl_obj_delete(menu);
    g_menu_demo_obj = NULL;
}

/* button callback: open the menu (only one instance at a time) */
static void sgl_menu_demo_open_cb(sgl_event_t *e)
{
    sgl_obj_t *menu;

    if (e->type != SGL_EVENT_PRESSED || g_menu_demo_obj != NULL)
        return;

    menu = sgl_menu_create(NULL, &g_menu_demo_main);
    if (menu == NULL)
        return;

    sgl_obj_set_pos(menu, 240, 30);
    sgl_obj_set_size(menu, 320, 420);
    sgl_menu_set_font(menu, &consolas24);
    sgl_menu_set_close_cb(menu, sgl_menu_demo_close_cb);

    if (g_menu_demo_group != NULL) {
        sgl_key_group_add_obj(g_menu_demo_group, menu);
    }

    g_menu_demo_obj = menu;
}

/**
 * @brief create the menu example
 * @param parent parent object, NULL creates the button on the active screen
 * @param group optional key group to add the button to
 * @return none
 */
void sgl_menu_demo(sgl_obj_t *parent, sgl_key_group_t *group)
{
    sgl_obj_t *btn;

    g_menu_demo_group = group;

    btn = sgl_button_create(parent);
    sgl_obj_set_pos(btn, 10, 250);
    sgl_obj_set_size(btn, 100, 30);
    sgl_button_set_font(btn, &consolas14);
    sgl_button_set_text(btn, "Open Menu");
    sgl_button_set_radius(btn, 10);
    sgl_obj_set_event_cb(btn, sgl_menu_demo_open_cb, NULL);

    if (group != NULL) {
        sgl_key_group_add_obj(group, btn);
    }
}
