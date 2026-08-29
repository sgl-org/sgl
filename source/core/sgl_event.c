/* source/core/sgl_event.c
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

#include <sgl_event.h>
#include <sgl_math.h>
#include <sgl_cfgfix.h>
#include <sgl_log.h>
#include <string.h>
#include <sgl_mm.h>

/* define event queue size */
#define SGL_EVENT_QUEUE_SIZE          (CONFIG_SGL_EVENT_QUEUE_SIZE)
#define SGL_EVENT_QUEUE_SIZE_MASK     (SGL_EVENT_QUEUE_SIZE - 1)

/**
 * @brief event queue struct
 * @buffer: event buffer to save all event data
 * @in: event queue in which is used to push event
 * @out: event queue out which is used to pop event
 * @count: event queue count which have been pushed
 */
typedef struct event_queue {
    sgl_event_t buffer[SGL_EVENT_QUEUE_SIZE];
    uint16_t    in;
    uint16_t    out;
} event_queue_t;

/**
 * @brief event context struct
 * @last_click: last click object which may be lost event
 * @last_touch: last touch position
 * @evtq: event queue
 */
static struct event_context {
    struct sgl_obj *last_click;
    struct sgl_obj *last_motion;
    sgl_event_pos_t last_touch;
    event_queue_t   evtq;
} evt_ctx;

/**
 * @brief key group active
 */
static sgl_key_group_t *key_grp_active = NULL;

#define grp_get_index(group, index)          (group)->obj[group->index]
#define grp_get_focused(group)               (group)->obj[group->focused]
#define grp_get_first(group)                 (group)->obj[0]
#define grp_get_last(group)                  (group)->obj[group->count - 1];
#define grp_is_focused(group)                ((group)->focused != -1)
#define grp_is_editing(group)                ((group)->editing)
#define grp_is_pressed(group)                ((group)->pressed)

#define grp_active_get_focused()             (key_grp_active->obj[key_grp_active->focused])
#define grp_active_is_editing()              (key_grp_active->editing)
#define grp_active_is_pressed()              (key_grp_active->pressed)
#define grp_active_is_focused()              (key_grp_active->focused != -1)
#define grp_active_set_focused(index)        (key_grp_active->focused = index)
#define grp_active_get_first()               (key_grp_active->obj[0])
#define grp_active_get_last()                (key_grp_active->obj[key_grp_active->count - 1])

/**
 * @brief Initialize the event queue
 * @param none
 * @return 0 on success, -1 on failure
 * @note !!!!!! the SGL_EVENT_QUEUE_SIZE must be power of 2 !!!!!!
 *       You must check the return value of this function.
 */
int sgl_event_queue_init(void)
{
    if (!sgl_is_pow2(SGL_EVENT_QUEUE_SIZE)) {
        SGL_LOG_ERROR("The capacity must be power of 2");
        return -1;
    }

    evt_ctx.evtq.in = evt_ctx.evtq.out = 0;
    return 0;
}

/**
 * @brief Check whether the event queue is empty
 * @param none
 * @return true if the event queue is empty, false otherwise
 */
static inline bool sgl_event_queue_is_empty(void)
{
    return evt_ctx.evtq.in == evt_ctx.evtq.out;
}

/**
 * @brief Check whether the event queue is full
 * @param none
 * @return true if the event queue is full, false otherwise
 */
static inline bool sgl_event_queue_is_full(void)
{
    return (evt_ctx.evtq.in - evt_ctx.evtq.out) >= SGL_EVENT_QUEUE_SIZE;
}

/**
 * @brief Push an event into the event queue
 * @param event The event to be pushed
 * @return 0 on success, -1 on failure
 */
void sgl_event_queue_push(sgl_event_t event)
{
    uint16_t index;
    if (unlikely(sgl_event_queue_is_full())) {
        SGL_LOG_ERROR("Event queue is full, maybe system is too slow");
        return;
    }

    index = evt_ctx.evtq.in % SGL_EVENT_QUEUE_SIZE;
    evt_ctx.evtq.buffer[index] = event;
    evt_ctx.evtq.in++;
}

