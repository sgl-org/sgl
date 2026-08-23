/* source/core/sgl_misc.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL  
 * Document reference link: https://sgl-docs.readthedocs.io
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sgl.h>
#include <sgl_draw.h>
#include <sgl_theme.h>

#if (CONFIG_SGL_BOOT_LOGO)
typedef struct sgl_logo {
    sgl_obj_t       obj;
    uint8_t         alpha;
} sgl_logo_t;

static void sgl_logo_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_logo_t *logo = (sgl_logo_t*)obj;
    const int16_t w = obj->coords.x2 - obj->coords.x1 + 1;
    const int16_t h = obj->coords.y2 - obj->coords.y1 + 1;

    #define NORMALIZATION_FACTOR    1024
    #define rel_value(v)            ((v) * w / NORMALIZATION_FACTOR)
    #define pos_x_val(x)            ((x) * w / NORMALIZATION_FACTOR + obj->coords.x1)
    #define pos_y_val(y)            ((y) * h / NORMALIZATION_FACTOR + obj->coords.y1)

    sgl_area_t rect = {
        .x1 = pos_x_val(176),
        .y1 = pos_y_val(176),
        .x2 = pos_x_val(848),
        .y2 = pos_y_val(848),
    };

    const int16_t pin_w = 60;
    const int16_t pin_gap = 66;

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        sgl_draw_fill_rect_border(surf, &obj->area, &rect,
                                  rel_value(40), SGL_COLOR_BLUE, rel_value(51), logo->alpha);

        rect = (sgl_area_t){
            .x1 = pos_x_val(369),
            .y1 = pos_y_val(340),
            .x2 = pos_x_val(655),
            .y2 = pos_y_val(490),
        };
        sgl_draw_fill_rect_border(surf, &obj->area, &rect,
                                  rel_value(75), SGL_COLOR_BLUE, rel_value(20), logo->alpha);
        rect = (sgl_area_t){
            .x1 = pos_x_val(369),
            .y1 = pos_y_val(539),
            .x2 = pos_x_val(655),
            .y2 = pos_y_val(684),
        };
        sgl_draw_fill_rect(surf, &obj->area, &rect,
                                  rel_value(75), SGL_COLOR_BLUE, logo->alpha);

        sgl_draw_fill_circle(surf, &obj->area, pos_x_val(446), pos_y_val(410), rel_value(60), SGL_COLOR_BLUE, logo->alpha);
        sgl_area_t pin_rect;

        for (int i = 0, x = 230; i < 5; i++) {
            pin_rect.x1 = pos_x_val(x);
            pin_rect.y1 = pos_y_val(0);
            pin_rect.x2 = pos_x_val(x + pin_w);
            pin_rect.y2 = pos_y_val(130);
            sgl_draw_fill_rect(surf, &obj->area, &pin_rect, 0, SGL_COLOR_BLUE, logo->alpha);
            x += pin_w + pin_gap;
        }

        for (int i = 0, x = 230; i < 5; i++) {
            pin_rect.x1 = pos_x_val(x);
            pin_rect.y1 = pos_y_val(894);
            pin_rect.x2 = pos_x_val(x + pin_w);
            pin_rect.y2 = pos_y_val(1024);
            sgl_draw_fill_rect(surf, &obj->area, &pin_rect, 0, SGL_COLOR_BLUE, logo->alpha);
            x += pin_w + pin_gap;
        }

        for (int i = 0, y = 230; i < 5; i++) {
            pin_rect.x1 = pos_x_val(0);
            pin_rect.y1 = pos_y_val(y);
            pin_rect.x2 = pos_x_val(130);
            pin_rect.y2 = pos_y_val(y + pin_w);
            sgl_draw_fill_rect(surf, &obj->area, &pin_rect, 0, SGL_COLOR_BLUE, logo->alpha);
            y += pin_w + pin_gap;
        }

        for(int i = 0, y = 230; i < 5; i++) {
            pin_rect.x1 = pos_x_val(894);
            pin_rect.y1 = pos_y_val(y);
            pin_rect.x2 = pos_x_val(1024);
            pin_rect.y2 = pos_y_val(y + pin_w);
            sgl_draw_fill_rect(surf, &obj->area, &pin_rect, 0, SGL_COLOR_BLUE, logo->alpha);
            y += pin_w + pin_gap;
        }
    }
}

sgl_obj_t* sgl_logo_create(sgl_obj_t* parent)
{
    sgl_logo_t *logo = sgl_malloc(sizeof(sgl_logo_t));
    if (logo == NULL) {
        SGL_LOG_ERROR("sgl_logo_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(logo, 0, sizeof(sgl_logo_t));

    sgl_obj_t *obj = &logo->obj;
    sgl_obj_init(&logo->obj, parent);
    obj->construct_fn = sgl_logo_construct_cb;
    logo->alpha = SGL_ALPHA_MAX;
    sgl_obj_set_border_width(obj, 0);
    return obj;
}

void sgl_logo_set_alpha(sgl_obj_t* obj, uint8_t alpha)
{
    sgl_logo_t *logo = (sgl_logo_t*)obj;
    logo->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

void sgl_logo_anim(sgl_anim_t *anim, int32_t value)
{
    sgl_obj_t *logo = (sgl_obj_t*)anim->data;
    sgl_logo_set_alpha(logo, value);
}

/**
 * @brief to show the sgl logo after sgl init
 * @param none
 * @return none
 * @note: you can call this function in your main function to show the sgl logo
 */
