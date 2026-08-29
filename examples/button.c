/* examples/button.c
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
 * Button widget examples:
 *  1. plain button with the default theme style
 *  2. custom color, border and rounded corners
 *  3. translucent button
 *  4. click counter, an event callback updates a label
 *  5. one button toggles the visibility of another one
 *  6. text alignment of the button caption
 */

/* target button of example 5 */
static sgl_obj_t *g_btn_toggle_target = NULL;

/* click counter of example 4 */
static uint32_t   g_btn_click_count   = 0;

/**
 * @brief click callback of example 4, shows the click count on the
 *        label passed through the event data
 * @param e event structure, e->event_data points to the label object
 * @return none
 */
static void sgl_button_count_cb(sgl_event_t *e)
{
    sgl_obj_t *label;

    if (e->type != SGL_EVENT_CLICKED)
        return;

    g_btn_click_count++;
    label = (sgl_obj_t *)e->event_data;
    sgl_label_set_text_fmt(label, "clicked %d times", (int)g_btn_click_count);
}

/**
 * @brief click callback of example 5, toggles the visibility of the
 *        target button
 * @param e event structure
 * @return none
 */
static void sgl_button_toggle_cb(sgl_event_t *e)
{
    if (e->type != SGL_EVENT_CLICKED || g_btn_toggle_target == NULL)
        return;

    if (sgl_obj_is_hidden(g_btn_toggle_target)) {
        sgl_obj_set_visible(g_btn_toggle_target);
    }
    else {
        sgl_obj_set_hidden(g_btn_toggle_target);
    }
}

/**
 * @brief create the button examples
 * @param parent parent object, NULL creates the buttons on the active screen
 * @return none
 */
void sgl_button_examples(sgl_obj_t *parent)
{
    sgl_obj_t *btn;
    sgl_obj_t *label;

    /* example 1: plain button with the default theme style */
    btn = sgl_button_create(parent);
    sgl_obj_set_pos(btn, 10, 40);
    sgl_obj_set_size(btn, 120, 34);
    sgl_button_set_font(btn, &consolas24);
    sgl_button_set_text(btn, "Default");
    sgl_button_set_radius(btn, 6);

    /* example 2: custom color, border and rounded corners */
    btn = sgl_button_create(parent);
    sgl_obj_set_pos(btn, 10, 90);
    sgl_obj_set_size(btn, 120, 34);
    sgl_button_set_font(btn, &consolas24);
    sgl_button_set_text(btn, "Styled");
    sgl_button_set_color(btn, sgl_rgb(46, 139, 87));
    sgl_button_set_text_color(btn, SGL_COLOR_WHITE);
    sgl_button_set_radius(btn, 17);
    sgl_button_set_border_width(btn, 2);
    sgl_button_set_border_color(btn, SGL_COLOR_YELLOW);

    /* example 3: translucent button */
    btn = sgl_button_create(parent);
    sgl_obj_set_pos(btn, 10, 140);
    sgl_obj_set_size(btn, 120, 34);
    sgl_button_set_font(btn, &consolas24);
    sgl_button_set_text(btn, "Alpha 96");
    sgl_button_set_alpha(btn, 96);
    sgl_button_set_radius(btn, 6);

    /* example 4: click counter, the label shows how often the
     * button was clicked */
    btn = sgl_button_create(parent);
    sgl_obj_set_pos(btn, 10, 190);
    sgl_obj_set_size(btn, 120, 34);
    sgl_button_set_font(btn, &consolas24);
    sgl_button_set_text(btn, "Count me");
    sgl_button_set_radius(btn, 6);

    label = sgl_label_create(parent);
    sgl_obj_set_pos(label, 150, 190);
    sgl_obj_set_size(label, 160, 34);
    sgl_label_set_font(label, &consolas24);
    sgl_label_set_text(label, "clicked 0 times");
    sgl_label_set_text_color(label, SGL_COLOR_CYAN);
    sgl_label_set_text_align(label, SGL_ALIGN_LEFT_MID);

    /* the label is handed to the callback through the event data */
    sgl_obj_set_event_cb(btn, sgl_button_count_cb, label);

    /* example 5: the "Toggle" button shows / hides the red button */
    btn = sgl_button_create(parent);
    sgl_obj_set_pos(btn, 10, 240);
    sgl_obj_set_size(btn, 120, 34);
    sgl_button_set_font(btn, &consolas24);
    sgl_button_set_text(btn, "Toggle");
    sgl_button_set_radius(btn, 6);
    sgl_obj_set_event_cb(btn, sgl_button_toggle_cb, NULL);

    g_btn_toggle_target = sgl_button_create(parent);
    sgl_obj_set_pos(g_btn_toggle_target, 150, 240);
    sgl_obj_set_size(g_btn_toggle_target, 120, 34);
    sgl_button_set_font(g_btn_toggle_target, &consolas24);
    sgl_button_set_text(g_btn_toggle_target, "Hide me");
    sgl_button_set_color(g_btn_toggle_target, SGL_COLOR_RED_ORANGE);
    sgl_button_set_text_color(g_btn_toggle_target, SGL_COLOR_WHITE);
    sgl_button_set_radius(g_btn_toggle_target, 6);

    /* example 6: caption aligned to the left edge */
    btn = sgl_button_create(parent);
    sgl_obj_set_pos(btn, 10, 290);
    sgl_obj_set_size(btn, 260, 34);
    sgl_button_set_font(btn, &consolas24);
    sgl_button_set_text(btn, "Left aligned caption");
    sgl_button_set_text_align(btn, SGL_ALIGN_LEFT_MID);
    sgl_button_set_radius(btn, 6);
}
