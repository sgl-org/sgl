/* source/core/sgl_anim.c
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
#include <sgl_log.h>
#include <sgl_mm.h>
#include <string.h>

/* animation context */
static SGL_LIST_HEAD(anim_head);

/**
 * @brief  Animation static initialization
 * @param  anim - Animation object
 * @return none
 */
void sgl_anim_init(sgl_anim_t *anim)
{
    sgl_list_init(&anim->node);
    anim->data = NULL;
    anim->act_delay = 0;
    anim->act_duration = 0;
    anim->start_value = 0;
    anim->end_value = 0;

    anim->path_cb = NULL;
    anim->path_algo = NULL;

    anim->finish_cb = NULL;
    anim->auto_free = 0;
    anim->finished = 1;
}

/**
 * @brief dynamic alloc animation object with initialization
 * @param  none
 * @return animation object
*/
sgl_anim_t* sgl_anim_create(void)
{
    sgl_anim_t *anim = sgl_malloc(sizeof(sgl_anim_t));
    if (anim == NULL) {
        SGL_LOG_ERROR("sgl_anim_create: malloc failed");
        return NULL;
    }

    sgl_anim_init(anim);
    return anim;
}

/**
 * @brief add animation object to animation list
 * @param  anim animation object
 * @return none
*/
static inline void sgl_anim_add(sgl_anim_t *anim)
{
    sgl_list_add_node_at_tail(&anim_head, &anim->node);
    anim->finished = 0;
}

/**
 * @brief remove animation object from animation list
 * @param  anim animation object
 * @return none
*/
static inline void sgl_anim_remove(sgl_anim_t *anim)
{
    SGL_ASSERT(anim != NULL);
    sgl_list_del_node(&anim->node);
}

/**
 * @brief start animation
 * @param  anim animation object
 * @para  repeat_cnt repeat count of animation
 * @return none
*/
void sgl_anim_start(sgl_anim_t *anim, uint32_t repeat_cnt)
{
    SGL_ASSERT(anim != NULL);
    if (anim->finished && repeat_cnt) {
        sgl_anim_add(anim);
        anim->finished = 0;
    }

    if (anim->act_duration == 0) {
        SGL_LOG_WARN("animation duration is 0, you must set animation duration larger than 0");
        return;
    }

    anim->act_time = (uint16_t)((uint16_t)sgl_tick_get() + anim->act_delay);
    anim->repeat_cnt = repeat_cnt & SGL_ANIM_REPEAT_LOOP;
    anim->last_value = anim->start_value;
}

/**
 * @brief stop animation
 * @param  anim animation object
 * @return none
*/
void sgl_anim_stop(sgl_anim_t *anim)
{
    SGL_ASSERT(anim != NULL);
    if (!anim->finished) {
        sgl_anim_remove(anim);
        anim->finished = 1;
    }
}

/**
 * @brief delete animation object
 * @param anim animation object
 * @return none
*/
void sgl_anim_delete(sgl_anim_t *anim)
{
    SGL_ASSERT(anim != NULL);
    if (!anim->finished) {
        sgl_anim_stop(anim);
    } 
    sgl_free(anim);
}

/**
 * @brief delete animation object by object
 * @param  obj object
 * @return none
*/
void sgl_anim_delete_by_obj(struct sgl_obj *obj)
{
    sgl_anim_t *anim = sgl_anim_get_by_obj(obj);
    if (anim) {
        sgl_anim_delete(anim);
    }
}

/**
 * @brief delete all animation object
 * @param  none
 * @return none
*/
void sgl_anim_delete_all(void)
{
    sgl_anim_t *pos = NULL, *next = NULL;

    sgl_list_for_each_entry_safe(pos, next, &anim_head, sgl_anim_t, node) {
        sgl_anim_delete(pos);
    }
}

/**
 * @brief animation task, it will foreach all animation
 * @param  none
 * @return none
 * @note   this function should be called in sgl_task()
 */
void sgl_anim_task(void)
{
    int32_t value = 0;
    uint16_t elaps_time = 0;
    const uint16_t current_tick = (uint16_t)sgl_tick_get();
    sgl_anim_t *anim = NULL, *next = NULL;

    sgl_list_for_each_entry_safe(anim, next, &anim_head, sgl_anim_t, node) {
        elaps_time = (uint16_t)(current_tick - anim->act_time);
        if (elaps_time & 0x8000U) {
            continue;
        }

        /* check callback function for debug */
        SGL_ASSERT(anim->path_cb != NULL);
        SGL_ASSERT(anim->path_algo != NULL);
        value = anim->path_algo(sgl_min(elaps_time, anim->act_duration), anim->act_duration, anim->start_value, anim->end_value);
        if (value != anim->last_value) {
            anim->path_cb(anim, value);
            anim->last_value = value;
        }

        if (!anim->finished && elaps_time > anim->act_duration) {
            if (anim->repeat_cnt != SGL_ANIM_REPEAT_LOOP) {
                anim->repeat_cnt--;
            }

            if (anim->finish_cb) {
                anim->finish_cb(anim);
            }

            /* remove anim object if repeat count is 0 */
            if (anim->repeat_cnt == 0) {
                sgl_anim_stop(anim);
            }
            anim->act_time = (uint16_t)(current_tick + anim->act_delay);
        }

        if (anim->finished && anim->auto_free) {
            sgl_free(anim);
        }
    }
}