void sgl_boot_logo(void)
{
    sgl_obj_t *logo = sgl_logo_create(NULL);
    const int16_t logo_size = sgl_min(SGL_SCREEN_WIDTH, SGL_SCREEN_HEIGHT) * 30 / 100;
    sgl_obj_set_size(logo, logo_size, logo_size);
    sgl_obj_set_pos_align(logo, SGL_ALIGN_CENTER);
    sgl_obj_set_radius(logo, 0);

    sgl_anim_t *anim = sgl_anim_create();
    sgl_anim_set_data(anim, logo);
    sgl_anim_set_act_duration(anim, 1000);
    sgl_anim_set_start_value(anim, SGL_ALPHA_MAX);
    sgl_anim_set_end_value(anim, SGL_ALPHA_MIN);
    sgl_anim_set_path(anim, sgl_logo_anim, SGL_ANIM_PATH_LINEAR);
    sgl_anim_start(anim, SGL_ANIM_REPEAT_ONCE);

    while (!sgl_anim_is_finished(anim)) {
        sgl_task_handler();
    }

    sgl_anim_delete(anim);
    sgl_obj_delete_sync(logo);
}

#endif // !CONFIG_SGL_BOOT_LOGO


#if (CONFIG_SGL_MONITOR_TRACE)
static const uint8_t font_bitmap[] = {
    /* U+0025 "%" */
    0x2c, 0xe7, 0x00, 0x42, 0x98, 0x0f, 0x14, 0xe3,
    0x88, 0x1f, 0x2c, 0x20, 0x1b, 0xd6, 0x00, 0x00,
    0x00, 0x00, 0x5d, 0xb0, 0x00, 0xc2, 0xe2, 0x97,
    0x0b, 0x71, 0xe0, 0x6a, 0x6b, 0x00, 0xf1, 0x97,
    0x01, 0x00, 0x6e, 0xc1,

    /* U+002E "." */
    0x07, 0x70, 0x2f, 0xf2, 0x0c, 0xc0,

    /* U+0030 "0" */
    0x00, 0x8e, 0xe8, 0x00, 0x07, 0xe4, 0x4e, 0x70,
    0x0d, 0x70, 0x07, 0xd0, 0x0f, 0x49, 0x93, 0xf0,
    0x1f, 0x3a, 0xa2, 0xf1, 0x0f, 0x40, 0x04, 0xf0,
    0x0d, 0x70, 0x07, 0xd0, 0x06, 0xe5, 0x4e, 0x60,
    0x00, 0x7e, 0xe7, 0x00,

    /* U+0031 "1" */
    0x17, 0xcf, 0x00, 0x02, 0x9c, 0xf0, 0x00, 0x00,
    0x6f, 0x00, 0x00, 0x06, 0xf0, 0x00, 0x00, 0x6f,
    0x00, 0x00, 0x06, 0xf0, 0x00, 0x00, 0x6f, 0x00,
    0x02, 0x38, 0xf3, 0x30, 0xcf, 0xff, 0xff, 0x10,

    /* U+0032 "2" */
    0x03, 0xbe, 0xd6, 0x00, 0x0d, 0x84, 0x9f, 0x40,
    0x00, 0x00, 0x0e, 0x80, 0x00, 0x00, 0x0e, 0x60,
    0x00, 0x00, 0x7e, 0x00, 0x00, 0x04, 0xf4, 0x00,
    0x00, 0x5f, 0x50, 0x00, 0x06, 0xf8, 0x33, 0x30,
    0x1f, 0xff, 0xff, 0xf0,

    /* U+0033 "3" */
    0x03, 0xbe, 0xe8, 0x00, 0xa8, 0x37, 0xf7, 0x00,
    0x00, 0x0d, 0x90, 0x00, 0x05, 0xf3, 0x00, 0x5f,
    0xf5, 0x00, 0x00, 0x36, 0xe7, 0x00, 0x00, 0x08,
    0xe1, 0xd7, 0x46, 0xeb, 0x05, 0xce, 0xea, 0x10,

    /* U+0034 "4" */
    0x00, 0x00, 0xdf, 0x10, 0x00, 0x0a, 0xcf, 0x10,
    0x00, 0x7d, 0x4f, 0x10, 0x03, 0xf2, 0x3f, 0x10,
    0x1e, 0x50, 0x3f, 0x10, 0x7f, 0xff, 0xff, 0xf5,
    0x12, 0x22, 0x5f, 0x40, 0x00, 0x00, 0x3f, 0x10,
    0x00, 0x00, 0x3f, 0x10,

    /* U+0035 "5" */
    0x06, 0xff, 0xff, 0x90, 0x07, 0xe3, 0x33, 0x20,
    0x08, 0xc0, 0x00, 0x00, 0x09, 0xed, 0xe9, 0x10,
    0x03, 0x63, 0x6e, 0xa0, 0x00, 0x00, 0x07, 0xf0,
    0x00, 0x00, 0x08, 0xe0, 0x1e, 0x84, 0x8f, 0x80,
    0x05, 0xbe, 0xe8, 0x00,

    /* U+0036 "6" */
    0x00, 0x3c, 0xfd, 0x60, 0x02, 0xf8, 0x47, 0x60,
    0x0a, 0xa0, 0x00, 0x00, 0x0e, 0x7a, 0xeb, 0x20,
    0x0f, 0xe5, 0x4c, 0xd0, 0x0f, 0x60, 0x04, 0xf1,
    0x0c, 0x90, 0x05, 0xf1, 0x05, 0xf6, 0x4c, 0xb0,
    0x00, 0x6d, 0xfa, 0x10,

    /* U+0037 "7" */
    0x1f, 0xff, 0xff, 0xf2, 0x03, 0x33, 0x3c, 0xb0,
    0x00, 0x00, 0x4e, 0x10, 0x00, 0x00, 0xd6, 0x00,
    0x00, 0x05, 0xf0, 0x00, 0x00, 0x0a, 0xb0, 0x00,
    0x00, 0x0d, 0x80, 0x00, 0x00, 0x0f, 0x60, 0x00,
    0x00, 0x1f, 0x50, 0x00,

    /* U+0038 "8" */
    0x00, 0x9e, 0xea, 0x00, 0x07, 0xe3, 0x3d, 0x80,
    0x09, 0xa0, 0x08, 0xa0, 0x03, 0xe5, 0x0c, 0x40,
    0x00, 0xbe, 0xfc, 0x00, 0x0b, 0x80, 0x3c, 0xb0,
    0x1f, 0x30, 0x04, 0xf1, 0x0e, 0xb3, 0x3b, 0xe0,
    0x02, 0xbe, 0xeb, 0x20,

    /* U+0039 "9" */
    0x02, 0xbf, 0xd6, 0x00, 0x0d, 0xb3, 0x5f, 0x50,
    0x1f, 0x40, 0x07, 0xc0, 0x0f, 0x90, 0x2b, 0xf0,
    0x05, 0xff, 0xd8, 0xf0, 0x00, 0x01, 0x07, 0xe0,
    0x00, 0x00, 0x0c, 0xa0, 0x09, 0x74, 0xaf, 0x20,
    0x05, 0xcf, 0xc3, 0x00,

    /* U+003A ":" */
    0x07, 0x70, 0x2f, 0xf2, 0x0c, 0xc0, 0x00, 0x00,
    0x00, 0x00, 0x07, 0x70, 0x2f, 0xf2, 0x0c, 0xc0,

    /* U+0045 "E" */
    0x9f, 0xff, 0xff, 0x09, 0xe3, 0x33, 0x30, 0x9d,
    0x00, 0x00, 0x09, 0xd0, 0x00, 0x00, 0x9f, 0xff,
    0xf5, 0x09, 0xe3, 0x33, 0x10, 0x9d, 0x00, 0x00,
    0x09, 0xe3, 0x33, 0x30, 0x9f, 0xff, 0xff, 0x10,

    /* U+0046 "F" */
    0x5f, 0xff, 0xff, 0x25, 0xf4, 0x33, 0x30, 0x5f,
    0x10, 0x00, 0x05, 0xf1, 0x00, 0x00, 0x5f, 0xff,
    0xf8, 0x05, 0xf4, 0x33, 0x10, 0x5f, 0x10, 0x00,
    0x05, 0xf1, 0x00, 0x00, 0x5f, 0x10, 0x00, 0x00,

    /* U+004D "M" */
    0x1f, 0xa0, 0x0a, 0xf1, 0x1f, 0xe0, 0x0e, 0xf1,
    0x1f, 0xc4, 0x4c, 0xf1, 0x1f, 0x98, 0x98, 0xf1,
    0x1f, 0x4d, 0xd4, 0xf1, 0x1f, 0x2d, 0xc2, 0xf1,
    0x1f, 0x25, 0x52, 0xf1, 0x1f, 0x20, 0x02, 0xf1,
    0x1f, 0x20, 0x02, 0xf1,

    /* U+0050 "P" */
    0xcf, 0xff, 0xc4, 0x0c, 0xb3, 0x3a, 0xf1, 0xca,
    0x00, 0x2f, 0x4c, 0xa0, 0x18, 0xf1, 0xcf, 0xff,
    0xe5, 0x0c, 0xb2, 0x20, 0x00, 0xca, 0x00, 0x00,
    0x0c, 0xa0, 0x00, 0x00, 0xca, 0x00, 0x00, 0x00,

    /* U+0053 "S" */
    0x00, 0x9e, 0xeb, 0x20, 0x08, 0xf7, 0x5a, 0xa0,
    0x0b, 0xc0, 0x00, 0x00, 0x06, 0xfa, 0x30, 0x00,
    0x00, 0x4d, 0xfb, 0x20, 0x00, 0x00, 0x4d, 0xe0,
    0x01, 0x00, 0x05, 0xf2, 0x0e, 0xb6, 0x6c, 0xe0,
    0x02, 0xae, 0xeb, 0x20
};

