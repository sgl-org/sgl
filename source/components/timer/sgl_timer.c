/* source/component/sgl_timer.c
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

#include <sgl_mm.h>
#include <sgl_log.h>
#include <string.h>
#include "sgl_timer.h"

#define SGL_SHORT_WHEEL_INIT(idx) { .next = &short_wheel[idx], .prev = &short_wheel[idx] }
#define SGL_LONG_WHEEL_INIT(idx)  { .next = &long_wheel[idx],  .prev = &long_wheel[idx] }

static sgl_list_node_t short_wheel[SGL_TIMER_SHORT_SLOT] = {
    SGL_SHORT_WHEEL_INIT(0),   SGL_SHORT_WHEEL_INIT(1),   SGL_SHORT_WHEEL_INIT(2),   SGL_SHORT_WHEEL_INIT(3),
    SGL_SHORT_WHEEL_INIT(4),   SGL_SHORT_WHEEL_INIT(5),   SGL_SHORT_WHEEL_INIT(6),   SGL_SHORT_WHEEL_INIT(7),
    SGL_SHORT_WHEEL_INIT(8),   SGL_SHORT_WHEEL_INIT(9),   SGL_SHORT_WHEEL_INIT(10),  SGL_SHORT_WHEEL_INIT(11),
    SGL_SHORT_WHEEL_INIT(12),  SGL_SHORT_WHEEL_INIT(13),  SGL_SHORT_WHEEL_INIT(14),  SGL_SHORT_WHEEL_INIT(15),
    SGL_SHORT_WHEEL_INIT(16),  SGL_SHORT_WHEEL_INIT(17),  SGL_SHORT_WHEEL_INIT(18),  SGL_SHORT_WHEEL_INIT(19),
    SGL_SHORT_WHEEL_INIT(20),  SGL_SHORT_WHEEL_INIT(21),  SGL_SHORT_WHEEL_INIT(22),  SGL_SHORT_WHEEL_INIT(23),
    SGL_SHORT_WHEEL_INIT(24),  SGL_SHORT_WHEEL_INIT(25),  SGL_SHORT_WHEEL_INIT(26),  SGL_SHORT_WHEEL_INIT(27),
    SGL_SHORT_WHEEL_INIT(28),  SGL_SHORT_WHEEL_INIT(29),  SGL_SHORT_WHEEL_INIT(30),  SGL_SHORT_WHEEL_INIT(31),
    SGL_SHORT_WHEEL_INIT(32),  SGL_SHORT_WHEEL_INIT(33),  SGL_SHORT_WHEEL_INIT(34),  SGL_SHORT_WHEEL_INIT(35),
    SGL_SHORT_WHEEL_INIT(36),  SGL_SHORT_WHEEL_INIT(37),  SGL_SHORT_WHEEL_INIT(38),  SGL_SHORT_WHEEL_INIT(39),
    SGL_SHORT_WHEEL_INIT(40),  SGL_SHORT_WHEEL_INIT(41),  SGL_SHORT_WHEEL_INIT(42),  SGL_SHORT_WHEEL_INIT(43),
    SGL_SHORT_WHEEL_INIT(44),  SGL_SHORT_WHEEL_INIT(45),  SGL_SHORT_WHEEL_INIT(46),  SGL_SHORT_WHEEL_INIT(47),
    SGL_SHORT_WHEEL_INIT(48),  SGL_SHORT_WHEEL_INIT(49),  SGL_SHORT_WHEEL_INIT(50),  SGL_SHORT_WHEEL_INIT(51),
    SGL_SHORT_WHEEL_INIT(52),  SGL_SHORT_WHEEL_INIT(53),  SGL_SHORT_WHEEL_INIT(54),  SGL_SHORT_WHEEL_INIT(55),
    SGL_SHORT_WHEEL_INIT(56),  SGL_SHORT_WHEEL_INIT(57),  SGL_SHORT_WHEEL_INIT(58),  SGL_SHORT_WHEEL_INIT(59),
    SGL_SHORT_WHEEL_INIT(60),  SGL_SHORT_WHEEL_INIT(61),  SGL_SHORT_WHEEL_INIT(62),  SGL_SHORT_WHEEL_INIT(63),
    SGL_SHORT_WHEEL_INIT(64),  SGL_SHORT_WHEEL_INIT(65),  SGL_SHORT_WHEEL_INIT(66),  SGL_SHORT_WHEEL_INIT(67),
    SGL_SHORT_WHEEL_INIT(68),  SGL_SHORT_WHEEL_INIT(69),  SGL_SHORT_WHEEL_INIT(70),  SGL_SHORT_WHEEL_INIT(71),
    SGL_SHORT_WHEEL_INIT(72),  SGL_SHORT_WHEEL_INIT(73),  SGL_SHORT_WHEEL_INIT(74),  SGL_SHORT_WHEEL_INIT(75),
    SGL_SHORT_WHEEL_INIT(76),  SGL_SHORT_WHEEL_INIT(77),  SGL_SHORT_WHEEL_INIT(78),  SGL_SHORT_WHEEL_INIT(79),
    SGL_SHORT_WHEEL_INIT(80),  SGL_SHORT_WHEEL_INIT(81),  SGL_SHORT_WHEEL_INIT(82),  SGL_SHORT_WHEEL_INIT(83),
    SGL_SHORT_WHEEL_INIT(84),  SGL_SHORT_WHEEL_INIT(85),  SGL_SHORT_WHEEL_INIT(86),  SGL_SHORT_WHEEL_INIT(87),
    SGL_SHORT_WHEEL_INIT(88),  SGL_SHORT_WHEEL_INIT(89),  SGL_SHORT_WHEEL_INIT(90),  SGL_SHORT_WHEEL_INIT(91),
    SGL_SHORT_WHEEL_INIT(92),  SGL_SHORT_WHEEL_INIT(93),  SGL_SHORT_WHEEL_INIT(94),  SGL_SHORT_WHEEL_INIT(95),
    SGL_SHORT_WHEEL_INIT(96),  SGL_SHORT_WHEEL_INIT(97),  SGL_SHORT_WHEEL_INIT(98),  SGL_SHORT_WHEEL_INIT(99),
    SGL_SHORT_WHEEL_INIT(100), SGL_SHORT_WHEEL_INIT(101), SGL_SHORT_WHEEL_INIT(102), SGL_SHORT_WHEEL_INIT(103),
    SGL_SHORT_WHEEL_INIT(104), SGL_SHORT_WHEEL_INIT(105), SGL_SHORT_WHEEL_INIT(106), SGL_SHORT_WHEEL_INIT(107),
    SGL_SHORT_WHEEL_INIT(108), SGL_SHORT_WHEEL_INIT(109), SGL_SHORT_WHEEL_INIT(110), SGL_SHORT_WHEEL_INIT(111),
    SGL_SHORT_WHEEL_INIT(112), SGL_SHORT_WHEEL_INIT(113), SGL_SHORT_WHEEL_INIT(114), SGL_SHORT_WHEEL_INIT(115),
    SGL_SHORT_WHEEL_INIT(116), SGL_SHORT_WHEEL_INIT(117), SGL_SHORT_WHEEL_INIT(118), SGL_SHORT_WHEEL_INIT(119),
    SGL_SHORT_WHEEL_INIT(120), SGL_SHORT_WHEEL_INIT(121), SGL_SHORT_WHEEL_INIT(122), SGL_SHORT_WHEEL_INIT(123),
    SGL_SHORT_WHEEL_INIT(124), SGL_SHORT_WHEEL_INIT(125), SGL_SHORT_WHEEL_INIT(126), SGL_SHORT_WHEEL_INIT(127)
};

static sgl_list_node_t long_wheel[SGL_TIMER_LONG_SLOT] = {
    SGL_LONG_WHEEL_INIT(0),   SGL_LONG_WHEEL_INIT(1),   SGL_LONG_WHEEL_INIT(2),   SGL_LONG_WHEEL_INIT(3),
    SGL_LONG_WHEEL_INIT(4),   SGL_LONG_WHEEL_INIT(5),   SGL_LONG_WHEEL_INIT(6),   SGL_LONG_WHEEL_INIT(7),
    SGL_LONG_WHEEL_INIT(8),   SGL_LONG_WHEEL_INIT(9),   SGL_LONG_WHEEL_INIT(10),  SGL_LONG_WHEEL_INIT(11),
    SGL_LONG_WHEEL_INIT(12),  SGL_LONG_WHEEL_INIT(13),  SGL_LONG_WHEEL_INIT(14),  SGL_LONG_WHEEL_INIT(15),
};

static uint16_t short_idx = 0;
static uint16_t long_idx = 0;

/**
 * @brief Add a timer to the wheel
 * @param timer Pointer to the timer structure
 * @return none
 */
