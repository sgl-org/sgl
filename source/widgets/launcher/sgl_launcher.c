#include "sgl_launcher.h"
#include <sgl.h>

#define CHANGE_PAGE_THRESHOLD  (5)

static uint8_t current_page = 0;

static int16_t launcher_item_pos(int16_t offset, int16_t total_len,
                                  int count, int16_t item_size, int i)
{
    if (count <= 0) return offset;
    if (count == 1) return offset + (total_len - item_size) / 2;

    int16_t total_gap   = total_len - count * item_size;
    int16_t base_stride = item_size + total_gap / (count - 1);
    int16_t extra       = total_gap % (count - 1);

    /* Accumulate extra pixels only for the first `extra` gaps */
    int16_t extra_acc = (i < extra) ? i : extra;
    return offset + i * base_stride + extra_acc;
}

static void sgl_launcher_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_launcher_t *launcher = (sgl_launcher_t *)obj;
    if (!launcher->page_count) {
        return;
    }

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN:
        if (launcher->page_count > 1) {
            const int16_t dot_r   = 4;
            const int16_t spacing = 20;  /* center-to-center */
            int16_t n       = launcher->page_count;
            int16_t cy      = SGL_SCREEN_HEIGHT - 20;
            int16_t cx0     = (SGL_SCREEN_WIDTH - (n - 1) * spacing) / 2;
            int16_t cur_page = current_page;

            for (int16_t i = 0; i < n; i++) {
                int16_t cx = cx0 + i * spacing;
                int16_t r  = (i == cur_page) ? dot_r + 1 : dot_r;
                sgl_color_t color = (i == cur_page) ? sgl_color_mixer(launcher->navigbar_color, SGL_COLOR_WHITE, 128) 
                                                    : launcher->navigbar_color;
                sgl_draw_fill_circle(surf, &obj->parent->area, cx, cy, r, color, SGL_ALPHA_MAX);
            }
        }
        break;
    case SGL_EVENT_PRESSED:
        launcher->drag_start_x = evt->pos.x;
    break;

    case SGL_EVENT_MOVE_LEFT:
    case SGL_EVENT_MOVE_RIGHT:
        sgl_obj_set_pos_x(obj, obj->coords.x1 + evt->distance);
    break;

    case SGL_EVENT_RELEASED: {
        int16_t delta     = evt->pos.x - launcher->drag_start_x;
        int16_t target_page = current_page;

        if (sgl_abs(delta) > CHANGE_PAGE_THRESHOLD) {
            if (delta < 0)
                target_page++;
            else
                target_page--;
        }

        target_page = sgl_clamp(target_page, 0, launcher->page_count - 1);
        current_page = target_page;
        int16_t target = -(target_page * launcher->page_width) - obj->coords.x1;
        sgl_anim_apply_obj_hori(obj, target, 200, SGL_ANIM_PATH_EASE_OUT);
    } break;

    case SGL_EVENT_DESTROYED:
        sgl_obj_delete(launcher->statusbar);
    break;
    default: break;
    }
}

/**
 * @brief create a launcher object
 * @param parent parent object
 * @return launcher object
 */
