/* examples/physical_key.c
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
 * Physical key example: navigate and operate widgets with sgl_key_group
 *
 *  1. create a key group with sgl_key_group_create(), then add every widget
 *     that should be reachable by physical keys with sgl_key_group_add_obj()
 *  2. activate the group with sgl_key_group_load()
 *  3. call sgl_physical_key_handler() from your hardware key driver
 *     (ISR or polling task) to feed key events into SGL
 *
 *  behavior of the focused widget (drawn with a border):
 *   - button: ENTER pressed/released generates SGL_EVENT_CLICKED
 *             (or SGL_EVENT_LONG_CLICKED when held)
 *   - switch: ENTER pressed toggles it
 *   - slider (editable): ENTER enters edit mode, LEFT/RIGHT change the
 *     value, ESC leaves edit mode
 */

/**
 * @brief physical key id, reported by the hardware key driver
 */
typedef enum physical_key {
    PHYSICAL_KEY_UP,
    PHYSICAL_KEY_DOWN,
    PHYSICAL_KEY_LEFT,
    PHYSICAL_KEY_RIGHT,
    PHYSICAL_KEY_ENTER,
    PHYSICAL_KEY_ESC,
} physical_key_t;

/**
 * @brief physical key handler, call it from your hardware key driver
 *        (ISR or polling task)
 * @param key the physical key id
 * @param pressed true - key pressed, false - key released
 * @return none
 */
void sgl_physical_key_handler(physical_key_t key, bool pressed)
{
    switch (key) {
    case PHYSICAL_KEY_UP:
        if (pressed)
            sgl_key_up();
        break;

    case PHYSICAL_KEY_DOWN:
        if (pressed)
            sgl_key_down();
        break;

    case PHYSICAL_KEY_LEFT:
        if (pressed)
            sgl_key_left();
        break;

    case PHYSICAL_KEY_RIGHT:
        if (pressed)
            sgl_key_right();
        break;

    case PHYSICAL_KEY_ENTER:
        if (pressed)
            sgl_key_enter_pressed();
        else
            sgl_key_enter_released();
        break;

    case PHYSICAL_KEY_ESC:
        if (pressed)
            sgl_key_esc();
        break;

    default:
        break;
    }
}

/* key group and status label, kept alive for the whole demo */
static sgl_key_group_t *g_key_group = NULL;
static sgl_obj_t       *g_key_label = NULL;

/**
 * @brief common event callback, reports the last key action on the
 *        status label passed through the event data
 * @param e event structure, e->event_data is the widget name string
 * @return none
 */
static void sgl_physical_key_cb(sgl_event_t *e)
{
    const char *name = (const char *)e->event_data;

    switch (e->type) {
    case SGL_EVENT_CLICKED:
        sgl_label_set_text_fmt(g_key_label, "%s clicked", name);
        break;

    case SGL_EVENT_LONG_CLICKED:
        sgl_label_set_text_fmt(g_key_label, "%s long clicked", name);
        break;

    case SGL_EVENT_PRESSED:
        sgl_label_set_text_fmt(g_key_label, "%s pressed", name);
        break;

    case SGL_EVENT_KEY_LEFT:
    case SGL_EVENT_KEY_RIGHT:
    case SGL_EVENT_KEY_UP:
    case SGL_EVENT_KEY_DOWN:
        /* editable widget in edit mode: report the new slider value */
        sgl_label_set_text_fmt(g_key_label, "%s value: %d",
                               name, (int)sgl_slider_get_value(e->obj));
        break;

    default:
        break;
    }
}

/**
 * @brief create the physical key example
 * @param parent parent object, NULL creates the widgets on the active screen
 * @return none
 */
void sgl_physical_key_examples(sgl_obj_t *parent)
{
    sgl_obj_t *btn1;
    sgl_obj_t *btn2;
    sgl_obj_t *sw;
    sgl_obj_t *slider;
    sgl_obj_t *hint;

    /* create a key group that holds up to 4 focusable widgets */
    g_key_group = sgl_key_group_create(4);
    if (g_key_group == NULL) {
        SGL_LOG_ERROR("physical key example: create key group failed");
        return;
    }

    /* hint label */
    hint = sgl_label_create(parent);
    sgl_obj_set_pos(hint, 600, 20);
    sgl_obj_set_size(hint, 195, 30);
    sgl_label_set_font(hint, &consolas14);
    sgl_label_set_text(hint, "Physical key demo");

    /* status label, updated by the event callbacks */
    g_key_label = sgl_label_create(parent);
    sgl_obj_set_pos(g_key_label, 600, 80);
    sgl_obj_set_size(g_key_label, 195, 30);
    sgl_label_set_font(g_key_label, &consolas14);
    sgl_label_set_text(g_key_label, "press a key...");

    /* button 1 */
    btn1 = sgl_button_create(parent);
    sgl_obj_set_pos(btn1, 600, 120);
    sgl_obj_set_size(btn1, 95, 30);
    sgl_button_set_font(btn1, &consolas14);
    sgl_button_set_text(btn1, "Button1");
    sgl_obj_set_event_cb(btn1, sgl_physical_key_cb, (void *)"Button1");
    sgl_key_group_add_obj(g_key_group, btn1);

    /* button 2 */
    btn2 = sgl_button_create(parent);
    sgl_obj_set_pos(btn2, 700, 120);
    sgl_obj_set_size(btn2, 95, 30);
    sgl_button_set_font(btn2, &consolas14);
    sgl_button_set_text(btn2, "Button2");
    sgl_obj_set_event_cb(btn2, sgl_physical_key_cb, (void *)"Button2");
    sgl_key_group_add_obj(g_key_group, btn2);

    /* switch */
    sw = sgl_switch_create(parent);
    sgl_obj_set_pos(sw, 600, 160);
    sgl_obj_set_size(sw, 60, 30);
    sgl_obj_set_event_cb(sw, sgl_physical_key_cb, (void *)"Switch");
    sgl_key_group_add_obj(g_key_group, sw);

    /* slider (editable: ENTER enters edit mode, LEFT/RIGHT adjust, ESC exits) */
    slider = sgl_slider_create(parent);
    sgl_obj_set_pos(slider, 600, 210);
    sgl_obj_set_size(slider, 195, 20);
    sgl_slider_set_value(slider, 30);
    sgl_obj_set_event_cb(slider, sgl_physical_key_cb, (void *)"Slider");
    sgl_key_group_add_obj(g_key_group, slider);

    /* activate the group, physical keys now control the widgets above */
    sgl_key_group_load(g_key_group);
}