/**
 * @brief get animation object by object
 * @param  obj object
 * @return animation object
*/
sgl_anim_t* sgl_anim_get_by_obj(struct sgl_obj *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_anim_t *pos = NULL;
    
    sgl_list_for_each_entry(pos, &anim_head, sgl_anim_t, node) {
        if (pos->data == obj) {
            return pos;
        }
    }
    return NULL;
}

/**
 * @brief animation finished callback function, it will delete animation object
 * @param anim animation object
 * @return none
*/
void sgl_anim_finished_free_obj_cb(sgl_anim_t *anim)
{
    struct sgl_obj *obj = (struct sgl_obj*)anim->data;
    if (obj) {
        sgl_obj_delete(obj);
    }
}

/**
 * Linear animation path calculation function
 *
 * Calculates the current interpolated value based on elapsed time and total duration
 * using linear interpolation.
 *
 * @param elaps     Elapsed time in milliseconds
 * @param duration  Total animation duration in milliseconds
 * @param start     Start value
 * @param end       End value
 *
 * @return          The interpolated value for the current time
 *
 * @note            Returns 'end' if elaps >= duration (animation finished)
 *                  Returns 'start' if elaps == 0 (animation just started)
 *                  Uses 32-bit integer arithmetic to avoid floating-point operations
 *                  for better performance on embedded systems
 */
int32_t sgl_anim_path_linear(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    int64_t progress_fixed, delta, result;

    // If duration is zero or elapsed time exceeds duration, return end value
    if (elaps >= duration) {
        return (int32_t)end;
    }

    // If no time has elapsed, return start value
    if (elaps == 0) {
        return (int32_t)start;
    }

    // Calculate progress (elaps / duration) as a fixed-point number with 16 fractional bits
    // Use 64-bit intermediate to prevent overflow during multiplication
    progress_fixed = ((int64_t)elaps << 16) / duration;

    // Calculate the difference between end and start
    delta = end - start;

    // Compute the interpolated result: start + delta * (elaps/duration)
    // Right-shift by 16 to scale back from fixed-point representation
    result = start + ((delta * progress_fixed) >> 16);

    return result;
}

/**
 * sgl_anim_path_ease_in_out - Cubic ease-in-out animation path
 *
 * This function creates a smooth animation curve that starts slow,
 * accelerates in the middle, and decelerates at the end.
 *
 * @param elaps     Elapsed time (ms)
 * @param duration  Total animation duration (ms)
 * @param start     Start value
 * @param end       End value
 * @return          Interpolated value at current time
 */
int32_t sgl_anim_path_ease_in_out(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    int32_t t_180, cos_val, delta;
    if (elaps >= duration)
        return end;
    if (elaps == 0)
        return start;

    t_180 = (elaps * 180) / duration;

    if (t_180 <= 90) {
        cos_val = sgl_sin(90 - t_180);
    } else {
        cos_val = -sgl_sin(t_180 - 90);
    }

    // Now: progress = 0.5 * (1 - cos_val/1000) = (1000 - cos_val) / 2000
    // So: result = start + (end - start) * (1000 - cos_val) / 2000
    delta = end - start;
    return start + (delta * (32767 - cos_val)) / 65535;
}

/**
 * sgl_anim_path_ease_out - Cubic ease-in animation path
 *
 * This function creates a smooth animation curve that starts accelerates,
 * it will be slow in the after
 *
 * @param elaps     Elapsed time (ms)
 * @param duration  Total animation duration (ms)
 * @param start     Start value
 * @param end       End value
 * @return          Interpolated value at current time
 */
int32_t sgl_anim_path_ease_out(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    int32_t angle, sin_val, delta;
    if (elaps >= duration)
        return end;
    if (elaps == 0)
        return start;

    // t ∈ [0, 90] degrees: sin(t * 90 / duration)
    angle = (elaps * 90) / duration;
    sin_val = sgl_sin(angle);  // Assume returns 0 ~ 65535 or Q15

    // If sgl_sin returns ×1000 (e.g., sin(90) = 32767)
    delta = end - start;
    return start + ((delta * sin_val) >> 15);
}

/**
 * sgl_anim_path_ease_in - Cubic ease-in animation path
 *
 * This function creates a smooth animation curve that starts accelerates,
 * accelerates in the after
 *
 * @param elaps     Elapsed time (ms)
 * @param duration  Total animation duration (ms)
 * @param start     Start value
 * @param end       End value
 * @return          Interpolated value at current time
 */
int32_t sgl_anim_path_ease_in(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    int32_t angle, cos_val, delta;
    if (elaps >= duration)
        return end;
    if (elaps == 0)
        return start;

    // t ∈ [0, 90] degrees: sin(t * 90 / duration)
    angle = (elaps * 90) / duration;
    cos_val = sgl_cos(angle);  // Assume returns 0 ~ 65535 or Q15

    // If sgl_sin returns ×1000 (e.g., sin(90) = 32767)
    delta = end - start;
    return start + ((delta * (32767 - cos_val)) >> 15);
}

