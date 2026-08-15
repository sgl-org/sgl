/* source/include/sgl_misc.h
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

#ifndef __SGL_MISC_H__
#define __SGL_MISC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <sgl_types.h>
#include <sgl_core.h>
#include <sgl_anim.h>

#if (CONFIG_SGL_BOOT_LOGO)
/**
 * @brief to show the sgl logo after sgl init
 * @param none
 * @return none
 * @note: you can call this function in your main function to show the sgl logo
 */
void sgl_boot_logo(void);

#endif // ! CONFIG_SGL_BOOT_LOGO

#if (CONFIG_SGL_MONITOR_TRACE)
#define  SGL_MONITOR_COORDS_WIDTH       CONFIG_SGL_MONITOR_COORDS_WIDTH
#define  SGL_MONITOR_COORDS_HEIGHT      CONFIG_SGL_MONITOR_COORDS_HEIGHT
#define  SGL_MONITOR_COORDS_X           (SGL_SCREEN_WIDTH - SGL_MONITOR_COORDS_WIDTH)
#define  SGL_MONITOR_COORDS_Y           (SGL_SCREEN_HEIGHT - SGL_MONITOR_COORDS_HEIGHT)
#define  SGL_MONITOR_COLOR              CONFIG_SGL_MONITOR_COLOR
#define  SGL_MONITOR_TEXT_COLOR         CONFIG_SGL_MONITOR_TEXT_COLOR
#define  SGL_MONITOR_ALPHA              CONFIG_SGL_MONITOR_ALPHA

#define  SGL_MONITOR_COORDS             (sgl_area_t){.x1 = SGL_MONITOR_COORDS_X,     \
                                                     .x2 = SGL_MONITOR_COORDS_X + SGL_MONITOR_COORDS_WIDTH - 1,     \
                                                     .y1 = SGL_MONITOR_COORDS_Y,     \
                                                     .y2 = SGL_MONITOR_COORDS_Y + SGL_MONITOR_COORDS_HEIGHT - 1,    \
                                                    }

void sgl_monitor_trace(sgl_surf_t *surf);
#endif // ! CONFIG_SGL_MONITOR_TRACE

/**
 * @brief Count number of options in \n-separated text
 * @param text newline-separated option string
 * @return number of options
 */
uint16_t sgl_string_option_get_count(const char *text);

/**
 * @brief Get byte offset of the Nth option in \n-separated text
 * @param text newline-separated option string
 * @param index zero-based option index
 * @return byte offset, or -1 if out of range
 */
int sgl_string_option_get_offset(const char *text, int index);

/**
 * @brief Get text length of one option at given byte offset
 * @param text option string
 * @param offset byte offset of the option
 * @return length (stops at \n or \0)
 */
int sgl_string_option_get_text_len(const char *text, int offset);

#define SGL_SCROLL_DRAG_THRESHOLD       4    /* drag start threshold (px) */
#define SGL_SCROLL_OVERSCROLL           40   /* rubber-band overscroll limit (px, 0 = disabled) */
#define SGL_SCROLL_INERTIA_NUM          7    /* coast decay: speed *= NUM/DEN per 16ms */
#define SGL_SCROLL_INERTIA_DEN          8
#define SGL_SCROLL_REBOUND_PULL_DIV     4    /* rebound step = overscroll amount / this value */
#define SGL_SCROLL_REBOUND_MAX_STEP     24   /* rebound step cap (px) */
#define SGL_SCROLL_VEL_WINDOW_MS        100  /* speed measurement sliding window length (ms) */
#define SGL_SCROLL_BAR_IDLE_MS          600  /* scrollbar full-opacity hold time (ms) */
#define SGL_SCROLL_BAR_FADE_STEP        8    /* scrollbar alpha decrement per 16ms */
#define SGL_SCROLL_BAR_RESIDENT_ALPHA   128  /* scrollbar resident minimum alpha (fade floor) */
#define SGL_SCROLL_BAR_ACTIVE_ALPHA     128  /* scrollbar alpha while active (wake value) */
#define SGL_SCROLL_BAR_WIDTH            4    /* scrollbar width (px) */

