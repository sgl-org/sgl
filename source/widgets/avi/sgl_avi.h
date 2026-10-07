/* source/widgets/avi/sgl_avi.h
 *
 * AVI container player widget.
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

#ifndef __SGL_AVI_H__
#define __SGL_AVI_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_fs.h>
#include <sgl_cfgfix.h>
#include <sgl_anim.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum decoded RGB565 frame buffer: 2 * decoded_width * decoded_height.
 * The decoder scales by 1/1, 1/2, 1/4 or 1/8 to fit this byte budget.
 * 160 KiB fits a full 320x240 frame.
 * Low-memory MCUs: a 48 KiB budget decodes a 320x240 source at 160x120
 * (1/2 scale) and saves ~100 KiB of heap. */
#ifndef SGL_AVI_PIXMAP_MAX
#define SGL_AVI_PIXMAP_MAX          (160 * 1024)
#endif

/* decoder work pool for the inlined JPEG decoder. 16 KiB covers the 4KB
 * stream input buffer, the fast-decode huffman LUTs (6 KiB), quant and
 * huffman tables plus MCU work areas of standard MJPEG streams */
#ifndef SGL_AVI_JDEC_POOL_SIZE
#define SGL_AVI_JDEC_POOL_SIZE      (16 * 1024)
#endif

/* video frame staging buffer. Each video chunk is bulk-read here first so
 * that the decoder runs from RAM at full speed. Chunks larger than this
 * budget fall back to a streaming decode straight from the file. */
#ifndef SGL_AVI_VBUF_MAX
#define SGL_AVI_VBUF_MAX            (18 * 1024)
#endif

/* audio file read / port write chunk size per pump pass */
#ifndef SGL_AVI_AUDIO_CHUNK
#define SGL_AVI_AUDIO_CHUNK         (1024)
#endif

/* PCM buffering owned and allocated by the AVI core. 8 KiB is about 190 ms
 * of 44.1 kHz / 16-bit / stereo audio; the pump refills it every few ms */
#ifndef SGL_AVI_AUDIO_BUFFER_SIZE
#define SGL_AVI_AUDIO_BUFFER_SIZE   (8 * 1024)
#endif

/**
 * @brief platform PCM output interface used by the AVI player
 *
 * Register one instance with sgl_avi_set_audio_port() before playback. The
 * AVI core owns a 16 KiB PCM ring and calls write() with complete PCM sample
 * frames. The port only needs to submit those bytes to its output device; it
 * does not need to allocate or own an intermediate PCM buffer.
 *
 * AVI audio is uncompressed PCM. The current parser accepts 1 or 2 channels
 * and 8-bit or 16-bit samples. Callbacks run from the SGL playback/task
 * context, except where an implementation's own device driver invokes its
 * asynchronous completion or interrupt handlers.
 */
typedef struct {
    /**
     * @brief platform context passed to every callback
     * @note the pointed-to object must remain valid until this port is unregistered
     */
    void *user_data;

    /**
     * @brief open or prepare the audio output for the AVI PCM format
     * @param user_data platform context
     * @param sample_rate samples per second from the AVI stream
     * @param channels number of channels (1 or 2)
     * @param bits bits per sample (8 or 16)
     * @return 0 on success; nonzero disables audio playback for this file
     * @note called whenever playback starts or resumes; reset the consumed
     *       byte counter to zero; stop() may still be called after failure
     */
    int (*start)(void *user_data, uint32_t sample_rate, uint8_t channels, uint8_t bits);

    /**
     * @brief stop the output and discard platform-queued PCM
     * @param user_data platform context
     * @note must be safe after a failed start(); called on pause, stop,
     *       playback completion, and widget destruction
     */
    void (*stop)(void *user_data);

    /**
     * @brief submit PCM bytes from the AVI core ring to the output device
     * @param user_data platform context
     * @param data PCM data, valid only for the duration of this call
     * @param size requested byte count, aligned to a complete sample frame
     * @return number of bytes accepted (0..size); return 0 when the device
     *         cannot accept data; partial writes are retried by the AVI core
     * @note copy data into a hardware/DMA queue before returning if the device
     *       consumes it asynchronously
     */
    int32_t (*write)(void *user_data, const uint8_t *data, uint32_t size);

    /**
     * @brief get the number of PCM bytes actually played by the device
     * @param user_data platform context
     * @return monotonic byte count since the most recent start() or flush()
     * @note count consumed bytes, not submitted or queued bytes; this is the
     *       master clock used to synchronize video presentation
     */
    uint32_t (*get_consumed_bytes)(void *user_data);

    /**
     * @brief discard queued platform PCM and reset its consumed-byte counter
     * @param user_data platform context
     * @note called when seeking; the AVI core separately clears its own PCM ring
     */
    void (*flush)(void *user_data);
} sgl_avi_audio_port_t;

/**
 * @brief playback state
 */