/**
 * @brief Pop an event from the event queue
 * @param out_event The event to be popped
 * @return 0 on success, -1 on failure
 */
static inline int sgl_event_queue_pop(sgl_event_t* out_event)
{
    uint16_t index;
    if (sgl_event_queue_is_empty()) {
        return -1;
    }

    index = evt_ctx.evtq.out % SGL_EVENT_QUEUE_SIZE;
    *out_event = evt_ctx.evtq.buffer[index];
    evt_ctx.evtq.out++;
    return 0;
}

/**
 * @brief Check whether the position is focus on the object
 * @param pos The position to be checked
 * @param rect The rectangle of the object
 * @param radius The radius of the object
 * @return true if the position is focus on the object, false otherwise
 */
static bool pos_is_focus_on_obj(sgl_event_pos_t *pos, sgl_area_t *rect, int16_t radius)
{
    if (pos->x < rect->x1 || pos->x > rect->x2 || pos->y < rect->y1 || pos->y > rect->y2) {
        return false;
    }
    else if(radius == 0) {
        return true;
    }

    if ((pos->x >= rect->x1 + radius) && (pos->x <= rect->x2 - radius)) {
        return true;
    }
    else if ((pos->y >= rect->y1 + radius) && (pos->y <= rect->y2 - radius)) {
        return true;
    }

    if (pos->x <= rect->x1 + radius && pos->y <= rect->y1 + radius) {
        int16_t dx = pos->x - (rect->x1 + radius);
        int16_t dy = pos->y - (rect->y1 + radius);
        return sgl_pow2(dx) + sgl_pow2(dy) <= sgl_pow2(radius);
    }
    else if (pos->x >= rect->x2 - radius && pos->y <= rect->y1 + radius) {
        int16_t dx = pos->x - (rect->x2 - radius);
        int16_t dy = pos->y - (rect->y1 + radius);
        return sgl_pow2(dx) + sgl_pow2(dy) <= sgl_pow2(radius);
    }
    else if (pos->x <= rect->x1 + radius && pos->y >= rect->y2 - radius) {
        int16_t dx = pos->x - (rect->x1 + radius);
        int16_t dy = pos->y - (rect->y2 - radius);
        return sgl_pow2(dx) + sgl_pow2(dy) <= sgl_pow2(radius);
    }
    else if (pos->x >= rect->x2 - radius && pos->y >= rect->y2 - radius) {
        int16_t dx = pos->x - (rect->x2 - radius);
        int16_t dy = pos->y - (rect->y2 - radius);
        return sgl_pow2(dx) + sgl_pow2(dy) <= sgl_pow2(radius);
    }

    return false;
}

/**
 * @brief check whether the position is clicked on the object
 * @param pos The position to be clicked
 * @return The object that is clicked on, NULL if no object is clicked
 */
static struct sgl_obj* click_detect_object(sgl_event_pos_t *pos)
{
    struct sgl_obj *stack[SGL_OBJ_DEPTH_MAX], *obj = sgl_screen_act()->child, *find = NULL;
    int top = 0;

    if (unlikely(obj == NULL)) {
        return NULL;
    }
    stack[top++] = obj;

    while (top > 0) {
        SGL_ASSERT(top < SGL_OBJ_DEPTH_MAX);
        obj = stack[--top];
        if (sgl_obj_has_sibling(obj)) {
            stack[top++] = obj->sibling;
        }

        if (unlikely(sgl_obj_is_hidden(obj))) {
            continue;
        }

        if (pos_is_focus_on_obj(pos, &obj->coords, obj->radius)) {
            find = obj;
            if (sgl_obj_has_child(obj)) {
                stack[top++] = obj->child;
            }
        }
    }

    /**
     * if the object is clickable, return it, otherwise return its parent 
     * because the object may be a label attached to the object
    */
    if (find != NULL) {
        return sgl_obj_is_clickable(find) ? find : find->parent;
    }

    return find;
}