typedef struct sgl_scroll {
    int32_t offset;     /* current scroll amount (px) */
    int32_t range;      /* scroll upper limit (content height - viewport height) */
    void (*commit)(struct sgl_scroll *sc); /* change commit callback (widget invalidate/layout) */
    sgl_anim_t *anim;   /* dynamic animation node (NULL when idle, auto-released on settle) */
    uint16_t step_tick; /* low 16 bits of the last animation step timestamp */
    int16_t grab_coord; /* main-axis coordinate at press */
    int16_t prev_coord; /* previous frame main-axis coordinate (incremental follow base) */
    int16_t speed;      /* coasting speed (px / 16ms, same direction as offset delta) */
    int16_t win_dist;   /* accumulated displacement within the speed window */
    uint16_t win_tick;  /* low 16 bits of the speed window start timestamp */
    uint16_t bar_idle;  /* scrollbar idle timer (ms) */
    uint8_t bar_alpha;  /* scrollbar current alpha */
    uint8_t touching : 1; /* inside a press sequence */
    uint8_t dragged  : 1; /* start threshold crossed */
    uint8_t coasting : 1; /* inertia/rebound in progress */
} sgl_scroll_t;

/**
 * @brief Reset scroll state (used on init / re-binding data)
 * @param sc scroll state
 * @note zeroes the whole struct then restores the resident scrollbar alpha
 */
void sgl_scroll_reset(sgl_scroll_t *sc);

/**
 * @brief Feed a press event: freeze coasting and start tracking this press sequence
 * @param sc scroll state
 * @param coord main-axis touch coordinate
 * @note stops any running inertia immediately (overscroll stays frozen until
 *       release), resets the speed window and anchors grab/prev coordinates
 */
void sgl_scroll_press(sgl_scroll_t *sc, int16_t coord);

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
uint8_t sgl_scroll_stay(sgl_scroll_t *sc, int16_t coord, int32_t range);

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
uint8_t sgl_scroll_release(sgl_scroll_t *sc, int32_t range);

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
uint8_t sgl_scroll_anim_step(sgl_scroll_t *sc, uint16_t elapsed_ms, int32_t range);

/**
 * @brief Wake the scrollbar (call on scroll value change / data binding)
 * @param sc scroll state
 * @note restores the active alpha and restarts the idle hold timer
 */
void sgl_scroll_bar_wake(sgl_scroll_t *sc);

/**
 * @brief Scrollbar fade-out step
 * @param sc scroll state
 * @param elapsed_ms elapsed time since the previous step
 * @return non-zero = alpha changed; always returns 0 once the resident value is reached.
 * @note holds full opacity for BAR_IDLE_MS first, then decreases FADE_STEP
 *       per 16ms slice down to the resident alpha
 */
uint8_t sgl_scroll_bar_step(sgl_scroll_t *sc, uint16_t elapsed_ms);

/**
 * @brief Start the inertia/rebound + scrollbar fade-out animation (shared by all widgets)
 * @param sc scroll state
 * @note creates the animation node dynamically (attached to sc->anim) and
 *       starts it with SGL_ANIM_REPEAT_LOOP; stopped and released
 *       automatically on settle (coasting finished and scrollbar faded to
 *       the resident value). Any previously running node is stopped first.
 *       The caller must set sc->commit beforehand
 */
void sgl_scroll_anim_start(sgl_scroll_t *sc);

/**
 * @brief Stop and release the scroll animation node early (used on widget
 *        destroy/collapse; a no-op when no animation is running)
 * @param sc scroll state
 */
void sgl_scroll_anim_stop(sgl_scroll_t *sc);

/**
 * @brief Draw the right-hand vertical scrollbar (called in the widget DRAW_MAIN)
 * @param surf drawing surface
 * @param obj widget object (provides x coordinates, border and corner radius)
 * @param sc scroll state (reads offset / bar_alpha)
 * @param range current scroll upper limit; nothing is drawn when range<=0 (no scrollable content)
 * @param viewport vertical track extent (y1/y2 of the scrollable list area,
 *                 which may start below the widget top, e.g. under a dropdown header)
 * @note thumb height is proportional to viewport / content height (min 8px);
 *       thumb position maps offset into the track; drawn with the theme
 *       scroll foreground color at bar_alpha opacity
 */
void sgl_scroll_draw_bar(sgl_surf_t *surf, sgl_obj_t *obj, const sgl_scroll_t *sc, int32_t range, const sgl_area_t *viewport);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif // ! __SGL_MISC_H__