static const sgl_font_table_t font_table[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 128, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 36, .adv_w = 128, .box_w = 4, .box_h = 3, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 42, .adv_w = 128, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 78, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 110, .adv_w = 128, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 146, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 178, .adv_w = 128, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 214, .adv_w = 128, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 250, .adv_w = 128, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 286, .adv_w = 128, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 322, .adv_w = 128, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 358, .adv_w = 128, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 394, .adv_w = 128, .box_w = 4, .box_h = 8, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 410, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 442, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 474, .adv_w = 128, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 510, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 542, .adv_w = 128, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0}
};

static const uint16_t unicode_list_0[] = {
    0x00, 0x09, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x20, 0x21, 0x28,
    0x2b, 0x2e
};

static const sgl_font_unicode_t font_unicode[] =
{
    { .offset = 0x25, .len = 18, .list = unicode_list_0, .tab_offset = 1, }
};

const sgl_font_t monitor_font = {
    .bitmap = font_bitmap,
    .table = font_table,
    .font_table_size = SGL_ARRAY_SIZE(font_table),
    .font_height = 9,
    .base_line = 0,
    .bpp = 4,
    .compress = 0,
    .unicode = font_unicode,
    .unicode_num = SGL_ARRAY_SIZE(font_unicode),
};

void sgl_monitor_trace(sgl_surf_t *surf)
{
    static char fps_str[16] = {0};
    static char mem_str[16] = {0};
    static sgl_obj_t *monitor = NULL;
    static sgl_obj_t *fps = NULL;
    static sgl_obj_t *mem = NULL;
    static uint32_t last_tick = 0;
    uint32_t cur_tick = sgl_last_tick_get();

    if (monitor) {
        uint32_t tick_used = cur_tick - last_tick;

        if (tick_used >= SGL_SYSTEM_TICK_MS) {
            uint32_t fps_count = 1000 / tick_used;
            last_tick = cur_tick;

            sgl_snprintf(fps_str, sizeof(fps_str), "FPS:%d", fps_count);
            sgl_snprintf(mem_str, sizeof(mem_str), "MEM:%d.%d%", sgl_mm_get_monitor().used_rate >> 8, sgl_mm_get_monitor().used_rate & 0xff);
        }

#if (CONFIG_SGL_FBDEV_RUNTIME_ROTATION)
        if ((monitor->coords.y2 + 1) == SGL_SCREEN_WIDTH || (monitor->coords.x2 + 1) == SGL_SCREEN_HEIGHT) {
            sgl_obj_delete(monitor);
            monitor = NULL;
            return;
        } 
#endif
        /* update monitor page */
        sgl_event_t evt = {0};
        sgl_obj_t *child;

        evt.type = SGL_EVENT_DRAW_MAIN;
        if (sgl_surf_area_is_overlap(surf, &monitor->area)) {
            monitor->construct_fn(surf, monitor, &evt);
        }

        /* update all child of monitor page */
        sgl_obj_for_each_child(child, monitor) {
            if (sgl_surf_area_is_overlap(surf, &child->area)) {
                child->construct_fn(surf, child, &evt);
            }
        }
    }
    else {
        monitor = sgl_obj_create(NULL);
        sgl_obj_set_pos(monitor, SGL_MONITOR_COORDS_X, SGL_MONITOR_COORDS_Y);
        sgl_obj_set_size(monitor, SGL_MONITOR_COORDS_WIDTH, SGL_MONITOR_COORDS_HEIGHT);
        monitor->area = monitor->coords;
        sgl_page_set_color(monitor, SGL_MONITOR_COLOR);
        sgl_page_set_alpha(monitor, SGL_MONITOR_ALPHA);

        fps = sgl_label_create(monitor);
        sgl_obj_set_pos(fps, 0, 0);
        sgl_obj_set_size(fps, SGL_MONITOR_COORDS_WIDTH, SGL_MONITOR_COORDS_HEIGHT / 2);
        fps->area = monitor->coords;
        sgl_label_set_font(fps, &monitor_font);
        sgl_label_set_text_align(fps, SGL_ALIGN_LEFT_MID);
        sgl_label_set_text_buffer(fps, fps_str, sizeof(fps_str));
        sgl_label_set_text_color(fps, SGL_MONITOR_TEXT_COLOR);

        mem = sgl_label_create(monitor);
        sgl_obj_set_pos(mem, 0, SGL_MONITOR_COORDS_HEIGHT / 2);
        sgl_obj_set_size(mem, SGL_MONITOR_COORDS_WIDTH, SGL_MONITOR_COORDS_HEIGHT / 2);
        mem->area = monitor->coords;
        sgl_label_set_font(mem, &monitor_font);
        sgl_label_set_text_align(mem, SGL_ALIGN_LEFT_MID);
        sgl_label_set_text_buffer(mem, mem_str, sizeof(mem_str));
        sgl_label_set_text_color(mem, SGL_MONITOR_TEXT_COLOR);
    }
}
#endif

