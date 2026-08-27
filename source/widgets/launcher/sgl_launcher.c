#include "sgl_launcher.h"
#include <sgl.h>

#define CHANGE_PAGE_THRESHOLD  (5)

static sgl_launcher_ctx_t ctx;

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
            int16_t cur_page = ctx.current_page;

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
        int16_t target_page = ctx.current_page;

        if (sgl_abs(delta) > CHANGE_PAGE_THRESHOLD) {
            if (delta < 0)
                target_page++;
            else
                target_page--;
        }

        target_page = sgl_clamp(target_page, 0, launcher->page_count - 1);
        ctx.current_page = target_page;
        int16_t target = -(target_page * launcher->page_width) - obj->coords.x1;
        sgl_anim_move_obj_hori(obj, target, 200, SGL_ANIM_PATH_EASE_OUT);
    } break;

    break;
    default: break;
    }
}

/**
 * @brief create a launcher object
 * @param parent parent object
 * @return launcher object
 */
static sgl_launcher_t *sgl_launcher_alloc(const sgl_launcher_attr_t *attr)
{
    sgl_launcher_t *launcher = sgl_malloc(sizeof(sgl_launcher_t));
    if(launcher == NULL) {
        SGL_LOG_ERROR("sgl_launcher_alloc: malloc failed");
        return NULL;
    }
    /* set object all member to zero */
    memset(launcher, 0, sizeof(sgl_launcher_t));

    sgl_obj_t *obj = &launcher->obj;
    sgl_obj_init(&launcher->obj, NULL);
    obj->construct_fn = sgl_launcher_construct_cb;
    sgl_obj_set_movable(obj);

    launcher->attr = attr;
    launcher->page_count = 1;
    launcher->page_width = SGL_SCREEN_WIDTH;
    launcher->page_height = SGL_SCREEN_HEIGHT;
    launcher->navigbar_color = SGL_COLOR_WHEAT;
    launcher->font = sgl_get_system_font();
    return launcher;
}

/**
 * @brief msgbox event callback
 * @param evt event
 * @return none
 */
static void launcher_msgbox_cb(sgl_event_t *evt)
{
    if (sgl_obj_is_destroyed(evt->obj) && strcmp(sgl_msgbox_get_current_btn(evt->obj), "YES") == 0) {
        sgl_obj_delete(NULL);
        if (ctx.app_exit_cb) {
            ctx.app_exit_cb();
        }
        sgl_launcher_create(ctx.attr, ctx.font, ctx.apps, ctx.app_count, ctx.cb);
    }
}

/**
 * @brief launcher exit event handler with msgbox
 * @param evt event
 * @return none
 */
void sgl_launcher_exit_msgbox_cb(sgl_event_t *evt)
{
    /* ctx.font is only set once a launcher exists; apps that register
     * this handler but run without a launcher must not crash on it */
    if (evt->type == SGL_EVENT_LONG_CLICKED && ctx.font != NULL) {
        sgl_obj_t *msgbox = sgl_msgbox_create(NULL);
        sgl_obj_set_size(msgbox, SGL_SCREEN_WIDTH * 2 / 3, SGL_SCREEN_HEIGHT / 4);
        sgl_obj_set_pos_align(msgbox, SGL_ALIGN_CENTER);
        sgl_msgbox_set_font(msgbox, ctx.font);
        sgl_msgbox_set_border_width(msgbox, 0);
        sgl_msgbox_set_alpha(msgbox, SGL_ALPHA_MAX);
        sgl_msgbox_set_color(msgbox, sgl_rgb(109, 125, 210));
        sgl_msgbox_set_title_text(msgbox, "Message Information");
        sgl_msgbox_set_msg_text(msgbox, "Are you sure you want to exit?");
        sgl_obj_set_event_cb(msgbox, launcher_msgbox_cb, evt->obj);
    }
}

/**
 * @brief launcher start event handler
 * @param evt event
 * @return none
 */
static void launcher_start_event(sgl_event_t *evt)
{
    sgl_launcher_app_t *app = evt->event_data;
    void (*start)(void *private_data);
    void *private_data = NULL;

    if (evt->type == SGL_EVENT_CLICKED) {
        start = app->start;
        private_data = app->private_data;
        ctx.app_exit_cb = app->exit;
        sgl_obj_delete(NULL);
        start(private_data);
    }
}

/**
 * @brief add an app to launcher
 * @param launcher the launcher object
 * @param app the app
 * @return 0 on success, -1 on failure
 */
