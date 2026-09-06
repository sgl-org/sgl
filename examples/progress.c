/**
 * @file sgl/examples/progress.c
 * @brief Progress widget example
 */

#include <sgl.h>

/**
 * @brief create the progress example
 * @param parent parent object, NULL creates the progress on the active screen
 * @return none
 */
void sgl_progress_examples(sgl_obj_t *parent)
{
    sgl_obj_t *progress1;
    sgl_obj_t *progress2;
    sgl_obj_t *progress3;
    
    /* Create first progress - horizontal loading bar */
    progress1 = sgl_progress_create(parent);
    if (progress1 != NULL) {
        sgl_obj_set_pos(progress1, 30, 300);
        sgl_obj_set_size(progress1, 250, 20);
        
        sgl_progress_set_track_color(progress1, SGL_COLOR_DARK_GRAY);
        sgl_progress_set_fill_color(progress1, SGL_COLOR_BLUE);
        sgl_progress_set_radius(progress1, 8);
        sgl_progress_set_border_color(progress1, SGL_COLOR_GRAY);
        /* Set current value to 75% */
        sgl_progress_set_value(progress1, 75);
    }
    
    /* Create second progress - vertical battery style */
    progress2 = sgl_progress_create(parent);
    if (progress2 != NULL) {
        sgl_obj_set_pos(progress2, 300, 300);
        sgl_obj_set_size(progress2, 100, 20);
        
        sgl_progress_set_track_color(progress2, sgl_rgb(60, 60, 70));
        sgl_progress_set_fill_color(progress2, SGL_COLOR_GREEN);
        sgl_progress_set_radius(progress2, 4);
        sgl_progress_set_border_color(progress2, SGL_COLOR_DARK_GRAY);
        sgl_progress_set_fill_radius(progress2, 2);
        sgl_progress_set_fill_width(progress2, 4);
        /* Set current value to 45% */
        sgl_progress_set_value(progress2, 45);
    }
}
