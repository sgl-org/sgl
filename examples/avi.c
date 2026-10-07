#include <sgl.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>

#ifndef SGL_AVI_DEMO_BASE_DIR
#define SGL_AVI_DEMO_BASE_DIR "."
#endif

#define AVI_SDL_AUDIO_QUEUE_LIMIT (4 * 1024)

typedef struct {
    SDL_AudioDeviceID device;
    uint32_t submitted;
    uint8_t sample_align;
} sgl_avi_sdl_audio_t;

static sgl_avi_sdl_audio_t g_avi_sdl_audio;

static int sgl_avi_sdl_audio_start(void *user_data, uint32_t sample_rate,
                                   uint8_t channels, uint8_t bits)
{
    SDL_AudioSpec requested;
    sgl_avi_sdl_audio_t *audio = &g_avi_sdl_audio;

    SGL_UNUSED(user_data);
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0 &&
        SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SGL_LOG_ERROR("avi example: SDL audio init failed: %s", SDL_GetError());
        return -1;
    }
    if (audio->device != 0) {
        SDL_CloseAudioDevice(audio->device);
        audio->device = 0;
    }
    audio->submitted = 0;

    SDL_memset(&requested, 0, sizeof(requested));
    requested.freq = (int)sample_rate;
    requested.channels = channels;
    requested.format = bits <= 8 ? AUDIO_U8 : AUDIO_S16SYS;
    requested.samples = 1024;
    audio->sample_align = (uint8_t)(channels * (bits <= 8 ? 1 : 2));
    audio->device = SDL_OpenAudioDevice(NULL, 0, &requested, NULL, 0);
    if (audio->device == 0) {
        SGL_LOG_ERROR("avi example: SDL audio open failed: %s", SDL_GetError());
        return -1;
    }
    SDL_PauseAudioDevice(audio->device, 0);
    return 0;
}

static void sgl_avi_sdl_audio_stop(void *user_data)
{
    sgl_avi_sdl_audio_t *audio = &g_avi_sdl_audio;

    SGL_UNUSED(user_data);
    if (audio->device != 0) {
        SDL_CloseAudioDevice(audio->device);
        audio->device = 0;
    }
    audio->submitted = 0;
}

static int32_t sgl_avi_sdl_audio_write(void *user_data, const uint8_t *data, uint32_t size)
{
    sgl_avi_sdl_audio_t *audio = &g_avi_sdl_audio;
    uint32_t queued;
    uint32_t free_space;

    SGL_UNUSED(user_data);
    if (audio->device == 0 || data == NULL || size == 0) return 0;
    queued = SDL_GetQueuedAudioSize(audio->device);
    free_space = queued >= AVI_SDL_AUDIO_QUEUE_LIMIT ? 0 : AVI_SDL_AUDIO_QUEUE_LIMIT - queued;
    if (size > free_space) size = free_space;
    if (audio->sample_align > 0) size -= size % audio->sample_align;
    if (size == 0) return 0;
    if (SDL_QueueAudio(audio->device, data, size) != 0) return 0;
    audio->submitted += size;
    return (int32_t)size;
}

static uint32_t sgl_avi_sdl_audio_consumed(void *user_data)
{
    sgl_avi_sdl_audio_t *audio = &g_avi_sdl_audio;
    uint32_t queued;

    SGL_UNUSED(user_data);
    if (audio->device == 0) return audio->submitted;
    queued = SDL_GetQueuedAudioSize(audio->device);
    return audio->submitted - queued;
}

static void sgl_avi_sdl_audio_flush(void *user_data)
{
    sgl_avi_sdl_audio_t *audio = &g_avi_sdl_audio;

    SGL_UNUSED(user_data);
    if (audio->device != 0) SDL_ClearQueuedAudio(audio->device);
    audio->submitted = 0;
}