/**
 * sgl_anim_path_overshoot - Overshoot animation path
 *
 * This function creates an animation curve that overshoots the target end value
 * slightly before settling back to it, creating a natural "bounce" or "spring-like"
 * effect for a more dynamic and realistic animation.
 *
 * @param elaps     Elapsed time (ms) since the animation started
 * @param duration  Total animation duration (ms)
 * @param start     Initial value of the animated property at the start of the animation
 * @param end       Target end value of the animated property
 * @return          Interpolated value of the animated property at the current elapsed time
 */
int32_t sgl_anim_path_overshoot(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    int64_t t, t1, t2, inv_t2, ease_back, diff, progress;
    if (elaps >= duration)
        return end;

    t = (int64_t)elaps * 16384 / duration;
    diff = end - start;

    if (t < 11468) {
        t1 = t * 16384 / 11468;
        progress = (t1 * (32768 - t1)) >> 14; 
        progress = (progress * 18841) >> 14; 
    }
    else {
        t2 = (t - 11468) * 16384 / (16384 - 11468);
        inv_t2 = 16384 - t2;
        ease_back = (inv_t2 * inv_t2) >> 14;
        progress = 16384 + (ease_back * (18841 - 16384) >> 14);
    }

    return start + (int32_t)((diff * progress) >> 14);
}

/**
 * sgl_anim_path_ease_out_back - Ease out with slight overshoot and settle back
 *
 * Standard cubic-bezier(0.18, 0.89, 0.32, 1.28) approximation using integer math.
 * Creates a natural "pop" effect when elements appear or stop.
 */
int32_t sgl_anim_path_ease_out_back(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    if (elaps >= duration) return end;
    if (elaps == 0) return start;

    // t in Q15 format [0, 32767]
    int64_t t = ((int64_t)elaps << 15) / duration;
    
    // c1 = 1.70158 -> Q15 = 55754
    // c3 = c1 + 1  -> Q15 = 88521 (but we use the formula directly to avoid overflow)
    // Formula: 1 + c3 * pow(t-1, 3) + c1 * pow(t-1, 2)
    // Rewritten for fixed point: ((t-32767)^2 * (55754*(t-32767) + 88521)) >> 30 + 32767
    int64_t t_minus_1 = t - 32767; 
    int64_t t_minus_1_sq = (t_minus_1 * t_minus_1) >> 15;
    
    // 55754 * t_minus_1 is safe in int64
    int64_t inner = ((55754LL * t_minus_1) >> 15) + 88521LL;
    int64_t progress = ((t_minus_1_sq * inner) >> 15) + 32767;

    int32_t delta = end - start;
    return start + (int32_t)((delta * progress) >> 15);
}

/**
 * sgl_anim_path_ease_in_out_sine - Smooth sinusoidal ease-in-out
 *
 * Uses cosine curve for perfectly symmetric acceleration/deceleration.
 * Gentler than cubic ease-in-out, ideal for page transitions.
 */
int32_t sgl_anim_path_ease_in_out_sine(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    if (elaps >= duration) return end;
    if (elaps == 0) return start;

    // Map elapsed time to [0, 180] degrees for full cosine period
    int32_t angle = ((int32_t)elaps * 180) / duration;
    
    // cos(0)=32767, cos(180)=-32767
    // progress = (1 - cos(angle)) / 2 = (32767 - cos(angle)) / 65534
    int32_t cos_val = sgl_cos(angle); 
    
    int32_t delta = end - start;
    // Use 65534 as divisor since max-min of cos in Q15 is 65534
    return start + (int32_t)(((int64_t)delta * (32767 - cos_val)) / 65534);
}

/**
 * sgl_anim_path_ease_out_quart - Quartic ease-out animation path
 *
 * Stronger deceleration than cubic ease-out.
 * Formula: 1 - (1-t)^4, implemented purely with integer multiplication.
 */
int32_t sgl_anim_path_ease_out_quart(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    if (elaps >= duration) return end;
    if (elaps == 0) return start;

    // t in Q15
    int64_t t = ((int64_t)elaps << 15) / duration;
    int64_t inv_t = 32767 - t;
    
    // (1-t)^4 in Q15: (((inv_t^2)>>15)^2)>>15
    int64_t inv_t2 = (inv_t * inv_t) >> 15;
    int64_t inv_t4 = (inv_t2 * inv_t2) >> 15;
    
    // progress = 1 - (1-t)^4
    int64_t progress = 32767 - inv_t4;

    int32_t delta = end - start;
    return start + (int32_t)((delta * progress) >> 15);
}

/**
 * sgl_anim_path_ease_in_out_elastic - Spring-like elastic animation
 *
 * Combines sine wave oscillation with exponential decay envelope.
 * Pure integer implementation avoiding float sin/cos/exp.
 */
int32_t sgl_anim_path_ease_in_out_elastic(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    if (elaps >= duration) return end;
    if (elaps == 0) return start;

    // Normalize t to [0, 32767]
    int64_t t = ((int64_t)elaps << 15) / duration;
    
    // Map to angle: t * 4.5 * PI ≈ t * 14.137 -> Q15 scale factor ~463400
    // We simplify: use 3 full sine cycles over the duration
    int32_t angle = (int32_t)((t * 3 * 180) >> 15); // 3 half-periods = 1.5 full cycles
    
    int32_t sin_val = sgl_sin(angle % 360); // Handle wrap-around if needed, or rely on sgl_sin
    
    // Exponential decay envelope: approximate 2^(-10*t) using linear fade for embedded perf
    // Envelope goes from 32767 at t=0 to 0 at t=32767
    int64_t envelope = 32767 - t; 
    
    // Combine: base linear progress + damped oscillation
    // Oscillation amplitude scales with envelope
    int64_t oscillation = ((int64_t)sin_val * envelope) >> 15;
    
    // Blend: 70% linear + 30% oscillation for subtle elastic feel
    int64_t progress = (t * 22937 + oscillation * 9830) >> 15; // 22937≈0.7*32767, 9830≈0.3*32767

    int32_t delta = end - start;
    return start + (int32_t)((delta * progress) >> 15);
}

