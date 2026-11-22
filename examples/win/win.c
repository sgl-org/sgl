/* examples/win.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL
 *
 * Window (sgl_win) widget example:
 *   - a "Open Window" button spawns a titled window on the active screen
 *   - the window shows a styled title bar, a body label and the close
 *     button (a vector cross drawn by sgl_draw_cross) at the right of the
 *     title bar
 *   - clicking the cross destroys the window; the button can open it again
 */

#include <sgl.h>

/* the currently open demo window, NULL when none is shown */
static sgl_obj_t *g_win_example_win = NULL;

static void sgl_win_example_win_event_cb(sgl_event_t *e);

/**
 * @brief build and show one demo window
 * @param none
 * @return none
 */
static void sgl_win_example_open(void)
{
    sgl_obj_t *win;
    sgl_obj_t *line1;
    sgl_obj_t *line2;

    win = sgl_win_create(NULL);
    if (win == NULL) {
        return;
    }

    /* geometry */
    sgl_obj_set_pos(win, 60, 90);
    sgl_obj_set_size(win, 320, 200);

    /* body / frame style */
    sgl_win_set_color(win, sgl_rgb(245, 245, 245));
    sgl_win_set_radius(win, 10);
    sgl_win_set_alpha(win, 255);
    sgl_win_set_border_width(win, 1);
    sgl_win_set_border_color(win, sgl_rgb(180, 180, 180));

    /* title bar style */
    sgl_win_set_title_height(win, 20);
    sgl_win_set_title_bg_color(win, sgl_rgb(86, 156, 214));
    sgl_win_set_title_font(win, &consolas14);
    sgl_win_set_title_text(win, "SGL Window");
    sgl_win_set_title_text_color(win, SGL_COLOR_WHITE);
    sgl_win_set_title_text_align(win, SGL_ALIGN_CENTER);

    /* close button (the cross) color */
    sgl_win_set_close_btn_color(win, sgl_rgb(220, 70, 60));

    /* two labels placed inside the window body, parented to the window so
     * they are destroyed together with the window (sgl_label is single-line,
     * so use one label per row) */
    line1 = sgl_label_create(win);
    sgl_obj_set_pos(line1, 16, 52);
    sgl_obj_set_size(line1, 288, 20);
    sgl_label_set_font(line1, &consolas14);
    sgl_label_set_text(line1, "This is a window demo.");
    sgl_label_set_text_color(line1, sgl_rgb(40, 40, 40));
    sgl_label_set_text_align(line1, SGL_ALIGN_LEFT_MID);

    line2 = sgl_label_create(win);
    sgl_obj_set_pos(line2, 16, 78);
    sgl_obj_set_size(line2, 288, 20);
    sgl_label_set_font(line2, &consolas14);
    sgl_label_set_text(line2, "Click the X (top-right) to close.");
    sgl_label_set_text_color(line2, sgl_rgb(40, 40, 40));
    sgl_label_set_text_align(line2, SGL_ALIGN_LEFT_MID);

    /* track the window: the cross sets the destroyed flag, and the user
     * event callback runs right after the widget handler, so we can drop
     * our reference as soon as the window is closed and allow re-opening */
    sgl_obj_set_event_cb(win, sgl_win_example_win_event_cb, NULL);
    g_win_example_win = win;
}

/**
 * @brief event callback of the demo window
 * @param e event structure, e->obj points to the window
 * @return none
 */
static void sgl_win_example_win_event_cb(sgl_event_t *e)
{
    if (e->obj != NULL && e->obj->destroyed) {
        g_win_example_win = NULL;
    }
}

/**
 * @brief click callback of the "Open Window" button
 * @param e event structure
 * @return none
 */
static void sgl_win_example_open_cb(sgl_event_t *e)
{
    if (e->type != SGL_EVENT_CLICKED) {
        return;
    }
    if (g_win_example_win != NULL) {
        return; /* a window is already open */
    }
    sgl_win_example_open();
}

/**
 * @brief create the window example
 * @param parent parent object, NULL creates the widgets on the active screen
 * @return none
 */
void sgl_win_examples(sgl_obj_t *parent)
{
    sgl_obj_t *btn;

    /* "Open Window" button */
    btn = sgl_button_create(parent);
    sgl_obj_set_pos(btn, 20, 20);
    sgl_obj_set_size(btn, 160, 44);
    sgl_button_set_font(btn, &consolas14);
    sgl_button_set_text(btn, "Open Window");
    sgl_button_set_color(btn, sgl_rgb(86, 156, 214));
    sgl_button_set_text_color(btn, SGL_COLOR_WHITE);
    sgl_button_set_radius(btn, 8);
    sgl_obj_set_event_cb(btn, sgl_win_example_open_cb, NULL);

    /* start with one window already shown */
    sgl_win_example_open();
}
