/* examples/charge.c
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
 * Charge widget examples, ported from widgets/charge/charge.html.
 *
 * The widget only needs five style knobs: border colour, liquid colour, body
 * colour, bubble colour and the font. Level, orientation, transparency and the
 * animation switch complete the set.
 *
 * The object box holds the terminal, the shell and the percentage readout.
 */

/* mid grey backdrop: the default border is black, and the readout is white,
 * so the widget needs a background that both of them read against */
static void charge_backdrop(sgl_obj_t *parent)
{
    sgl_obj_t *bg = sgl_rect_create(parent);

    if (bg == NULL) return;
    sgl_obj_set_pos(bg, 0, 0);
    sgl_obj_set_size(bg, 800, 480);
    sgl_rect_set_color(bg, sgl_rgb(0x4a, 0x55, 0x61));
    sgl_rect_set_radius(bg, 0);
}

/* one charge widget, configured in place */
static sgl_obj_t* charge_setup(sgl_obj_t *parent, int16_t x, int16_t y,
                               int16_t w, int16_t h, bool vertical,
                               uint8_t level)
{
    sgl_obj_t *ch = sgl_charge_create(parent);

    if (ch == NULL) return NULL;
    sgl_obj_set_pos(ch, x, y);
    sgl_obj_set_size(ch, w, h);
    sgl_charge_set_vertical(ch, vertical);
    sgl_charge_set_level(ch, level);
    sgl_charge_set_font(ch, &song23);
    return ch;
}

/**
 * @brief create the charge examples
 * @param parent parent object, NULL creates the widgets on the active screen
 * @return none
 */
void sgl_charge_examples(sgl_obj_t *parent)
{
    sgl_obj_t *ch;

    charge_backdrop(parent);

    /* example 1: the reference look, vertical and charging at 78% */
    ch = charge_setup(parent, 20, 16, 140, 250, true, 78);
    if (ch) sgl_charge_set_fill_color(ch, sgl_rgb(0, 220, 120));

    /* example 2: low level, red liquid */
    ch = charge_setup(parent, 180, 16, 140, 250, true, 12);
    if (ch) {
        sgl_charge_set_fill_color(ch, sgl_rgb(235, 70, 60));
        sgl_charge_set_bubble_color(ch, sgl_rgb(255, 190, 180));
    }

    /* example 3: empty cell, nothing but the tube */
    ch = charge_setup(parent, 340, 16, 140, 250, true, 50);
    if (ch) sgl_charge_set_body_color(ch, sgl_rgb(24, 30, 40));

    /* example 4: horizontal 62%, the cylinder laid on its side */
    ch = charge_setup(parent, 500, 20, 280, 116, false, 62);
    if (ch) sgl_charge_set_fill_color(ch, sgl_rgb(0, 190, 255));

    /* example 5: horizontal 35%, custom liquid and bubble colours */
    ch = charge_setup(parent, 500, 150, 280, 110, false, 35);
    if (ch) {
        sgl_charge_set_fill_color(ch, sgl_rgb(140, 100, 255));
        sgl_charge_set_bubble_color(ch, sgl_rgb(225, 215, 255));
        sgl_charge_set_body_color(ch, sgl_rgb(44, 38, 66));
    }
}
