#ifndef __SGL_LAUNCHER_H__
#define __SGL_LAUNCHER_H__

#include <sgl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sgl_launcher_app sgl_launcher_app_t;
typedef struct sgl_launcher sgl_launcher_t;

typedef struct sgl_launcher_ctx {
    const sgl_font_t *font;
    const sgl_launcher_app_t *apps;
    void (*cb)(sgl_launcher_t *launcher);
    void (*app_exit_cb)(void);
    int16_t app_count;
    uint8_t current_page;
} sgl_launcher_ctx_t;

struct sgl_launcher {
    sgl_obj_t obj;
    int16_t icon_size;
    int16_t margin_left;
    int16_t margin_top;
    int16_t margin_right;
    int16_t margin_bottom;
    int16_t grid_row;
    int16_t grid_col;
    int16_t page_width;
    int16_t page_height;
    int16_t drag_start_x;
    int16_t app_count;
    uint8_t page_count;
    sgl_color_t navigbar_color;
    sgl_color_t label_color;
    const sgl_font_t *font;
    const sgl_launcher_app_t *apps;
};

struct sgl_launcher_app {
    void (*start)(void *private_data);
    void (*exit)(void);
    void *private_data;
    const sgl_pixmap_t *icon;
    const char *name;
    int16_t radius;
};

/**
 * @brief launcher exit event handler with msgbox
 * @param evt event
 * @return none
 */
void sgl_launcher_exit_msgbox_cb(sgl_event_t *evt);

/**
 * @brief create launcher
 * @param label_font the label font
 * @param apps the apps
 * @param app_num the number of apps
 * @param cb the callback function
 * @return the launcher object
 */
sgl_obj_t *sgl_launcher_create(const sgl_font_t *label_font, const sgl_launcher_app_t *apps, int16_t app_num, void (*cb)(sgl_launcher_t *launcher));

/**
 * @brief set launcher margin
 * @param launcher the launcher object
 * @param left left margin
 * @param top top margin
 * @param right right margin
 * @param bottom bottom margin
 * @return none
 */
void sgl_launcher_set_margin(sgl_obj_t *launcher, int16_t left, int16_t top, int16_t right, int16_t bottom);

/**
 * @brief set launcher icon size
 * @param launcher the launcher object
 * @param size the icon size
 * @return none
 */
void sgl_launcher_set_icon_size(sgl_obj_t *launcher, int16_t size);

/**
 * @brief set launcher grid size
 * @param launcher the launcher object
 * @param cols the number of columns
 * @param rows the number of rows
 * @return none
 */
void sgl_launcher_set_grid_size(sgl_obj_t *launcher, int16_t cols, int16_t rows);

/**
 * @brief set launcher label color
 * @param launcher the launcher object
 * @param color the label color
 * @return none
 */
void sgl_launcher_set_label_color(sgl_obj_t *launcher, sgl_color_t color);

/**
 * @brief set launcher navigation bar color
 * @param launcher the launcher object
 * @param color the navigation bar color
 * @return none
 */
void sgl_launcher_set_navigbar_color(sgl_obj_t *launcher, sgl_color_t color);

/**
 * @brief get launcher current page
 * @param launcher the launcher object
 * @return current page
 */
int16_t sgl_launcher_get_current_page(sgl_obj_t *launcher);

/**
 * @brief set launcher current page
 * @param launcher the launcher object
 * @param page the page to set
 * @return none
 */
void sgl_launcher_set_current_page(sgl_obj_t *launcher, int16_t page);

#ifdef __cplusplus
}
#endif

#endif /* __SGL_LAUNCHER_H__ */