/**
 * @brief Handle the position event
 * @param pos The position to be handled
 * @param type The type of the event
 * @return none
 */
void sgl_event_send_pos(sgl_event_pos_t pos, sgl_event_type_t type)
{
    sgl_event_t event = {
        .obj = NULL,
        .type = type,
        .pos = pos,
    };

    if (type == SGL_EVENT_PRESSED) {
        evt_ctx.last_touch = pos;
    }

    sgl_event_queue_push(event);
}

/**
 * @brief get information of motion event type
 * @param evt [in][out] event to be handled
 * @return none
 */
static void sgl_get_move_info(sgl_event_t *evt)
{
    int16_t dx = evt->pos.x - evt_ctx.last_touch.x;
    int16_t dy = evt->pos.y - evt_ctx.last_touch.y;

    if (sgl_abs(dx) > sgl_abs(dy)) {
        evt->type = dx > 0 ? SGL_EVENT_MOVE_RIGHT : SGL_EVENT_MOVE_LEFT;
        evt->distance = dx;
    }
    else {
        evt->type = dy < 0 ? SGL_EVENT_MOVE_UP : SGL_EVENT_MOVE_DOWN;
        evt->distance = dy;
    }

    evt_ctx.last_touch = evt->pos;
}

/**
 * @brief Callback function for event
 * @param obj The object that triggered the event
 * @param evt The event that triggered the callback
 * @return none
 */
static inline void event_callback(sgl_obj_t *obj, sgl_event_t *evt)
{
    SGL_ASSERT(obj->construct_fn);
    evt->event_data = obj->event_data;
    evt->obj = obj;
    obj->construct_fn(NULL, obj, evt);
}

/**
 * @brief Inject motion event to the object
 * @param obj The object that triggered the event
 * @param evt The event that triggered the callback
 * @return none
 */
static inline void event_inject_motion(sgl_obj_t *obj, sgl_event_t *evt)
{
    while (!sgl_obj_is_movable(obj) && obj != sgl_screen_act()) {
        obj = obj->parent;
    }
    evt_ctx.last_motion = obj;
    event_callback(obj, evt);

    /* call user event function */
    if(obj->event_fn) {
        obj->event_fn(evt);
    }
}

/**
 * @brief All event task in SGL, this function will traverse all elements in the event queue, 
 *        respond to each element with an event, so that all events will trigger and point to the 
 *        corresponding callback function
 * @param none
 * @return none
*/
void sgl_event_task(void)
{
    sgl_event_t evt;
    struct sgl_obj *obj = NULL;

    /* Get event from event queue */
    while (sgl_event_queue_pop(&evt) == 0) {
        obj = evt.obj;

        /* if obj is NULL, it means the event from the input device */
        if (obj == NULL) {
            if (evt.type != SGL_EVENT_MOTION) {
                obj = click_detect_object(&evt.pos);
            } else {
                obj = evt_ctx.last_click;
                if (obj) {
                    sgl_get_move_info(&evt);
                    event_inject_motion(obj, &evt);
                }
                continue;
            }
        }

        if (obj) {
            evt.pos.x = sgl_clamp(evt.pos.x, obj->coords.x1, obj->coords.x2);
            evt.pos.y = sgl_clamp(evt.pos.y, obj->coords.y1, obj->coords.y2);

            if (evt.type == SGL_EVENT_PRESSED) {
                if (obj->pressed) {
                    continue;
                }
                obj->pressed = true;
                evt_ctx.last_click = obj;
            }
            else if (evt.type == SGL_EVENT_RELEASED) {
                if (!obj->pressed) {
                    if (evt_ctx.last_click && evt_ctx.last_click != obj) {
                        evt.obj = evt_ctx.last_click;
                        sgl_event_queue_push(evt);
                    }
                    continue;
                }
                if (evt_ctx.last_motion == obj) {
                    evt_ctx.last_motion = NULL;
                }
                obj->pressed = false;
                evt_ctx.last_click = NULL;
            }

            event_callback(obj, &evt);
            if (evt_ctx.last_motion && evt_ctx.last_motion != obj) {
                event_callback(evt_ctx.last_motion, &evt);
                evt_ctx.last_motion = NULL;
            }

            /* call user event function */
            if(obj->event_fn) {
                obj->event_fn(&evt);
            }
        }
        else {
            SGL_LOG_TRACE("pos is out of object or no event_lost, skip event");
            if (evt.type == SGL_EVENT_RELEASED && evt_ctx.last_click != NULL) {
                evt.obj = evt_ctx.last_click;
                sgl_event_queue_push(evt);
            }
            else {
                obj = sgl_screen_act();
                if(obj->event_fn) {
                    obj->event_fn(&evt);
                }
            }
        }
    }
}