sgl_obj_t *sgl_launcher_create(sgl_obj_t *parent)
{
    sgl_launcher_t *launcher = sgl_malloc(sizeof(sgl_launcher_t));
    if(launcher == NULL) {
        SGL_LOG_ERROR("sgl_launcher_create: malloc failed");
        return NULL;
    }
    /* set object all member to zero */
    memset(launcher, 0, sizeof(sgl_launcher_t));

    sgl_obj_t *obj = &launcher->obj;
    sgl_obj_init(&launcher->obj, parent);
    obj->construct_fn = sgl_launcher_construct_cb;
    sgl_obj_set_movable(obj);

    launcher->margin_left = 30;
    launcher->margin_top = 40;
    launcher->margin_right = 30;
    launcher->margin_bottom = 60;
    launcher->grid_col = 3;
    launcher->grid_row = 4;
    launcher->icon_size = 48;
    launcher->page_count = 1;
    launcher->page_width = SGL_SCREEN_WIDTH;
    launcher->page_height = SGL_SCREEN_HEIGHT;
    launcher->navigbar_color = SGL_COLOR_WHEAT;
    launcher->font = sgl_get_system_font();

    launcher->statusbar = sgl_statusbar_create(NULL);
    if (launcher->statusbar == NULL) {
        SGL_LOG_ERROR("sgl_launcher_create: statusbar create failed");
        return NULL;
    }
    sgl_obj_set_pos(launcher->statusbar, 0, -22);
    sgl_obj_set_size(launcher->statusbar, SGL_SCREEN_WIDTH, 22);
    sgl_anim_apply_obj_vert(launcher->statusbar, 22, 500, SGL_ANIM_PATH_EASE_OUT_BOUNCE);
    return obj;
}

/**
 * @brief get launcher statusbar
 * @param launcher the launcher object
 * @return statusbar object
 */
sgl_obj_t* sgl_launcher_statusbar(sgl_obj_t *launcher)
{
    sgl_launcher_t *launcher_obj = (sgl_launcher_t *)launcher;
    return launcher_obj->statusbar;
}

/**
 * @brief set launcher font
 * @param launcher the launcher object
 * @param font the font
 * @return none
 */
void sgl_launcher_set_font(sgl_obj_t *launcher, const sgl_font_t *font)
{
    sgl_launcher_t *launcher_obj = (sgl_launcher_t *)launcher;
    launcher_obj->font = font;
}

/**
 * @brief set launcher margin
 * @param launcher the launcher object
 * @param left left margin
 * @param top top margin
 * @param right right margin
 * @param bottom bottom margin
 * @return none
 */
void sgl_launcher_set_margin(sgl_obj_t *launcher, int16_t left, int16_t top, int16_t right, int16_t bottom)
{
    sgl_launcher_t *launcher_obj = (sgl_launcher_t *)launcher;
    launcher_obj->margin_left = left;
    launcher_obj->margin_top = top;
    launcher_obj->margin_right = right;
    launcher_obj->margin_bottom = bottom;
    sgl_obj_set_dirty(launcher);
}

/**
 * @brief set launcher icon size
 * @param launcher the launcher object
 * @param size the icon size
 * @return none
 */
void sgl_launcher_set_icon_size(sgl_obj_t *launcher, int16_t size)
{
    sgl_launcher_t *launcher_obj = (sgl_launcher_t *)launcher;
    launcher_obj->icon_size = size;
}

/**
 * @brief set launcher grid size
 * @param launcher the launcher object
 * @param cols the number of columns
 * @param rows the number of rows
 * @return none
 */
void sgl_launcher_set_grid_size(sgl_obj_t *launcher, int16_t cols, int16_t rows)
{
    sgl_launcher_t *launcher_obj = (sgl_launcher_t *)launcher;
    launcher_obj->grid_col = cols;
    launcher_obj->grid_row = rows;
}

/**
 * @brief add an app to launcher
 * @param launcher the launcher object
 * @param app the app
 * @return none
 */