/**
 * @brief Count number of options in \n-separated text
 * @param text newline-separated option string
 * @return number of options
 */
uint16_t sgl_string_option_get_count(const char *text)
{
    if (!text || !*text) return 0;
    uint16_t count = 0;
    const char *p = text;
    while (*p) {
        if (*p == '\n') count++;
        p++;
    }
    if (p > text && *(p - 1) != '\n') count++;
    return count;
}

/**
 * @brief Get byte offset of the Nth option in \n-separated text
 * @param text newline-separated option string
 * @param index zero-based option index
 * @return byte offset, or -1 if out of range
 */
int sgl_string_option_get_offset(const char *text, int index)
{
    if (!text) return -1;
    int cur = 0;
    const char *p = text;
    while (cur < index) {
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        else return -1;
        cur++;
    }
    return (int)(p - text);
}

/**
 * @brief Get text length of one option at given byte offset
 * @param text option string
 * @param offset byte offset of the option
 * @return length (stops at \n or \0)
 */
int sgl_string_option_get_text_len(const char *text, int offset)
{
    const char *p = text + offset;
    int len = 0;
    while (*p && *p != '\n') {
        len++;
        p++;
    }
    return len;
}

/**
 * @brief Reset scroll state (used on init / re-binding data)
 * @param sc scroll state
 * @return none
 * @note zeroes the whole struct then restores the resident scrollbar alpha
 */