static const sgl_avi_audio_port_t g_avi_sdl_audio_port = {
    NULL,
    sgl_avi_sdl_audio_start,
    sgl_avi_sdl_audio_stop,
    sgl_avi_sdl_audio_write,
    sgl_avi_sdl_audio_consumed,
    sgl_avi_sdl_audio_flush,
};

typedef struct {
    sgl_obj_t *avi;
    sgl_obj_t *slider;
    sgl_obj_t *time_label;
    sgl_timer_t *timer;
    char time_text[48];
    uint8_t dragging;
} sgl_avi_progress_t;

static sgl_avi_progress_t g_avi_progress;

static void sgl_avi_example_status(sgl_obj_t *avi, const char *path)
{
    sgl_obj_t *label = sgl_label_create(avi);
    if (label == NULL) {
        return;
    }

    sgl_obj_set_pos(label, 12, 12);
    sgl_obj_set_size(label, 280, 18);
    sgl_label_set_text(label, path);
    sgl_label_set_font(label, &consolas14);
    sgl_label_set_text_color(label, sgl_rgb(40, 40, 40));
}

static void sgl_avi_format_time(char *buffer, size_t buffer_size, int32_t milliseconds)
{
    int32_t total_seconds = milliseconds / 1000;
    int32_t hours = total_seconds / 3600;
    int32_t minutes = (total_seconds / 60) % 60;
    int32_t seconds = total_seconds % 60;

    if (hours > 0) {
        snprintf(buffer, buffer_size, "%d:%02d:%02d", (int)hours, (int)minutes, (int)seconds);
    } else {
        snprintf(buffer, buffer_size, "%02d:%02d", (int)minutes, (int)seconds);
    }
}

static void sgl_avi_progress_refresh(sgl_avi_progress_t *progress)
{
    char current_text[16];
    char duration_text[16];
    char next_text[48];
    int32_t duration;
    int32_t position;

    if (progress == NULL || progress->avi == NULL || progress->slider == NULL ||
        progress->time_label == NULL) {
        return;
    }

    duration = sgl_avi_get_duration(progress->avi);
    if (duration <= 0) {
        return;
    }

    if (progress->dragging) {
        position = (int32_t)((int64_t)duration * sgl_slider_get_value(progress->slider) / 100);
    } else {
        position = sgl_avi_get_position(progress->avi);
        if (position < 0) position = 0;
        if (position > duration) position = duration;

        {
            uint8_t value = (uint8_t)((int64_t)position * 100 / duration);
            if (sgl_slider_get_value(progress->slider) != value) {
                sgl_slider_set_value(progress->slider, value);
            }
        }
    }

    sgl_avi_format_time(current_text, sizeof(current_text), position);
    sgl_avi_format_time(duration_text, sizeof(duration_text), duration);
    snprintf(next_text, sizeof(next_text), "%s / %s", current_text, duration_text);
    if (strcmp(progress->time_text, next_text) != 0) {
        snprintf(progress->time_text, sizeof(progress->time_text), "%s", next_text);
        sgl_label_set_text(progress->time_label, progress->time_text);
    }
}

static void sgl_avi_progress_timer_cb(const sgl_timer_t *timer, void *user_data)
{
    SGL_UNUSED(timer);
    sgl_avi_progress_refresh((sgl_avi_progress_t *)user_data);
}

static void sgl_avi_progress_event_cb(sgl_event_t *event)
{
    sgl_avi_progress_t *progress = (sgl_avi_progress_t *)event->event_data;

    if (progress == NULL) {
        return;
    }

    if (event->type == SGL_EVENT_PRESSED) {
        progress->dragging = 1;
        sgl_avi_progress_refresh(progress);
    } else if (event->type == SGL_EVENT_MOVE_DOWN || event->type == SGL_EVENT_MOVE_UP ||
               event->type == SGL_EVENT_MOVE_LEFT || event->type == SGL_EVENT_MOVE_RIGHT) {
        if (progress->dragging) {
            sgl_avi_progress_refresh(progress);
        }
    } else if (event->type == SGL_EVENT_RELEASED && progress->dragging) {
        sgl_avi_seek_percent(progress->avi, sgl_slider_get_value(progress->slider));
        progress->dragging = 0;
        sgl_avi_progress_refresh(progress);
    }
}

