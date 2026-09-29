/* source/widgets/sgl_gif.c
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
#include <sgl_log.h>
#include <sgl_mm.h>
#include <string.h>
#include "sgl_gif.h"
#include "widgets/img/sgl_img.h"

/* ----------------------------------------------------------------------
 *  Frame advance / playback state machine.
 *
 *  This module does NOT draw pixels itself.  It delegates every visible
 *  frame to an internal sgl_img child (the same widget ui_logo.c uses to
 *  render the boot logo), which handles QOI / RLE / RGB565 decoding and
 *  sliced-surface rendering.  The gif object only owns the timing:
 *
 *    sgl_gif_construct_cb (SGL_EVENT_DRAW_MAIN)
 *        -> if interval elapsed: sgl_img_set_pixmap(img, &frames[frame_idx])
 *        -> keep gif dirty while PLAYING so the loop keeps running
 *
 *  The internal img child is created in sgl_gif_create() and freed together
 *  with the gif object through the normal object-chain teardown.
 * -------------------------------------------------------------------- */

/* Show the pixmap at frames[idx] through the internal img child.
 * The caller is responsible for idx staying inside [0, frame_cnt). */
static void sgl_gif_show_frame(sgl_gif_t *gif, uint16_t idx)
{
    gif->frame_idx = idx;
    sgl_img_set_pixmap(gif->img, &gif->frames[idx]);
}

/* Bring the animation back to "about to play frame 0 of cycle 1" without
 * touching frame_interval_ms / finish_delay_ms / callbacks.  Shared by
 * set_frames / start / reset so the reset sequence lives in one place. */
static void sgl_gif_reset_state(sgl_gif_t *gif)
{
    gif->frame_idx        = 0u;
    gif->repeat_left      = gif->repeat_count;
    gif->last_frame_tick  = 0u;   /* force frame 0 on the next PLAYING draw */
    gif->in_finish_delay  = 0u;
}

/* ----------------------------------------------------------------------
 *  Widget construct / event callback
 * -------------------------------------------------------------------- */