void sgl_scroll_reset(sgl_scroll_t *sc)
{
    memset(sc, 0, sizeof(sgl_scroll_t));
    sc->bar_alpha = 128;
}

/**
 * @brief Feed a press event: freeze coasting and start tracking this press sequence
 * @param sc scroll state
 * @param coord main-axis touch coordinate
 * @return none
 * @note stops any running inertia immediately (overscroll stays frozen until
 *       release), resets the speed window and anchors grab/prev coordinates
 */
void sgl_scroll_press(sgl_scroll_t *sc, int16_t coord)
{
    sc->coasting = 0U;
    sc->speed = 0;
    sc->dragged = 0U;
    sc->win_dist = 0;
    sc->touching = 1U;
    sc->grab_coord = coord;
    sc->prev_coord = coord;
    sc->win_tick = (uint16_t)sgl_tick_get();
}

/**
 * @brief Soft clamp: allow overscroll for rubber-band effect
 * @param offset scroll offset
 * @param range maximum scroll distance
 * @return clamped scroll offset
 */
static int32_t sgl_scroll_soft_limit(int32_t offset, int32_t range)
{
    if (offset < -(int32_t)SGL_SCROLL_OVERSCROLL)
        offset = -(int32_t)SGL_SCROLL_OVERSCROLL;
    if (offset > range + (int32_t)SGL_SCROLL_OVERSCROLL)
        offset = range + (int32_t)SGL_SCROLL_OVERSCROLL;
    return offset;
}

/**
 * @brief Feed a move event: drag-start check + incremental follow + window speed sampling
 * @param sc scroll state
 * @param coord main-axis touch coordinate
 * @param range current scroll upper limit (content height - viewport height)
 * @return 0 = not started yet; 1 = threshold just crossed this frame
 *         (caller should cancel the pressed highlight); 2 = dragging in progress.
 * @note follows by per-frame delta (offset -= coord - prev_coord) with
 *       rubber-band soft clamp; speed is sampled as displacement/time over a
 *       VEL_WINDOW_MS window, independent of the event rate
 */