static void timer_add(sgl_timer_t *timer)
{
    uint16_t pos, slot;

    if (timer->interval < SGL_TIMER_SHORT_SLOT) {
        pos = (short_idx + timer->interval) & (SGL_TIMER_SHORT_SLOT - 1);
        sgl_list_add_node_at_tail(&short_wheel[pos], &timer->node);
    } else {
        slot = timer->interval / SGL_TIMER_LONG_STEP;
        pos = (long_idx + slot) & (SGL_TIMER_LONG_SLOT - 1);
        sgl_list_add_node_at_tail(&long_wheel[pos], &timer->node);
    }
}

/**
 * @brief create a timer by dynamically allocating memory
 * @return Pointer to the timer structure
 */
sgl_timer_t *sgl_timer_create(void)
{
    sgl_timer_t *timer = (sgl_timer_t *)sgl_malloc(sizeof(sgl_timer_t));
    if (!timer) {
        SGL_LOG_ERROR("sgl_timer_create: malloc failed");
        return NULL;
    }

    memset(timer, 0, sizeof(sgl_timer_t));
    return timer;
}

/**
 * @brief delete a timer if your time is created dynamically
 * @param timer Pointer to the timer structure to be removed
 * @return none
 */
void sgl_timer_delete(sgl_timer_t *timer)
{
    if (timer) {
        timer->destroyed = 1;
    }
}

