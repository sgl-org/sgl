/* examples/textedit.c
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
#include <string.h>

/**
 * Textedit widget examples:
 *  1. single-line textedit with keyboard input
 *  2. multi-line textedit with keyboard input and scrolling
 */

/* text buffers */
static char g_single_line_buf[64];
static char g_multi_line_buf[256];

/* keyboard object (created once, reused) */
static sgl_obj_t *g_keyboard = NULL;

/* current active textedit */
static sgl_obj_t *g_active_textedit = NULL;

/**
 * @brief keyboard key callback, forwards key events to the active textedit
 * @param keyboard_obj keyboard object
 * @param key key code
 * @param user_data user data (unused)
 * @return none
 */
static void keyboard_key_cb(sgl_obj_t *keyboard_obj, uint8_t key, void *user_data)
{
    SGL_UNUSED(keyboard_obj);
    SGL_UNUSED(user_data);

    if (g_active_textedit == NULL) {
        return;
    }

    switch (key) {
    case '\b':
        sgl_textedit_backspace(g_active_textedit);
        break;
    case '\n':
        /* newline only in multi-line mode */
        sgl_textedit_insert_char(g_active_textedit, '\n');
        break;
    case '\r':
        /* enter/confirm: hide keyboard */
        if (g_keyboard) {
            sgl_obj_set_hidden(g_keyboard);
            g_active_textedit = NULL;
        }
        break;
    default:
        sgl_textedit_insert_char(g_active_textedit, (char)key);
        break;
    }
}

/**
 * @brief textedit event callback, shows keyboard when textedit is clicked
 * @param e event structure
 * @return none
 */
static void textedit_event_cb(sgl_event_t *e)
{
    if (e->type == SGL_EVENT_CLICKED) {
        /* stop cursor blink on previous active textedit */
        if (g_active_textedit != NULL && g_active_textedit != e->obj) {
            sgl_textedit_cursor_blink_stop(g_active_textedit);
        }
        g_active_textedit = e->obj;
        if (g_keyboard) {
            sgl_obj_set_visible(g_keyboard);
        }
    }
}

/**
 * @brief create the textedit examples
 * @param parent parent object, NULL creates the widgets on the active screen
 * @return none
 */
void sgl_textedit_examples(sgl_obj_t *parent)
{
    sgl_obj_t *edit;
    sgl_obj_t *label;

    /* title label */
    label = sgl_label_create(parent);
    sgl_obj_set_pos(label, 10, 5);
    sgl_obj_set_size(label, 300, 20);
    sgl_label_set_font(label, &consolas14);
    sgl_label_set_text(label, "TextEdit Demo");
    sgl_label_set_text_color(label, SGL_COLOR_BLUE);

    /* example 1: single-line textedit */
    label = sgl_label_create(parent);
    sgl_obj_set_pos(label, 10, 30);
    sgl_obj_set_size(label, 100, 20);
    sgl_label_set_font(label, &consolas14);
    sgl_label_set_text(label, "Single:");

    edit = sgl_textedit_create(parent);
    sgl_obj_set_pos(edit, 80, 30);
    sgl_obj_set_size(edit, 220, 28);
    sgl_textedit_set_mode(edit, SGL_TEXTEDIT_SINGLE_LINE);
    sgl_textedit_set_text_buffer(edit, g_single_line_buf, sizeof(g_single_line_buf));
    sgl_textedit_set_text(edit, "Hello SGL");
    sgl_textedit_set_text_font(edit, &consolas14);
    sgl_textedit_set_radius(edit, 4);
    sgl_textedit_set_border_color(edit, SGL_COLOR_GRAY);
    sgl_obj_set_event_cb(edit, textedit_event_cb, NULL);

    /* example 2: multi-line textedit */
    label = sgl_label_create(parent);
    sgl_obj_set_pos(label, 10, 70);
    sgl_obj_set_size(label, 100, 20);
    sgl_label_set_font(label, &consolas14);
    sgl_label_set_text(label, "Multi:");

    edit = sgl_textedit_create(parent);
    sgl_obj_set_pos(edit, 80, 70);
    sgl_obj_set_size(edit, 220, 100);
    sgl_textedit_set_mode(edit, SGL_TEXTEDIT_MULTI_LINE);
    sgl_textedit_set_text_buffer(edit, g_multi_line_buf, sizeof(g_multi_line_buf));
    sgl_textedit_set_text(edit, "Line 1\nLine 2\nLine 3\nLine 4\nLine 5\nLine 6\nLine 7\nLine 8");
    sgl_textedit_set_text_font(edit, &consolas14);
    sgl_textedit_set_radius(edit, 4);
    sgl_textedit_set_border_color(edit, SGL_COLOR_GRAY);
    sgl_textedit_set_line_margin(edit, 2);
    sgl_obj_set_event_cb(edit, textedit_event_cb, NULL);

    /* create on-screen keyboard at bottom */
    g_keyboard = sgl_keyboard_create(parent);
    sgl_obj_set_pos(g_keyboard, 10, 180);
    sgl_obj_set_size(g_keyboard, 300, 140);
    sgl_keyboard_set_text_font(g_keyboard, &consolas14);
    sgl_keyboard_set_key_callback(g_keyboard, keyboard_key_cb, NULL);
    sgl_obj_set_hidden(g_keyboard); /* hidden by default, shown when textedit is clicked */
}
