/* source/core/sgl_core.c
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

#include <sgl_core.h>
#include <sgl_anim.h>
#include <sgl_math.h>
#include <sgl_mm.h>
#include <sgl_log.h>
#include <string.h>
#include <sgl_draw.h>
#include <sgl_font.h>
#include <sgl_theme.h>

/* current sgl system variable, do not used it */
sgl_system_t sgl_system;

/**
 * @brief Alpha blending table for 4 bpp and 2 bpp
 */
const uint8_t sgl_opa4_table[16] = {0,  17, 34,  51, 68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238, 255 };
const uint8_t sgl_opa2_table[4]  = {0, 85, 170, 255};

/**
 * the memory pool, it will be used to allocate memory for the page pool
*/
static uint8_t sgl_mem_pool[CONFIG_SGL_HEAP_MEMORY_SIZE];

/**
 * @brief register the frame buffer device
 * @param fbinfo the frame buffer device information
 * @return int, 0 if success, -1 if failed
 * @note you must check the result of this function
 */
int sgl_fbdev_register(sgl_fbinfo_t *fbinfo)
{
    sgl_check_ptr_return(fbinfo, -1);

    if (fbinfo->buffer[0] == NULL) {
        SGL_LOG_ERROR("You haven't set up the frame buffer.");
        SGL_ASSERT(0);
        return -1;
    }

    if (fbinfo->flush_area == NULL) {
        SGL_LOG_ERROR("You haven't set up the flush area.");
        SGL_ASSERT(0);
        return -1;
    }

    if (fbinfo->buffer_size == 0) {
        SGL_LOG_ERROR("You haven't set up the frame buffer size.");
        SGL_ASSERT(0);
        return -1;
    }

    sgl_system.fbdev.fbinfo = *fbinfo;

/* if panel is rotated at 90 or 270, swap the resolution of the frame buffer */
#if (CONFIG_SGL_FBDEV_ROTATION == 90 || CONFIG_SGL_FBDEV_ROTATION == 270)
    sgl_swap(&sgl_system.fbdev.fbinfo.xres, &sgl_system.fbdev.fbinfo.yres);
#endif

    sgl_system.fbdev.surf.buffer = (sgl_color_t*)fbinfo->buffer[0];
    sgl_system.fbdev.surf.x1 = 0;
    sgl_system.fbdev.surf.y1 = 0;
    sgl_system.fbdev.surf.x2 = fbinfo->xres - 1;
    sgl_system.fbdev.surf.y2 = fbinfo->yres - 1;
    sgl_system.fbdev.surf.size = fbinfo->buffer_size;
    sgl_system.fbdev.surf.w = fbinfo->xres;

    sgl_system.tick_ms = 0;
    sgl_system.fbdev.fb_ready = 3;
    sgl_system.fbdev.fb_emit = 0;

    return 0;
}

/**
 * @brief register the frame buffer device
 * @param buffer0 the frame buffer device buffer0
 * @param buffer1 the frame buffer device buffer1
 * @param buffer_size the frame buffer device buffer size, for example: 16bit color and 320 * 10 line buffer, you should set it to 320 * 10
 * @param resolution_x the frame buffer device resolution x
 * @param resolution_y the frame buffer device resolution y
 * @return int, 0 if success, -1 if failed
 * @note you must check the result of this function
 */
int sgl_fbdev_register_dev(sgl_color_t *buffer0, sgl_color_t *buffer1, uint32_t buffer_size, uint16_t resolution_x, uint16_t resolution_y)
{
    sgl_fbinfo_t fbinfo = {
        .buffer[0] = buffer0,
        .buffer[1] = buffer1,
        .buffer_size = buffer_size,
        .xres = resolution_x,
        .yres = resolution_y,
    };

    return sgl_fbdev_register(&fbinfo);
}

/**
 * @brief get pixmap bytes of per pixel
 * @param pixmap pointer to pixmap
 * @return pixmap bytes of per pixel
 */
uint8_t sgl_pixmal_get_pixel_bytes(const sgl_pixmap_t *pixmap)
{
    static const uint8_t s_bytes_per_pixel[] = {
        [SGL_PIXMAP_FMT_NONE]         = sizeof(sgl_color_t),
        [SGL_PIXMAP_FMT_RGB332]       = 1,
        [SGL_PIXMAP_FMT_ARGB2222]     = 1,
        [SGL_PIXMAP_FMT_RLE_RGB332]   = 1,
        [SGL_PIXMAP_FMT_RLE_ARGB2222] = 1,
        [SGL_PIXMAP_FMT_RGB565]       = 2,
        [SGL_PIXMAP_FMT_ARGB4444]     = 2,
        [SGL_PIXMAP_FMT_RLE_RGB565]   = 2,
        [SGL_PIXMAP_FMT_RLE_ARGB4444] = 2,
        [SGL_PIXMAP_FMT_RGB888]       = 3,
        [SGL_PIXMAP_FMT_RLE_RGB888]   = 3,
        [SGL_PIXMAP_FMT_ARGB8888]     = 4,
        [SGL_PIXMAP_FMT_RLE_ARGB8888] = 4,
        [SGL_PIXMAP_FMT_QOI_RGB565]   = 2,
        [SGL_PIXMAP_FMT_ARGB8565]     = 3,
    };

    SGL_ASSERT(pixmap != NULL);
    if (pixmap->format >= sizeof(s_bytes_per_pixel)) {
        SGL_LOG_ERROR("pixmap format error");
        return 0;
    }
    return s_bytes_per_pixel[pixmap->format];
}

/**
 * @brief add object to parent
 * @param parent: pointer of parent object
 * @param obj: pointer of object
 * @return none
 */
void sgl_obj_add_child(sgl_obj_t *parent, sgl_obj_t *obj)
{
    SGL_ASSERT(parent != NULL && obj != NULL);
    sgl_obj_t *tail = parent->child;

    if (parent->child) {
        while (tail->sibling != NULL) {
            tail = tail->sibling;
        };
        tail->sibling = obj;
    }
    else {
        parent->child = obj;
    }

    obj->parent = parent;
}

/**
 * @brief remove an object from its parent
 * @param obj object to remove
 * @return none
 * @note This function will remove the object from its parent, of course, his children will also be removed
 */
void sgl_obj_remove(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);

    sgl_obj_t *parent = obj->parent;
    sgl_obj_t *pos = NULL;

    if (parent->child != obj) {
        pos = parent->child;
        while (pos->sibling != obj) {
            pos = pos->sibling;
        }
        pos->sibling = obj->sibling;
    }
    else {
        parent->child = obj->sibling;
    }

    obj->sibling = NULL;
}

/**
 * @brief move object child position
 * @param obj point to object
 * @param ofs_x: x offset position
 * @param ofs_y: y offset position
 * @return none
 */
void sgl_obj_move_child_pos(sgl_obj_t *obj, int16_t ofs_x, int16_t ofs_y)
{
    SGL_ASSERT(obj != NULL);
    sgl_obj_t *stack[SGL_OBJ_DEPTH_MAX];
    int top = 0;

    if (obj->child == NULL) {
        return;
    }
    stack[top++] = obj->child;

    while (top > 0) {
        SGL_ASSERT(top < SGL_OBJ_DEPTH_MAX);
        obj = stack[--top];

        obj->dirty = 1;
        obj->coords.x1 += ofs_x;
        obj->coords.x2 += ofs_x;
        obj->coords.y1 += ofs_y;
        obj->coords.y2 += ofs_y;

        if (obj->sibling != NULL) {
            stack[top++] = obj->sibling;
        }

        if (obj->child != NULL) {
            stack[top++] = obj->child;
        }
    }
}

/**
 * @brief Set object absolute position
 * @param obj point to object
 * @param abs_x: x absolute position
 * @param abs_y: y absolute position
 * @return none
 */
void sgl_obj_set_abs_pos(sgl_obj_t *obj, int16_t abs_x, int16_t abs_y)
{
    SGL_ASSERT(obj != NULL);
    int16_t x_diff = abs_x - obj->coords.x1;
    int16_t y_diff = abs_y - obj->coords.y1;

    sgl_obj_set_dirty(obj);
    obj->coords.x1 += x_diff;
    obj->coords.x2 += x_diff;
    obj->coords.y1 += y_diff;
    obj->coords.y2 += y_diff;

    sgl_obj_move_child_pos(obj, x_diff, y_diff);
}

/**
 * @brief zoom object size
 * @param obj point to object
 * @param zoom zoom size
 * @return none
 * @note if you want to zoom out, the zoom should be positive, if you want to zoom in, the zoom should be negative
 */
void sgl_obj_size_zoom(sgl_obj_t *obj, int16_t zoom)
{
    SGL_ASSERT(obj != NULL);
    obj->coords.x1 -= zoom;
    obj->coords.x2 += zoom;
    obj->coords.y1 -= zoom;
    obj->coords.y2 += zoom;
}

/**
 * @brief zoom object circle size
 * @param obj point to object
 * @param radius circle radius
 * @return none
 */
void sgl_obj_circle_zoom(sgl_obj_t *obj, int16_t radius)
{
    SGL_ASSERT(obj != NULL);
    const int16_t cx = (obj->coords.x1 + obj->coords.x2) / 2;
    const int16_t cy = (obj->coords.y1 + obj->coords.y2) / 2;
    obj->coords.x1 = cx - radius + 1;
    obj->coords.x2 = cx + radius;
    obj->coords.y1 = cy - radius + 1;
    obj->coords.y2 = cy + radius;
    obj->radius = radius;
}

/**
 * @brief get last child of an object
 * @param obj the object
 * @return the last child of the object
 */