/**
 * sgl_anim_path_ease_out_bounce - Realistic bouncing ball effect
 *
 * Simulates gravity and elastic collision using piecewise quadratic curves.
 * Pure integer implementation of the standard CSS/Android bounce easing.
 */
int32_t sgl_anim_path_ease_out_bounce(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    if (elaps >= duration) return end;
    if (elaps == 0) return start;

    // Normalize time to Q15 [0, 32767]
    int64_t t = ((int64_t)elaps << 15) / duration;
    int64_t progress = 0;

    // Piecewise quadratic approximation in Q15
    // Boundaries: 1/2.75≈0.3636(11911), 2/2.75≈0.7272(23822), 2.5/2.75≈0.9090(29778)
    if (t < 11911) {
        // 7.5625 * t^2 -> Q15 coeff ≈ 247868 (clamped to safe shift)
        progress = (247868LL * ((t * t) >> 15)) >> 15;
    } else if (t < 23822) {
        int64_t t2 = t - 18181; // t - 1.5/2.75
        progress = (247868LL * ((t2 * t2) >> 15) >> 15) + 24576; // +0.75*32767
    } else if (t < 29778) {
        int64_t t2 = t - 27272; // t - 2.25/2.75
        progress = (247868LL * ((t2 * t2) >> 15) >> 15) + 29490; // +0.9*32767
    } else {
        int64_t t2 = t - 31111; // t - 2.625/2.75
        progress = (247868LL * ((t2 * t2) >> 15) >> 15) + 31539; // +0.9625*32767
    }

    int32_t delta = end - start;
    return start + (int32_t)((delta * progress) >> 15);
}

/**
 * sgl_anim_path_ease_out_bounce_hold - Bouncing ball effect with hold
 *
 * Similar to sgl_anim_path_ease_out_bounce, but holds at the end.
 * Useful for animating objects that need to stay in place after reaching their destination.
 */
int32_t sgl_anim_path_ease_out_bounce_hold(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    if (elaps >= duration) return end;
    if (elaps == 0) return start;

    uint16_t half_duration = duration / 2;
    if (elaps >= half_duration) {
        return end;
    }

    int64_t t = ((int64_t)elaps << 15) / half_duration;
    int64_t progress = 0;

    // Piecewise quadratic approximation in Q15
    // Boundaries: 1/2.75≈0.3636(11911), 2/2.75≈0.7272(23822), 2.5/2.75≈0.9090(29778)
    if (t < 11911) {
        // 7.5625 * t^2 -> Q15 coeff ≈ 247868 (clamped to safe shift)
        progress = (247868LL * ((t * t) >> 15)) >> 15;
    } else if (t < 23822) {
        int64_t t2 = t - 18181; // t - 1.5/2.75
        progress = (247868LL * ((t2 * t2) >> 15) >> 15) + 24576; // +0.75*32767
    } else if (t < 29778) {
        int64_t t2 = t - 27272; // t - 2.25/2.75
        progress = (247868LL * ((t2 * t2) >> 15) >> 15) + 29490; // +0.9*32767
    } else {
        int64_t t2 = t - 31111; // t - 2.625/2.75
        progress = (247868LL * ((t2 * t2) >> 15) >> 15) + 31539; // +0.9625*32767
    }

    int32_t delta = end - start;
    return start + (int32_t)((delta * progress) >> 15);
}

/**
 * sgl_anim_path_sine_wave - Continuous sinusoidal wave trajectory
 *
 * Creates a smooth, periodic oscillation between start and end values.
 * Ideal for floating elements, breathing effects, or loading indicators.
 * @note This is a cyclic path; it does NOT settle at 'end' value.
 */
int32_t sgl_anim_path_sine_wave(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    if (duration == 0) return start;

    // Map elapsed time to full sine cycles
    // Adjust multiplier (e.g., *360) to control wave frequency
    int32_t angle = ((int32_t)elaps * 360) / duration; 
    
    // sgl_sin returns [-32767, 32767], normalize to [0, 32767] range for amplitude
    int32_t sin_val = sgl_sin(angle);
    
    // Amplitude is half the distance between start and end
    int32_t amplitude = (end - start) / 2;
    int32_t center = start + amplitude;

    // Center + amplitude * sin(angle)
    return center + (int32_t)(((int64_t)amplitude * sin_val) >> 15);
}

/**
 * sgl_anim_path_damped_spring - Decaying spring oscillation settling at end
 *
 * Combines sine wave with linear decay envelope.
 * Starts with oscillation and smoothly settles exactly at 'end' value.
 */