void sgl_launcher_add_app(sgl_obj_t *launcher, sgl_launcher_app_t *app)
{
    sgl_launcher_t *launcher_obj = (sgl_launcher_t *)launcher;
    int16_t width = 0;
    sgl_obj_t *icon = NULL;

    sgl_obj_t *label = sgl_label_create(launcher);
    if (label == NULL) {
        SGL_LOG_ERROR("sgl_launcher_add_app: label create failed");
        return;
    }

    int16_t cols = launcher_obj->grid_col;
    int16_t rows = launcher_obj->grid_row;
    int16_t launcher_w = launcher_obj->page_width  - launcher_obj->margin_left - launcher_obj->margin_right;
    int16_t launcher_h = launcher_obj->page_height - launcher_obj->margin_top  - launcher_obj->margin_bottom;

    int16_t icon_index = launcher_obj->count % (cols * rows);
    int16_t page_index = launcher_obj->count / (cols * rows);
    int16_t x_ofs = page_index * launcher_obj->page_width;

    int16_t row = icon_index / cols;  /* which row (0-based) */
    int16_t col = icon_index % cols;  /* which col (0-based) */

    int16_t x = launcher_item_pos(launcher_obj->margin_left, launcher_w, cols, launcher_obj->icon_size, col);
    int16_t y = launcher_item_pos(launcher_obj->margin_top,  launcher_h, rows, launcher_obj->icon_size, row);

    if (app->icon->format == SGL_PIXMAP_FMT_RLE_ARGB4444) {
        icon = sgl_sprite_create(launcher);
        if (icon == NULL) {
            SGL_LOG_ERROR("sgl_launcher_add_app: sprite create failed");
            return;
        }
        sgl_sprite_set_pixmap(icon, app->icon);
    }
    else if (app->icon->format == SGL_PIXMAP_FMT_RLE_RGB565){
        icon = sgl_rect_create(launcher);
        if (icon == NULL) {
            SGL_LOG_ERROR("sgl_launcher_add_app: sprite create failed");
            return;
        }
        sgl_rect_set_pixmap(icon, app->icon);
        sgl_rect_set_border_width(icon, 0);
    }

    sgl_obj_set_pos(icon, x + x_ofs, y);
    sgl_obj_set_size(icon, launcher_obj->icon_size, launcher_obj->icon_size);
    sgl_obj_set_clickable(icon);
    sgl_obj_set_event_cb(icon, app->event_cb, app->private_data);

    sgl_obj_set_pos(label, x + x_ofs, y + launcher_obj->icon_size + 2);
    sgl_obj_set_size(label, launcher_obj->icon_size, sgl_font_get_height(launcher_obj->font));
    sgl_label_set_text(label, app->name);
    sgl_label_set_font(label, launcher_obj->font);
    sgl_label_set_text_color(label, launcher_obj->label_color);

    if (page_index >= launcher_obj->page_count) {
        launcher_obj->page_count ++;
        width = launcher_obj->page_width * launcher_obj->page_count;
        sgl_obj_set_width(launcher, width);
    }

    launcher_obj->count++;
    sgl_obj_set_pos_x(launcher, - (current_page * launcher_obj->page_width));
}

/**
 * @brief set launcher label color
 * @param launcher the launcher object
 * @param color the label color
 * @return none
 */
void sgl_launcher_set_label_color(sgl_obj_t *launcher, sgl_color_t color)
{
    sgl_launcher_t *launcher_obj = (sgl_launcher_t *)launcher;
    launcher_obj->label_color = color;
}

/**
 * @brief set launcher navigation bar color
 * @param launcher the launcher object
 * @param color the navigation bar color
 * @return none
 */
void sgl_launcher_set_navigbar_color(sgl_obj_t *launcher, sgl_color_t color)
{
    sgl_launcher_t *launcher_obj = (sgl_launcher_t *)launcher;
    launcher_obj->navigbar_color = color;
}

/**
 * @brief get launcher current page
 * @param launcher the launcher object
 * @return current page
 */
int16_t sgl_launcher_get_current_page(sgl_obj_t *launcher)
{
    SGL_UNUSED(launcher);
    return current_page;
}

/**
 * @brief set launcher current page
 * @param launcher the launcher object
 * @param page the page to set
 * @return none
 */
void sgl_launcher_set_current_page(sgl_obj_t *launcher, int16_t page)
{
    sgl_launcher_t *launcher_obj = (sgl_launcher_t *)launcher;
    if (page < 0 || page >= launcher_obj->page_count) return;
    current_page = page;
    sgl_obj_set_pos_x(launcher, - (page * launcher_obj->page_width));
}
