/* source/include/sgl_event.h
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

#ifndef __SGL_EVENT_H__
#define __SGL_EVENT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sgl_cfgfix.h>
#include <stddef.h>
#include <sgl_list.h>
#include <sgl_types.h>

/* Forward declaration of sgl_obj and sgl_page*/
struct sgl_obj;
struct sgl_page;

/**
 * @brief Event type enumeration
 * @note  The event type is used to describe the type of event, such as click, 
 *        long press, etc. The event type is used to distinguish different events, 
 *        and the event type is used to trigger different callbacks
 */
/* General event type */
#define  SGL_EVENT_NULL                 (0)
#define  SGL_EVENT_NORMAL               (1)
#define  SGL_EVENT_PRESSED              (2)
#define  SGL_EVENT_RELEASED             (3)
#define  SGL_EVENT_CLICKED              (4)
#define  SGL_EVENT_LONG_CLICKED         (5)
#define  SGL_EVENT_MOTION               (6)
#define  SGL_EVENT_MOVE_UP              (7)
#define  SGL_EVENT_MOVE_DOWN            (8)
#define  SGL_EVENT_MOVE_LEFT            (9)
#define  SGL_EVENT_MOVE_RIGHT           (10)
#define  SGL_EVENT_LONG_PRESSED         (11)
#define  SGL_EVENT_DRAW_INIT            (12)
#define  SGL_EVENT_DRAW_MAIN            (13)
#define  SGL_EVENT_FOCUSED              (14)
#define  SGL_EVENT_UNFOCUSED            (15)
#define  SGL_EVENT_DESTROYED            (16)
/* Key event type */
#define  SGL_EVENT_KEY_UP               (17)
#define  SGL_EVENT_KEY_DOWN             (18)
#define  SGL_EVENT_KEY_LEFT             (19)
#define  SGL_EVENT_KEY_RIGHT            (20)
#define  SGL_EVENT_KEY_ESC              (21)
#define  SGL_EVENT_KEY_CANCEL           (22)

#define  sgl_event_type_t               uint8_t

#define  SGL_EVENT_MOVE_THRESHOLD       (2)
#define  SGL_EVENT_CLICK_INTERVAL       (500)

/**
* @brief Event location structure, Used to represent the coordinates of an event, 
*        such as the click position of the mouse, the click position of the touch screen, 
*        and so on
*
* @x: x coordinate
* @y: y coordinate
*/
typedef struct sgl_event_pos {
    int16_t x;
    int16_t y;
} sgl_event_pos_t;

/**
 * @brief Event structure, Used to represent an event, such as click, long press, etc.
 *
 * @obj: The object that triggered the event
 * @event_data: The private data of the event
 * @pos: The position of the event
 * @type: The type of the event
 * @distance: The move distance of the event
 */
typedef struct sgl_event {
    struct sgl_obj   *obj;
    void             *event_data;
    sgl_event_pos_t  pos;
    uint16_t         type;
    int16_t          distance;
} sgl_event_t;

/**
 * @brief key group struct
 * @count: The number of keys in the group
 * @pressed: Whether the key group is pressed
 * @editing: Whether the key group is editing
 * @focused: The focused of the key group
 * @enter_start_ms: Timestamp when ENTER was pressed
 * @obj: The object of the key group
 */
typedef struct sgl_key_group {
    uint16_t         count : 14;
    uint16_t         pressed : 1;
    uint16_t         editing : 1;
    int16_t          focused;
    uint32_t         enter_start_ms;  
    struct sgl_obj   *obj[];
} sgl_key_group_t;

/**
 * @brief Get the target object of the event
 * @param event The event to be handled
 * @return The target object of the event
 */
static inline struct sgl_obj* sgl_event_get_target(sgl_event_t *event)
{
    return event->obj;
}

/**
 * @brief Get the data of the event
 * @param event The event to be handled
 * @return The data of the event
 */
static inline void* sgl_event_get_data(sgl_event_t *event)
{
    return event->event_data;
}

/**
 * @brief Get the position of the event
 * @param event The event to be handled
 * @return The position of the event
 */
static inline sgl_event_pos_t sgl_event_get_pos(sgl_event_t *event)
{
    return event->pos;
}

/**
 * @brief Get the type of the event
 * @param event The event to be handled
 * @return The type of the event
 */
static inline sgl_event_type_t sgl_event_get_type(sgl_event_t *event)
{
    return event->type;
}

/**
 * @brief Get the move distance of the event
 * @param event The event to be handled
 * @return The move distance of the event
 */
static inline uint16_t sgl_event_get_move_distance(sgl_event_t *event)
{
    return event->distance;
}

/**
 * @brief Initialize the event queue
 * @param none
 * @return 0 on success, -1 on failure
 * @note !!!!!! the SGL_EVENT_QUEUE_SIZE must be power of 2 !!!!!!
 *       You must check the return value of this function.
 */
int sgl_event_queue_init(void);

/**
 * @brief Push an event into the event queue
 * @param event The event to be pushed
 * @return 0 on success, -1 on failure
 */
void sgl_event_queue_push(sgl_event_t event);

/**
 * @brief Clear event context references if object being deleted is referenced
 * @param obj point to the object about to be freed
 * @return none
 * @note This prevents use-after-free when the object is still referenced in event context.
 *       This is an internal API, called by sgl_core.c during object cleanup.
 */
void sgl_event_ctx_cleanup(struct sgl_obj *obj);

/**
 * @brief Handle the position event
 * @param pos The position to be handled
 * @param type The type of the event
 * @return none
 */
void sgl_event_send_pos(sgl_event_pos_t pos, sgl_event_type_t type);

/**
 * @brief Send an event to the specified object
 * @param event The event to be sent
 * @return none
 */