int32_t sgl_anim_path_damped_spring(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    if (elaps >= duration) return end;
    if (elaps == 0) return start;

    int64_t t = ((int64_t)elaps << 15) / duration;
    
    // Linear base progress from start to end
    int64_t base_progress = t;
    
    // Oscillation: 2.5 full cycles (2.5 * 360 = 900 degrees mapping)
    int32_t angle = (int32_t)((t * 900) >> 15);
    int32_t sin_val = sgl_sin(angle);
    
    // Decay envelope: (1 - t) in Q15, ensures oscillation dies out at t=1
    int64_t envelope = 32767 - t;
    
    // Blend: base progress + damped oscillation offset
    // Oscillation strength controlled by divisor (adjust 3 for more/less wobble)
    int64_t oscillation = ((int64_t)sin_val * envelope) >> 15;
    int64_t progress = base_progress + (oscillation / 3);

    int32_t delta = end - start;
    return start + (int32_t)((delta * progress) >> 15);
}

/**
 * sgl_anim_path_step - Discrete stepped animation (no interpolation)
 *
 * Divides animation into equal steps and jumps between them.
 * Perfect for sprite animations, digit counters, or segmented progress bars.
 * @param steps     Number of discrete steps (pass via 'start' param repurposed, 
 *                  or hardcode. Here we use 10 steps as example)
 */
int32_t sgl_anim_path_step(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    if (elaps >= duration) return end;
    if (elaps == 0) return start;

    // Define number of steps (can be made configurable via macro or extra param)
    const int32_t NUM_STEPS = 10; 
    
    // Calculate current step index using integer division
    int64_t step_index = ((int64_t)elaps * NUM_STEPS) / duration;
    
    // Quantized progress = step_index / NUM_STEPS in Q15
    int64_t progress = (step_index << 15) / NUM_STEPS;

    int32_t delta = end - start;
    return start + (int32_t)((delta * progress) >> 15);
}

/**
 * sgl_anim_obj_hori_cb - Callback function for horizontal animation
 * @param anim      Pointer to the animation object
 * @param value     Current value of the animation
 * @return none
 */
static void sgl_anim_obj_hori_cb(sgl_anim_t *anim, int32_t value)
{
    struct sgl_obj *obj = (struct sgl_obj *)anim->data;
    sgl_obj_set_pos_x(obj, value);
}

/**
 * sgl_anim_obj_vert_cb - Callback function for vertical animation
 * @param anim      Pointer to the animation object
 * @param value     Current value of the animation
 * @return none
 */
static void sgl_anim_obj_vert_cb(sgl_anim_t *anim, int32_t value)
{
    struct sgl_obj *obj = (struct sgl_obj *)anim->data;
    sgl_obj_set_pos_y(obj, value);
}

/**
 * sgl_anim_alloc_and_init - Allocate and initialize an animation object
 * @param obj           Pointer to the object to animate
 * @param start_value   Starting value of the animation
 * @param end_value     Ending value of the animation
 * @param duration      Duration of the animation (in milliseconds)
 * @param cb            Callback function for animation path
 * @param effect        Animation path effect
 * @return              Pointer to the allocated and initialized animation object, or NULL on failure
 */
static sgl_anim_t *sgl_anim_alloc_and_init(struct sgl_obj *obj, int16_t start_value, int16_t end_value, uint16_t duration, sgl_anim_path_cb_t cb, sgl_anim_path_algo_t effect)
{
    sgl_anim_t *anim = sgl_anim_create();
    if (!anim) {
        SGL_LOG_ERROR("Failed to create animation object");
        return NULL;
    }

    sgl_anim_set_data(anim, obj);
    sgl_anim_set_auto_free(anim);
    sgl_anim_set_start_value(anim, start_value);
    sgl_anim_set_end_value(anim, end_value);
    sgl_anim_set_act_duration(anim, duration);
    sgl_anim_set_path(anim, cb, effect);

    return anim;
}

/**
 * sgl_anim_move_obj_hori - Move an object horizontally
 * @param obj       Pointer to the object to move
 * @param distance  Distance to move the object horizontally
 * @param duration  Duration of the animation (in milliseconds)
 * @param effect    Animation path effect (e.g., SGL_ANIM_PATH_EASE_IN_OUT, SGL_ANIM_PATH_EASE_IN, SGL_ANIM_PATH_EASE_OUT)
 * @return none
 */
void sgl_anim_move_obj_hori(struct sgl_obj *obj, int16_t distance, uint16_t duration, sgl_anim_path_algo_t effect)
{
    sgl_anim_t *anim = sgl_anim_alloc_and_init(obj, obj->coords.x1, obj->coords.x1 + distance, duration, sgl_anim_obj_hori_cb, effect);
    if (!anim) {
        SGL_LOG_ERROR("Failed to create horizontal animation object");
        return;
    }
    sgl_anim_start(anim, SGL_ANIM_REPEAT_ONCE);
}

/**
 * sgl_anim_move_obj_hori_with_free - Move an object horizontally and free object automatically
 * @param obj       Pointer to the object to move
 * @param distance  Distance to move the object horizontally
 * @param duration  Duration of the animation (in milliseconds)
 * @param effect    Animation path effect (e.g., SGL_ANIM_PATH_EASE_IN_OUT, SGL_ANIM_PATH_EASE_IN, SGL_ANIM_PATH_EASE_OUT)
 * @return none
 */
