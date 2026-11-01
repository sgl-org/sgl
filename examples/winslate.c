/* examples/winslate.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL
 *
 * Slate window (sgl_winslate) widget example:
 *   - an "Open Slate Window" button spawns a flat Modern-UI window on the
 *     active screen; every click adds another one, up to
 *     WINSLATE_EX_WIN_NUM at a time, each cascaded so the caption bars of
 *     the windows below stay reachable
 *   - a window has a solid caption bar, a title string and two caption
 *     buttons at the right: maximize / restore and close
 *   - clicking the close button destroys that window only and frees its
 *     slot, so the button can spawn a new one; clicking maximize toggles
 *     that window between its normal geometry and full-screen
 *   - the body carries two labels that are parented to the window so they
 *     are destroyed together with it
 */

#include <sgl.h>

/* max number of demo windows alive at the same time */
#define WINSLATE_EX_WIN_NUM     (16)
/* cascade step applied to the geometry of a new window */
#define WINSLATE_EX_STEP        (28)
/* text storage of every window slot */
#define WINSLATE_EX_TEXT_LEN    (24)

/* live demo windows, NULL when the slot is free */
static sgl_obj_t *g_winslate_wins[WINSLATE_EX_WIN_NUM];
/* cascade offset (0 .. WINSLATE_EX_WIN_NUM - 1) taken by each slot */
static uint8_t    g_winslate_offs[WINSLATE_EX_WIN_NUM];
/* title and body texts, they must outlive the window that shows them */
static char       g_winslate_titles[WINSLATE_EX_WIN_NUM][WINSLATE_EX_TEXT_LEN];
static char       g_winslate_msgs[WINSLATE_EX_WIN_NUM][WINSLATE_EX_TEXT_LEN];
/* number given to the next created window */
static int        g_winslate_seq = 1;

static void sgl_winslate_example_event_cb(sgl_event_t *e);

/**
 * @brief build and show one more demo Slate window, cascaded from the live
 *        ones; the request is dropped when all the slots are occupied
 * @param none
 * @return none
 */
static void sgl_winslate_example_open(void)
{
    sgl_obj_t *win;
    sgl_obj_t *line1;
    sgl_obj_t *line2;
    int slot;
    int i;
    int off;
    int no;

    /* take the first free slot */
    for (slot = 0; slot < WINSLATE_EX_WIN_NUM; slot++) {
        if (g_winslate_wins[slot] == NULL) {
            break;
        }
    }
    if (slot >= WINSLATE_EX_WIN_NUM) {
        return; /* too many windows are already open */
    }

    /* the first cascade offset that no live window occupies */
    for (off = 0; off < WINSLATE_EX_WIN_NUM; off++) {
        for (i = 0; i < WINSLATE_EX_WIN_NUM; i++) {
            if (g_winslate_wins[i] != NULL && g_winslate_offs[i] == off) {
                break;
            }
        }
        if (i >= WINSLATE_EX_WIN_NUM) {
            break;
        }
    }

    no = g_winslate_seq++;
    win = sgl_winslate_create(NULL);
    if (win == NULL) {
        return;
    }

    /* geometry */
    sgl_obj_set_pos(win, (int16_t)(80 + off * WINSLATE_EX_STEP),
                        (int16_t)(90 + off * WINSLATE_EX_STEP));
    sgl_obj_set_size(win, 360, 220);

    /* chrome style (flat: white caption, light body, thin grey outline) */
    // sgl_winslate_set_title_bg_color(win, SGL_COLOR_WHITE);
    // sgl_winslate_set_body_color(win, sgl_rgb(0xF2, 0xF2, 0xF2));
    // sgl_winslate_set_border_color(win, sgl_rgb(0xCC, 0xCC, 0xCC));
    sgl_winslate_set_title_height(win, 20);
    sgl_winslate_set_alpha(win, SGL_ALPHA_MAX);

    /* title, numbered so the stacked windows can be told apart */
    sgl_winslate_set_title_font(win, &consolas14);
    sgl_snprintf(g_winslate_titles[slot], sizeof(g_winslate_titles[slot]),
                 "Slate Window %d", no);
    sgl_winslate_set_title_text(win, g_winslate_titles[slot]);
    sgl_winslate_set_title_text_color(win, sgl_rgb(0x1A, 0x1A, 0x1A));

    /* two labels placed inside the window body, parented to the window so
     * they are destroyed together with the window (sgl_label is single-line,
     * so use one label per row) */
    line1 = sgl_label_create(win);
    sgl_obj_set_pos(line1, 16, 52);
    sgl_obj_set_size(line1, 320, 20);
    sgl_label_set_font(line1, &consolas14);
    sgl_snprintf(g_winslate_msgs[slot], sizeof(g_winslate_msgs[slot]),
                 "This is Slate window #%d.", no);
    sgl_label_set_text(line1, g_winslate_msgs[slot]);
    sgl_label_set_text_color(line1, sgl_rgb(40, 40, 40));
    sgl_label_set_text_align(line1, SGL_ALIGN_LEFT_MID);

    line2 = sgl_label_create(win);
    sgl_obj_set_pos(line2, 16, 78);
    sgl_obj_set_size(line2, 320, 20);
    sgl_label_set_font(line2, &consolas14);
    sgl_label_set_text(line2, "Drag the caption; maximize goes full screen.");
    sgl_label_set_text_color(line2, sgl_rgb(40, 40, 40));
    sgl_label_set_text_align(line2, SGL_ALIGN_LEFT_MID);

    /* free our slot as soon as this window is closed */
    sgl_obj_set_event_cb(win, sgl_winslate_example_event_cb, NULL);
    g_winslate_offs[slot] = (uint8_t)off;
    g_winslate_wins[slot] = win;
}

/**
 * @brief event callback of one demo window, releases its slot once the
 *        window is closed so a new one can take its place
 * @param e event structure, e->obj points to the window
 * @return none
 */
static void sgl_winslate_example_event_cb(sgl_event_t *e)
{
    int i;

    if (e->obj == NULL || !sgl_obj_is_destroyed(e->obj)) {
        return;
    }
    for (i = 0; i < WINSLATE_EX_WIN_NUM; i++) {
        if (g_winslate_wins[i] == e->obj) {
            g_winslate_wins[i] = NULL;
            break;
        }
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
    sgl_winslate_example_open(); /* add one more window */
}

/**
 * @brief create the Slate window example
 * @param parent parent object, NULL creates the widgets on the active screen
 * @return none
 */
void sgl_winslate_examples(sgl_obj_t *parent)
{
    sgl_obj_t *btn;
    sgl_obj_t *tip;

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

    /* hint: every click of the button adds one more window */
    tip = sgl_label_create(parent);
    sgl_obj_set_pos(tip, 230, 32);
    sgl_obj_set_size(tip, 300, 20);
    sgl_label_set_font(tip, &consolas14);
    sgl_label_set_text(tip, "click again to open more windows (up to 4)");
    sgl_label_set_text_color(tip, sgl_rgb(40, 40, 40));
    sgl_label_set_text_align(tip, SGL_ALIGN_LEFT_MID);

    /* start with two windows already shown */
    sgl_winslate_example_open();
    sgl_winslate_example_open();
}