static void sgl_gif_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    (void)surf;
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);

    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        if (gif->frames == NULL || gif->frame_cnt == 0u || gif->img == NULL) {
            return;
        }

        /* Cache the current tick once: it drives both the finish-delay
         * polling and the frame-advance timing below. */
        uint32_t now = sgl_tick_get();

        /* ---- IDLE / PAUSED / FINISHED (no delay): the frozen frame is
         *      already on the child img, nothing to drive here. ---------- */
        if (gif->state != SGL_GIF_STATE_PLAYING) {
            return;
        }

        /* ---- PLAYING path --------------------------------------------- */

        /* First PLAYING draw: show frame 0 immediately, fire start_cb once
         * (this branch runs exactly once per start()/resume() because
         * reset_state re-arms last_frame_tick == 0), then wait one full
         * interval before advancing to frame 1. */
        if (gif->last_frame_tick == 0u) {
            gif->last_frame_tick = now;
            if (gif->start_cb != NULL) {
                gif->start_cb(obj);
            }
            sgl_gif_show_frame(gif, 0u);
            if (gif->ready_cb != NULL) {
                gif->ready_cb(obj);
            }
            sgl_obj_set_dirty(obj);
            return;
        }

        /* ---- per-cycle finish-delay polling: after every full cycle of
         *      frames we hold the last frame for finish_delay_ms before
         *      either starting the next cycle or firing complete_cb.  The
         *      hold is therefore part of every complete animation cycle. */
        if (gif->in_finish_delay) {
            if (now - gif->delay_start_tick >= (uint32_t)gif->finish_delay_ms) {
                gif->in_finish_delay = 0u;
                if (gif->repeat_count != SGL_GIF_REPEAT_INFINITE &&
                    gif->repeat_left == 0u) {
                    /* last cycle (and its finish_delay) fully done */
                    gif->state = SGL_GIF_STATE_FINISHED;
                    if (gif->complete_cb != NULL) {
                        gif->complete_cb(obj);
                    }
                    return;
                }
                /* roll into the next cycle from frame 0 */
                gif->last_frame_tick = now;
                sgl_gif_show_frame(gif, 0u);
                if (gif->ready_cb != NULL) {
                    gif->ready_cb(obj);
                }
            }
            sgl_obj_set_dirty(obj);
            return;
        }

        if (gif->frame_interval_ms == 0u ||
            now - gif->last_frame_tick >= (uint32_t)gif->frame_interval_ms) {
            /* advance to the next frame */
            gif->last_frame_tick = now;
            gif->frame_idx++;

            if (gif->frame_idx >= gif->frame_cnt) {
                /* end of one full cycle of frames */
                if (gif->repeat_count != SGL_GIF_REPEAT_INFINITE) {
                    gif->repeat_left--;
                }

                if (gif->finish_delay_ms > 0u) {
                    /* per-cycle hold on the last frame (part of the cycle) */
                    sgl_gif_show_frame(gif, (uint16_t)(gif->frame_cnt - 1u));
                    gif->in_finish_delay  = 1u;
                    gif->delay_start_tick = now;
                    sgl_obj_set_dirty(obj);
                    return;
                }

                /* no finish_delay: if this was the final cycle, finish on the
                 * last frame; otherwise fall through to show frame 0 below. */
                if (gif->repeat_count != SGL_GIF_REPEAT_INFINITE &&
                    gif->repeat_left == 0u) {
                    sgl_gif_show_frame(gif, (uint16_t)(gif->frame_cnt - 1u));
                    gif->state = SGL_GIF_STATE_FINISHED;
                    if (gif->complete_cb != NULL) {
                        gif->complete_cb(obj);
                    }
                    return;
                }
                gif->frame_idx = 0u; /* restart the next cycle at frame 0 */
            }

            /* show the new current frame through the internal img child */
            sgl_gif_show_frame(gif, gif->frame_idx);

            if (gif->ready_cb != NULL) {
                gif->ready_cb(obj);
            }
        }

        /* keep the animation polling alive */
        sgl_obj_set_dirty(obj);
    }
    /* SGL_EVENT_DESTROYED needs no handling: the internal img child is freed
     * by the object-chain teardown, and frames[] are externally owned. */
}

/* ======================================================================
 *                          PUBLIC API
 * ====================================================================== */

sgl_obj_t* sgl_gif_create(sgl_obj_t* parent)
{
    sgl_gif_t *gif = sgl_malloc(sizeof(sgl_gif_t));
    if (gif == NULL) {
        SGL_LOG_ERROR("sgl_gif_create: malloc failed (%u bytes)",
                      (unsigned)sizeof(sgl_gif_t));
        return NULL;
    }
    memset(gif, 0, sizeof(sgl_gif_t));

    sgl_obj_t *obj = &gif->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_gif_construct_cb;

    /* memset() above already zeroed state / frame_interval_ms /
     * finish_delay_ms; only the non-zero default (play once) needs setting. */
    gif->repeat_count      = 1u;
    gif->repeat_left       = 1u;

    /* Internal renderer: a child sgl_img that owns the actual pixel
     * decoding + drawing (identical to ui_logo.c's anim_icon). */
    gif->img = sgl_img_create(obj);
    if (gif->img == NULL) {
        SGL_LOG_ERROR("sgl_gif_create: internal img child create failed");
        sgl_free(gif);
        return NULL;
    }

    return obj;
}

