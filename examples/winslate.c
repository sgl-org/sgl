/* examples/winslate.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL
 *
 * Slate window (sgl_winslate) widget example:
 *   - an "Open Slate Window" button spawns a flat Modern-UI window on the
 *     active screen
 *   - the window has a solid caption bar, a title string and two caption
 *     buttons at the right: maximize / restore and close
 *   - clicking the close button destroys the window; clicking maximize
 *     toggles the window between its normal geometry and full-screen
 *   - the body carries two labels that are parented to the window so they
 *     are destroyed together with it
 */

#include <sgl.h>

/* the currently open demo window, NULL when none is shown */
static sgl_obj_t *g_winslate_win = NULL;

static void sgl_winslate_example_event_cb(sgl_event_t *e);

/**
 * @brief build and show one demo Slate window
 * @param none
 * @return none
 */
static void sgl_winslate_example_open(void)
{
    sgl_obj_t *win;
    sgl_obj_t *line1;
    sgl_obj_t *line2;

    win = sgl_winslate_create(NULL);
    if (win == NULL) {
        return;
    }

    /* geometry */
    sgl_obj_set_pos(win, 80, 90);
    sgl_obj_set_size(win, 360, 220);

    /* chrome style (flat: white caption, light body, thin grey outline) */
    sgl_winslate_set_title_bg_color(win, SGL_COLOR_WHITE);
    sgl_winslate_set_body_color(win, sgl_rgb(0xF2, 0xF2, 0xF2));
    sgl_winslate_set_border_color(win, sgl_rgb(0xCC, 0xCC, 0xCC));
    sgl_winslate_set_title_height(win, 20);
    sgl_winslate_set_alpha(win, SGL_ALPHA_MAX);

    /* title */
    sgl_winslate_set_title_font(win, &consolas14);
    sgl_winslate_set_title_text(win, "Slate Window");
    sgl_winslate_set_title_text_color(win, sgl_rgb(0x1A, 0x1A, 0x1A));

    /* two labels placed inside the window body, parented to the window so
     * they are destroyed together with the window (sgl_label is single-line,
     * so use one label per row) */
    line1 = sgl_label_create(win);
    sgl_obj_set_pos(line1, 16, 52);
    sgl_obj_set_size(line1, 320, 20);
    sgl_label_set_font(line1, &consolas14);
    sgl_label_set_text(line1, "This is a Slate window demo.");
    sgl_label_set_text_color(line1, sgl_rgb(40, 40, 40));
    sgl_label_set_text_align(line1, SGL_ALIGN_LEFT_MID);

    line2 = sgl_label_create(win);
    sgl_obj_set_pos(line2, 16, 78);
    sgl_obj_set_size(line2, 320, 20);
    sgl_label_set_font(line2, &consolas14);
    sgl_label_set_text(line2, "Maximize toggles full screen; X closes.");
    sgl_label_set_text_color(line2, sgl_rgb(40, 40, 40));
    sgl_label_set_text_align(line2, SGL_ALIGN_LEFT_MID);

    /* drop our reference as soon as the window is closed so it can reopen */
    sgl_obj_set_event_cb(win, sgl_winslate_example_event_cb, NULL);
    g_winslate_win = win;
}

/**
 * @brief event callback of the demo window
 * @param e event structure, e->obj points to the window
 * @return none
 */
static void sgl_winslate_example_event_cb(sgl_event_t *e)
{
    if (e->obj != NULL && e->obj->destroyed) {
        g_winslate_win = NULL;
    }
}

/**
 * @brief click callback of the "Open Slate Window" button
 * @param e event structure
 * @return none
 */
static void sgl_winslate_example_open_cb(sgl_event_t *e)
{
    if (e->type != SGL_EVENT_CLICKED) {
        return;
    }
    if (g_winslate_win != NULL) {
        return; /* a window is already open */
    }
    sgl_winslate_example_open();
}

/**
 * @brief create the Slate window example
 * @param parent parent object, NULL creates the widgets on the active screen
 * @return none
 */
void sgl_winslate_examples(sgl_obj_t *parent)
{
    sgl_obj_t *btn;

    /* "Open Slate Window" button */
    btn = sgl_button_create(parent);
    sgl_obj_set_pos(btn, 20, 20);
    sgl_obj_set_size(btn, 200, 44);
    sgl_button_set_font(btn, &consolas14);
    sgl_button_set_text(btn, "Open Slate Window");
    sgl_button_set_color(btn, sgl_rgb(0x2B, 0x57, 0x9A));
    sgl_button_set_text_color(btn, SGL_COLOR_WHITE);
    sgl_button_set_radius(btn, 0);
    sgl_obj_set_event_cb(btn, sgl_winslate_example_open_cb, NULL);

    /* start with one window already shown */
    sgl_winslate_example_open();
}
