/* examples/dropdown.c
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
 * Dropdown widget examples:
 *  1. basic dropdown with static options
 *  2. dropdown with dynamic options and custom colors
 *  3. dropdown with custom font and selected color
 *  4. dropdown with visibility row control and transparency
 *
 * Click on each dropdown to expand/collapse the option list.
 */

/**
 * @brief create the dropdown examples
 * @param parent parent object, NULL creates the dropdowns on the active screen
 * @return none
 */
void sgl_dropdown_examples(sgl_obj_t *parent)
{
    sgl_obj_t *dd;
    const char *options1 = "Option 1\nOption 2\nOption 3\nOption 4\nOption 5";
    const char *options2 = "Browser\nSettings\nGallery\nMusic\nMessages\nInternet";
    const char *options3 = "High Priority\nNormal Priority\nLow Priority\nBackground Mode";

    /* example 1: basic dropdown with static options */
    dd = sgl_dropdown_create(parent);
    sgl_obj_set_pos(dd, 20, 60);
    sgl_obj_set_size(dd, 200, 20);
    sgl_dropdown_set_text_font(dd, &consolas14);
    sgl_dropdown_set_option_static(dd, options1);
    sgl_dropdown_set_radius(dd, 4);

    /* example 2: dropdown with dynamic options and custom border color */
    dd = sgl_dropdown_create(parent);
    sgl_obj_set_pos(dd, 240, 60);
    sgl_obj_set_size(dd, 220, 20);
    sgl_dropdown_set_text_font(dd, &consolas14);
    sgl_dropdown_set_option_dynamic(dd, options2);
    sgl_dropdown_set_border_color(dd, SGL_COLOR_BLUE);
    sgl_dropdown_set_selected_color(dd, SGL_COLOR_MAGENTA);
    sgl_dropdown_set_radius(dd, 4);

    /* example 3: dropdown with custom font and different text color */
    dd = sgl_dropdown_create(parent);
    sgl_obj_set_pos(dd, 480, 20);
    sgl_obj_set_size(dd, 200, 20);
    sgl_dropdown_set_option_static(dd, options3);
    sgl_dropdown_set_text_font(dd, &consolas14);
    sgl_dropdown_set_text_color(dd, SGL_COLOR_YELLOW);
    sgl_dropdown_set_selected_color(dd, SGL_COLOR_GREEN);
    sgl_dropdown_set_radius(dd, 4);

    /* example 4: dropdown with max visible rows and semi-transparent */
    dd = sgl_dropdown_create(parent);
    sgl_obj_set_pos(dd, 20, 20);
    sgl_obj_set_size(dd, 200, 20);
    sgl_dropdown_set_text_font(dd, &consolas14);
    sgl_dropdown_set_option_dynamic(dd, options1);
    sgl_dropdown_set_visible_rows(dd, 3);
    sgl_dropdown_set_alpha(dd, 160);
    sgl_dropdown_set_bg_color(dd, sgl_rgb(30, 30, 40));
    sgl_dropdown_set_radius(dd, 4);
}