void sgl_gif_set_frames(sgl_obj_t *obj,
                        uint16_t canvas_w, uint16_t canvas_h,
                        uint16_t frame_cnt,
                        const sgl_pixmap_t *frames)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);

    if (frames == NULL || frame_cnt == 0u || canvas_w == 0u || canvas_h == 0u) {
        SGL_LOG_ERROR("sgl_gif_set_frames: invalid args (NULL/zero)");
        return;
    }

    /* Validate full-frame geometry up-front so any mismatch is caught at
     * attach-time, not mid-animation.  Warn but don't reject: mismatched
     * frames may still display but likely look wrong.
     * Note: per-frame pixel format is intentionally NOT checked here — the
     * actual decoding is delegated to the internal sgl_img child, which is
     * the single source of truth for which formats it can render. */
    for (uint16_t i = 0u; i < frame_cnt; i++) {
        const sgl_pixmap_t *pm = &frames[i];
        if (pm->width != canvas_w || pm->height != canvas_h) {
            SGL_LOG_WARN("sgl_gif_set_frames: frames[%u] size=%ux%u mismatches "
                         "canvas=%ux%u (bbox/differential frames not supported)",
                         i,
                         (unsigned)pm->width, (unsigned)pm->height,
                         (unsigned)canvas_w,    (unsigned)canvas_h);
        }
    }

    gif->frame_cnt     = frame_cnt;
    gif->frames        = frames;
    gif->state         = SGL_GIF_STATE_IDLE;
    sgl_gif_reset_state(gif);

    /* Resize + reposition the internal img child to fill the canvas, and
     * show the very first frame so the widget is non-blank even before
     * sgl_gif_start().  (img is guaranteed non-NULL: create() returns NULL
     * on img-create failure.) */
    sgl_obj_set_size(gif->img, (int16_t)canvas_w, (int16_t)canvas_h);
    sgl_obj_set_pos(gif->img, 0, 0);
    sgl_gif_show_frame(gif, 0u);

    sgl_obj_set_size(obj, (int16_t)canvas_w, (int16_t)canvas_h);
    sgl_obj_set_dirty(obj);
}

void sgl_gif_set_repeat_count(sgl_obj_t *obj, uint16_t count)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);

    /* Public API counts EXTRA repeats: 0 = once, 1 = twice, N = N+1 times.
     * Internally repeat_count holds the total full cycles, so translate.
     * SGL_GIF_REPEAT_INFINITE is preserved as-is for the infinite marker. */
    if (count >= SGL_GIF_REPEAT_INFINITE) {
        gif->repeat_count = SGL_GIF_REPEAT_INFINITE;
    } else {
        gif->repeat_count = (uint16_t)(count + 1u);
    }
    gif->repeat_left = gif->repeat_count;
}

void sgl_gif_set_frame_interval(sgl_obj_t *obj, uint16_t ms)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);
    gif->frame_interval_ms = ms;
}

void sgl_gif_set_finish_delay(sgl_obj_t *obj, uint16_t ms)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);
    gif->finish_delay_ms = ms;
}

void sgl_gif_set_start_cb(sgl_obj_t *obj, sgl_gif_cb_t cb)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);
    gif->start_cb = cb;
}

void sgl_gif_set_ready_cb(sgl_obj_t *obj, sgl_gif_cb_t cb)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);
    gif->ready_cb = cb;
}

void sgl_gif_set_complete_cb(sgl_obj_t *obj, sgl_gif_cb_t cb)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);
    gif->complete_cb = cb;
}

void sgl_gif_start(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);

    if (gif->frames == NULL || gif->frame_cnt == 0u) {
        SGL_LOG_ERROR("sgl_gif_start: frames not set — call sgl_gif_set_frames() first");
        return;
    }

    sgl_gif_reset_state(gif);
    gif->state = SGL_GIF_STATE_PLAYING;

    sgl_obj_set_dirty(obj);
}

void sgl_gif_stop(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);
    gif->in_finish_delay  = 0u;
    gif->state            = SGL_GIF_STATE_IDLE;
}

void sgl_gif_pause(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);
    if (gif->state == SGL_GIF_STATE_PLAYING) {
        gif->state = SGL_GIF_STATE_PAUSED;
    }
}

void sgl_gif_resume(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);
    if (gif->state == SGL_GIF_STATE_PAUSED) {
        gif->state           = SGL_GIF_STATE_PLAYING;
        gif->last_frame_tick = 0u;  /* re-show current frame then keep going */
        sgl_obj_set_dirty(obj);
    }
}

void sgl_gif_reset(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);
    sgl_gif_reset_state(gif);
    gif->state = SGL_GIF_STATE_IDLE;
    sgl_gif_show_frame(gif, 0u);
    sgl_obj_set_dirty(obj);
}