static int sgl_launcher_add_app(sgl_obj_t *launcher, const sgl_launcher_app_t *app)
{
    sgl_launcher_t *launcher_obj = (sgl_launcher_t *)launcher;
    int16_t width = 0;
    sgl_obj_t *icon = NULL;

    sgl_obj_t *label = sgl_label_create(launcher);
    if (label == NULL) {
        SGL_LOG_ERROR("sgl_launcher_add_app: label create failed");
        return -1;
    }

    int16_t cols = launcher_obj->attr->grid_col;
    int16_t rows = launcher_obj->attr->grid_row;
    int16_t launcher_w = launcher_obj->page_width  - launcher_obj->attr->margin_left - launcher_obj->attr->margin_right;
    int16_t launcher_h = launcher_obj->page_height - launcher_obj->attr->margin_top  - launcher_obj->attr->margin_bottom;

    int16_t icon_index = launcher_obj->app_count % (cols * rows);
    int16_t page_index = launcher_obj->app_count / (cols * rows);
    int16_t x_ofs = page_index * launcher_obj->page_width;

    int16_t row = icon_index / cols;  /* which row (0-based) */
    int16_t col = icon_index % cols;  /* which col (0-based) */

    int16_t x = launcher_item_pos(launcher_obj->attr->margin_left, launcher_w, cols, launcher_obj->attr->icon_size, col);
    int16_t y = launcher_item_pos(launcher_obj->attr->margin_top,  launcher_h, rows, launcher_obj->attr->icon_size, row);

    if (app->icon->format == SGL_PIXMAP_FMT_ARGB4444) {
        icon = sgl_sprite_create(launcher);
        if (icon == NULL) {
            SGL_LOG_ERROR("sgl_launcher_add_app: sprite create failed");
            return -1;
        }
        sgl_sprite_set_pixmap(icon, app->icon);
    }
    else if (app->icon->format == SGL_PIXMAP_FMT_RGB565){
        icon = sgl_rect_create(launcher);
        if (icon == NULL) {
            SGL_LOG_ERROR("sgl_launcher_add_app: sprite create failed");
            return -1;
        }
        sgl_rect_set_pixmap(icon, app->icon);
        sgl_rect_set_border_width(icon, 0);
        sgl_rect_set_radius(icon, app->radius);
    }

    sgl_obj_set_pos(icon, x + x_ofs, y);
    sgl_obj_set_size(icon, launcher_obj->attr->icon_size, launcher_obj->attr->icon_size);
    sgl_obj_set_clickable(icon);
    sgl_obj_set_event_cb(icon, launcher_start_event, (void*)app);

    sgl_obj_set_pos(label, x + x_ofs, y + launcher_obj->attr->icon_size + 2);
    sgl_obj_set_size(label, launcher_obj->attr->icon_size, sgl_font_get_height(launcher_obj->font));
    sgl_label_set_font(label, launcher_obj->font);
    sgl_label_set_text(label, app->name);
    sgl_label_set_text_color(label, launcher_obj->label_color);

    if (page_index >= launcher_obj->page_count) {
        launcher_obj->page_count ++;
        width = launcher_obj->page_width * launcher_obj->page_count;
        sgl_obj_set_width(launcher, width);
    }

    launcher_obj->app_count++;
    sgl_obj_set_pos_x(launcher, - (ctx.current_page * launcher_obj->page_width));
    return 0;
}

/**
 * @brief create launcher
 * @param attr the launcher attributes
 * @param label_font the label font
 * @param apps the apps
 * @param app_num the number of apps
 * @param cb the callback function
 * @return the launcher object
 */
sgl_obj_t *sgl_launcher_create(const sgl_launcher_attr_t *attr, const sgl_font_t *label_font, const sgl_launcher_app_t *apps, int16_t app_num, void (*cb)(sgl_launcher_t *launcher))
{
    sgl_launcher_t *launcher = sgl_launcher_alloc(attr);
    if (launcher == NULL) {
        SGL_LOG_ERROR("sgl_launcher_create: launcher create failed");
        return NULL;
    }

    sgl_obj_t *obj = &launcher->obj;
    ctx.font = label_font;
    ctx.apps = apps;
    ctx.attr = attr;
    ctx.app_count = app_num;
    ctx.cb = cb;

    sgl_obj_set_pos(obj, 0, 0);
    sgl_obj_set_size(obj, SGL_SCREEN_WIDTH, SGL_SCREEN_HEIGHT);
    launcher->font = label_font;

    for (int i = 0; i < app_num; i++) {
        sgl_launcher_add_app(obj, &apps[i]);
    }

    if (cb) {
        cb(launcher);
    }

    return obj;
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
    return ctx.current_page;
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
    ctx.current_page = page;
    sgl_obj_set_pos_x(launcher, - (page * launcher_obj->page_width));
}
