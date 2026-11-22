/* examples/tabview.c
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
 * TabView widget example:
 *  A tab container with a tab bar on top and one content page per tab.
 *   - tap a tab button (or call sgl_tabview_set_active()) to switch pages
 *   - each page is a plain container: add any child widgets to it
 *   - only the active page is visible, inactive pages are hidden
 *
 * This demo fills the whole screen, so it works best as a standalone demo:
 * uncomment sgl_tabview_examples(NULL) in main.c (and comment out the other
 * examples) to run it.
 */

#define TV_EX_X   (10)
#define TV_EX_Y   (10)
#define TV_EX_W   (SGL_SCREEN_WIDTH - 20)
#define TV_EX_H   (SGL_SCREEN_HEIGHT - 20)

/**
 * @brief fill the "Buttons" tab page
 * @param page tab page object
 * @return none
 */
static void tabview_fill_button_page(sgl_obj_t *page)
{
    sgl_obj_t *obj;

    obj = sgl_label_create(page);
    sgl_obj_set_pos(obj, 20, 20);
    sgl_obj_set_size(obj, 300, 30);
    sgl_label_set_font(obj, &consolas23);
    sgl_label_set_text(obj, "Button page");

    obj = sgl_button_create(page);
    sgl_obj_set_pos(obj, 20, 70);
    sgl_obj_set_size(obj, 160, 50);
    sgl_button_set_font(obj, &consolas14);
    sgl_button_set_text(obj, "OK");
    sgl_button_set_radius(obj, 10);

    obj = sgl_button_create(page);
    sgl_obj_set_pos(obj, 200, 70);
    sgl_obj_set_size(obj, 160, 50);
    sgl_button_set_font(obj, &consolas14);
    sgl_button_set_text(obj, "Cancel");
    sgl_button_set_color(obj, SGL_COLOR_RED_ORANGE);
    sgl_button_set_radius(obj, 10);

    obj = sgl_button_create(page);
    sgl_obj_set_pos(obj, 380, 70);
    sgl_obj_set_size(obj, 160, 50);
    sgl_button_set_font(obj, &consolas14);
    sgl_button_set_text(obj, "Apply");
    sgl_button_set_color(obj, SGL_COLOR_GREEN);
    sgl_button_set_radius(obj, 25);
}

/**
 * @brief fill the "Sliders" tab page
 * @param page tab page object
 * @return none
 */
static void tabview_fill_slider_page(sgl_obj_t *page)
{
    sgl_obj_t *obj;

    obj = sgl_label_create(page);
    sgl_obj_set_pos(obj, 20, 20);
    sgl_obj_set_size(obj, 300, 30);
    sgl_label_set_font(obj, &consolas23);
    sgl_label_set_text(obj, "Slider page");

    obj = sgl_slider_create(page);
    sgl_obj_set_pos(obj, 20, 80);
    sgl_obj_set_size(obj, 400, 20);
    sgl_slider_set_direct(obj, SGL_DIRECT_HORIZONTAL);
    sgl_slider_set_fill_color(obj, SGL_COLOR_BLUE);
    sgl_slider_set_knob_color(obj, SGL_COLOR_CYAN);
    sgl_slider_set_track_color(obj, sgl_rgb(60, 60, 80));
    sgl_slider_set_value(obj, 75);
    sgl_slider_set_radius(obj, 10);

    obj = sgl_slider_create(page);
    sgl_obj_set_pos(obj, 20, 130);
    sgl_obj_set_size(obj, 400, 20);
    sgl_slider_set_direct(obj, SGL_DIRECT_HORIZONTAL);
    sgl_slider_set_fill_color(obj, SGL_COLOR_RED_ORANGE);
    sgl_slider_set_knob_color(obj, SGL_COLOR_WHITE);
    sgl_slider_set_track_color(obj, SGL_COLOR_GRAY);
    sgl_slider_set_value(obj, 35);
    sgl_slider_set_thickness(obj, 3);

    obj = sgl_progress_create(page);
    sgl_obj_set_pos(obj, 20, 180);
    sgl_obj_set_size(obj, 400, 20);
    sgl_progress_set_track_color(obj, SGL_COLOR_DARK_GRAY);
    sgl_progress_set_fill_color(obj, SGL_COLOR_GREEN);
    sgl_progress_set_radius(obj, 8);
    sgl_progress_set_border_color(obj, SGL_COLOR_GRAY);
    sgl_progress_set_value(obj, 60);
}

/**
 * @brief fill the "About" tab page
 * @param page tab page object
 * @return none
 */
static void tabview_fill_about_page(sgl_obj_t *page)
{
    sgl_obj_t *obj;

    obj = sgl_label_create(page);
    sgl_obj_set_pos(obj, 20, 20);
    sgl_obj_set_size(obj, 300, 30);
    sgl_label_set_font(obj, &consolas23);
    sgl_label_set_text(obj, "About tabview");

    obj = sgl_label_create(page);
    sgl_obj_set_pos(obj, 20, 70);
    sgl_obj_set_size(obj, 700, 24);
    sgl_label_set_font(obj, &consolas14);
    sgl_label_set_text(obj, "A tab container: tab bar on top, one page per tab.");

    obj = sgl_label_create(page);
    sgl_obj_set_pos(obj, 20, 100);
    sgl_obj_set_size(obj, 700, 24);
    sgl_label_set_font(obj, &consolas14);
    sgl_label_set_text(obj, "Tap a tab button to switch the visible page.");
}

/**
 * @brief create the tabview example
 * @param parent parent object, NULL creates the tabview on the active screen
 * @return none
 */
void sgl_tabview_examples(sgl_obj_t *parent)
{
    sgl_obj_t *tv;
    sgl_obj_t *page;

    tv = sgl_tabview_create(parent);
    sgl_obj_set_pos(tv, TV_EX_X, TV_EX_Y);
    sgl_obj_set_size(tv, TV_EX_W, TV_EX_H);
    sgl_tabview_set_font(tv, &consolas23);
    sgl_tabview_set_radius(tv, 5);
    sgl_tabview_set_bg_color(tv, sgl_rgb(245, 245, 245));
    sgl_tabview_set_bar_color(tv, sgl_rgb(220, 226, 235));
    sgl_tabview_set_tab_color(tv, sgl_rgb(220, 226, 235));
    sgl_tabview_set_tab_active_color(tv, sgl_rgb(245, 245, 245));
    sgl_tabview_set_text_color(tv, SGL_COLOR_GRAY);
    sgl_tabview_set_text_active_color(tv, SGL_COLOR_BLUE);

    page = sgl_tabview_add_tab(tv, "Buttons");
    tabview_fill_button_page(page);

    page = sgl_tabview_add_tab(tv, "Sliders");
    tabview_fill_slider_page(page);

    page = sgl_tabview_add_tab(tv, "About");
    tabview_fill_about_page(page);

    /* start on the first tab */
    sgl_tabview_set_active(tv, 0);
}