void sgl_anim_move_obj_hori_with_free(struct sgl_obj *obj, int16_t distance, uint16_t duration, sgl_anim_path_algo_t effect)
{
    sgl_anim_t *anim = sgl_anim_alloc_and_init(obj, obj->coords.x1, obj->coords.x1 + distance, duration, sgl_anim_obj_hori_cb, effect);
    if (!anim) {
        SGL_LOG_ERROR("Failed to create horizontal animation object");
        return;
    }
    sgl_anim_set_finish_cb(anim, sgl_anim_finished_free_obj_cb);
    sgl_anim_start(anim, SGL_ANIM_REPEAT_ONCE);
}

/**
 * sgl_anim_move_obj_vert - Move an object vertically
 * @param obj       Pointer to the object to move
 * @param distance  Distance to move the object vertically
 * @param duration  Duration of the animation (in milliseconds)
 * @param effect    Animation path effect (e.g., SGL_ANIM_PATH_EASE_IN_OUT, SGL_ANIM_PATH_EASE_IN, SGL_ANIM_PATH_EASE_OUT)
 * @return none
 */
void sgl_anim_move_obj_vert(struct sgl_obj *obj, int16_t distance, uint16_t duration, sgl_anim_path_algo_t effect)
{
    sgl_anim_t *anim = sgl_anim_alloc_and_init(obj, obj->coords.y1, obj->coords.y1 + distance, duration, sgl_anim_obj_vert_cb, effect);
    if (!anim) {
        SGL_LOG_ERROR("Failed to create vertical animation object");
        return;
    }
    sgl_anim_start(anim, SGL_ANIM_REPEAT_ONCE);
}

/**
 * sgl_anim_move_obj_vert_with_free - Move an object vertically and free object automatically
 * @param obj       Pointer to the object to move
 * @param distance  Distance to move the object vertically
 * @param duration  Duration of the animation (in milliseconds)
 * @param effect    Animation path effect (e.g., SGL_ANIM_PATH_EASE_IN_OUT, SGL_ANIM_PATH_EASE_IN, SGL_ANIM_PATH_EASE_OUT)
 * @return none
 */
void sgl_anim_move_obj_vert_with_free(struct sgl_obj *obj, int16_t distance, uint16_t duration, sgl_anim_path_algo_t effect)
{
    sgl_anim_t *anim = sgl_anim_alloc_and_init(obj, obj->coords.y1, obj->coords.y1 + distance, duration, sgl_anim_obj_vert_cb, effect);
    if (!anim) {
        SGL_LOG_ERROR("Failed to create vertical animation object");
        return;
    }
    sgl_anim_set_finish_cb(anim, sgl_anim_finished_free_obj_cb);
    sgl_anim_start(anim, SGL_ANIM_REPEAT_ONCE);
}

/**
 * sgl_anim_setup_action - Create and start a one-shot animation with a custom path callback
 * @param start     Start value of the animation
 * @param end       End value of the animation
 * @param duration  Duration of the animation (in milliseconds)
 * @param cb        Callback function for the animation
 * @param effect    Animation path effect (e.g., SGL_ANIM_PATH_EASE_IN_OUT, SGL_ANIM_PATH_EASE_IN, SGL_ANIM_PATH_EASE_OUT)
 * @param finish_cb Callback function to be called when the animation finishes
 * @return none
 */
void sgl_anim_setup_action(int16_t start, int16_t end, uint16_t duration, sgl_anim_path_cb_t cb, sgl_anim_path_algo_t effect, sgl_anim_finish_cb_t finish_cb)
{
    sgl_anim_t *anim = sgl_anim_alloc_and_init(NULL, start, end, duration, cb, effect);
    if (!anim) {
        SGL_LOG_ERROR("Failed to create animation object for sgl_anim_setup_action");
        return;
    }
    sgl_anim_set_finish_cb(anim, finish_cb);
    sgl_anim_start(anim, SGL_ANIM_REPEAT_ONCE);
}

/**
 * sgl_anim_setup_action_loop - Create and start a looping animation with a custom path callback
 * @param start     Start value of the animation
 * @param end       End value of the animation
 * @param duration  Duration of the animation (in milliseconds)
 * @param cb        Callback function for the animation
 * @param effect    Animation path effect (e.g., SGL_ANIM_PATH_EASE_IN_OUT, SGL_ANIM_PATH_EASE_IN, SGL_ANIM_PATH_EASE_OUT)
 * @param finish_cb Callback function to be called when the animation finishes
 * @return none
 */
void sgl_anim_setup_action_loop(int16_t start, int16_t end, uint16_t duration, sgl_anim_path_cb_t cb, sgl_anim_path_algo_t effect, sgl_anim_finish_cb_t finish_cb)
{
    sgl_anim_t *anim = sgl_anim_alloc_and_init(NULL, start, end, duration, cb, effect);
    if (!anim) {
        SGL_LOG_ERROR("Failed to create animation object for sgl_anim_setup_action_loop");
        return;
    }
    sgl_anim_set_finish_cb(anim, finish_cb);
    sgl_anim_start(anim, SGL_ANIM_REPEAT_LOOP);
}