static void sgl_avi_progress_create(sgl_obj_t *avi)
{
    sgl_avi_progress_t *progress = &g_avi_progress;

    memset(progress, 0, sizeof(*progress));
    progress->avi = avi;
    progress->slider = sgl_slider_create(NULL);
    progress->time_label = sgl_label_create(NULL);
    if (progress->slider == NULL || progress->time_label == NULL) {
        SGL_LOG_ERROR("avi: progress controls create failed");
        return;
    }

    sgl_obj_set_pos(progress->slider, 20, 250);
    sgl_obj_set_size(progress->slider, 360, 22);
    sgl_slider_set_direct(progress->slider, SGL_DIRECT_HORIZONTAL);
    sgl_slider_set_fill_color(progress->slider, SGL_COLOR_BLUE);
    sgl_slider_set_track_color(progress->slider, SGL_COLOR_GRAY);
    sgl_slider_set_knob_color(progress->slider, SGL_COLOR_WHITE);
    sgl_slider_set_radius(progress->slider, 8);
    sgl_slider_set_value(progress->slider, 0);
    sgl_obj_set_event_cb(progress->slider, sgl_avi_progress_event_cb, progress);

    sgl_obj_set_pos(progress->time_label, 20, 278);
    sgl_obj_set_size(progress->time_label, 180, 20);
    sgl_label_set_font(progress->time_label, &consolas14);
    sgl_label_set_text_color(progress->time_label, sgl_rgb(40, 40, 40));

    progress->timer = sgl_timer_create();
    if (progress->timer != NULL) {
        sgl_timer_setup(progress->timer, sgl_avi_progress_timer_cb, 100, -1, progress);
    }
    sgl_avi_progress_refresh(progress);
}

void sgl_avi_examples(sgl_obj_t *parent)
{
    static const char *paths[] = {
        "/movie.avi",
        NULL,
    };
    sgl_winfs_config_t winfs_cfg = { SGL_AVI_DEMO_BASE_DIR };
    sgl_obj_t *avi = NULL;
    const char *selected = NULL;
    sgl_stat_t st;
    int i;

    if (sgl_avi_set_audio_port(&g_avi_sdl_audio_port) != 0) {
        SGL_LOG_ERROR("avi example: audio port registration failed");
    }
    if (sgl_winfs_register() != 0) {
        SGL_LOG_WARN("avi: winfs already registered");
    }
    if (sgl_fs_mount("/", "winfs", NULL, &winfs_cfg) != 0) {
        SGL_LOG_WARN("avi: mount / with winfs failed");
    }

    avi = sgl_avi_create(parent);
    if (avi == NULL) {
        SGL_LOG_ERROR("avi: create failed");
        return;
    }

    sgl_obj_set_pos(avi, 20, 20);
    sgl_obj_set_size(avi, 320, 240);

    for (i = 0; paths[i] != NULL; ++i) {
        if (sgl_fs_stat(paths[i], &st) == 0) {
            selected = paths[i];
            break;
        }
    }

    if (selected == NULL) {
        sgl_avi_example_status(avi, "No local .avi file found");
        SGL_LOG_INFO("avi: no .avi file found in the local working tree; set a path via sgl_avi_load_file()");
        return;
    }

    if (sgl_avi_load_file(avi, selected) == 0) {
        sgl_avi_set_decode_scale(avi, 0);
        sgl_avi_progress_create(avi);
        sgl_avi_play(avi);
        sgl_avi_example_status(avi, selected);
        SGL_LOG_INFO("avi: playing %s", selected);
    } else {
        sgl_avi_example_status(avi, "AVI load failed");
        SGL_LOG_ERROR("avi: failed to open %s", selected);
    }
}