sgl_obj_t* sgl_obj_get_last_child(sgl_obj_t* obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_obj_t *child = NULL;

    sgl_obj_for_each_child(child, obj) {
        if (child->sibling == NULL) {
            return child;
        }
    }
    return child;
}

/**
 * @brief get previous sibling of an object
 * @param obj the object
 * @return the previous sibling of the object
 */
sgl_obj_t* sgl_obj_get_prev_sibling(sgl_obj_t* obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_obj_t *child = NULL;

    sgl_obj_for_each_child(child, obj) {
        if (child->sibling == obj) {
            return child;
        }
    }
    return child;
}

/**
 * @brief move object up a level layout
 * @param obj point to object
 * @return none
 * @note Only move among sibling objects
 */
void sgl_obj_move_up(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_obj_t *parent = obj->parent;
    sgl_obj_t *prev = NULL;
    sgl_obj_t *next = NULL;

    /* if the object is the last child, do not move it */
    if (obj->sibling == NULL) {
        return;
    }
    else if (parent->child == obj) {
        parent->child = obj->sibling;
        obj->sibling = obj->sibling->sibling;
        /* mark object as dirty */
        sgl_obj_set_dirty(obj);
        return;
    }

    /* move the object to its next sibling */
    sgl_obj_for_each_child(prev, parent) {
        if (prev->sibling == obj) {
            next = obj->sibling;
            obj->sibling = next->sibling;
            prev->sibling = next;
            next->sibling = obj;
            /* mark object as dirty */
            sgl_obj_set_dirty(obj);
            return;
        }
    }
}

/**
 * @brief move object down a level layout
 * @param obj point to object
 * @return none
 * @note Only move among sibling objects
 */
void sgl_obj_move_down(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_obj_t *parent = obj->parent;
    sgl_obj_t *prev_prev = NULL;
    sgl_obj_t *prev = NULL;

    if (parent->child == obj) {
        return;
    }

    // Find the previous sibling node (prev) and the node before it (prev_prev)
    sgl_obj_for_each_child(prev, parent) {
        if (prev->sibling == obj) {
            break;
        }
        prev_prev = prev;
    }

    if (prev != NULL) {
        if (prev_prev != NULL) {
            prev_prev->sibling = obj;
        }
        else {
            parent->child = obj;
        }

        prev->sibling = obj->sibling;
        obj->sibling = prev;
        sgl_obj_set_dirty(obj);
    }
}

/**
 * @brief move object top level layout
 * @param obj point to object
 * @return none
 * @note Only move among sibling objects
 */
void sgl_obj_move_top(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL && obj->parent != NULL);

    sgl_obj_t *parent = obj->parent;
    sgl_obj_t *prev = NULL, *curr = parent->child, *last = NULL;

    /* if the object is the last child, do not move it */
    if (obj->sibling == NULL) {
        return;
    }

    while (curr != NULL && curr != obj) {
        prev = curr;
        curr = curr->sibling;
    }

    if (prev == NULL) {
        parent->child = obj->sibling;
    }
    else {
        prev->sibling = obj->sibling;
    }

    last = parent->child;
    if (last == NULL) {
        parent->child = obj;
        obj->sibling = NULL;
    }
    else {
        while (last->sibling != NULL) {
            last = last->sibling;
        }
        last->sibling = obj;
        obj->sibling = NULL;
    }

    sgl_obj_set_dirty(obj);
}

/**
 * @brief move object bottom level layout
 * @param obj point to object
 * @return none
 * @note Only move among sibling objects
 */
void sgl_obj_move_bottom(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_obj_t *parent = obj->parent;
    sgl_obj_t *prev = NULL;

    /* if the object is the first child, do not move it */
    if (parent->child == obj) {
        return;
    }

    sgl_obj_for_each_child(prev, parent) {
        if (prev->sibling == obj) {
            break;
        }
    }

    prev->sibling = obj->sibling;
    obj->sibling = parent->child;
    parent->child = obj;
    /* mark object as dirty */
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get fix radius of object
 * @param obj object
 * @param radius: radius that you want to set
 * @return none
 * @note if radius is larger than object's width or height, fix radius will be returned
 */
void sgl_obj_set_radius(sgl_obj_t *obj, size_t radius)
{
    int16_t w = (obj->coords.x2 - obj->coords.x1 + 1);
    int16_t h = (obj->coords.y2 - obj->coords.y1 + 1);
    int16_t d_min = w > h ? h : w;

    if ((int16_t)radius >= (d_min / 2)) {
        radius = sgl_is_odd(d_min) ? d_min / 2 : (d_min - 1) / 2;
    }

    obj->radius = radius & 0xFFF;
}

#if (CONFIG_SGL_OBJ_USE_NAME && CONFIG_SGL_DEBUG)
/**
 * @brief print object name that include this all child
 * @param obj point to object
 * @return none
 */
void sgl_obj_print_name(sgl_obj_t *obj)
{
    int top = 0;
    sgl_obj_t *stack[SGL_OBJ_DEPTH_MAX];
    stack[top++] = obj;

    while (top > 0) {
        SGL_ASSERT(top < SGL_OBJ_DEPTH_MAX);
        obj = stack[--top];

        if (obj->name == NULL) {
            SGL_LOG_INFO("[OBJ NAME]: %s", "NULL");
        }
        else {
            SGL_LOG_INFO("[OBJ NAME]: %s", obj->name);
        }

        if (obj->sibling != NULL) {
            stack[top++] = obj->sibling;
        }

        if (obj->child != NULL) {
            stack[top++] = obj->child;
        }
    }
}
#endif

/**
 * @brief page construct callback function
 * @param surf surface pointer
 * @param obj page object
 * @param evt event
 * @return none
 * @note evt not used
 */
static void sgl_page_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_page_t *page = (sgl_page_t *)obj;
    const sgl_pixmap_t *pixmap = page->pixmap;

    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        if (pixmap == NULL) {
            sgl_draw_fill_rect(surf, &obj->area, &obj->coords, 0, page->color, page->alpha);
        }
        else {
            sgl_draw_fill_rect_pixmap(surf, &obj->area, &obj->coords, 0, pixmap, page->alpha);
        }
    }
}

/**
 * @brief set page background color
 * @param obj point to object
 * @param color background color
 * @return none
 */
void sgl_page_set_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_page_t* page = (sgl_page_t*)obj;
    page->color = color;
    page->pixmap = NULL;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set page background pixmap
 * @param obj point to object
 * @param pixmap background pixmap
 * @return none
 */