/**
 * sgl_anim_setup_obj_action - Create and start a one-shot animation attached to an object
 * @param obj       Pointer to the object to attach the animation to
 * @param start     Start value of the animation
 * @param end       End value of the animation
 * @param duration  Duration of the animation (in milliseconds)
 * @param cb        Callback function for the animation
 * @param effect    Animation path effect (e.g., SGL_ANIM_PATH_EASE_IN_OUT, SGL_ANIM_PATH_EASE_IN, SGL_ANIM_PATH_EASE_OUT)
 * @param finish_cb Callback function to be called when the animation finishes
 * @return none
 */
void sgl_anim_setup_obj_action(struct sgl_obj *obj, int16_t start, int16_t end, uint16_t duration, sgl_anim_path_cb_t cb, sgl_anim_path_algo_t effect, sgl_anim_finish_cb_t finish_cb)
{
    sgl_anim_t *anim = sgl_anim_alloc_and_init(obj, start, end, duration, cb, effect);
    if (!anim) {
        SGL_LOG_ERROR("Failed to create animation object for sgl_anim_setup_obj_action");
        return;
    }
    sgl_anim_set_finish_cb(anim, finish_cb);
    sgl_anim_start(anim, SGL_ANIM_REPEAT_ONCE);
}

/**
 * sgl_anim_setup_obj_action_loop - Create and start a looping animation with a custom path callback
 * @param obj       Pointer to the object to attach the animation to
 * @param start     Start value of the animation
 * @param end       End value of the animation
 * @param duration  Duration of the animation (in milliseconds)
 * @param cb        Callback function for the animation
 * @param effect    Animation path effect (e.g., SGL_ANIM_PATH_EASE_IN_OUT, SGL_ANIM_PATH_EASE_IN, SGL_ANIM_PATH_EASE_OUT)
 * @param finish_cb Callback function to be called when the animation finishes
 * @return none
 */
void sgl_anim_setup_obj_action_loop(struct sgl_obj *obj, int16_t start, int16_t end, uint16_t duration, sgl_anim_path_cb_t cb, sgl_anim_path_algo_t effect, sgl_anim_finish_cb_t finish_cb)
{
    sgl_anim_t *anim = sgl_anim_alloc_and_init(obj, start, end, duration, cb, effect);
    if (!anim) {
        SGL_LOG_ERROR("Failed to create animation object for sgl_anim_setup_obj_action_loop");
        return;
    }
    sgl_anim_set_finish_cb(anim, finish_cb);
    sgl_anim_start(anim, SGL_ANIM_REPEAT_LOOP);
}

#if (CONFIG_SGL_ANIM_TRANSITION)
/* one-row scratch, reused across bands; grows to the panel width on demand */
static sgl_color_t *s_trans_row;
static uint16_t     s_trans_row_w;

static sgl_color_t *trans_row_scratch(uint16_t w)
{
    if (s_trans_row == NULL || s_trans_row_w < w) {
        if (s_trans_row != NULL) {
            sgl_free(s_trans_row);
        }
        s_trans_row = (sgl_color_t *)sgl_malloc((size_t)w * sizeof(sgl_color_t));
        s_trans_row_w = (s_trans_row != NULL) ? w : 0;
    }
    return s_trans_row;
}

uint8_t sgl_anim_trans_reverse(uint8_t type)
{
    static const int8_t rev[SGL_ANIM_TRANS_COUNT] = {
        SGL_ANIM_TRANS_SLIDE_R, SGL_ANIM_TRANS_SLIDE_L,
        SGL_ANIM_TRANS_SLIDE_D, SGL_ANIM_TRANS_SLIDE_U,
        SGL_ANIM_TRANS_COVER_D, SGL_ANIM_TRANS_COVER_U,
        SGL_ANIM_TRANS_FADE,
        SGL_ANIM_TRANS_GENIE_IN, SGL_ANIM_TRANS_GENIE_OUT,
        SGL_ANIM_TRANS_S_IN, SGL_ANIM_TRANS_S_OUT,
    };
    if ((int8_t)type == SGL_ANIM_TRANS_NONE || type >= SGL_ANIM_TRANS_COUNT) {
        return type;
    }
    return (uint8_t)rev[type];
}

/*
 * Band-local genie / S funnel toward the bottom dock.  dst is written in
 * place; base holds the static (destination) page for these rows and warp
 * holds the page being sucked into / poured out of the dock.  As q grows the
 * outgoing page is absorbed from the top (far) edge downward, and every
 * surviving row is resampled into a span that stays full width at the
 * receding top edge and funnels narrower down to the dock -- the classic
 * genie silhouette.  All reads stay inside the current band, so a single
 * band-sized scratch is enough (no cross-band fetch).
 */