uint8_t sgl_scroll_stay(sgl_scroll_t *sc, int16_t coord, int32_t range)
{
    int16_t d = (int16_t)(coord - sc->prev_coord);
    uint8_t just_started = 0U;

    if (!sc->touching)
        return 0U;

    if (!sc->dragged) {
        /* startup check: total displacement since press vs threshold */
        int16_t span = (int16_t)(coord - sc->grab_coord);
        int16_t mag = (span >= 0) ? span : (int16_t)(-span);

        if (mag < SGL_SCROLL_DRAG_THRESHOLD)
            return 0U;
        sc->dragged = 1U;
        just_started = 1U;
    }

    sc->offset = sgl_scroll_soft_limit(sc->offset - (int32_t)d, range);
    if (d != 0) {
        /* velocity sampling: distance over a fixed time window, independent
         * of the event rate */
        uint16_t now = (uint16_t)sgl_tick_get();
        uint16_t span_ms = (uint16_t)(now - sc->win_tick);

        sc->win_dist = (int16_t)(sc->win_dist + d);
        if (span_ms >= SGL_SCROLL_VEL_WINDOW_MS) {
            sc->speed = (int16_t)(-(int32_t)sc->win_dist * 16 / (int32_t)span_ms);
            sc->win_dist = 0;
            sc->win_tick = now;
        }
    }
    sc->prev_coord = coord;
    return just_started ? 1U : 2U;
}

/**
 * @brief Feed a release event: settle the final speed and decide whether inertia/rebound is needed
 * @param sc scroll state
 * @param range current scroll upper limit (content height - viewport height)
 * @return non-zero = animation needed (caller then invokes sgl_scroll_anim_start); 0 = at rest.
 * @note the final speed comes from the still-open measurement window; if the
 *       pointer has been still for more than one window the release is
 *       treated as parked (speed = 0). Out-of-range releases always animate
 *       so the content snaps back
 */
uint8_t sgl_scroll_release(sgl_scroll_t *sc, int32_t range)
{
    uint8_t was_drag = sc->dragged;
    uint8_t out_of_range = (sc->offset < 0 || sc->offset > range) ? 1U : 0U;
    uint16_t since_win = (uint16_t)((uint16_t)sgl_tick_get() - sc->win_tick);

    /* final speed: settle the open window; treat as parked when the pointer
     * has been still for more than one window */
    if (since_win > SGL_SCROLL_VEL_WINDOW_MS) {
        sc->speed = 0;
    } else if (sc->win_dist != 0) {
        /* normalize over the measured span itself; the floor only guards
         * against div-by-zero / single-tick noise, a 16ms floor would
         * under-estimate the launch speed of quick flicks */
        if (since_win < 4U)
            since_win = 4U;
        sc->speed = (int16_t)(-(int32_t)sc->win_dist * 16 / (int32_t)since_win);
    }

    sc->touching = 0U;
    sc->dragged = 0U;
    sc->win_dist = 0;

    if ((was_drag && sc->speed != 0) || out_of_range) {
        sc->coasting = 1U;
        return 1U;
    }
    sc->speed = 0;
    return 0U;
}

/**
 * @brief Rubber-band pull back toward the nearest bound
 * @param offset scroll offset (out of range)
 * @param range maximum scroll distance
 * @return offset moved one easing step toward [0, range]
 */
static int32_t sgl_scroll_snap_back(int32_t offset, int32_t range)
{
    int32_t over = (offset < 0) ? -offset : offset - range;
    int32_t step = over / SGL_SCROLL_REBOUND_PULL_DIV;

    if (step < 1)
        step = 1;
    if (step > SGL_SCROLL_REBOUND_MAX_STEP)
        step = SGL_SCROLL_REBOUND_MAX_STEP;
    return (offset < 0) ? (offset + step) : (offset - step);
}

/**
 * @brief Inertia/rebound step (driven by sgl_scroll_anim_step_cb)
 * @param sc scroll state
 * @param elapsed_ms elapsed time since the previous step
 * @param range current scroll upper limit (content height - viewport height)
 * @return non-zero = offset changed; coasting is cleared automatically on settle.
 * @note phase 1 glides by speed * elapsed / 16 and decays speed by NUM/DEN
 *       once per 16ms slice (halved again while overscrolled); phase 2 eases
 *       the offset back into [0, range]. elapsed_ms is clamped to 64ms to
 *       bound the per-frame jump
 */
uint8_t sgl_scroll_anim_step(sgl_scroll_t *sc, uint16_t elapsed_ms, int32_t range)
{
    int32_t next;
    uint16_t step_ms;

    if (!sc->coasting || elapsed_ms == 0U)
        return 0U;

    step_ms = (elapsed_ms > 64U) ? 64U : elapsed_ms;
    next = sc->offset;

    /* phase 1: glide with 7/8 decay per 16ms slice */
    if (sc->speed != 0) {
        int32_t delta = ((int32_t)sc->speed * (int32_t)step_ms) / 16;
        uint16_t slices = (uint16_t)(step_ms >> 4);

        if (delta == 0)
            delta = (sc->speed > 0) ? 1 : -1; /* keep crawling at low speed */
        next = sgl_scroll_soft_limit(next + delta, range);

        while (slices--)
            sc->speed = (int16_t)(((int32_t)sc->speed * SGL_SCROLL_INERTIA_NUM) / SGL_SCROLL_INERTIA_DEN);
        if (next < 0 || next > range)
            sc->speed = (int16_t)(sc->speed / 2); /* shed speed faster in overscroll */
    }

    /* phase 2: settle back into bounds */
    if (next < 0 || next > range)
        next = sgl_scroll_snap_back(next, range);

    if (sc->speed == 0 && next >= 0 && next <= range)
        sc->coasting = 0U;

    if (next == sc->offset)
        return 0U;
    sc->offset = next;
    return 1U;
}