typedef enum {
    SGL_AVI_STATE_STOPPED = 0,
    SGL_AVI_STATE_PLAYING,
    SGL_AVI_STATE_PAUSED,
} sgl_avi_state_t;

/* opaque type, defined in sgl_avi.c */
typedef struct sgl_avi sgl_avi_t;

/**
 * @brief create an AVI player object
 * @param parent parent object; NULL creates the player on the active screen
 * @return pointer to the player object, or NULL if allocation fails
 */
sgl_obj_t* sgl_avi_create(sgl_obj_t *parent);

/**
 * @brief register the platform PCM output callbacks
 * @param port callback table to register; NULL unregisters the current port
 * @return 0 on success, or -1 if a required callback is missing
 * @note register the port before sgl_avi_play(); video can play without an
 *       audio port, but PCM audio will be disabled
 */
int sgl_avi_set_audio_port(const sgl_avi_audio_port_t *port);

/**
 * @brief open an AVI file and parse its stream headers and indexes
 * @param obj AVI player object
 * @param path file path accessible through the mounted SGL filesystem
 * @return 0 on success, or -1 if the file cannot be opened or parsed
 * @note supports MJPEG video and optional uncompressed PCM audio
 */
int sgl_avi_load_file(sgl_obj_t *obj, const char *path);

/**
 * @brief start or resume AVI audio and video playback
 * @param obj AVI player object
 * @return none
 * @note does nothing if no file is open or playback is already running
 */
void sgl_avi_play(sgl_obj_t *obj);

/**
 * @brief pause playback and keep the current frame on screen
 * @param obj AVI player object
 * @return none
 */
void sgl_avi_pause(sgl_obj_t *obj);

/**
 * @brief stop playback and rewind audio and video to the first frame
 * @param obj AVI player object
 * @return none
 */
void sgl_avi_stop(sgl_obj_t *obj);

/**
 * @brief override the frame rate read from the AVI header
 * @param obj AVI player object
 * @param fps playback rate in frames per second; clamped to 1..60
 * @return none
 */
void sgl_avi_set_fps(sgl_obj_t *obj, uint8_t fps);

/**
 * @brief set the minimum JPEG downscale factor
 * @param obj AVI player object
 * @param scale 0=full, 1=half, 2=quarter, or 3=eighth resolution
 * @return none
 * @note the decoder may downscale further to satisfy SGL_AVI_PIXMAP_MAX
 */
void sgl_avi_set_decode_scale(sgl_obj_t *obj, uint8_t scale);

/**
 * @brief seek to a video frame and align audio to the same playback position
 * @param obj AVI player object
 * @param frame_index zero-based frame index; values are clamped to the file
 * @return 0 on success, or -1 if no valid AVI file is open
 * @note MJPEG frames are independently decodable keyframes
 */
int sgl_avi_seek_frame(sgl_obj_t *obj, int32_t frame_index);

/**
 * @brief seek to a percentage of the AVI duration
 * @param obj AVI player object
 * @param percent playback position from 0 to 100; values are clamped
 * @return 0 on success, or -1 if no file or frame index is available
 */
int sgl_avi_seek_percent(sgl_obj_t *obj, int32_t percent);

/**
 * @brief get the total number of video frames
 * @param obj AVI player object
 * @return indexed frame count, header frame count, or 0 if unknown
 */
int32_t sgl_avi_get_frame_total(sgl_obj_t *obj);

/**
 * @brief get the index of the most recently decoded frame
 * @param obj AVI player object
 * @return zero-based frame index; returns 0 before the first frame is decoded
 */
int32_t sgl_avi_tell_frame(sgl_obj_t *obj);

/**
 * @brief get the total AVI duration
 * @param obj AVI player object
 * @return duration in milliseconds, or 0 if it cannot be determined
 */
int32_t sgl_avi_get_duration(sgl_obj_t *obj);

/**
 * @brief get the current playback position
 * @param obj AVI player object
 * @return position in milliseconds, using the audio clock when available or
 *         the decoded video frame otherwise
 */
int32_t sgl_avi_get_position(sgl_obj_t *obj);

/**
 * @brief get the opened AVI file size
 * @param obj AVI player object
 * @return file size in bytes, or -1 if no file is open
 */
int32_t sgl_avi_get_file_size(sgl_obj_t *obj);

/**
 * @brief set whether playback restarts when the stream ends
 * @param obj AVI player object
 * @param loop true to repeat playback, false to stop at the end
 * @return none
 */
void sgl_avi_set_loop(sgl_obj_t *obj, bool loop);

/**
 * @brief get the current playback state
 * @param obj AVI player object
 * @return current state; returns SGL_AVI_STATE_STOPPED for NULL
 */
sgl_avi_state_t sgl_avi_get_state(sgl_obj_t *obj);

/**
 * @brief check whether the opened AVI contains a supported PCM audio stream
 * @param obj AVI player object
 * @return true if a supported PCM stream is present, otherwise false
 */
bool sgl_avi_has_audio(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* __SGL_AVI_H__ */
