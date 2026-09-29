/* source/widgets/sgl_gif.h
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

#ifndef __SGL_GIF_H__
#define __SGL_GIF_H__

#include <sgl_core.h>

/* repeat count constants
 * sgl_gif_set_repeat_count() takes the number of EXTRA repeats:
 *   0            = play the animation once, then stop (no repeat);
 *   1            = play it twice;
 *   N            = play it N+1 times;
 *   SGL_GIF_REPEAT_INFINITE = loop forever.
 */
#define SGL_GIF_REPEAT_INFINITE   (0xFFFFu)
#define SGL_GIF_REPEAT_ONCE       (0u)

/* playback state */
typedef enum {
    SGL_GIF_STATE_IDLE = 0,
    SGL_GIF_STATE_PLAYING,
    SGL_GIF_STATE_PAUSED,
    SGL_GIF_STATE_FINISHED,
} sgl_gif_state_t;

/**
 * @brief gif callback function type
 * @param obj top-level sgl_obj_t pointer of the gif widget
 */
typedef void (*sgl_gif_cb_t)(sgl_obj_t *obj);

/**
 * @brief sgl_gif_t — widget instance struct.
 *
 * Field layout is ordered to keep 4-byte aligned scalar accesses on all
 */
typedef struct sgl_gif {
    sgl_obj_t           obj;            /* must be first — sgl_container_of() */

    /* --- internal renderer: a child sgl_img object ------------------------
     * Every visible frame is drawn by this img; the gif only swaps its
     * pixmap.  NULL until the frames are attached. */
    sgl_obj_t           *img;

    /* --- frame source --- */
    const sgl_pixmap_t *frames; /* array[frame_cnt] of full-frame pixmaps (contiguous) */

    /* --- callbacks --- */
    sgl_gif_cb_t        start_cb;       /* called once per sgl_gif_start() at first frame render */
    sgl_gif_cb_t        ready_cb;       /* called after each frame is shown */
    sgl_gif_cb_t        complete_cb;    /* called after all cycles + finish_delay */

    /* --- timing (4-byte primitives first) --- */
    uint32_t            last_frame_tick;   /* tick value of last frame advance */
    uint32_t            delay_start_tick;  /* tick value at start of finish_delay */
    uint16_t            frame_interval_ms; /* delay between consecutive frames in ms */
    uint16_t            finish_delay_ms;   /* post-last-frame delay before complete_cb, ms */

    /* --- dimensions / counters --- */
    uint16_t            frame_cnt;
    uint16_t            frame_idx;          /* current visible frame index [0..frame_cnt-1] */
    uint16_t            repeat_count;       /* configured repeat count */
    uint16_t            repeat_left;        /* remaining full cycles until complete */

    /* --- packed flags (keep as uint8_t block to minimise padding) --- */
    uint8_t             in_finish_delay;    /* 1 = between last frame and complete_cb */

    /* --- state enum kept at end for minimal padding --- */
    sgl_gif_state_t     state;
} sgl_gif_t;

/**
 * @brief create a gif widget
 * @param parent parent object (e.g. screen, page)
 * @return pointer to embedded sgl_obj_t, or NULL on OOM
 */
sgl_obj_t* sgl_gif_create(sgl_obj_t* parent);

/**
 * @brief attach the full-frame pixmap array that defines the animation.
 *
 * @param obj       gif widget
 * @param canvas_w  logical canvas width  (widget size)
 * @param canvas_h  logical canvas height (widget size)
 * @param frame_cnt number of frames
 * @param frames    contiguous array of frame_cnt sgl_pixmap_t.  The widget
 *                  only stores the pointer; the array must outlive the widget.
 */
void sgl_gif_set_frames(sgl_obj_t *obj,
                        uint16_t canvas_w, uint16_t canvas_h,
                        uint16_t frame_cnt,
                        const sgl_pixmap_t *frames);

/**
 * @brief configure the number of EXTRA animation repeats.
 *
 * @param obj   gif widget
 * @param count 0 = play once then stop; 1 = play twice; N = play N+1 times;
 *              SGL_GIF_REPEAT_INFINITE = loop forever.
 */
void sgl_gif_set_repeat_count(sgl_obj_t *obj, uint16_t count);

/**
 * @brief set the delay between consecutive frames in milliseconds.
 *        Default = 0 ms (advance as fast as the renderer permits).
 */
void sgl_gif_set_frame_interval(sgl_obj_t *obj, uint16_t ms);

/**
 * @brief set a hold-off delay that is applied AFTER the last frame of EVERY
 *        animation cycle has been shown: each complete cycle ends with a
 *        finish_delay_ms hold on the last frame before either rolling into
 *        the next cycle or firing complete_cb / becoming FINISHED.
 *        Useful to give the user time to see the last frame (e.g. boot logo
 *        "停 N ms 再进主界面" behaviour).
 *        Default = 0 ms (complete_cb fires immediately, no per-cycle hold).
 */
void sgl_gif_set_finish_delay(sgl_obj_t *obj, uint16_t ms);

/** @brief set the start callback. */
void sgl_gif_set_start_cb(sgl_obj_t *obj, sgl_gif_cb_t cb);

/** @brief set the per-frame-shown ready callback. */
void sgl_gif_set_ready_cb(sgl_obj_t *obj, sgl_gif_cb_t cb);

/** @brief set the post-delay complete callback. */
void sgl_gif_set_complete_cb(sgl_obj_t *obj, sgl_gif_cb_t cb);

/**
 * @brief (re)start playback from frame 0 of cycle 1.
 *        Always triggers a redraw so the first frame becomes visible.
 */
void sgl_gif_start(sgl_obj_t *obj);

/**
 * @brief stop playback and freeze on the last drawn frame.
 *        Use sgl_gif_start() to restart from frame 0.
 */
void sgl_gif_stop(sgl_obj_t *obj);

/** @brief pause playback, preserving the current frame index. */
void sgl_gif_pause(sgl_obj_t *obj);

/** @brief resume playback from the current paused frame index. */
void sgl_gif_resume(sgl_obj_t *obj);

/**
 * @brief seek back to frame 0 of cycle 1 WITHOUT starting playback
 *        (state becomes IDLE).  Redraws once so the idle frame is visible.
 */
void sgl_gif_reset(sgl_obj_t *obj);

/** @return 1 if state == FINISHED (incl. any finish_delay has elapsed) */
uint8_t  sgl_gif_is_finished(sgl_obj_t *obj);

/** @return 1 if state == PLAYING */
uint8_t  sgl_gif_is_playing (sgl_obj_t *obj);

/** @return current frame index [0..frame_cnt-1] */
uint16_t sgl_gif_get_frame_idx(sgl_obj_t *obj);

/** @return total frame count */
uint16_t sgl_gif_get_frame_cnt(sgl_obj_t *obj);

#endif /* !__SGL_GIF_H__ */