/**
 * @brief Touch event read, this function will be called by user
 * @param x: touch x position
 * @param y: touch y position
 * @param flag: touch flag, it means touch event down or up:
 *              true : touch down
 *              false: touch up
 * @return none
 * @note: for example, you can call it in 30ms tick handler function
 *        void example_30ms_tick_handler(void)
 *        {
 *            int pos_x, pos_y;
 *            bool button_status;
 * 
 *            bsp_touch_read_pos(&pos_x, &pos_y);
 *            button_status = bsp_touch_read_status();
 *            
 *            sgl_event_pos_input(pos_x, pos_y, button_status);
 *        }
 */
void sgl_event_pos_input(int16_t x, int16_t y, bool flag)
{
    static sgl_event_pos_t first_pos, curr_pos;
    static uint32_t start_ms;
    static bool touch;
    const uint32_t tick = sgl_tick_get();
    int16_t dx, dy, pos_x = x, pos_y = y;

    /* rotate touch position */
#if (CONFIG_SGL_FBDEV_ROTATION != 0)
#if (CONFIG_SGL_FBDEV_ROTATION == 90)
    pos_x = sgl_min(SGL_SCREEN_WIDTH - y, SGL_SCREEN_WIDTH - 1);
    pos_y = sgl_min(x, SGL_SCREEN_HEIGHT - 1);
#elif (CONFIG_SGL_FBDEV_ROTATION == 180)
    pos_x = SGL_SCREEN_WIDTH - x - 1;
    pos_y = SGL_SCREEN_HEIGHT - y - 1;
#elif (CONFIG_SGL_FBDEV_ROTATION == 270)
    pos_x = sgl_min(y, SGL_SCREEN_WIDTH - 1);
    pos_y = sgl_min(SGL_SCREEN_HEIGHT - x, SGL_SCREEN_HEIGHT - 1);
#else
    #error "CONFIG_SGL_FBDEV_ROTATION is invalid rotation value (only 0/90/180/270 supported)"
#endif
#elif (CONFIG_SGL_FBDEV_RUNTIME_ROTATION)
    switch (sgl_system.angle) {
    case 90:
        pos_x = sgl_min(SGL_SCREEN_WIDTH - y, SGL_SCREEN_WIDTH - 1);
        pos_y = sgl_min(x, SGL_SCREEN_HEIGHT - 1);
        break;
    case 180:
        pos_x = SGL_SCREEN_WIDTH - x - 1;
        pos_y = SGL_SCREEN_HEIGHT - y - 1;
        break;
    case 270:
        pos_x = sgl_min(y, SGL_SCREEN_WIDTH - 1);
        pos_y = sgl_min(SGL_SCREEN_HEIGHT - x, SGL_SCREEN_HEIGHT - 1);
        break;
    default:
        break;
    }
#endif //!CONFIG_SGL_FBDEV_ROTATION

    if (flag) {
        if (!touch) {
            touch = true;
            first_pos.x = pos_x; first_pos.y = pos_y;
            curr_pos = first_pos;
            start_ms = tick;
            sgl_event_send_pos(first_pos, SGL_EVENT_PRESSED);
            SGL_LOG_INFO("Touch PRESSED: pos: %d, %d", pos_x, pos_y);
        }
        else {
            dx = pos_x - curr_pos.x;
            dy = pos_y - curr_pos.y;
            if (sgl_square_sum(dx, dy) > sgl_pow2(SGL_EVENT_MOVE_THRESHOLD)) {
                curr_pos.x = pos_x;
                curr_pos.y = pos_y;
                sgl_event_send_pos(curr_pos, SGL_EVENT_MOTION);
                SGL_LOG_INFO("Touch MOTION: pos: %d, %d", pos_x, pos_y);
            }
        }
    }
    else {
        if (touch) {
            touch = false;
            dx = first_pos.x - curr_pos.x;
            dy = first_pos.y - curr_pos.y;
            if (sgl_square_sum(dx, dy) <= sgl_pow2(SGL_EVENT_MOVE_THRESHOLD)) {
                if (tick - start_ms < SGL_EVENT_CLICK_INTERVAL) {
                    sgl_event_send_pos(first_pos, SGL_EVENT_CLICKED);
                    SGL_LOG_INFO("Touch CLICKED: pos: %d, %d", first_pos.x, first_pos.y);
                } else {
                    sgl_event_send_pos(first_pos, SGL_EVENT_LONG_CLICKED);
                    SGL_LOG_INFO("Touch LONG_CLICKED: pos: %d, %d", first_pos.x, first_pos.y);
                }
            }
            sgl_event_send_pos(curr_pos, SGL_EVENT_RELEASED);
            SGL_LOG_INFO("Touch RELEASED: pos: %d, %d", curr_pos.x, curr_pos.y);
        }
    }
}

