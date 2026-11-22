/**
 * @file sgl/examples/coverflow.c
 * @brief CoverFlow widget example: stacked horizontal card gallery
 *
 * The cards come from imgs_pixmap.c: six 128x128 ARGB8565 icons. A
 * card pixmap is mandatory, so every card below has one and the card size is
 * exactly the pixmap size; the centered card keeps that size and the side
 * cards shrink.
 *
 * Interaction:
 *  - drag left / right: cards follow the finger with a live scale effect,
 *    release snaps to the nearest card (flick = inertia glide)
 *  - tap a side card: it slides to the center
 *  - tap the centered card: the card activation callback fires with its
 *    index and an info msgbox pops up with its details
 *  - keyboard: LEFT / RIGHT / UP / DOWN arrows scroll card by card (no focus
 *    needed, the widget is a main menu); ENTER activates the centered card,
 *    running its callback and popping up the same info msgbox
 */

#include <sgl.h>

/* ARGB8565 test icons, defined in imgs_pixmap.c */
extern const sgl_pixmap_t Icons__8__image;
extern const sgl_pixmap_t Icons__127__image;
extern const sgl_pixmap_t iMovie_cat_image;
extern const sgl_pixmap_t iTunes7indigo_image;
extern const sgl_pixmap_t My_Pictures_image;
extern const sgl_pixmap_t Regedit_image;

/* number of cards */
#define COVERFLOW_EX_CARD_NUM             (6)

/* card names, used by the info msgbox (msg_text stores the pointer,
 * so the strings must outlive the dialog) */
static const char *const ex_card_names[COVERFLOW_EX_CARD_NUM] = {
    "Icons 8", "Icons 127", "iMovie", "iTunes 7", "My Pictures", "Regedit",
};

/* body text of the info msgbox: a static buffer because sgl_msgbox_set_msg_text
 * keeps the pointer instead of copying the string */
static char ex_msg_buf[96];

/**
 * @brief pop up an info dialog describing the activated card
 * @param index activated card index
 * @return none
 * @note the dialog destroys itself when either button is pressed
 */
static void coverflow_show_msgbox(int16_t index)
{
    sgl_obj_t *msgbox = sgl_msgbox_create(NULL);
    if (msgbox == NULL) {
        return;
    }

    /* centered dialog, about one third of the screen */
    sgl_obj_set_size(msgbox, 360, 180);
    sgl_obj_set_pos_align(msgbox, SGL_ALIGN_CENTER);

    sgl_msgbox_set_color(msgbox, sgl_rgb(250, 250, 250));
    sgl_msgbox_set_border_color(msgbox, sgl_rgb(70, 74, 84));
    sgl_msgbox_set_border_width(msgbox, 2);
    sgl_msgbox_set_radius(msgbox, 8);
    sgl_msgbox_set_alpha(msgbox, 255);
    sgl_msgbox_set_font(msgbox, &consolas14);

    /* build the message from the card index + name */
    const char *name = (index >= 0 && index < COVERFLOW_EX_CARD_NUM) ?
                       ex_card_names[index] : "Unknown";
    sgl_snprintf(ex_msg_buf, sizeof(ex_msg_buf),
                 "Card #%d activated!\nName: %s\n\nPress a button to close.",
                 (int)index, name);

    sgl_msgbox_set_title_text(msgbox, "CoverFlow");
    sgl_msgbox_set_title_text_color(msgbox, sgl_rgb(40, 44, 52));
    sgl_msgbox_set_title_height(msgbox, 32);
    sgl_msgbox_set_msg_text(msgbox, ex_msg_buf);
    sgl_msgbox_set_msg_text_color(msgbox, sgl_rgb(40, 44, 52));
    sgl_msgbox_set_msg_line_margin(msgbox, 4);
    sgl_msgbox_set_msg_x_offset(msgbox, 16);
    sgl_msgbox_set_msg_y_offset(msgbox, 12);

    /* Close on the left, OK on the right */
    sgl_msgbox_set_left_btn_text(msgbox, "Close");
    sgl_msgbox_set_left_btn_text_color(msgbox, SGL_COLOR_WHITE);
    sgl_msgbox_set_left_btn_color(msgbox, sgl_rgb(120, 120, 120));
    sgl_msgbox_set_right_btn_text(msgbox, "OK");
    sgl_msgbox_set_right_btn_text_color(msgbox, SGL_COLOR_WHITE);
    sgl_msgbox_set_right_btn_color(msgbox, sgl_rgb(0, 122, 255));
}

/**
 * @brief card activation callback: show which card was activated
 * @param evt event forwarded by the widget, evt->obj is the coverflow object
 * @param index activated card index
 * @note fired when the centered card is tapped, and also when ENTER is
 *       pressed (the widget forwards the key click to the center card)
 */
static void coverflow_card_event_cb(sgl_event_t *evt, int16_t index)
{
    if (evt->type != SGL_EVENT_CLICKED) {
        return;
    }

    SGL_LOG_INFO("coverflow: card %d activated", index);

    /* popup an info dialog for the activated card */
    coverflow_show_msgbox(index);
}

/**
 * @brief create the coverflow example
 * @param parent parent object, NULL creates it on the active screen
 * @return none
 */
void sgl_coverflow_examples(sgl_obj_t *parent)
{
    /* every card must have a pixmap: it defines the card size */
    static const sgl_coverflow_card_t cards[COVERFLOW_EX_CARD_NUM] = {
        { .pixmap = &Icons__8__image,        .text = "Icons 8"     },
        { .pixmap = &Icons__127__image,      .text = "Icons 127"   },
        { .pixmap = &iMovie_cat_image,       .text = "iMovie"      },
        { .pixmap = &iTunes7indigo_image,    .text = "iTunes 7"    },
        { .pixmap = &My_Pictures_image,      .text = "My Pictures" },
        { .pixmap = &Regedit_image,          .text = "Regedit"     },
    };

    /* title */
    sgl_obj_t *title = sgl_label_create(parent);
    sgl_label_set_font(title, &consolas23);
    sgl_label_set_text(title, "CoverFlow Example");
    sgl_label_set_text_color(title, SGL_COLOR_BLACK);
    sgl_obj_set_pos(title, 320, 20);

    /* card gallery: the centered 256 px icon keeps its size, its neighbours
     * shrink, so the widget only has to be as tall as one card */
    sgl_obj_t *coverflow = sgl_coverflow_create(parent, cards, COVERFLOW_EX_CARD_NUM);
    if (coverflow == NULL) {
        return;
    }
    sgl_obj_set_pos(coverflow, 0, 80);
    sgl_obj_set_size(coverflow, 800, 300);

    /* card activation callback: when the centered card is tapped (or ENTER
     * is pressed), the widget fires this callback with the card index */
    sgl_coverflow_set_card_event_cb(coverflow, coverflow_card_event_cb, NULL);

    /* start on the third card */
    sgl_coverflow_set_index(coverflow, 2);
}