static inline void sgl_event_send(sgl_event_t event)
{
    sgl_event_queue_push(event);
}

/**
 * @brief Send an event to the specified object
 * @param obj The object to be sent
 * @param type The type of the event
 * @return none
 * @note This function is used to send an event to the specified object, for example, 
 *       if you want to send an event to the button, you can call:
 *       ---- press button case  : sgl_event_send_obj(button, SGL_EVENT_PRESSED);
 *       ---- release button case: sgl_event_send_obj(button, SGL_EVENT_RELEASED);
 * 
 * @tip: You can also send you own event to the specified object, for example, 
 *       The MY_EVENT_TYPE is defined by you, and you can use it in your own event handler, for example:
 *       ...
 *       sgl_event_send_obj(button, MY_EVENT_TYPE);
 *       ...
 *       void my_event_handler(sgl_event_t *event)
 *       {
 *           if (event->type == MY_EVENT_TYPE) {
 *               // do something
 *           }
 *       }
 */
static inline void sgl_event_send_obj(struct sgl_obj *obj, sgl_event_type_t type)
{
    sgl_event_t event = {0};
    event.obj = obj;
    event.type = type;
    sgl_event_send(event);
}

/**
 * @brief Send a motion event to the specified object
 * @param pos The position of the motion event
 * @param type The type of the motion event
 * @param distance The distance of the motion event
 * @return none
 */
static inline void sgl_event_send_motion(sgl_event_pos_t pos, sgl_event_type_t type, uint16_t distance)
{
    sgl_event_t event = {0};
    event.pos = pos;
    event.type = type;
    event.distance = distance;
    sgl_event_send(event);
}

/**
 * @brief Send signal to key event
 * @param evt The event to send signal
 * @param signal The signal to send
 * @return none
 */
static inline void sgl_key_send_signal(sgl_event_t *evt, sgl_event_type_t signal)
{
    evt->type = signal;
}

/**
 * @brief Get signal from key event
 * @param evt The event to get signal
 * @return The signal of the event
 */
static inline sgl_event_type_t sgl_key_get_signal(sgl_event_t *evt)
{
    return evt->type;
}

/**
 * @brief All event task in SGL, this function will traverse all elements in the event queue, 
 *        respond to each element with an event, so that all events will trigger and point to the 
 *        corresponding callback function
 * @param none
 * @return none
*/
void sgl_event_task(void);

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
void sgl_event_pos_input(int16_t x, int16_t y, bool flag);

/**
 * @brief create a key group
 * @param max_num The maximum number of objects in the group
 * @return The pointer to the key group
 * @note: you must check the return value before using the key group
 */
sgl_key_group_t* sgl_key_group_create(uint16_t max_num);

/**
 * @brief Delete a key group
 * @param grp The pointer to the key group
 * @return none
 */
void sgl_key_group_delete(sgl_key_group_t *group);

/**
 * @brief Add an object to the key group
 * @param group The pointer to the key group
 * @param obj The object to add
 * @return > 0 : success, -1 : failed
 */
int sgl_key_group_add_obj(sgl_key_group_t *group, struct sgl_obj *obj);

/**
 * @brief Remove an object from the key group
 * @param group The pointer to the key group
 * @param obj The object to remove
 * @return > 0 : success, -1 : failed
 */
int sgl_key_group_remove_obj(sgl_key_group_t *group, struct sgl_obj *obj);

/**
 * @brief Load a key group
 * @param group The pointer to the key group
 * @return none
 */
void sgl_key_group_load(sgl_key_group_t *group);

/**
 * @brief Navigate to next/prev focus
 * @param type The event type
 * @param forward The direction of navigation
 * @return none
 */
void sgl_key_navigate(sgl_event_type_t type, bool forward);

/**
 * @brief Physical keyboard event UP
 * @param none
 * @return none
 * @note: you can call it in physical keyboard event handler function
 */
static inline void sgl_key_up(void)
{
    sgl_key_navigate(SGL_EVENT_KEY_UP, false);
}

/**
 * @brief Physical keyboard event DOWN
 * @param none
 * @return none
 * @note: you can call it in physical keyboard event handler function
 */
static inline void sgl_key_down(void)
{
    sgl_key_navigate(SGL_EVENT_KEY_DOWN, true);
}

/**
 * @brief Physical keyboard event LEFT
 * @param none
 * @return none
 * @note: you can call it in physical keyboard event handler function
 */
static inline void sgl_key_left(void)
{
    sgl_key_navigate(SGL_EVENT_KEY_LEFT, false);
}

/**
 * @brief Physical keyboard event RIGHT
 * @param none
 * @return none
 * @note: you can call it in physical keyboard event handler function
 */
static inline void sgl_key_right(void)
{
    sgl_key_navigate(SGL_EVENT_KEY_RIGHT, true);
}

/**
 * @brief Physical keyboard event ENTER pressed
 * @param none
 * @return none
 * @note: you can call it in physical keyboard event handler function
 */
void sgl_key_enter_pressed(void);

/**
 * @brief Physical keyboard event ENTER released
 * @param none
 * @return none
 * @note: you can call it in physical keyboard event handler function
 */
void sgl_key_enter_released(void);

/**
 * @brief Physical keyboard event ESC
 * @param none
 * @return none
 * @note: you can call it in physical keyboard event handler function
 */
void sgl_key_esc(void);

/**
 * @brief Physical encoder input
 * @param diff: encoder delta value (positive=CW, negative=CCW)
 * @param pressed: button pressed status (true=pressed, false=released)
 * @return none
 * @note: Call this function in your encoder ISR/timer handler. Handles both navigation and long-press.
 */
void sgl_encoder_input(int8_t diff, bool pressed);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif //!__SGL_EVENT_H__