/**
 * @brief Set focus to object
 * @param obj The object to set focus
 * @param flag The flag to set focus
 * @return none
 */
static void event_set_focus(struct sgl_obj *obj, bool flag)
{
    if (obj->focus != flag) {
        obj->focus = flag;
        sgl_obj_set_dirty(obj);
    }
}

/**
 * @brief Callback function for event type
 * @param obj The object that triggered the event
 * @param evt The event
 * @param type The type of the event
 * @return none
 */
static void event_type_callback(struct sgl_obj *obj, sgl_event_t *evt, sgl_event_type_t type)
{
    evt->event_data = obj->event_data;
    evt->type = type;
    evt->obj = obj;
    evt->pos.x = SGL_POS_MIN;
    evt->pos.y = SGL_POS_MIN;
    obj->construct_fn(NULL, obj, evt);

    if (obj->event_fn) {
        obj->event_fn(evt);
    }
}

/**
 * @brief get the prev/next focused object in the key group
 * @param forward: true: get next focused object, false: get prev focused object
 * @return The index of the next/prev focused object
 */
static int16_t grp_active_get_focused_neighbor(bool forward)
{
    if (!key_grp_active || key_grp_active->count == 0) {
        return -1;
    }

    uint16_t count = key_grp_active->count;
    int16_t  cur   = key_grp_active->focused;
    uint16_t idx;

    if (cur < 0 || cur >= (int16_t)count) {
        idx = forward ? 0 : (count - 1);
    }
    else {
        idx = forward ? (uint16_t)((cur + 1) % count)
                      : (uint16_t)((cur == 0) ? (count - 1) : (cur - 1));
    }

    for (uint16_t i = 0; i < count; i++) {
        if (key_grp_active->obj[idx] != NULL) {
            return (int16_t)idx;
        }
        idx = forward ? (uint16_t)((idx + 1) % count)
                      : (uint16_t)((idx == 0) ? (count - 1) : (idx - 1));
    }

    return -1;
}

/**
 * @brief get the previous focused object in the key group
 * @return The index of the previous focused object
 */
static inline int16_t grp_active_get_focused_prev(void)
{
    return grp_active_get_focused_neighbor(false);
}

/**
 * @brief get the next focused object in the key group
 * @return The index of the next focused object
 */
static inline int16_t grp_active_get_focused_next(void)
{
    return grp_active_get_focused_neighbor(true);
}

/**
 * @brief create a key group
 * @param max_num The maximum number of objects in the group
 * @return The pointer to the key group
 * @note: you must check the return value before using the key group
 */
