/**
 * @file sgl/examples/textlist.c
 * @brief TextList widget example
 */

#include <sgl.h>

/**
 * @brief create the textlist example
 * @param parent parent object, NULL creates the textlist on the active screen
 * @return none
 */
void sgl_textlist_examples(sgl_obj_t *parent)
{
    sgl_obj_t *textlist;
    
    /* Create Text List widget */
    textlist = sgl_textlist_create(parent);
    if (textlist == NULL) return;
    
    /* Set position and size - list with scrollbar */
    sgl_obj_set_pos(textlist, 30, 100);
    sgl_obj_set_size(textlist, 200, 250);
    
    /* Configure appearance */
    sgl_textlist_set_bg_color(textlist, SGL_COLOR_LIGHT_GRAY);
    sgl_textlist_set_border_color(textlist, sgl_rgb(0x2F, 0x4F, 0x4F)); /* DarkSlateGray */
    sgl_textlist_set_border_width(textlist, 2);
    sgl_textlist_set_radius(textlist, 6);
    sgl_textlist_set_alpha(textlist, 255);
    
    /* Set font - use consolas14 for good readability */
    sgl_textlist_set_text_font(textlist, &consolas14);
    
    /* Set colors: normal text yellow-gray, selected item dark blue background */
    sgl_textlist_set_text_color(textlist, SGL_COLOR_YELLOW);
    sgl_textlist_set_selected_color(textlist, SGL_COLOR_BLUE);
    
    /* Add multiple items to demonstrate scrolling */
    sgl_textlist_add_item(textlist, "Python");
    sgl_textlist_add_item(textlist, "JavaScript");
    sgl_textlist_add_item(textlist, "C++");
    sgl_textlist_add_item(textlist, "Rust");
    sgl_textlist_add_item(textlist, "Go");
    sgl_textlist_add_item(textlist, "Java");
    sgl_textlist_add_item(textlist, "TypeScript");
    sgl_textlist_add_item(textlist, "PHP");
    sgl_textlist_add_item(textlist, "Swift");
    sgl_textlist_add_item(textlist, "Kotlin");
    sgl_textlist_add_item(textlist, "Dart");
    sgl_textlist_add_item(textlist, "Ruby");
    
    /* Optional: set pixmap (icon) for each item */
    /* sgl_textlist_set_pixmap(textlist, &some_icon_pixmap); */
}
