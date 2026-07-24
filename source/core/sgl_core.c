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
#include <sgl_misc.h>

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
    sgl_system.fbdev.fb_status = 3;
    sgl_system.fbdev.fb_swap = 0;

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
    else {
        if (obj->event_fn) {
            obj->event_fn(evt);
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
    if (area_b->y1 > area_a->y2 || area_b->y2 < area_a->y1 || area_b->x1 > area_a->x2 || area_b->x2 < area_a->x1) {
        return false;
    }

    clip->x1 = sgl_max(area_a->x1, area_b->x1);
    clip->x2 = sgl_min(area_a->x2, area_b->x2);
    clip->y1 = sgl_max(area_a->y1, area_b->y1);
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
    if (area->y1 > clip->y2 || area->y2 < clip->y1 || area->x1 > clip->x2 || area->x2 < clip->x1) {
        return false;
    }

    clip->x1 = sgl_max(clip->x1, area->x1);
    clip->x2 = sgl_min(clip->x2, area->x2);
    clip->y1 = sgl_max(clip->y1, area->y1);
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
 * @brief  free an object
 * @param  obj: object to free
 * @retval none
 * @note this function will free all the itself and children of the object
 */
void sgl_obj_free(sgl_obj_t *obj)
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

        sgl_free(obj);
    }
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
 * @brief  Clear all dirty areas of the object and its children.
 * @param[in] obj  The object to clear.
 * @return  None
 * @note   This function is used to clear all dirty areas of the object and its children.
 */
void sgl_obj_clear_all_dirty(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_obj_t *stack[SGL_OBJ_DEPTH_MAX];
    int top = 0;
    stack[top++] = obj;

    while (top > 0) {
        SGL_ASSERT(top < SGL_OBJ_DEPTH_MAX);
        obj = stack[--top];
        obj->dirty = 0;

        if (obj->sibling != NULL) {
            stack[top++] = obj->sibling;
        }

        if (obj->child != NULL) {
            stack[top++] = obj->child;
        }
    }
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
            sgl_obj_free(obj->child);
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
    SGL_ASSERT(font != NULL);
    int32_t len = 0;
    uint32_t unicode = 0;
    uint32_t ch_index = 0;
    while (*str) {
        str += sgl_utf8_to_unicode(str, &unicode);
        ch_index = sgl_search_unicode_ch_index(font, unicode);
        len += ((font->table[ch_index].adv_w + 8)>> 4);
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

        ch_width = ((font->table[ch_index].adv_w + 8)>> 4);

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
        }

        if (obj->child != NULL) {
            stack[top++] = obj->child;
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

            sgl_event_t evt = {
                .type = SGL_EVENT_DESTROYED,
            };

            /* check construct function */
            SGL_ASSERT(obj->construct_fn != NULL);
            obj->construct_fn(NULL, obj, &evt);

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

        /* check child need init coords */
        if (unlikely(sgl_obj_is_needinit(obj))) {
            sgl_event_t evt = {
                .type = SGL_EVENT_DRAW_INIT,
            };

            /* check construct function */
            SGL_ASSERT(obj->construct_fn != NULL);
            obj->construct_fn(NULL, obj, &evt);
            /* maybe no need to clear flag */
            sgl_obj_clear_needinit(obj);
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
        SGL_LOG_TRACE("[fb:%d]sgl_draw_task: dirty area x1:%d y1:%d x2:%d y2:%d", fbdev->fb_swap, dirty->x1, dirty->y1, dirty->x2, dirty->y2);
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

            /* wait current framebuffer for ready */
            while (sgl_fbdev_flush_wait_ready(fbdev));

            /* reset current framebuffer ready flag */
            fbdev->fb_status = (fbdev->fb_status & (2 - fbdev->fb_swap));

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
    /* event task */
    sgl_event_task();

#if (CONFIG_SGL_ANIMATION)
    sgl_anim_task();
#endif // !CONFIG_SGL_ANIMATION

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