sgl_key_group_t* sgl_key_group_create(uint16_t max_num)
{
    if (max_num == 0) {
        SGL_LOG_ERROR("sgl_key_group_create: max_num is zero");
        return NULL;
    }

    sgl_key_group_t *grp = (sgl_key_group_t*)sgl_malloc(sizeof(struct sgl_key_group) + sizeof(struct sgl_obj*) * max_num);
    if (!grp) {
        SGL_LOG_ERROR("sgl_key_group_create: alloc group failed");
        return NULL;
    }

    grp->count = max_num;
    grp->focused = -1;
    grp->pressed = false;
    grp->editing = false;
    memset(grp->obj, 0, sizeof(struct sgl_obj*) * max_num);

    if (key_grp_active == NULL) {
        key_grp_active = grp;
    }
    return grp;
}

/**
 * @brief Delete a key group
 * @param grp The pointer to the key group
 * @return none
 */
void sgl_key_group_delete(sgl_key_group_t *group)
{
    if (group != NULL) {
        sgl_free(group);
    }
}

/**
 * @brief Add an object to the key group
 * @param group The pointer to the key group
 * @param obj The object to add
 * @return > 0 : success, -1 : failed
 */
int sgl_key_group_add_obj(sgl_key_group_t *group, struct sgl_obj *obj)
{
    if (!group || !obj || group->count == 0) {
        return -1;
    }

    for (uint16_t i = 0; i < group->count; i++) {
        if (group->obj[i] == NULL) {
            group->obj[i] = obj;
            return i;
        }
    }
    return -1;
}

/**
 * @brief Remove an object from the key group
 * @param group The pointer to the key group
 * @param obj The object to remove
 * @return > 0 : success, -1 : failed
 */
int sgl_key_group_remove_obj(sgl_key_group_t *group, struct sgl_obj *obj)
{
    if (!group || !obj || group->count == 0) {
        return -1;
    }

    for (int i = 0; i < group->count; i++) {
        if (group->obj[i] == obj) {
            if (grp_active_get_focused() == obj) {
                key_grp_active->focused = grp_active_get_focused_prev();
                key_grp_active->editing = 0;
                event_set_focus(grp_active_get_focused(), true);
            }

            group->obj[i] = NULL;
            return i;
        }
    }
    return -1;
}

/**
 * @brief Load a key group
 * @param group The pointer to the key group
 * @return none
 */
void sgl_key_group_load(sgl_key_group_t *group)
{
    if (group) {
        if (grp_active_is_focused()) {
            event_set_focus(grp_active_get_focused(), false);
            key_grp_active->editing = 0;
        }

        key_grp_active = group;
        key_grp_active->editing = 0;
        if (grp_active_is_focused()) {
            event_set_focus(grp_active_get_focused(), true);
        }
    }
}

/**
 * @brief Ensure focus
 * @return true: focused, false: not focused
 */
static bool sgl_key_ensure_focus(void)
{
    if (!key_grp_active || key_grp_active->count == 0) {
        return false;
    }

    if (!grp_active_is_focused()) {
        key_grp_active->focused = 0;
        event_set_focus(grp_active_get_focused(), true);
        return false;
    }

    return true;
}

/**
 * @brief Navigate to next/prev focus
 * @param type The event type
 * @param forward The direction of navigation
 * @return none
 */
void sgl_key_navigate(sgl_event_type_t type, bool forward)
{
    if (!sgl_key_ensure_focus()) return;

    if (key_grp_active->editing) {
        sgl_event_t evt;
        event_type_callback(grp_active_get_focused(), &evt, type);
        return;
    }

    if (grp_active_get_focused()) {
        event_set_focus(grp_active_get_focused(), false);
        key_grp_active->focused = forward
                                  ? grp_active_get_focused_next()
                                  : grp_active_get_focused_prev();
        event_set_focus(grp_active_get_focused(), true);
    }
}

/**
 * @brief Physical keyboard event ENTER pressed
 * @param none
 * @return none
 * @note: you can call it in physical keyboard event handler function
 */