void sgl_page_set_pixmap(sgl_obj_t* obj, const sgl_pixmap_t *pixmap)
{
    sgl_page_t* page = (sgl_page_t*)obj;
    page->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set page background alpha
 * @param obj point to object
 * @param alpha background alpha
 * @return none
 */
void sgl_page_set_alpha(sgl_obj_t* obj, uint8_t alpha)
{
    sgl_page_t* page = (sgl_page_t*)obj;
    page->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief create a page
 * @param none
 * @return sgl_page_t* the page pointer
 */
static sgl_page_t* sgl_page_create(void)
{
    sgl_page_t *page = sgl_malloc(sizeof(sgl_page_t));
    if (page == NULL) {
        SGL_LOG_ERROR("sgl_page_create: malloc failed");
        return NULL;
    }

    /* clear the page all fields */
    memset(page, 0, sizeof(sgl_page_t));

    sgl_obj_t *obj = &page->obj;

    if (sgl_system.fbdev.fbinfo.buffer[0] == NULL) {
        SGL_LOG_ERROR("sgl_page_create: framebuffer is NULL");
        sgl_free(page);
        return NULL;
    }

    page->color = SGL_THEME_DESKTOP;
    page->alpha = SGL_ALPHA_MAX;

    obj->parent = obj;
    obj->clickable = 0;
    obj->construct_fn = sgl_page_construct_cb;
    obj->dirty = 1;
    obj->page = 1;
    obj->border = 0;
    obj->coords = (sgl_area_t) {
        .x1 = 0,
        .y1 = 0,
        .x2 = sgl_system.fbdev.fbinfo.xres - 1,
        .y2 = sgl_system.fbdev.fbinfo.yres - 1,
    };

    obj->area = obj->coords;

    /* init child list */
    sgl_obj_node_init(&page->obj);

    if (sgl_system.fbdev.active == NULL) {
        sgl_system.fbdev.active = &page->obj;
    }

    return page;
}

/**
 * @brief Create an object
 * @param parent parent object
 * @return sgl_obj_t
 * @note if parent is NULL, the object will be as an new page
 */
sgl_obj_t* sgl_obj_create(sgl_obj_t *parent)
{
    sgl_obj_t *obj;

    /* create page object */
    if (parent == NULL) {
        sgl_page_t *page = sgl_page_create();
        if (page == NULL) {
            SGL_LOG_ERROR("sgl_obj_create: create page failed");
            return NULL;
        }
        obj = &page->obj;
        return obj;
    }
    else {
        obj = (sgl_obj_t*)sgl_malloc(sizeof(sgl_obj_t));
        if (obj == NULL) {
            SGL_LOG_ERROR("malloc failed");
            return NULL;
        }

        obj->coords = parent->coords;
        obj->parent = parent;
        obj->event_fn = NULL;
        obj->event_data = 0;
        obj->construct_fn = NULL;
        obj->dirty = 1;

        /* init node */
        sgl_obj_node_init(obj);
        /* add the child into parent's child list */
        sgl_obj_add_child(parent, obj);

        return obj;
    }
}

/**
 * @brief initialize global dirty area
 * @param none
 * @return none
 */
static inline void sgl_dirty_area_init(void)
{
    sgl_system.fbdev.dirty_num = 0;
}

/**
 * @brief sgl global initialization
 * @param none
 * @return int, 0 means success, others means failed
 * @note you should call this function before using sgl and you should call this function after register framebuffer device
 */
int sgl_init(void)
{
    sgl_obj_t *obj = NULL;

    /* init memory pool */
    sgl_mm_init(sgl_mem_pool, sizeof(sgl_mem_pool));

    /* initialize current context */
    sgl_system.fbdev.active = NULL;

    /* initialize dirty area */
    sgl_dirty_area_init();

    /* create a screen object for drawing */
    obj = sgl_obj_create(NULL);
    if (obj == NULL) {
        SGL_LOG_ERROR("sgl_init: create screen object failed");
        return -1;
    }

    /* if the rotation is not 0, we need to alloc a buffer for rotation */
#if ((CONFIG_SGL_FBDEV_ROTATION != 0) || CONFIG_SGL_FBDEV_RUNTIME_ROTATION)
    sgl_system.rotation = (sgl_color_t*)sgl_malloc(sgl_system.fbdev.fbinfo.buffer_size * sizeof(sgl_color_t));
    if (sgl_system.rotation == NULL) {
        SGL_LOG_ERROR("sgl_init: alloc rotation buffer failed");
        return -1;
    }
#if (CONFIG_SGL_FBDEV_RUNTIME_ROTATION)
    sgl_system.angle = 0;
#endif
#endif
    /* create event queue */
    if (sgl_event_queue_init()) {
        SGL_LOG_ERROR("sgl_init: event queue init failed");
        sgl_free(obj);
        return -1;
    }

#if (CONFIG_SGL_BOOT_LOGO)
    sgl_boot_logo();
#endif
    return 0;
}

/**
 * @brief set current object as screen object
 * @param obj object, that you want to set an object as active page
 * @return none
 */
void sgl_screen_load(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_system.fbdev.active = obj;

    /* initialize dirty area */
    sgl_dirty_area_init();
    sgl_obj_set_dirty(obj);
}

#if (CONFIG_SGL_FBDEV_RUNTIME_ROTATION)
/**
 * @brief set framebuffer device rotation angle
 * @param angle [in] rotation angle, that is 0, 90, 180, 270
 * @return none
 * @note Rotation angle must be 0, 90, 180, 270
 */
void sgl_fbdev_set_angle(uint16_t angle)
{
    if (angle == sgl_system.angle) {
        return;
    }

    const uint8_t cur_status = (sgl_system.angle == 0 || sgl_system.angle == 180) ? 0 : 
                               (sgl_system.angle == 90 || sgl_system.angle == 270) ? 1 : 2;

    const uint8_t new_status = (angle == 0 || angle == 180) ? 0 : 
                               (angle == 90 || angle == 270) ? 1 : 2;

    if (cur_status == 2 || new_status == 2) {
        SGL_LOG_WARN("sgl_fbdev_set_angle: invalid angle");
        return;
    }

    if (cur_status != new_status) {
        sgl_swap(&sgl_system.fbdev.fbinfo.xres, &sgl_system.fbdev.fbinfo.yres);
        sgl_swap(&sgl_system.fbdev.active->coords.x2, &sgl_system.fbdev.active->coords.y2);
        sgl_swap(&sgl_system.fbdev.active->area.x2, &sgl_system.fbdev.active->area.y2);
    }

    sgl_system.angle = angle;
    sgl_obj_set_dirty(sgl_system.fbdev.active);
}
#endif // !CONFIG_SGL_FBDEV_RUNTIME_ROTATION

/**
 * @brief  Get area intersection between two areas
 * @param area_a: area a
 * @param area_b: area b
 * @param clip: intersection area
 * @return true: intersect, otherwise false
 * @note: this function is unsafe, you should check the area_a and area_b and clip is not NULL by yourself
 */
bool sgl_area_clip(sgl_area_t *area_a, sgl_area_t *area_b, sgl_area_t *clip)
{
    SGL_ASSERT(area_a != NULL && area_b != NULL && clip != NULL);

    if (unlikely(area_b->x2 < area_a->x1 || area_b->x1 > area_a->x2 ||
                 area_b->y2 < area_a->y1 || area_b->y1 > area_a->y2)) {
        return false;
    }

    clip->x1 = sgl_max(area_a->x1, area_b->x1);
    clip->y1 = sgl_max(area_a->y1, area_b->y1);
    clip->x2 = sgl_min(area_a->x2, area_b->x2);
    clip->y2 = sgl_min(area_a->y2, area_b->y2);

    return true;
}

/**
 * @brief clip area with another area
 * @param clip [in][out] clip area
 * @param area [in] area
 * @return true if clip area is valid, otherwise two area is not overlapped
 * @note: this function is unsafe, you should check the clip and area is not NULL by yourself
 */
bool sgl_area_selfclip(sgl_area_t *clip, sgl_area_t *area)
{
    SGL_ASSERT(clip != NULL && area != NULL);

    if (unlikely(area->x2 < clip->x1 || area->x1 > clip->x2 ||
                 area->y2 < clip->y1 || area->y1 > clip->y2)) {
        return false;
    }

    clip->x1 = sgl_max(clip->x1, area->x1);
    clip->y1 = sgl_max(clip->y1, area->y1);
    clip->x2 = sgl_min(clip->x2, area->x2);
    clip->y2 = sgl_min(clip->y2, area->y2);

    return true;
}

/**
 * @brief Computes the total boundary expansion (in Manhattan distance) required to merge rectangle b into rectangle a.
 *
 * @param a[in]    Pointer to the b rectangle
 * @param b[in]    Pointer to the a rectangle
 * @return int32_t Total expansion amount (always non-negative)
 */
static inline int32_t sgl_area_growth(sgl_area_t *a, sgl_area_t *b)
{
    return (a->x1 - sgl_min(a->x1, b->x1)) + (sgl_max(a->x2, b->x2) - a->x2)
           + (a->y1 - sgl_min(a->y1, b->y1)) + (sgl_max(a->y2, b->y2) - a->y2);
}

/**
 * @brief Quickly determines if two rectangles are close enough to be merged.
 *
 * This fast heuristic is useful in performance-critical contexts (e.g., real-time segmentation or region merging)
 * to avoid excessive fragmentation while preventing merges between distant regions.
 *
 * @param a[in] Pointer to the first rectangle
 * @param b[in] Pointer to the second rectangle
 * @return bool true if the rectangles are sufficiently close for merging; false otherwise
 */
static inline bool sgl_merge_determines(sgl_area_t* a, sgl_area_t* b)
{
    if ((a->x1 > b->x2 && a->x1 - b->x2 > 32) || (b->x1 > a->x2 && b->x1 - a->x2 > 32)) {
        return false;
    }
    if ((a->y1 > b->y2 && a->y1 - b->y2 > 32) || (b->y1 > a->y2 && b->y1 - a->y2 > 32)) {
        return false;
    }

    const int16_t mw = sgl_max(a->x2, b->x2) - sgl_min(a->x1, b->x1) + 1;
    const int16_t mh = sgl_max(a->y2, b->y2) - sgl_min(a->y1, b->y1) + 1;
    const int32_t sum = (int32_t)(a->x2-a->x1+1)*(a->y2-a->y1+1) + (int32_t)(b->x2-b->x1+1)*(b->y2-b->y1+1);

    return sum > (int32_t)(mw * mh);
}

/**
 * @brief merge an area into global dirty area
 * 
 * This function calculates how much rectangle 'a' would need to grow in each direction (left, right, top, bottom)
 * to fully enclose both 'a' and 'b'. The result is the sum of the expansions along all four sides.
 * Note: This is not the increase in area, th is a lightweight heuristic for merge cost in bounding-box algorithms.
 * 
 * @param area [in] Pointer to the area
 * @return none
 * @warning This function is unsafe, you should check the area is not out of screen size by yourself
 */
void sgl_dirty_area_push(sgl_area_t *area)
{
    SGL_ASSERT(area != NULL);
    int32_t best_idx = -1, min_growth = INT32_MAX, growth = INT32_MAX;
    /* skip invalid area */
    if (area->x1 > area->x2 || area->y1 > area->y2) {
        return;
    }

    for (uint8_t i = 0; i < sgl_system.fbdev.dirty_num; i++) {
        if (sgl_merge_determines(&sgl_system.fbdev.dirty[i], area)) {
            growth = sgl_area_growth(&sgl_system.fbdev.dirty[i], area);
            if (growth == 0) {
                /* already contains the area */
                return;
            }
            else if (growth < min_growth) {
                min_growth = growth;
                best_idx = i;
            }
        }
    }

    if (best_idx >= 0) {
        /* merge object area into best_idx dirty area */
        sgl_area_selfmerge(&sgl_system.fbdev.dirty[best_idx], area);
        return;
    }

    if (sgl_system.fbdev.dirty_num < SGL_DIRTY_AREA_NUM_MAX) {
        /* add new dirty area */
        sgl_system.fbdev.dirty[sgl_system.fbdev.dirty_num++] = *area;
    } else {
        /* merge object area into last dirty area */
        sgl_area_selfmerge(&sgl_system.fbdev.dirty[SGL_DIRTY_AREA_NUM_MAX - 1], area);
    }
}

/**
 * @brief initialize object
 * @param obj object
 * @param parent parent object
 * @return int, 0 means successful, -1 means failed
 */
int sgl_obj_init(sgl_obj_t *obj, sgl_obj_t *parent)
{
    SGL_ASSERT(obj != NULL);

    if (parent == NULL) {
        parent = sgl_screen_act();
        if (parent == NULL) {
            SGL_LOG_ERROR("sgl_obj_init: have no active page");
            return -1;
        }
    }

    /* set essential member */
    obj->coords = parent->coords;
    obj->parent = parent;
    sgl_obj_set_dirty(obj);

    /* init object area to invalid */
    sgl_area_init(&obj->area);

    /* add the child into parent's child list */
    sgl_obj_add_child(parent, obj);

    return 0;
}

/**
 * @brief  cleanup an object
 * @param  obj: object to cleanup
 * @return none
 * @note this function will call the construct function of the object to free some resources
 */
static void sgl_obj_cleanup(sgl_obj_t *obj)
{
    sgl_event_t evt = { .type = SGL_EVENT_DESTROYED,};
    sgl_event_ctx_cleanup(obj);
    /* check construct function */
    SGL_ASSERT(obj->construct_fn != NULL);
    obj->construct_fn(NULL, obj, &evt);
    sgl_free(obj);
}

/**
 * @brief  free an object chain
 * @param  obj: object to free
 * @retval none
 * @note this function will free all the itself and children and siblings of the object
 */
static void sgl_obj_free_chain(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_obj_t *stack[SGL_OBJ_DEPTH_MAX];
    int top = 0;
    stack[top++] = obj;

    while (top > 0) {
        SGL_ASSERT(top < SGL_OBJ_DEPTH_MAX);
        obj = stack[--top];

        if (obj->sibling != NULL) {
            stack[top++] = obj->sibling;
        }

        if (obj->child != NULL) {
            stack[top++] = obj->child;
        }
        sgl_obj_cleanup(obj);
    }
}

/**
 * @brief  free an object
 * @param  obj: object to free
 * @retval none
 * @note this function will free the object itself and all of its children, but not its siblings.
 *       If you want to free the object and all of its siblings, please use sgl_obj_free_chain() instead.
 */
void sgl_obj_free(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    if (obj->child) {
        sgl_obj_free_chain(obj->child);
    }
    sgl_obj_cleanup(obj);
}

/**
 * @brief Set object to dirty
 * @param obj point to object
 * @return none
 * @note this function will set object to dirty, include its children
 */
void sgl_obj_set_dirty(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->dirty = 1;
    sgl_system.fbdev.update_flag = 1;
}

/**
 * @brief  Set the object to be destroyed
 * @param  obj: the object to set
 * @retval None
 * @note this function is used to set the destroyed flag of the object, then next draw cycle, the object will be removed
 *       the object should be not NULL.
 */
void sgl_obj_set_destroyed(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->destroyed = 1;
    sgl_system.fbdev.update_flag = 1;
}

/**
 * @brief update object area
 * @param area point to area that need update
 * @return none, this function will force update object area
 * @note this function will update object area, and the object area will be merged into the dirty area
 */
void sgl_update_area(sgl_area_t *area)
{
    SGL_ASSERT(area != NULL);
    sgl_area_t clip = sgl_system.fbdev.active->area;
    clip.x1 = sgl_max(clip.x1, area->x1);
    clip.x2 = sgl_min(clip.x2, area->x2);
    clip.y1 = sgl_max(clip.y1, area->y1);
    clip.y2 = sgl_min(clip.y2, area->y2);
    sgl_dirty_area_push(&clip);
}

/**
 * @brief update object area
 * @param area point to area that need update
 * @return none, this function will force update object area
 * @note this function will update object area, it will be added into dirty area without merging
 */
void sgl_obj_update_area(sgl_area_t *area)
{
    SGL_ASSERT(area != NULL);
    sgl_area_t clip = sgl_system.fbdev.active->area;

    clip.x1 = sgl_max(clip.x1, area->x1);
    clip.x2 = sgl_min(clip.x2, area->x2);
    clip.y1 = sgl_max(clip.y1, area->y1);
    clip.y2 = sgl_min(clip.y2, area->y2);

    if (clip.x1 > clip.x2 || clip.y1 > clip.y2) {
        return;
    }

    if (sgl_system.fbdev.dirty_num < SGL_DIRTY_AREA_NUM_MAX) {
        /* add new dirty area */
        sgl_system.fbdev.dirty[sgl_system.fbdev.dirty_num++] = clip;
    } else {
        /* merge object area into last dirty area */
        sgl_area_selfmerge(&sgl_system.fbdev.dirty[SGL_DIRTY_AREA_NUM_MAX - 1], &clip);
    }
}

/**
 * @brief Set object size
 * @param obj point to object
 * @param width: width that you want to set
 * @param height: height that you want to set
 * @return none
 */
void sgl_obj_set_size(sgl_obj_t *obj, int16_t width, int16_t height)
{
    SGL_ASSERT(obj != NULL);
    obj->coords.x2 = obj->coords.x1 + width - 1;
    obj->coords.y2 = obj->coords.y1 + height - 1;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set object width
 * @param obj point to object
 * @param width: width that you want to set
 * @return none
 */
void sgl_obj_set_width(sgl_obj_t *obj, int16_t width)
{
    SGL_ASSERT(obj != NULL);
    obj->coords.x2 = obj->coords.x1 + width - 1;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set object height
 * @param obj point to object
 * @param height: height that you want to set
 * @return none
 */
void sgl_obj_set_height(sgl_obj_t *obj, int16_t height)
{
    SGL_ASSERT(obj != NULL);
    obj->coords.y2 = obj->coords.y1 + height - 1;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set object border width
 * @param obj point to object
 * @param border: border width that you want to set
 * @return none
 */
void sgl_obj_set_border_width(sgl_obj_t *obj, uint8_t border)
{
    SGL_ASSERT(obj != NULL);
    obj->border = sgl_min3(border, sgl_obj_get_width(obj) / 2, sgl_obj_get_height(obj) / 2);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief delete object
 * @param obj point to object
 * @return none
 * @note this function will set object and his childs to be destroyed, then next draw cycle, the object will be removed.
 *       if object is NULL, the all objects of active page will be delete, but the page object will not be deleted.
 *       if object is a page, the page object will be deleted and all its children will be deleted.
 */
void sgl_obj_delete(sgl_obj_t *obj)
{
    if (obj == NULL || obj == sgl_screen_act()) {
        obj = sgl_screen_act();
        if (obj->child) {
            sgl_obj_free_chain(obj->child);
        }
        sgl_obj_node_init(obj);
        sgl_obj_set_dirty(obj);
        return;
    }
    else if (obj->page == 1) {
        sgl_obj_free(obj);
        return;
    }

    sgl_obj_set_destroyed(obj);
}

/**
 * @brief delete object
 * @param obj point to object
 * @return none
 * @note this function will take effect immediately
 */
void sgl_obj_delete_sync(sgl_obj_t *obj)
{
    sgl_obj_delete(obj);
    sgl_task_handler_sync();
}

/**
 * @brief delete object's all children
 * @param obj point to object
 * @return none
 * @note this function will delete object's all children, but not the object itself.
 */
void sgl_obj_delete_children(sgl_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }
    if (obj->child) {
        sgl_obj_free_chain(obj->child);
    }
    sgl_obj_node_init(obj);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief delete object's all children immediately
 * @param obj point to object
 * @return none
 * @note this function will delete object's all children immediately, but not the object itself.
 */
void sgl_obj_delete_children_sync(sgl_obj_t *obj)
{
    sgl_obj_delete_children(obj);
    sgl_task_handler_sync();
}

/**
 * @brief Convert UTF-8 string to Unicode
 * @param utf8_str Pointer to the UTF-8 string to be converted
 * @param p_unicode_buffer Pointer to the buffer where the converted Unicode will be stored
 * @return The number of bytes in the UTF-8 string
 */
uint32_t sgl_utf8_to_unicode(const char *utf8_str, uint32_t *p_unicode_buffer)
{
    uint8_t* ptr = (uint8_t*)utf8_str;
    if ((*ptr) < 0x80) { // 1-byte/7-bit ASCII
        *p_unicode_buffer = ptr[0];
        return 1;
    }
    else if (((*ptr) & 0xE0) == 0xC0) { // 2-byte
        *p_unicode_buffer = (ptr[0] & 0x1F) << 6 | (ptr[1] & 0x3F);
        return 2;
    }
    else if (((*ptr) & 0xF0) == 0xE0) { // 3-byte
        *p_unicode_buffer = (ptr[0] & 0x0F) << 12 | (ptr[1] & 0x3F) << 6 | (ptr[2] & 0x3F);
        return 3;
    }
    else if (((*ptr) & 0xF8) == 0xF0) { // 4-byte
        *p_unicode_buffer = (ptr[0] & 0x07) << 18 | (ptr[1] & 0x3F) << 12 | (ptr[2] & 0x3F) << 6  | (ptr[3] & 0x3F);
        return 4;
    }
    return 0;
}

/**
 * @brief Search for the index of a Unicode character in the font table
 * @param font Pointer to the font structure containing character data
 * @param unicode Unicode of the character to be searched
 * @return Index of the character in the font table
 */
uint32_t sgl_search_unicode_ch_index(const sgl_font_t *font, uint32_t unicode)
{
    uint32_t i, left = 0, right = 0, mid = 0;
    uint32_t target = unicode;
    const sgl_font_unicode_t *code = font->unicode;

    for (i = 0; i < font->unicode_num - 1; i ++) {
        if ((target >= code[i].offset) && (target < code[i + 1].offset)) {
            break;
        }
    }

    code += i;
    target -= code->offset;

    if (code->list == NULL) {
        if (target >= code->len) {
            SGL_LOG_WARN("sgl_search_unicode_ch_index: [0x%x]unicode not found in font table", unicode);
            return 0;
        }
        return target + code->tab_offset;
    }

    right = code->len - 1;
    while (left <= right) {
        mid = left + (right - left) / 2;

        if (code->list[mid] == target) {
            return mid + code->tab_offset;
        }
        else if (code->list[mid] < target) {
            left = mid + 1;
        }
        else {
            right = mid - 1;
        }
    }

    SGL_LOG_WARN("sgl_search_unicode_ch_index: [0x%x]unicode not found in font table", unicode);
    return 0;
}

/**
 * @brief get the width of a string
 * @param str string
 * @param font sgl font
 * @return width of string
 */
int32_t sgl_font_get_string_width(const char *str, const sgl_font_t *font)
{
    int32_t len = 0;
    uint32_t unicode = 0;
    uint32_t ch_index = 0;

    if (font == NULL || str == NULL) {
        return 0;
    }

    while (*str) {
        str += sgl_utf8_to_unicode(str, &unicode);
        ch_index = sgl_search_unicode_ch_index(font, unicode);
#if (CONFIG_SGL_FLASH_FONT)
        len += ((font->table[(font->format == SGL_FONT_FMT_EXT_FLASH_FIXED) ? 0 : ch_index].adv_w + 8) >> 4);
#else
        len += ((font->table[ch_index].adv_w + 8) >> 4);
#endif
    }
    return len;
}

/**
 * @brief get the height of a string, which is in a rect area
 * @param width width of the rect area
 * @param str string
 * @param font sgl font of the string
 * @param line_space peer line space
 * @return height size of string
 */
int32_t sgl_font_get_string_height(int16_t width, const char *str, const sgl_font_t *font, uint8_t line_space)
{
    int16_t offset_x = 0;
    int16_t ch_index;
    int16_t ch_width;
    int16_t lines = 1;
    uint32_t unicode = 0;

    while (*str) {
        if (*str == '\n') {
            lines ++;
            offset_x = 0;
            str ++;
            continue;
        }

        str += sgl_utf8_to_unicode(str, &unicode);
        ch_index = sgl_search_unicode_ch_index(font, unicode);

#if (CONFIG_SGL_FLASH_FONT)
        ch_width = ((font->table[(font->format == SGL_FONT_FMT_EXT_FLASH_FIXED) ? 0 : ch_index].adv_w + 8) >> 4);
#else
        ch_width = ((font->table[ch_index].adv_w + 8)>> 4);
#endif

        if ((offset_x + ch_width) >= width) {
            offset_x = 0;
            lines ++;
        }

        offset_x += ch_width;
    }

    return lines * (font->font_height + line_space);
}

/**
 * @brief get the alignment position
 * @param parent_size parent size
 * @param size object size
 * @param type alignment type
 * @return alignment position offset
 */
sgl_pos_t sgl_get_align_pos(sgl_size_t *parent_size, sgl_size_t *size, sgl_align_type_t type)
{
    SGL_ASSERT(parent_size != NULL && size != NULL);
    sgl_pos_t ret = {.x = 0, .y = 0};

    switch (type) {
    case SGL_ALIGN_CENTER:
        ret.x = (parent_size->w - size->w) / 2;
        ret.y = (parent_size->h - size->h) / 2;
    break;

    case SGL_ALIGN_TOP_MID:
        ret.x = (parent_size->w - size->w) / 2;
        ret.y = 0;
    break;

    case SGL_ALIGN_TOP_LEFT:
        ret.x = 0;
        ret.y = 0;
    break;

    case SGL_ALIGN_TOP_RIGHT:
        ret.x = parent_size->w - size->w;
        ret.y = 0;
    break;

    case SGL_ALIGN_BOT_MID:
        ret.x = (parent_size->w - size->w) / 2;
        ret.y = parent_size->h - size->h;
    break;

    case SGL_ALIGN_BOT_LEFT:
        ret.x = 0;
        ret.y = parent_size->h - size->h;
    break;

    case SGL_ALIGN_BOT_RIGHT:
        ret.x = parent_size->w - size->w;
        ret.y = parent_size->h - size->h;
    break;

    case SGL_ALIGN_LEFT_MID:
        ret.x = 0;
        ret.y = (parent_size->h - size->h) / 2;
    break;

    case SGL_ALIGN_RIGHT_MID:
        ret.x = parent_size->w - size->w;
        ret.y = (parent_size->h - size->h) / 2;
    break;

    default:
        SGL_LOG_WARN("invalid align type");
    break;
    }
    return ret;
}

/**
 * @brief get the text position in the area
 * @param area point to area
 * @param font point to font
 * @param text text string
 * @param offset text offset
 * @param type alignment type
 * @return sgl_pos_t position of text
 */
sgl_pos_t sgl_get_text_pos(sgl_area_t *area, const sgl_font_t *font, const char *text, int16_t offset, sgl_align_type_t type)
{
    SGL_ASSERT(area != NULL && font != NULL);
    sgl_pos_t ret = {.x = 0, .y = 0};
    sgl_size_t parent_size = {
        .w = area->x2 - area->x1 + 1,
        .h = area->y2 - area->y1 + 1,
    };

    sgl_size_t text_size = {
        .w = sgl_font_get_string_width(text, font) + offset,
        .h = sgl_font_get_height(font),
    };

    ret = sgl_get_align_pos(&parent_size, &text_size, type);
    ret.x += area->x1;
    ret.y += area->y1;

    return ret;
}

/**
 * @brief get the icon position of area
 * @param area point to area
 * @param icon point to icon
 * @param offset offset
 * @param type align type
 */
sgl_pos_t sgl_get_icon_pos(sgl_area_t *area, const sgl_icon_pixmap_t *icon, int16_t offset, sgl_align_type_t type)
{
    SGL_ASSERT(area != NULL && icon != NULL);
    sgl_pos_t ret = {.x = 0, .y = 0};
    sgl_size_t parent_size = {
        .w = area->x2 - area->x1 + 1,
        .h = area->y2 - area->y1 + 1,
    };

    sgl_size_t text_size = {
        .w = icon->width + offset,
        .h = icon->height,
    };

    ret = sgl_get_align_pos(&parent_size, &text_size, type);
    ret.x += area->x1;
    ret.y += area->y1;

    return ret;
}

/**
 * @brief Set the alignment position of the object relative to its parent object.
 * @param obj The object to set the alignment position.
 * @param type The alignment type.
 * @return none
 * @note type should be one of the sgl_align_type_t values:
 *       - SGL_ALIGN_CENTER    : Center the object in the parent object.
 *       - SGL_ALIGN_TOP_MID   : Align the object at the top middle of the parent object.
 *       - SGL_ALIGN_TOP_LEFT  : Align the object at the top left of the parent object.
 *       - SGL_ALIGN_TOP_RIGHT : Align the object at the top right of the parent object.
 *       - SGL_ALIGN_BOT_MID   : Align the object at the bottom middle of the parent object.
 *       - SGL_ALIGN_BOT_LEFT  : Align the object at the bottom left of the parent object.
 *       - SGL_ALIGN_BOT_RIGHT : Align the object at the bottom right of the parent object.
 *       - SGL_ALIGN_LEFT_MID  : Align the object at the left middle of the parent object.
 *       - SGL_ALIGN_RIGHT_MID : Align the object at the right middle of the parent object.
 * 
 * @warning You must set the size of object before calling this function.
 */
void sgl_obj_set_pos_align(sgl_obj_t *obj, sgl_align_type_t type)
{
    SGL_ASSERT(obj != NULL);

    sgl_size_t p_size   = {0};
    sgl_pos_t  p_pos    = {0};
    sgl_pos_t  obj_pos  = {0};
    sgl_size_t obj_size = {
        .w = obj->coords.x2 - obj->coords.x1 + 1,
        .h = obj->coords.y2 - obj->coords.y1 + 1,
    };

    p_size = (sgl_size_t){
        .w = obj->parent->coords.x2 - obj->parent->coords.x1 + 1,
        .h = obj->parent->coords.y2 - obj->parent->coords.y1 + 1,
    };
    p_pos = (sgl_pos_t){
        .x = obj->parent->coords.x1,
        .y = obj->parent->coords.y1,
    };

    obj_pos = sgl_get_align_pos(&p_size, &obj_size, type);

    sgl_obj_set_abs_pos(obj, p_pos.x + obj_pos.x, p_pos.y + obj_pos.y);
}

/**
 * @brief Set the alignment position of the object relative to sibling object.
 * @param ref The reference object, it should be the sibling object.
 * @param obj The object to set the alignment position.
 * @param type The alignment type.
 * @return none
 * @note type should be one of the sgl_align_type_t values:
 *       - SGL_ALIGN_VERT_LEFT  : Align the object at the left side of the reference object.
 *       - SGL_ALIGN_VERT_RIGHT : Align the object at the right side of the reference object.
 *       - SGL_ALIGN_VERT_MID   : Align the object at the middle of the reference object.
 *       - SGL_ALIGN_HORIZ_TOP  : Align the object at the top side of the reference object.
 *       - SGL_ALIGN_HORIZ_BOT  : Align the object at the bottom side of the reference object.
 *       - SGL_ALIGN_HORIZ_MID  : Align the object at the middle of the reference object.
 * 
 * @warning You must set the size of object before calling this function.
 */
void sgl_obj_set_pos_align_ref(sgl_obj_t *ref, sgl_obj_t *obj, sgl_align_type_t type)
{
    SGL_ASSERT(ref != NULL && obj != NULL);

    if (unlikely(ref == obj->parent)) {
        sgl_obj_set_pos_align(obj, type);
        return;
    }

    int16_t ref_w = ref->coords.x2 - ref->coords.x1 + 1;
    int16_t obj_w = obj->coords.x2 - obj->coords.x1 + 1;
    int16_t ref_h = ref->coords.y2 - ref->coords.y1 + 1;
    int16_t obj_h = obj->coords.y2 - obj->coords.y1 + 1;

    switch (type) {
    case SGL_ALIGN_VERT_MID:
        obj->coords.x1 = ref->coords.x1 + (ref_w - obj_w) / 2;
        obj->coords.x2 = obj->coords.x1 + obj_w - 1;
    break;

    case SGL_ALIGN_VERT_LEFT:
        obj->coords.x1 = ref->coords.x1;
        obj->coords.x2 = obj->coords.x1 + obj_w - 1;
    break;

    case SGL_ALIGN_VERT_RIGHT:
        obj->coords.x1 = ref->coords.x2 - obj_w + 1;
        obj->coords.x2 = obj->coords.x1 + obj_w - 1;
    break;

    case SGL_ALIGN_HORIZ_MID:
        obj->coords.y1 = ref->coords.y1 + (ref_h - obj_h) / 2;
        obj->coords.y2 = obj->coords.y1 + obj_h - 1;
    break;

    case SGL_ALIGN_HORIZ_TOP:
        obj->coords.y1 = ref->coords.y1;
        obj->coords.y2 = obj->coords.y1 + obj_h - 1;
    break;

    case SGL_ALIGN_HORIZ_BOT:
        obj->coords.y1 = ref->coords.y2 - obj_h + 1;
        obj->coords.y2 = obj->coords.y1 + obj_h - 1;
    break;

    default:
        SGL_LOG_WARN("invalid align type");
    break;
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the layout of the object.
 * @param obj The object to set the layout.
 * @param desc The layout description.
 * @return none
 * @note The layout description should be one of the sgl_layout_desc_t values:
 *       - SGL_LAYOUT_NONE        : No layout.
 *       - SGL_LAYOUT_HORIZONTAL  : Horizontal layout.
 *       - SGL_LAYOUT_VERTICAL    : Vertical layout.
 *       - SGL_LAYOUT_GRID        : Grid layout.
 * @warning You must set col_num and row_num if the layout type is SGL_LAYOUT_GRID.
 */
void sgl_obj_set_layout(sgl_obj_t *obj, sgl_layout_desc_t *desc)
{
    SGL_ASSERT(obj != NULL);
    if ((!sgl_obj_has_child(obj)) || (desc->type == SGL_LAYOUT_NONE)) {
        return;
    }

    sgl_obj_t *child = NULL;
    size_t child_num = sgl_obj_get_child_count(obj);
    int16_t child_span_x[64] = {0}, child_span_y[64] = {0}, x = 0, y = 0, child_pos_x = 0, child_pos_y = 0;

    switch (desc->type ) {
    case SGL_LAYOUT_HORIZONTAL:
        sgl_split_len_avg((obj->coords.x2 - obj->coords.x1 + 1 - obj->border * 2 - desc->left_space - desc->right_space), 
                            child_num, desc->col_space, child_span_x
                         );
        child_pos_x = obj->coords.x1 + obj->border + desc->left_space;

        sgl_obj_for_each_child(child, obj) {
            child->coords.x1 = child_pos_x;
            child->coords.x2 = child_pos_x + child_span_x[x] - 1;
            child->coords.y1 = obj->coords.y1 + desc->top_space;
            child->coords.y2 = obj->coords.y2 - desc->bottom_space;
            child_pos_x += (child_span_x[x++] + desc->col_space);
            child->area = child->coords;
        }
        break;

    case SGL_LAYOUT_VERTICAL:
        sgl_split_len_avg((obj->coords.y2 - obj->coords.y1 + 1 - obj->border * 2 - desc->top_space - desc->bottom_space), 
                            child_num, desc->row_space, child_span_y
                         );
        child_pos_y = obj->coords.y1 + obj->border + desc->top_space;

        sgl_obj_for_each_child(child, obj) {
            child->coords.x1 = obj->coords.x1 + desc->left_space;
            child->coords.x2 = obj->coords.x2 - desc->right_space;
            child->coords.y1 = child_pos_y;
            child->coords.y2 = child_pos_y + child_span_y[y] - 1;
            child_pos_y += (child_span_y[y++] + desc->row_space);
            child->area = child->coords;
        }
        break;

    case SGL_LAYOUT_GRID:
        if (desc->col_num == 0 || desc->row_num == 0 || child_num == 0) {
            break;
        }

        sgl_split_len_avg((obj->coords.x2 - obj->coords.x1 + 1 - obj->border * 2 - desc->left_space - desc->right_space), 
                            desc->col_num, desc->col_space, child_span_x
                         );
        sgl_split_len_avg((obj->coords.y2 - obj->coords.y1 + 1 - obj->border * 2 - desc->top_space - desc->bottom_space), 
                            desc->row_num, desc->row_space, child_span_y
                         );

        sgl_obj_for_each_child(child, obj) {
            if (y >= desc->row_num || x >= desc->col_num) {
                break;
            }

            child_pos_x = obj->coords.x1 + obj->border + desc->left_space;
            for (int i = 0; i < x; i++) {
                child_pos_x += child_span_x[i] + desc->col_space;
            }

            child_pos_y = obj->coords.y1 + obj->border + desc->top_space;
            for (int i = 0; i < y; i++) {
                child_pos_y += child_span_y[i] + desc->row_space;
            }

            child->coords.x1 = child_pos_x;
            child->coords.x2 = child_pos_x + child_span_x[x] - 1;
            child->coords.y1 = child_pos_y;
            child->coords.y2 = child->coords.y1 + child_span_y[y] - 1;
            child->area = child->coords;

            x++;
            if (x >= desc->col_num) {
                x = 0;
                y++;
            }
        }
        break;

    default:
        SGL_LOG_WARN("invalid layout type");
        return;
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief draw object slice completely
 * @param obj it should point to active root object
 * @param surf surface that draw to
 * @param dirty_h dirty height
 * @return none
 */
static inline void draw_obj_slice(sgl_obj_t *obj, sgl_surf_t *surf)
{
    int top = 0;
    sgl_event_t evt;
    sgl_obj_t *stack[SGL_OBJ_DEPTH_MAX];

    SGL_ASSERT(obj != NULL);
    stack[top++] = obj;

    while (top > 0) {
        SGL_ASSERT(top < SGL_OBJ_DEPTH_MAX);
        obj = stack[--top];

        if (obj->sibling != NULL) {
            stack[top++] = obj->sibling;
        }

        if (sgl_obj_is_hidden(obj)) {
            continue;
        }

        if (sgl_surf_area_is_overlap(surf, &obj->area)) {
            evt.type = SGL_EVENT_DRAW_MAIN;
            SGL_ASSERT(obj->construct_fn != NULL);
            obj->construct_fn(surf, obj, &evt);

#if (CONFIG_SGL_FOCUSED_WIDTH)
            /* draw focus border */
            if (unlikely(obj->focus)) {
                sgl_draw_wireframe(surf, &obj->area, &obj->coords, obj->radius, SGL_FOCUSED_WIDTH, SGL_FOCUSED_COLOR, SGL_ALPHA_MAX);
            }
#endif
            if (obj->child != NULL) {
                stack[top++] = obj->child;
            }
        }
    }

#if (CONFIG_SGL_DIRTY_AREA_TRACE)
    if (sgl_system.fbdev.trace_flag) {
        sgl_draw_wireframe(surf, (sgl_area_t*)surf, surf->dirty, 0, 1, SGL_DIRTY_AREA_TRACE_COLOR, SGL_ALPHA_MAX);
    }
#endif
#if (CONFIG_SGL_MONITOR_TRACE)
    sgl_monitor_trace(surf);
#endif
    /* flush dirty area into screen */
    sgl_fbdev_flush_area((sgl_area_t*)surf, surf->buffer);
}

/**
 * @brief collect all dirty area by for each all object that is dirty and visible
 * @param obj it should point to active root object
 * @return none
 * @note if there is no dirty area, the dirty area will remain unchanged
 */
static inline void sgl_dirty_area_harvest(sgl_obj_t *obj)
{
    sgl_obj_t *stack[SGL_OBJ_DEPTH_MAX];
    int top = 0;
    stack[top++] = obj;

    /* for each all object from the first task of page */
    while (top > 0) {
        SGL_ASSERT(top < SGL_OBJ_DEPTH_MAX);
        obj = stack[--top];

        /* if sibling exists, push it to stack, it will be pop in next loop */
        if (obj->sibling != NULL) {
            stack[top++] = obj->sibling;
        }

        /* check if obj is destroyed */
        if (unlikely(sgl_obj_is_destroyed(obj))) {
            /* merge destroy area */
            sgl_dirty_area_push(&obj->area);
            /* remove obj from parent */
            sgl_obj_remove(obj);
            /* free obj resource */
            sgl_obj_free(obj);
            /* object is destroyed, skip */
            continue;
        }

        /* if object is hidden, skip it */
        if (unlikely(sgl_obj_is_hidden(obj))) {
            continue;
        }

        /* check child dirty and merge all dirty area */
        if (sgl_obj_is_dirty(obj)) {
            /* merge dirty area */
            sgl_dirty_area_push(&obj->area);

            sgl_area_t fill_area = sgl_obj_get_fill_rect(obj->parent);
            /* update obj area */
            if (unlikely(!sgl_area_clip(&fill_area, &obj->coords, &obj->area))) {
                sgl_area_init(&obj->area);
                sgl_obj_clear_dirty(obj);
                continue;
            }

            /* merge dirty area */
            sgl_dirty_area_push(&obj->area);

            /* clear dirty flag */
            sgl_obj_clear_dirty(obj);
        }

        if (obj->child != NULL) {
            stack[top++] = obj->child;
        }
    }
}

/**
 * @brief sgl to draw complete frame
 * @param fbdev point to  frame buffer device
 * @param dirty_area point to dirty area
 * @param dirty_num dirty area number
 * @return none
 * @note this function should be called in deamon thread or cyclic thread
 */
static inline void sgl_draw_task(sgl_fbdev_t *fbdev, sgl_area_t *dirty_area, uint8_t dirty_num)
{
    sgl_surf_t *surf = &fbdev->surf;
    sgl_obj_t  *head = fbdev->active;
    sgl_area_t *dirty = NULL;

    /* dirty area number must less than SGL_DIRTY_AREA_MAX */
    for (uint8_t i = 0; i < dirty_num; i++) {
        dirty = &dirty_area[i];

#if (CONFIG_SGL_FBDEV_EVEN_COORDS)
        /* floor and ceil dirty area, it is for QSPI display*/
        dirty->x1 = sgl_floor_even(dirty->x1);
        dirty->y1 = sgl_floor_even(dirty->y1);
        dirty->x2 = sgl_ceil_odd(dirty->x2);
        dirty->y2 = sgl_ceil_odd(dirty->y2);
#endif
        surf->dirty = dirty;

#if (CONFIG_SGL_FBDEV_RUNTIME_ROTATION)
        /* if the dirty area is out of screen, skip it */
        sgl_area_t screen = { .x1 = 0, .y1 = 0, .x2 = SGL_SCREEN_WIDTH - 1, .y2 = SGL_SCREEN_HEIGHT - 1 };
        if (!sgl_area_selfclip(dirty, &screen)) {
            continue;
        }
#endif
        //SGL_LOG_TRACE("[fb:%d]sgl_draw_task: dirty area x1:%d y1:%d x2:%d y2:%d", fbdev->fb_emit, dirty->x1, dirty->y1, dirty->x2, dirty->y2);
        /* check dirty area, ensure it is valid */
        SGL_ASSERT(dirty->x1 >= 0 && dirty->y1 >= 0 && dirty->x2 < SGL_SCREEN_WIDTH && dirty->y2 < SGL_SCREEN_HEIGHT);

#if (!CONFIG_SGL_USE_FBDEV_VRAM)

        uint16_t draw_h = 0;
        surf->h = dirty->y2 - dirty->y1 + 1;
        surf->x1 = dirty->x1;
        surf->y1 = dirty->y1;
        surf->x2 = dirty->x2;
        surf->w  = surf->x2 - surf->x1 + 1;
        surf->h  = sgl_min(surf->size / surf->w, surf->h);

        while (surf->y1 <= dirty->y2) {
            draw_h = sgl_min(dirty->y2 - surf->y1 + 1, surf->h);
            surf->y2 = surf->y1 + draw_h - 1;

            /* wait until at least one framebuffer buffer is ready */
            while (sgl_fbdev_flush_wait_ready(fbdev));

            /* draw into the ready buffer, switch if the emitting one is not ready */
            if (fbdev->fbinfo.buffer[1] != NULL && !(fbdev->fb_ready & (1 << fbdev->fb_emit))) {
                fbdev->fb_emit ^= 1;
                surf->buffer = (sgl_color_t *)fbdev->fbinfo.buffer[fbdev->fb_emit];
            }

            /* reset the ready flag of the buffer to be drawn */
            fbdev->fb_ready &= ~(1 << fbdev->fb_emit);

            /* draw object slice until the dirty area is finished */
            draw_obj_slice(head, surf);
            surf->y1 += draw_h;
        }
#else
        /* wait current framebuffer for ready */
        while (sgl_fbdev_flush_wait_ready(fbdev));
        draw_obj_slice(head, surf);
#endif
    }
}

/**
 * @brief sgl task handler function with sync mode
 * @param none
 * @return none
 * @note you can call this function for force update screen
 */
void sgl_task_handler_sync(void)
{
    /* event and animation task */
    sgl_event_task();
    sgl_anim_task();

    if (sgl_system.fbdev.update_flag) {
        /* foreach all object tree and calculate dirty area */
        sgl_dirty_area_harvest(sgl_system.fbdev.active);
        /* reset the flag */
        sgl_system.fbdev.update_flag = 0;
    }

#if (CONFIG_SGL_DIRTY_AREA_TRACE)
    /* update trace dirty area */
    if (sgl_system.fbdev.dirty_num) {
        sgl_system.fbdev.trace_flag = false;
        sgl_draw_task(&sgl_system.fbdev, sgl_system.fbdev.trace_dirty, sgl_system.fbdev.trace_dirty_num);

        sgl_system.fbdev.trace_dirty_num = sgl_system.fbdev.dirty_num;
        sgl_system.fbdev.trace_flag = true;

        for (int i = 0; i < sgl_system.fbdev.dirty_num; i++) {
            sgl_system.fbdev.trace_dirty[i] = sgl_system.fbdev.dirty[i];
        }
    }
#endif
#if (CONFIG_SGL_MONITOR_TRACE)
    if (sgl_system.fbdev.dirty_num) {
        sgl_update_area(&SGL_MONITOR_COORDS);
    }
#endif

    /* draw all object into screen */
    sgl_draw_task(&sgl_system.fbdev, sgl_system.fbdev.dirty, sgl_system.fbdev.dirty_num);

    /* clear dirty area and fullscreen flag */
    sgl_system.fbdev.dirty_num = 0;
}

/**
 * @brief sgl task handler function
 * @param none
 * @return none
 * @note this function should be called in main loop or timer or thread
 */
void sgl_task_handler(void)
{
    const uint32_t tick = sgl_tick_get();
    /* If the system tick time has not been reached, skip directly. */
    if ((tick - sgl_last_tick_get()) < SGL_SYSTEM_TICK_MS) {
        return;
    }
    /* sync last tick */
    sgl_system.last_tick = tick;

    /* If the system tick time has been reached, execute the task. */
    sgl_task_handler_sync();
}

#if (CONFIG_SGL_BOOT_LOGO)
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
    0x2c, 0xe7, 0x00, 0x42, 0x98, 0x0f, 0x14, 0xe3, 0x88, 0x1f, 0x2c, 0x20, 0x1b, 0xd6, 0x00, 0x00, 0x00, 0x00, 0x5d, 0xb0, 0x00, 0xc2, 0xe2, 0x97, 0x0b, 0x71, 0xe0, 0x6a, 0x6b, 0x00, 0xf1, 0x97,
    0x01, 0x00, 0x6e, 0xc1, 0x07, 0x70, 0x2f, 0xf2, 0x0c, 0xc0, 0x00, 0x8e, 0xe8, 0x00, 0x07, 0xe4, 0x4e, 0x70, 0x0d, 0x70, 0x07, 0xd0, 0x0f, 0x49, 0x93, 0xf0, 0x1f, 0x3a, 0xa2, 0xf1, 0x0f, 0x40,
    0x04, 0xf0, 0x0d, 0x70, 0x07, 0xd0, 0x06, 0xe5, 0x4e, 0x60, 0x00, 0x7e, 0xe7, 0x00, 0x17, 0xcf, 0x00, 0x02, 0x9c, 0xf0, 0x00, 0x00, 0x6f, 0x00, 0x00, 0x06, 0xf0, 0x00, 0x00, 0x6f, 0x00, 0x00,
    0x06, 0xf0, 0x00, 0x00, 0x6f, 0x00, 0x02, 0x38, 0xf3, 0x30, 0xcf, 0xff, 0xff, 0x10, 0x03, 0xbe, 0xd6, 0x00, 0x0d, 0x84, 0x9f, 0x40, 0x00, 0x00, 0x0e, 0x80, 0x00, 0x00, 0x0e, 0x60, 0x00, 0x00,
    0x7e, 0x00, 0x00, 0x04, 0xf4, 0x00, 0x00, 0x5f, 0x50, 0x00, 0x06, 0xf8, 0x33, 0x30, 0x1f, 0xff, 0xff, 0xf0, 0x03, 0xbe, 0xe8, 0x00, 0xa8, 0x37, 0xf7, 0x00, 0x00, 0x0d, 0x90, 0x00, 0x05, 0xf3,
    0x00, 0x5f, 0xf5, 0x00, 0x00, 0x36, 0xe7, 0x00, 0x00, 0x08, 0xe1, 0xd7, 0x46, 0xeb, 0x05, 0xce, 0xea, 0x10, 0x00, 0x00, 0xdf, 0x10, 0x00, 0x0a, 0xcf, 0x10, 0x00, 0x7d, 0x4f, 0x10, 0x03, 0xf2,
    0x3f, 0x10, 0x1e, 0x50, 0x3f, 0x10, 0x7f, 0xff, 0xff, 0xf5, 0x12, 0x22, 0x5f, 0x40, 0x00, 0x00, 0x3f, 0x10, 0x00, 0x00, 0x3f, 0x10, 0x06, 0xff, 0xff, 0x90, 0x07, 0xe3, 0x33, 0x20, 0x08, 0xc0,
    0x00, 0x00, 0x09, 0xed, 0xe9, 0x10, 0x03, 0x63, 0x6e, 0xa0, 0x00, 0x00, 0x07, 0xf0, 0x00, 0x00, 0x08, 0xe0, 0x1e, 0x84, 0x8f, 0x80, 0x05, 0xbe, 0xe8, 0x00, 0x00, 0x3c, 0xfd, 0x60, 0x02, 0xf8,
    0x47, 0x60, 0x0a, 0xa0, 0x00, 0x00, 0x0e, 0x7a, 0xeb, 0x20, 0x0f, 0xe5, 0x4c, 0xd0, 0x0f, 0x60, 0x04, 0xf1, 0x0c, 0x90, 0x05, 0xf1, 0x05, 0xf6, 0x4c, 0xb0, 0x00, 0x6d, 0xfa, 0x10, 0x1f, 0xff,
    0xff, 0xf2, 0x03, 0x33, 0x3c, 0xb0, 0x00, 0x00, 0x4e, 0x10, 0x00, 0x00, 0xd6, 0x00, 0x00, 0x05, 0xf0, 0x00, 0x00, 0x0a, 0xb0, 0x00, 0x00, 0x0d, 0x80, 0x00, 0x00, 0x0f, 0x60, 0x00, 0x00, 0x1f,
    0x50, 0x00, 0x00, 0x9e, 0xea, 0x00, 0x07, 0xe3, 0x3d, 0x80, 0x09, 0xa0, 0x08, 0xa0, 0x03, 0xe5, 0x0c, 0x40, 0x00, 0xbe, 0xfc, 0x00, 0x0b, 0x80, 0x3c, 0xb0, 0x1f, 0x30, 0x04, 0xf1, 0x0e, 0xb3,
    0x3b, 0xe0, 0x02, 0xbe, 0xeb, 0x20, 0x02, 0xbf, 0xd6, 0x00, 0x0d, 0xb3, 0x5f, 0x50, 0x1f, 0x40, 0x07, 0xc0, 0x0f, 0x90, 0x2b, 0xf0, 0x05, 0xff, 0xd8, 0xf0, 0x00, 0x01, 0x07, 0xe0, 0x00, 0x00,
    0x0c, 0xa0, 0x09, 0x74, 0xaf, 0x20, 0x05, 0xcf, 0xc3, 0x00, 0x07, 0x70, 0x2f, 0xf2, 0x0c, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x07, 0x70, 0x2f, 0xf2, 0x0c, 0xc0, 0x9f, 0xff, 0xff, 0x09, 0xe3, 0x33,
    0x30, 0x9d, 0x00, 0x00, 0x09, 0xd0, 0x00, 0x00, 0x9f, 0xff, 0xf5, 0x09, 0xe3, 0x33, 0x10, 0x9d, 0x00, 0x00, 0x09, 0xe3, 0x33, 0x30, 0x9f, 0xff, 0xff, 0x10, 0x5f, 0xff, 0xff, 0x25, 0xf4, 0x33,
    0x30, 0x5f, 0x10, 0x00, 0x05, 0xf1, 0x00, 0x00, 0x5f, 0xff, 0xf8, 0x05, 0xf4, 0x33, 0x10, 0x5f, 0x10, 0x00, 0x05, 0xf1, 0x00, 0x00, 0x5f, 0x10, 0x00, 0x00, 0x1f, 0xa0, 0x0a, 0xf1, 0x1f, 0xe0,
    0x0e, 0xf1, 0x1f, 0xc4, 0x4c, 0xf1, 0x1f, 0x98, 0x98, 0xf1, 0x1f, 0x4d, 0xd4, 0xf1, 0x1f, 0x2d, 0xc2, 0xf1, 0x1f, 0x25, 0x52, 0xf1, 0x1f, 0x20, 0x02, 0xf1, 0x1f, 0x20, 0x02, 0xf1, 0xcf, 0xff,
    0xc4, 0x0c, 0xb3, 0x3a, 0xf1, 0xca, 0x00, 0x2f, 0x4c, 0xa0, 0x18, 0xf1, 0xcf, 0xff, 0xe5, 0x0c, 0xb2, 0x20, 0x00, 0xca, 0x00, 0x00, 0x0c, 0xa0, 0x00, 0x00, 0xca, 0x00, 0x00, 0x00, 0x00, 0x9e,
    0xeb, 0x20, 0x08, 0xf7, 0x5a, 0xa0, 0x0b, 0xc0, 0x00, 0x00, 0x06, 0xfa, 0x30, 0x00, 0x00, 0x4d, 0xfb, 0x20, 0x00, 0x00, 0x4d, 0xe0, 0x01, 0x00, 0x05, 0xf2, 0x0e, 0xb6, 0x6c, 0xe0, 0x02, 0xae,
    0xeb, 0x20
};

static const sgl_font_table_t font_table[] = {
    {.bitmap_index = 0,   .adv_w = 0,   .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0,   .adv_w = 128, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 36,  .adv_w = 128, .box_w = 4, .box_h = 3, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 42,  .adv_w = 128, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 78,  .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
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
    0x00, 0x09, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x20, 0x21, 0x28, 0x2b, 0x2e
};
static const sgl_font_unicode_t font_unicode[] = {
    { .offset = 0x25, .len = 18, .list = unicode_list_0, .tab_offset = 1, }
};

const sgl_font_t monitor_font = {
    .bitmap = font_bitmap,
    .table = font_table,
    .font_table_size = SGL_ARRAY_SIZE(font_table),
    .font_height = 9,
    .base_line = 0,
    .bpp = 4,
    .format = SGL_FONT_FMT_NORMAL,
    .unicode = font_unicode,
    .unicode_num = SGL_ARRAY_SIZE(font_unicode),
};

/**
 * @brief draw the fps and memory usage monitor overlay onto the surface
 * @param surf surface that draw to, only the area overlapping with the monitor will be redrawn
 * @return none
 * @note on the first call it creates the monitor page with two children: an fps
 *       label and a memory usage label, then it refreshes both strings once per
 *       system tick and redraws the monitor onto every flushed slice. if runtime
 *       rotation moves the monitor out of the screen, it will be deleted
 */
void sgl_monitor_trace(sgl_surf_t *surf)
{
    uint32_t cur_tick = sgl_last_tick_get();
    sgl_area_t monitor_area = {
        .x1 = SGL_MONITOR_COORDS_X,
        .x2 = SGL_MONITOR_COORDS_X + SGL_MONITOR_COORDS_WIDTH - 1,
        .y1 = SGL_MONITOR_COORDS_Y,
        .y2 = SGL_MONITOR_COORDS_Y + SGL_MONITOR_COORDS_HEIGHT - 1,
    };
    sgl_event_t evt = {0};
    sgl_obj_t *child;
    static char fps_str[16] = {0};
    static char mem_str[16] = {0};
    static sgl_obj_t *fps = NULL;
    static sgl_obj_t *mem = NULL;
    static sgl_monitor_fps_t fps_calc = {0};

    uint32_t tick_used = cur_tick - fps_calc.last_tick;
    if (tick_used >= SGL_SYSTEM_TICK_MS) {
        uint32_t instant_fps = 1000 / tick_used;
        fps_calc.history[fps_calc.index] = instant_fps;
        fps_calc.index = (fps_calc.index + 1) % 8;
        if (fps_calc.count < 8) fps_calc.count++;

        uint32_t fps_sum = 0;
        for (uint8_t i = 0; i < fps_calc.count; i++) {
            fps_sum += fps_calc.history[i];
        }
        uint32_t fps_avg = fps_calc.count > 0 ? fps_sum / fps_calc.count : 0;
        fps_calc.last_tick = cur_tick;

        sgl_snprintf(fps_str, sizeof(fps_str), "FPS:%d", fps_avg);
        sgl_snprintf(mem_str, sizeof(mem_str), "MEM:%d.%d%", sgl_mm_get_monitor().used_rate >> 8, sgl_mm_get_monitor().used_rate & 0xff);
    }

#if (CONFIG_SGL_FBDEV_RUNTIME_ROTATION)
    if ((monitor->coords.y2 + 1) == SGL_SCREEN_WIDTH || (monitor->coords.x2 + 1) == SGL_SCREEN_HEIGHT) {
        sgl_obj_delete(monitor);
        monitor = NULL;
        return;
    }
#endif
    sgl_draw_fill_rect(surf, &monitor_area, &monitor_area, 0, SGL_MONITOR_BG_COLOR, SGL_MONITOR_ALPHA);

    sgl_draw_string(surf, &monitor_area, monitor_area.x1, monitor_area.y1 + 3, (const char*)fps_str,
                                                    SGL_MONITOR_TEXT_COLOR, SGL_MONITOR_ALPHA, &monitor_font);
    sgl_draw_string(surf, &monitor_area, monitor_area.x1, monitor_area.y1 + 3 + SGL_MONITOR_COORDS_HEIGHT / 2, (const char*)mem_str,
                                                    SGL_MONITOR_TEXT_COLOR, SGL_MONITOR_ALPHA, &monitor_font);
}
#endif

/**
 * @brief Count number of options in \n-separated text
 * @param text newline-separated option string
 * @return number of options
 */
uint16_t sgl_separated_option_get_count(const char *text)
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
int sgl_separated_option_get_offset(const char *text, int index)
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
int sgl_separated_option_get_text_len(const char *text, int offset)
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

    if (since_win > SGL_SCROLL_VEL_WINDOW_MS) {
        sc->speed = 0;
    } else if (sc->win_dist != 0) {
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

    if (sc->speed != 0) {
        int32_t delta = ((int32_t)sc->speed * (int32_t)step_ms) / 16;
        uint16_t slices = (uint16_t)(step_ms >> 4);

        if (delta == 0)
            delta = (sc->speed > 0) ? 1 : -1;
        next = sgl_scroll_soft_limit(next + delta, range);

        while (slices--)
            sc->speed = (int16_t)(((int32_t)sc->speed * SGL_SCROLL_INERTIA_NUM) / SGL_SCROLL_INERTIA_DEN);
        if (next < 0 || next > range)
            sc->speed = (int16_t)(sc->speed / 2);
    }

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

    sgl_draw_fill_rect(surf, &obj->area, &thumb, (int16_t)(SGL_SCROLL_BAR_WIDTH / 2), color, sc->bar_alpha);
}