uint8_t sgl_gif_is_finished(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);
    return (gif->state == SGL_GIF_STATE_FINISHED) ? 1u : 0u;
}

uint8_t sgl_gif_is_playing(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);
    return (gif->state == SGL_GIF_STATE_PLAYING) ? 1u : 0u;
}

uint16_t sgl_gif_get_frame_idx(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);
    return gif->frame_idx;
}

uint16_t sgl_gif_get_frame_cnt(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gif_t *gif = sgl_container_of(obj, sgl_gif_t, obj);
    return gif->frame_cnt;
}

// #define LOG_TAG_CONST ON  // ON / OFF
// #define LOG_TAG "ui_logo"
// #include "contiki_os.h"

// #include "resource/img_logo.c"
// /*--------------------------------------------------------------------------------------------*/
// #define LOGO_FRAME_INTERVAL_MS  (30)   /* 逐帧间隔 */
// #define LOGO_FINISH_DELAY_MS    (1000)  /* 动画结束后等待 N ms 再删除 */

// /* img_logo[] 是 90 帧 sgl_pixmap_t 的连续数组, 直接传给 sgl_gif_set_frames() */
// static sgl_obj_t *s_logo = NULL;
// /*--------------------------------------------------------------------------------------------*/
// PROCESS_NAME(launcher_process);

// PROCESS(app_logo_process, "ui logo process");
// PROCESS_THREAD(app_logo_process, ev, data)
// {
//     PROCESS_BEGIN();

//     /* 动画播放结束(含结束延时)后, complete_cb 通过 process_poll 唤醒本进程,
//      * 删除 logo 控件并切换到启动器 */
//     while (s_logo != NULL && !sgl_gif_is_finished(s_logo)) {
//         PROCESS_WAIT_EVENT();
//     }

//     ui_logo_delete();

//     PROCESS_END();
// }

// /* sgl_gif 播放完成回调: 只负责唤醒处理进程, 避免在绘制上下文中删除控件 */
// static void logo_complete_cb(sgl_obj_t *obj)
// {
//     (void)obj;
//     process_poll(&app_logo_process);
// }
// /*--------------------------------------------------------------------------------------------*/
// void app_logo_task_init(void)
// {
//     if (s_logo != NULL) {
//         return;
//     }

//     const uint16_t frame_cnt = (uint16_t)SGL_ARRAY_SIZE(img_logo);
//     const uint16_t logo_w    = img_logo[0].width;
//     const uint16_t logo_h    = img_logo[0].height;

//     sgl_obj_t *scr = sgl_screen_act();
//     s_logo = sgl_gif_create(scr);
//     if (s_logo == NULL) {
//         SGL_LOG_ERROR("app_logo_task_init: gif create failed");
//         return;
//     }

//     sgl_gif_set_frames(s_logo, logo_w, logo_h, frame_cnt, img_logo);
//     sgl_gif_set_frame_interval(s_logo, LOGO_FRAME_INTERVAL_MS);
//     sgl_gif_set_finish_delay(s_logo, LOGO_FINISH_DELAY_MS);
//     sgl_gif_set_repeat_count(s_logo, 1);
//     sgl_gif_set_complete_cb(s_logo, logo_complete_cb);

//     /* 屏幕居中 */
//     sgl_obj_set_pos(s_logo,
//                     (SGL_SCREEN_WIDTH  - logo_w) / 2,
//                     (SGL_SCREEN_HEIGHT - logo_h) / 2);

//     process_start(&app_logo_process, NULL);
//     sgl_gif_start(s_logo);
//     LOG_I("app_logo_task_init!");
// }

// void ui_logo_delete(void)
// {
//     if (s_logo == NULL) {
//         return;
//     }

//     sgl_obj_t *logo = s_logo;
//     s_logo = NULL;
//     sgl_obj_delete_sync(logo);
//     process_exit(&app_logo_process);

//     process_start(&launcher_process, NULL);
// }