void sgl_key_enter_pressed(void)
{
    if (!grp_active_is_focused()) return;

    sgl_obj_t *obj = grp_active_get_focused();
    if (!sgl_obj_is_editable(obj)) {
        sgl_event_t evt;
        event_type_callback(obj, &evt, SGL_EVENT_PRESSED);
        key_grp_active->pressed = 1;
    }
    else {
        if (!grp_active_is_editing()) {
            key_grp_active->editing = 1;
        }
        else {
            sgl_event_t evt;
            sgl_event_type_t type = sgl_obj_is_keypress_click(obj) ? SGL_EVENT_CLICKED : SGL_EVENT_PRESSED;
            event_type_callback(obj, &evt, type);
            key_grp_active->pressed = 1;
        }
    }
}

/**
 * @brief Physical keyboard event ENTER released
 * @param none
 * @return none
 * @note: you can call it in physical keyboard event handler function
 */
void sgl_key_enter_released(void)
{
    if (!grp_active_is_focused()) return;

    if (grp_active_is_pressed()) {
        sgl_event_t evt;
        event_type_callback(grp_active_get_focused(), &evt, SGL_EVENT_RELEASED);
        key_grp_active->pressed = 0;

        if (sgl_key_get_signal(&evt) == SGL_EVENT_DESTROYED) {
            sgl_key_group_remove_obj(key_grp_active, grp_active_get_focused());
        }
    }
}

/**
 * @brief Physical keyboard event ESC
 * @param none
 * @return none
 * @note: you can call it in physical keyboard event handler function
 */
void sgl_key_esc(void)
{
    if (!grp_active_is_focused()) return;

    if (grp_active_is_editing()) {
        sgl_event_t evt;
        event_type_callback(grp_active_get_focused(), &evt, SGL_EVENT_KEY_ESC);
        if (sgl_key_get_signal(&evt) != SGL_EVENT_KEY_CANCEL) {
            key_grp_active->editing = 0;
        }
    }
    else {
        event_set_focus(grp_active_get_focused(), false);
        key_grp_active->focused = -1;
    }
}

/**
 * @brief Physical encoder input
 * @param diff: encoder delta value (positive=CW, negative=CCW)
 * @param pressed: button pressed status (true=pressed, false=released)
 * @return none
 * @note: Call this function in your encoder ISR/timer handler. Handles both navigation and long-press.
 */
void sgl_encoder_input(int8_t diff, bool pressed)
{
    static bool encoder_status = false;
    static uint32_t press_start_ms = 0;
    const uint32_t tick = sgl_tick_get();

    /* Handle rotation (navigation) */
    if (diff > 0) {
        /* CW rotation - move forward to next item */
        for (int i = 0; i < diff; i++) {
            sgl_key_navigate(SGL_EVENT_KEY_DOWN, true);
        }
    }
    else if (diff < 0) {
        /* CCW rotation - move backward to prev item */
        int16_t steps = sgl_abs(diff);
        for (int i = 0; i < steps; i++) {
            sgl_key_navigate(SGL_EVENT_KEY_UP, false);
        }
    }

    /* Handle button press/release with long-press detection */
    if (encoder_status != pressed) {
        if (pressed) {
            /* Button just pressed - record start time */
            encoder_status = true;
            press_start_ms = tick;
            sgl_key_enter_pressed();
            SGL_LOG_INFO("Encoder button PRESSED at %lu", (unsigned long)tick);
        }
        else {
            /* Button just released - check duration */
            encoder_status = false;
            uint32_t duration = tick - press_start_ms;

            /* Check if it's a long press (>500ms) */
            if (duration >= SGL_EVENT_CLICK_INTERVAL) {
                SGL_LOG_INFO("Encoder LONG-PRESSED: %lu ms", (unsigned long)duration);
                /* Trigger ESC action for long press */
                sgl_key_esc();
            }
            else {
                SGL_LOG_INFO("Encoder CLICKED: %lu ms", (unsigned long)duration);
            }
            /* Release the button event */
            sgl_key_enter_released();
        }
    }
}