static void trans_warp_rows(sgl_color_t *dst, const sgl_color_t *base,
                            const sgl_color_t *warp, int y1, int bw, int bh,
                            int H, int q, int is_s)
{
    sgl_color_t *rs = trans_row_scratch((uint16_t)bw);
    const int half = bw / 2;
    const int span = (256 - q > 0) ? (256 - q) : 1;   /* rows not yet absorbed, Q8 */
    if (rs == NULL) {
        return;
    }
    for (int r = 0; r < bh; r++) {
        const sgl_color_t *brow = base + (size_t)r * bw;
        const sgl_color_t *wrow = warp + (size_t)r * bw;
        sgl_color_t *drow = dst + (size_t)r * bw;
        int vpos = (H > 1) ? ((y1 + r) * 256 / (H - 1)) : 256;   /* 0 top .. 256 dock */

        if (vpos < q) {                     /* top rows absorbed first: keep base */
            memcpy(drow, brow, (size_t)bw * sizeof(sgl_color_t));
            continue;
        }
        /* depth toward the dock: 0 at the receding top edge .. 256 at the dock */
        int h = (int)((long)(vpos - q) * 256 / span);
        if (h > 256) {
            h = 256;
        }
        int shrink = (int)(((long)q * h * (512 - h)) >> 16);   /* concave: 0 at edge .. q at dock */
        int hwf = 256 - shrink;                         /* width factor Q8, > 0 */
        int halfW = (hwf > 0) ? (int)((long)half * hwf >> 8) : 0;
        int cxi = half;
        if (is_s) {
            cxi += (int)(((long)(h - 128) * q * half) >> 16);   /* lateral bow toward the dock */
        }
        for (int x = 0; x < bw; x++) {
            int dx = x - cxi;
            if (hwf > 0 && dx >= -halfW && dx <= halfW) {
                int xs = cxi + (int)(((long)dx * 256) / hwf);
                if (xs < 0) xs = 0;
                else if (xs >= bw) xs = bw - 1;
                rs[x] = wrow[xs];
            } else {
                rs[x] = brow[x];
            }
        }
        memcpy(drow, rs, (size_t)bw * sizeof(sgl_color_t));
    }
}

void sgl_anim_tramsition(struct sgl_surf *surf, uint8_t type)
{
    if (sgl_system.trans_head == NULL) {
        return;
    }

    sgl_surf_t *s = (sgl_surf_t *)surf;
    const int bw = s->x2 - s->x1 + 1;
    const int bh = s->y2 - s->y1 + 1;
    if (bw <= 0 || bh <= 0) {
        return;
    }

    const int H = sgl_system.fbdev.fbinfo.yres;
    sgl_color_t *newp = s->buffer;              /* base page drawn by caller = destination page */
    sgl_color_t *oldp = sgl_system.trans_buf;   /* outgoing page rendered into the scratch band  */
    int prog = sgl_system.trans_prog;           /* 0..256 */
    if (prog > 256) {
        prog = 256;
    }
    const size_t total = (size_t)bw * bh;

    switch (type) {
    case SGL_ANIM_TRANS_FADE: {
        uint8_t a = (uint8_t)(prog > 255 ? 255 : prog);
        for (size_t i = 0; i < total; i++) {
            newp[i] = sgl_color_mixer(newp[i], oldp[i], a);
        }
        break;
    }
    case SGL_ANIM_TRANS_SLIDE_L:
    case SGL_ANIM_TRANS_SLIDE_R: {
        sgl_color_t *rs = trans_row_scratch((uint16_t)bw);
        if (rs == NULL) {
            break;
        }
        int shift = (int)((long)prog * (bw - 1) / 256);
        for (int r = 0; r < bh; r++) {
            sgl_color_t *d = newp + (size_t)r * bw;
            sgl_color_t *o = oldp + (size_t)r * bw;
            if (type == SGL_ANIM_TRANS_SLIDE_L) {
                for (int x = 0; x < bw; x++) {
                    int v = x + shift;
                    rs[x] = (v < bw) ? o[v] : d[v - bw];
                }
            } else {
                for (int x = 0; x < bw; x++) {
                    rs[x] = (x >= shift) ? o[x - shift] : d[bw - shift + x];
                }
            }
            memcpy(d, rs, (size_t)bw * sizeof(sgl_color_t));
        }
        break;
    }
    case SGL_ANIM_TRANS_SLIDE_U:
    case SGL_ANIM_TRANS_SLIDE_D:
    case SGL_ANIM_TRANS_COVER_U:
    case SGL_ANIM_TRANS_COVER_D: {
        /* vertical reveal seam (band-local): rows split by a moving edge, the
         * destination page wipes in over the static one. A fully translated
         * vertical push needs neighbouring bands, out of scope for one band. */
        int shift = (int)((long)prog * (H - 1) / 256);
        int from_bottom = (type == SGL_ANIM_TRANS_SLIDE_U || type == SGL_ANIM_TRANS_COVER_U);
        for (int r = 0; r < bh; r++) {
            int y = s->y1 + r;
            int shown = from_bottom ? (y >= H - 1 - shift) : (y <= shift);
            if (!shown) {
                sgl_color_t *d = newp + (size_t)r * bw;
                const sgl_color_t *o = oldp + (size_t)r * bw;
                memcpy(d, o, (size_t)bw * sizeof(sgl_color_t));
            }
        }
        break;
    }
    case SGL_ANIM_TRANS_GENIE_OUT:
    case SGL_ANIM_TRANS_S_OUT:
        /* destination = new (base), outgoing = old (warped away) */
        trans_warp_rows(newp, newp, oldp, s->y1, bw, bh, H, prog,
                        type == SGL_ANIM_TRANS_S_OUT);
        break;
    case SGL_ANIM_TRANS_GENIE_IN:
    case SGL_ANIM_TRANS_S_IN:
        /* destination = new (warped in), base = old */
        trans_warp_rows(newp, oldp, newp, s->y1, bw, bh, H, 256 - prog,
                        type == SGL_ANIM_TRANS_S_IN);
        break;
    default:
        break;
    }
}
#endif