/**
 * @brief Wake the scrollbar (call on scroll value change / data binding)
 * @param sc scroll state
 * @return none
 * @note restores the active alpha and restarts the idle hold timer
 */
void sgl_scroll_bar_wake(sgl_scroll_t *sc)
{
    sc->bar_idle = 0U;
    sc->bar_alpha = SGL_SCROLL_BAR_ACTIVE_ALPHA;
}

/**
 * @brief Scrollbar fade-out step
 * @param sc scroll state
 * @param elapsed_ms elapsed time since the previous step
 * @return non-zero = alpha changed; always returns 0 once the resident value is reached.
 * @note holds full opacity for BAR_IDLE_MS first, then decreases FADE_STEP
 *       per 16ms slice down to the resident alpha
 */
uint8_t sgl_scroll_bar_step(sgl_scroll_t *sc, uint16_t elapsed_ms)
{
    uint16_t slices;
    uint16_t dec;

    if (sc->bar_alpha <= SGL_SCROLL_BAR_RESIDENT_ALPHA)
        return 0U;

    /* hold at full opacity until the idle grace period is consumed */
    if (sc->bar_idle < SGL_SCROLL_BAR_IDLE_MS) {
        uint16_t remaining = (uint16_t)(SGL_SCROLL_BAR_IDLE_MS - sc->bar_idle);

        if (elapsed_ms < remaining) {
            sc->bar_idle = (uint16_t)(sc->bar_idle + elapsed_ms);
            return 0U;
        }
        sc->bar_idle = SGL_SCROLL_BAR_IDLE_MS;
        elapsed_ms = (uint16_t)(elapsed_ms - remaining);
        if (elapsed_ms == 0U)
            return 0U;
    }

    /* fade one alpha step per 16ms slice, floor at the resident value */
    slices = (uint16_t)(elapsed_ms / 16U);
    if (slices == 0U)
        slices = 1U;
    dec = (uint16_t)(slices * (uint16_t)SGL_SCROLL_BAR_FADE_STEP);
    if ((uint16_t)sc->bar_alpha > (uint16_t)SGL_SCROLL_BAR_RESIDENT_ALPHA + dec)
        sc->bar_alpha = (uint8_t)(sc->bar_alpha - dec);
    else
        sc->bar_alpha = SGL_SCROLL_BAR_RESIDENT_ALPHA;

    return 1U;
}

/**
 * @brief Scroll animation step callback (used as the path_cb of sgl_anim)
 * @param anim animation node whose data pointer references the scroll state
 * @param value monotonic elapsed time produced by sgl_anim_path_linear
 * @return none
 * @note advances the physics and scrollbar fade on a >=16ms cadence, commits
 *       changes through sc->commit and stops the node on settle (released by
 *       the animation task together with auto_free). Stops immediately if
 *       the widget cleared sc->commit
 */
void sgl_scroll_anim_step_cb(sgl_anim_t *anim, int32_t value)
{
    sgl_scroll_t *sc = (sgl_scroll_t *)anim->data;
    uint16_t elapsed;
    uint8_t changed = 0U;

    SGL_ASSERT(sc != NULL);
    if (sc->commit == NULL) {
        sc->anim = NULL;
        sgl_anim_stop(anim);
        return;
    }

    elapsed = (uint16_t)((int32_t)value - (int32_t)sc->step_tick);
    if (elapsed < 16U)
        return;
    sc->step_tick = (uint16_t)value;

    if (sc->coasting) {
        if (sgl_scroll_anim_step(sc, elapsed, sc->range))
            changed = 1U;
    }

    if (sc->bar_alpha > SGL_SCROLL_BAR_RESIDENT_ALPHA) {
        if (sgl_scroll_bar_step(sc, elapsed))
            changed = 1U;
    }

    if (changed && sc->commit)
        sc->commit(sc);

    if (!sc->coasting && sc->bar_alpha <= SGL_SCROLL_BAR_RESIDENT_ALPHA) {
        sc->anim = NULL;
        sgl_anim_stop(anim);
    }
}