/**
 * @brief setup a timer
 * @param timer Pointer to the timer structure
 * @param callback Callback function to be called when timer expires
 * @param interval Timer interval in ticks, ms
 * @param repeat_cnt Repeat count, -1 for infinite
 * @param user_data User data passed to callback function
 * @return true if successful, false if failed
 */
bool sgl_timer_setup(sgl_timer_t *timer, sgl_timer_callback_t callback, uint16_t interval, int32_t repeat_cnt, void *user_data)
{
    SGL_ASSERT(interval < (SGL_TIMER_LONG_SLOT * SGL_TIMER_LONG_STEP));

    if (!timer || !callback || interval == 0) {
        return false;
    }

    timer->callback = callback;
    timer->user_data = user_data;
    timer->interval = interval;
    timer->count = repeat_cnt;
    timer->destroyed = 0;

    timer_add(timer);
    return true;
}

/**
 * @brief Process short wheel all timers
 * @param none
 * @return none
 * @note This function processes all timers in the short wheel
 */
static void process_short_wheel(void)
{
    sgl_list_node_t *head = &short_wheel[short_idx];
    sgl_timer_t *pos = NULL, *next = NULL;

    sgl_list_for_each_entry_safe(pos, next, head, sgl_timer_t, node) {
        sgl_list_del_node(&pos->node);
        if (pos->destroyed) {
            sgl_free(pos);
            continue;
        }

        pos->callback(pos, pos->user_data);

        if (pos->count > 0) {
            pos->count--;
        }

        if (pos->count == 0) {
            sgl_free(pos);
            continue;
        }

        timer_add(pos);
    }

    short_idx = (short_idx + 1) & (SGL_TIMER_SHORT_SLOT - 1);
}

/**
 * @brief Process long wheel all timers
 * @param none
 * @return none
 * @note This function processes all timers in the long wheel
 */
static void process_long_wheel(void)
{
    sgl_list_node_t *head = &long_wheel[long_idx];
    sgl_timer_t *pos, *next;

    sgl_list_for_each_entry_safe(pos, next, head, sgl_timer_t, node)
    {
        sgl_list_del_node(&pos->node);

        if (pos->destroyed) {
            sgl_free(pos);
            continue;
        }

        pos->callback(pos, pos->user_data);

        if (pos->count > 0) {
            pos->count--;
        }

        if (pos->count == 0) {
            sgl_free(pos);
            continue;
        }

        timer_add(pos);
    }

    long_idx = (long_idx + 1) & (SGL_TIMER_LONG_SLOT - 1);
}

/**
 * @brief Timer handler function, should be called periodically
 * @note This function checks all timers and executes callbacks if expired
 * @warning Must be called frequently enough to not miss timer events
 */
void sgl_timer_handler(void)
{
    static uint32_t last_tick = 0;
    const uint32_t now = sgl_tick_get();
    const uint32_t diff = now - last_tick;

    if (diff == 0) {
        return;
    }

    for (uint32_t i = 0; i < diff; i++) {
        process_short_wheel();
        if (short_idx == 0) {
            process_long_wheel();
        }
    }

    last_tick = now;
}
