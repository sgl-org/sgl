/**
 * @file sgl/examples/msgbox.c
 * @brief MessageBox widget example
 */

#include <sgl.h>

/**
 * @brief create the msgbox example
 * @param parent parent object, NULL creates the msgbox on the active screen
 * @return none
 */
void sgl_msgbox_examples(sgl_obj_t *parent)
{
    sgl_obj_t *msgbox;
    
    /* Create message box - centered dialog */
    msgbox = sgl_msgbox_create(parent);
    if (msgbox == NULL) return;
    
    /* Set position and size */
    sgl_obj_set_pos(msgbox, 250, 180);
    sgl_obj_set_size(msgbox, 300, 180);
    
    /* Configure appearance */
    sgl_msgbox_set_color(msgbox, SGL_COLOR_WHITE);
    sgl_msgbox_set_border_color(msgbox, SGL_COLOR_DARK_GRAY);
    sgl_msgbox_set_border_width(msgbox, 2);
    sgl_msgbox_set_radius(msgbox, 4);
    sgl_msgbox_set_alpha(msgbox, 255);
    sgl_msgbox_set_main_alpha(msgbox, 255);
    sgl_msgbox_set_border_alpha(msgbox, 255);

    /* Set font */
    sgl_msgbox_set_font(msgbox, &consolas14);

    /* Set title */
    sgl_msgbox_set_title_text(msgbox, "System Notice");
    sgl_msgbox_set_title_text_color(msgbox, SGL_COLOR_BLACK);
    sgl_msgbox_set_title_height(msgbox, 35);

    /* Set message text (multi-line supported) */
    sgl_msgbox_set_msg_text(msgbox, "Settings have been updated successfully.\nPlease restart the application to apply changes.");
    sgl_msgbox_set_msg_text_color(msgbox, SGL_COLOR_BLACK);
    sgl_msgbox_set_msg_line_margin(msgbox, 4);
    sgl_msgbox_set_msg_x_offset(msgbox, 15);
    sgl_msgbox_set_msg_y_offset(msgbox, 10);

    /* Set left button */
    sgl_msgbox_set_left_btn_text(msgbox, "Restart");
    sgl_msgbox_set_left_btn_text_color(msgbox, SGL_COLOR_WHITE);
    sgl_msgbox_set_left_btn_color(msgbox, SGL_COLOR_BLUE);
    
    /* Set right button */
    sgl_msgbox_set_right_btn_text(msgbox, "Later");
    sgl_msgbox_set_right_btn_text_color(msgbox, SGL_COLOR_WHITE);
    sgl_msgbox_set_right_btn_color(msgbox, sgl_rgb(120, 120, 120));
}