/**
 * @brief Start the inertia/rebound + scrollbar fade-out animation (shared by all widgets)
 * @param sc scroll state
 * @return none
 * @note creates the animation node dynamically (attached to sc->anim) and
 *       starts it with SGL_ANIM_REPEAT_LOOP; stopped and released
 *       automatically on settle (coasting finished and scrollbar faded to
 *       the resident value). Any previously running node is stopped first.
 *       The caller must set sc->commit beforehand
 */
void sgl_scroll_anim_start(sgl_scroll_t *sc)
{
    sgl_anim_t *anim;

    if (sc->anim != NULL)
        sgl_scroll_anim_stop(sc);

    anim = sgl_anim_create();
    if (anim == NULL)
        return;

    sc->anim = anim;
    /* pre-charge step_tick so the first path_cb call (value ~= 1) already
     * sees elapsed >= 16ms and steps immediately, avoiding a 16ms frozen
     * gap between release and the first inertia frame (uint16 wrap is
     * intentional: elapsed = value - 0xFFF0 ~= value + 16) */
    sc->step_tick = (uint16_t)(0 - 16);

    sgl_anim_set_data(anim, sc);
    /* linear path with start=0/end=0x7FFF yields value == elaps while
     * elaps < duration, so path_cb fires every frame with a monotonic tick */
    sgl_anim_set_path(anim, sgl_scroll_anim_step_cb, sgl_anim_path_linear);
    sgl_anim_set_start_value(anim, 0);
    sgl_anim_set_end_value(anim, 0x7FFF);
    sgl_anim_set_act_duration(anim, 0x7FFF);
    sgl_anim_set_auto_free(anim);
    sgl_anim_start(anim, SGL_ANIM_REPEAT_LOOP);
}

/**
 * @brief Stop and release the scroll animation node early (used on widget
 *        destroy/collapse; a no-op when no animation is running)
 * @param sc scroll state
 * @return none
 */
void sgl_scroll_anim_stop(sgl_scroll_t *sc)
{
    if (sc->anim != NULL) {
        sgl_anim_delete(sc->anim);
        sc->anim = NULL;
    }
}

/**
 * @brief Draw the right-hand vertical scrollbar (called in the widget DRAW_MAIN)
 * @param surf drawing surface
 * @param obj widget object (provides x coordinates, border and corner radius)
 * @param sc scroll state (reads offset / bar_alpha)
 * @param range current scroll upper limit; nothing is drawn when range<=0 (no scrollable content)
 * @param viewport vertical track extent (y1/y2 of the scrollable list area,
 *                 which may start below the widget top, e.g. under a dropdown header)
 * @param color scrollbar color
 * @return none
 * @note thumb height is proportional to viewport / content height (min 8px);
 *       thumb position maps offset into the track; drawn with the theme
 *       scroll foreground color at bar_alpha opacity
 */
void sgl_scroll_draw_bar(sgl_surf_t *surf, sgl_obj_t *obj, const sgl_scroll_t *sc,
                         int32_t range, const sgl_area_t *viewport, sgl_color_t color)
{
    int viewport_h;
    int thumb_h;
    int thumb_y;
    int bar_x1;
    int bar_x2;
    int v_margin;
    sgl_area_t thumb;

    if (range <= 0)
        return;

    viewport_h = viewport->y2 - viewport->y1 + 1;
    if (viewport_h + (int)range <= 0)
        return;

    v_margin = (int)obj->border + 1;
    if (v_margin * 2 >= viewport_h)
        v_margin = viewport_h / 2 - 1;
    if (v_margin < 1)
        v_margin = 1;

    thumb_h = viewport_h * viewport_h / (viewport_h + (int)range);
    if (thumb_h < 8)
        thumb_h = 8;
    if (thumb_h > viewport_h - 2 * v_margin)
        thumb_h = viewport_h - 2 * v_margin;
    if (thumb_h < 1)
        thumb_h = 1;

    thumb_y = viewport->y1 + v_margin
              + (int)((int32_t)sc->offset * (viewport_h - 2 * v_margin - thumb_h) / range);
    if (thumb_y < viewport->y1 + v_margin)
        thumb_y = viewport->y1 + v_margin;
    if (thumb_y + thumb_h > viewport->y2 + 1 - v_margin)
        thumb_y = viewport->y2 + 1 - v_margin - thumb_h;

    bar_x2 = obj->coords.x2 - ((obj->radius >= 1) ? (int)obj->radius : 1);
    bar_x1 = bar_x2 - (int)SGL_SCROLL_BAR_WIDTH + 1;
    if (bar_x1 < obj->coords.x1)
        bar_x1 = obj->coords.x1;

    thumb.x1 = (int16_t)bar_x1;
    thumb.y1 = (int16_t)thumb_y;
    thumb.x2 = (int16_t)bar_x2;
    thumb.y2 = (int16_t)(thumb_y + thumb_h - 1);

    sgl_draw_fill_rect(surf, &obj->area, &thumb, (int16_t)(SGL_SCROLL_BAR_WIDTH / 2),
                                              color, sc->bar_alpha);
}
