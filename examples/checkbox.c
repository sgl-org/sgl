/**
 * @file sgl/examples/checkbox.c
 * @brief CheckBox widget example
 */

#include <sgl.h>

/**
 * @brief create the checkbox example
 * @param parent parent object, NULL creates the checkbox on the active screen
 * @return none
 */
void sgl_checkbox_examples(sgl_obj_t *parent)
{
    sgl_obj_t *checkbox1;
    sgl_obj_t *checkbox2;
    sgl_obj_t *checkbox3;
    
    /* Create first checkbox - basic usage, unchecked by default */
    checkbox1 = sgl_checkbox_create(parent);
    if (checkbox1 != NULL) {
        sgl_obj_set_pos(checkbox1, 30, 400);
        sgl_obj_set_size(checkbox1, 150, 30);
        
        sgl_checkbox_set_text(checkbox1, "Enable WiFi");
        sgl_checkbox_set_font(checkbox1, &consolas14);
        sgl_checkbox_set_text_color(checkbox1, SGL_COLOR_WHITE);
        sgl_checkbox_set_box_color(checkbox1, SGL_COLOR_DARK_GRAY);
        sgl_checkbox_set_check_color(checkbox1, SGL_COLOR_BLUE);
        sgl_checkbox_set_radius(checkbox1, 4);
    }
    
    /* Create second checkbox - checked by default with custom colors */
    checkbox2 = sgl_checkbox_create(parent);
    if (checkbox2 != NULL) {
        sgl_obj_set_pos(checkbox2, 200, 400);
        sgl_obj_set_size(checkbox2, 180, 30);
        
        sgl_checkbox_set_text(checkbox2, "Dark Mode Enabled");
        sgl_checkbox_set_font(checkbox2, &consolas14);
        sgl_checkbox_set_text_color(checkbox2, SGL_COLOR_YELLOW);
        sgl_checkbox_set_box_color(checkbox2, sgl_rgb(50, 50, 60));
        sgl_checkbox_set_check_color(checkbox2, SGL_COLOR_GREEN);
        sgl_checkbox_set_radius(checkbox2, 4);
        /* Start in checked state */
        sgl_checkbox_set_status(checkbox2, true);
    }
    
    /* Create third checkbox - disabled style with transparency */
    checkbox3 = sgl_checkbox_create(parent);
    if (checkbox3 != NULL) {
        sgl_obj_set_pos(checkbox3, 400, 400);
        sgl_obj_set_size(checkbox3, 160, 30);
        
        sgl_checkbox_set_text(checkbox3, "Notifications");
        sgl_checkbox_set_font(checkbox3, &consolas14);
        sgl_checkbox_set_text_color(checkbox3, sgl_rgb(0, 0, 0));
        sgl_checkbox_set_box_color(checkbox3, SGL_COLOR_DARK_GRAY);
        sgl_checkbox_set_check_color(checkbox3, SGL_COLOR_ORANGE);
        sgl_checkbox_set_radius(checkbox3, 6);
    }
}
