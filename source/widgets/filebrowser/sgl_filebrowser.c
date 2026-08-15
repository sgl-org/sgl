/* source/widgets/sgl_filebrowser.c
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
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <sgl_fs.h>
#include <stdio.h>
#include <string.h>
#include "sgl_filebrowser.h"

#define SGL_FILEBROWSER_ITEM_PAD         (3)
#define SGL_FILEBROWSER_ITEM_SPACE       (3)
#define SGL_FILEBROWSER_PARENT_NAME      ".."

static inline bool sgl_filebrowser_is_root(const sgl_filebrowser_t *fb)
{
    return fb->current_path[0] == '/' && fb->current_path[1] == '\0';
}

static inline bool sgl_filebrowser_is_valid_dir_entry(const char *name)
{
    if (name[0] == '\0') return false;
    if (name[0] == '.' && name[1] == '\0') return false;
    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') return false;
    return true;
}

static int sgl_filebrowser_strcasecmp(const char *a, const char *b)
{
    for (;;) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb + 32);
        if (ca != cb) return (int)ca - (int)cb;
        if (ca == 0)  return 0;
    }
}

static void sgl_filebrowser_trim_trailing_slash(char *path)
{
    size_t len = strlen(path);
    while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        path[--len] = '\0';
    }
}

static void sgl_filebrowser_join_path(char *dst, size_t dst_size,
                                      const char *base, const char *name)
{
    if (dst == NULL || dst_size == 0 || base == NULL || name == NULL) return;
    bool is_root = (base[0] == '/' && base[1] == '\0');
    size_t base_len = is_root ? 0 : strlen(base);
    size_t name_len = strlen(name);
    if (base_len + 1 + name_len + 1 > dst_size) { dst[0] = '\0'; return; }

    char *out = dst;
    if (base_len > 0) { memcpy(out, base, base_len); out += base_len; }
    *out++ = '/';
    memcpy(out, name, name_len);
    out[name_len] = '\0';
}

static void sgl_filebrowser_update_full_path(sgl_filebrowser_t *fb)
{
    const char *prefix = fb->path_prefix ? fb->path_prefix : "";
    size_t p_len = strlen(prefix);
    size_t c_len = strlen(fb->current_path);

    if (p_len + c_len + 1 > sizeof(fb->full_path)) {
        if (c_len + 1 <= sizeof(fb->full_path)) {
            memcpy(fb->full_path, fb->current_path, c_len + 1);
        } else {
            fb->full_path[0] = '\0';
        }
        return;
    }
    memcpy(fb->full_path, prefix, p_len);
    memcpy(fb->full_path + p_len, fb->current_path, c_len + 1);
}

static const char *sgl_filebrowser_match_icon(const sgl_filebrowser_t *fb, const sgl_filebrowser_item_t *item)
{
    if (fb->icons == NULL || item == NULL) return NULL;

    const char *default_icon = NULL;
    const char *dir_icon     = NULL;
    const char *parent_icon  = NULL;
    size_t item_len = strlen(item->text);

    for (const sgl_filebrowser_icon_t *e = fb->icons; e->pattern != NULL; ++e) {
        const char *p  = e->pattern;
        const char *ic = e->icon;
        if (p == NULL || ic == NULL) continue;

        if (p[0] == '*' && p[1] == '\0') { default_icon = ic; continue; }
        if (p[0] == '.' && p[1] == '.' && p[2] == '\0') { parent_icon = ic; continue; }
        if (p[0] == 'd' && p[1] == 'i' && p[2] == 'r' && p[3] == '\0') { dir_icon = ic; continue; }

        size_t plen = strlen(p);
        if (plen == 0 || plen > item_len) continue;
        if (sgl_filebrowser_strcasecmp(item->text + item_len - plen, p) == 0) {
            return ic;
        }
    }
    if (item->text[0] == '.' && item->text[1] == '.' && item->text[2] == '\0' && parent_icon)
        return parent_icon;
    if (item->type == SGL_S_IFDIR && dir_icon != NULL)
        return dir_icon;
    return default_icon;
}

static void sgl_filebrowser_resolve_item_icon(sgl_filebrowser_t *fb,
                                              sgl_filebrowser_item_t *item)
{
    item->icon   = sgl_filebrowser_match_icon(fb, item);
    item->icon_w = (item->icon != NULL)
                   ? (int16_t)sgl_font_get_string_width(item->icon, fb->font)
                   : (int16_t)0;
}

static void sgl_filebrowser_close_dir(sgl_filebrowser_t *fb)
{
    if (fb->dir_handle >= 0) {
        sgl_fs_closedir(fb->dir_handle);
        fb->dir_handle = -1;
    }
    fb->dir_cursor = 0;
}

/* Ensure the readdir cursor points to the entry whose logical index (in the
 * "content" sequence, i.e. excluding "..") is `target`.
 * Returns true on success (handle is open, cursor == target).
 * On failure the handle is closed and cursor is 0. */
static bool sgl_filebrowser_seek_dir(sgl_filebrowser_t *fb, int16_t target)
{
    if (target < 0) target = 0;

    /* If we need to go backwards, we must reopen. Some FS provide seekdir but
     * SGL's abstraction here does not, so rewind = closedir + opendir. */
    if (fb->dir_handle < 0 || fb->dir_cursor > target) {
        sgl_filebrowser_close_dir(fb);
        if (sgl_fs_opendir(fb->current_path, &fb->dir_handle) != SGL_FS_OK) {
            fb->dir_handle = -1;
            fb->dir_cursor = 0;
            return false;
        }
        fb->dir_cursor = 0;
    }

    /* Fast-forward. */
    char name[SGL_FILEBROWSER_NAME_MAX_LEN];
    uint32_t type;
    while (fb->dir_cursor < target) {
        int r = sgl_fs_readdir(fb->dir_handle, name, sizeof(name), &type);
        if (r <= 0) {
            /* Directory shorter than expected: bail. */
            sgl_filebrowser_close_dir(fb);
            return false;
        }
        if (sgl_filebrowser_is_valid_dir_entry(name)) {
            fb->dir_cursor++;
        }
    }
    return true;
}

/* Read the next valid entry from the persistent handle.
 * Returns 1 on success, 0 on EOF / error. */
static int sgl_filebrowser_read_next(sgl_filebrowser_t *fb,
                                     char *name, size_t name_size, uint32_t *type)
{
    if (fb->dir_handle < 0) return 0;
    for (;;) {
        int r = sgl_fs_readdir(fb->dir_handle, name, name_size, type);
        if (r <= 0) return 0;
        if (sgl_filebrowser_is_valid_dir_entry(name)) {
            fb->dir_cursor++;
            return 1;
        }
    }
}

static void sgl_filebrowser_init_item(sgl_filebrowser_item_t *item, const char *name, uint32_t type)
{
    if (item == NULL || name == NULL) return;
    sgl_snprintf(item->text, sizeof(item->text), "%s", name);
    item->type   = type;
    item->icon   = NULL;
    item->icon_w = 0;
}

static void sgl_filebrowser_release_cache(sgl_filebrowser_t *fb)
{
    if (fb->cache_items != NULL) {
        sgl_free(fb->cache_items);
        fb->cache_items = NULL;
    }
    fb->item_capacity     = 0;
    fb->cache_start_index = -1;
    fb->cache_count       = 0;
}

static bool sgl_filebrowser_ensure_cache(sgl_filebrowser_t *fb, uint16_t capacity)
{
    if (capacity < SGL_FILEBROWSER_CACHE_MIN) {
        capacity = SGL_FILEBROWSER_CACHE_MIN;
    }
    if (fb->cache_items != NULL && fb->item_capacity == capacity) {
        return true;
    }

    sgl_filebrowser_release_cache(fb);
    fb->cache_items = sgl_malloc(sizeof(sgl_filebrowser_item_t) * capacity);
    if (fb->cache_items == NULL) {
        SGL_LOG_ERROR("sgl_filebrowser: cache alloc failed (%u items)", (unsigned)capacity);
        return false;
    }
    fb->item_capacity     = capacity;
    fb->cache_start_index = -1;
    fb->cache_count       = 0;
    return true;
}

static inline bool sgl_filebrowser_cache_covers(const sgl_filebrowser_t *fb,
                                                int16_t first, int16_t last /* inclusive */)
{
    if (fb->cache_items == NULL || fb->cache_count == 0 || fb->cache_start_index < 0)
        return false;
    return first >= fb->cache_start_index &&
           last  <  fb->cache_start_index + (int16_t)fb->cache_count;
}

static void sgl_filebrowser_fill_window(sgl_filebrowser_t *fb, int16_t start_index)
{
    if (fb->cache_items == NULL || fb->item_capacity == 0) return;
    if (fb->item_num == 0) {
        fb->cache_start_index = 0;
        fb->cache_count       = 0;
        return;
    }

    if (start_index < 0) start_index = 0;
    if (start_index > (int16_t)fb->item_num - 1) start_index = (int16_t)fb->item_num - 1;

    /* Already correct? */
    uint16_t want = fb->item_capacity;
    if ((int)want > (int)fb->item_num - start_index) {
        want = (uint16_t)(fb->item_num - start_index);
    }
    if (fb->cache_start_index == start_index && fb->cache_count == want) {
        return;
    }

    fb->cache_start_index = start_index;
    fb->cache_count       = 0;

    const bool has_parent = !sgl_filebrowser_is_root(fb);
    uint16_t written = 0;

    /* 1. Emit ".." if it falls inside the window. */
    if (has_parent && start_index == 0 && written < want) {
        sgl_filebrowser_init_item(&fb->cache_items[written],
                                  SGL_FILEBROWSER_PARENT_NAME, SGL_S_IFDIR);
        sgl_filebrowser_resolve_item_icon(fb, &fb->cache_items[written]);
        written++;
    }

    int16_t content_start = start_index - (has_parent ? 1 : 0);
    if (content_start < 0) content_start = 0;

    if (!sgl_filebrowser_seek_dir(fb, content_start)) {
        fb->cache_count = written;
        return;
    }

    char name[SGL_FILEBROWSER_NAME_MAX_LEN];
    uint32_t type = 0;
    while (written < want &&
           sgl_filebrowser_read_next(fb, name, sizeof(name), &type)) {
        sgl_filebrowser_init_item(&fb->cache_items[written], name, type);
        sgl_filebrowser_resolve_item_icon(fb, &fb->cache_items[written]);
        written++;
    }
    fb->cache_count = written;
}

static void sgl_filebrowser_ensure_visible(sgl_filebrowser_t *fb, int16_t first, int16_t last)
{
    if (fb->cache_items == NULL || fb->item_capacity == 0 || fb->item_num == 0)
        return;
    if (first < 0) first = 0;
    if (last  > (int16_t)fb->item_num - 1) last = (int16_t)fb->item_num - 1;
    if (first > last) first = last;

    if (sgl_filebrowser_cache_covers(fb, first, last)) return;

    const int16_t cap    = (int16_t)fb->item_capacity;
    const int16_t total  = (int16_t)fb->item_num;
    int16_t new_start;

    if (fb->cache_start_index < 0) {
        new_start = first;
    } else if (first >= fb->cache_start_index + fb->cache_count) {
        new_start = first;
    } else if (last < fb->cache_start_index) {
        new_start = last - cap + 1;
    } else {
        int16_t range_mid = (int16_t)((first + last) / 2);
        new_start = (int16_t)(range_mid - cap / 2);
    }

    if (new_start < 0)              new_start = 0;
    if (new_start > total - cap)    new_start = (int16_t)(total - cap);
    if (new_start < 0)              new_start = 0;

    sgl_filebrowser_fill_window(fb, new_start);
}

static sgl_filebrowser_item_t *sgl_filebrowser_get_item(sgl_filebrowser_t *fb, int16_t index)
{
    if (index < 0 || index >= (int16_t)fb->item_num) return NULL;
    if (!sgl_filebrowser_cache_covers(fb, index, index)) {
        sgl_filebrowser_ensure_visible(fb, index, index);
    }
    if (!sgl_filebrowser_cache_covers(fb, index, index)) return NULL;
    return &fb->cache_items[index - fb->cache_start_index];
}

static void sgl_filebrowser_clear_items(sgl_filebrowser_t *fb)
{
    sgl_filebrowser_close_dir(fb);
    sgl_filebrowser_release_cache(fb);

    fb->selected              = &fb->selected_item;
    fb->selected_item.text[0] = '\0';
    fb->selected_item.type    = 0;
    fb->selected_item.icon    = NULL;
    fb->selected_item.icon_w  = 0;
    fb->item_num              = 0;
    fb->item_selected         = -1;
    sgl_scroll_anim_stop(&fb->sc);
    sgl_scroll_reset(&fb->sc);
    fb->pending_dir_index     = -1;
}

static void sgl_filebrowser_select_index(sgl_filebrowser_t *fb, int16_t index)
{
    if (index < 0 || index >= (int16_t)fb->item_num) {
        fb->selected              = &fb->selected_item;
        fb->selected_item.text[0] = '\0';
        fb->selected_item.type    = 0;
        fb->selected_item.icon    = NULL;
        fb->selected_item.icon_w  = 0;
        fb->item_selected         = -1;
        return;
    }
    if (index == fb->item_selected) return;

    sgl_filebrowser_item_t *it = sgl_filebrowser_get_item(fb, index);
    if (it == NULL) {
        fb->selected              = &fb->selected_item;
        fb->selected_item.text[0] = '\0';
        fb->selected_item.type    = 0;
        fb->selected_item.icon    = NULL;
        fb->selected_item.icon_w  = 0;
        fb->item_selected         = -1;
        return;
    }
    fb->selected_item = *it;
    fb->selected      = &fb->selected_item;
    fb->item_selected = index;
}

static void sgl_filebrowser_load_dir(sgl_filebrowser_t *fb, const char *path)
{
    sgl_filebrowser_clear_items(fb);

    if (path == NULL || path[0] == '\0') {
        strncpy(fb->current_path, "/", sizeof(fb->current_path) - 1);
    } else {
        strncpy(fb->current_path, path, sizeof(fb->current_path) - 1);
    }
    fb->current_path[sizeof(fb->current_path) - 1] = '\0';
    sgl_filebrowser_trim_trailing_slash(fb->current_path);
    sgl_filebrowser_update_full_path(fb);

    /* Count entries once. Cache stays empty; will be filled lazily on draw. */
    uint16_t count = sgl_filebrowser_is_root(fb) ? 0 : 1; /* ".." */
    int dd = -1;
    if (sgl_fs_opendir(fb->current_path, &dd) == SGL_FS_OK) {
        char name[SGL_FILEBROWSER_NAME_MAX_LEN];
        uint32_t type;
        while (sgl_fs_readdir(dd, name, sizeof(name), &type) > 0) {
            if (sgl_filebrowser_is_valid_dir_entry(name)) count++;
        }
        sgl_fs_closedir(dd);
    }
    fb->item_num = count;

    if (count > 0) {
        sgl_filebrowser_select_index(fb, 0);
    }
}

static int32_t sgl_filebrowser_max_scroll(const sgl_filebrowser_t *fb, const sgl_obj_t *obj, int item_height)
{
    const int list_y1   = obj->coords.y1 + item_height; /* list starts below the path bar */
    const int list_h    = obj->coords.y2 - list_y1 + 1;
    const int content_h = (int)fb->item_num * item_height;
    return sgl_max(0, content_h - list_h);
}

static void sgl_filebrowser_scroll_commit(sgl_scroll_t *sc)
{
    sgl_filebrowser_t *fb = sgl_container_of(sc, sgl_filebrowser_t, sc);
    sgl_scroll_bar_wake(sc);
    sgl_obj_set_dirty(&fb->obj);
}

/* Scroll the selected item into the list viewport (keyboard navigation) */
static void sgl_filebrowser_ensure_selected_visible(sgl_filebrowser_t *fb, sgl_obj_t *obj, int item_height)
{
    if (fb->item_selected < 0) return;
    const int list_h       = obj->coords.y2 - (obj->coords.y1 + item_height) + 1;
    const int selected_y   = fb->item_selected * item_height;
    const int view_top     = (int)fb->sc.offset;
    const int view_bottom  = view_top + list_h;
    const int32_t max_scroll = sgl_filebrowser_max_scroll(fb, obj, item_height);

    if (selected_y < view_top) {
        fb->sc.offset = selected_y;
    }
    else if (selected_y + item_height > view_bottom) {
        fb->sc.offset = selected_y + item_height - list_h;
    }

    if (fb->sc.offset < 0)
        fb->sc.offset = 0;
    if (fb->sc.offset > max_scroll)
        fb->sc.offset = max_scroll;
}

static void sgl_filebrowser_draw_path_bar(sgl_surf_t *surf, sgl_filebrowser_t *fb, sgl_obj_t *obj, int item_height)
{
    const int16_t path_y1 = obj->coords.y1 + obj->border;
    const int16_t path_y2 = path_y1 + item_height - 1;

    sgl_area_t path_area = {
        .x1 = obj->area.x1 + obj->border,   .y1 = path_y1,
        .x2 = obj->area.x2 - obj->border,   .y2 = path_y2,
    };
    sgl_area_t path_rect = {
        .x1 = obj->coords.x1 + obj->border, .y1 = path_y1,
        .x2 = obj->coords.x2 - obj->border, .y2 = path_y2 + obj->radius + obj->border,
    };
    sgl_draw_fill_rect(surf, &path_area, &path_rect, obj->radius, fb->path_color, fb->alpha);
    sgl_draw_string(surf, &obj->area, path_rect.x1 + obj->radius + SGL_FILEBROWSER_ITEM_PAD, path_rect.y1 + SGL_FILEBROWSER_ITEM_PAD,
                          fb->full_path, fb->item_text_color, fb->alpha, fb->font);
}

static void sgl_filebrowser_update_selection_area(sgl_obj_t *obj, int16_t item_y, int16_t item_height)
{
    sgl_area_t select = {
        .x1 = obj->coords.x1 + obj->border,
        .y1 = item_y - SGL_FILEBROWSER_ITEM_SPACE,
        .x2 = obj->coords.x2 - obj->border,
        .y2 = item_y - SGL_FILEBROWSER_ITEM_SPACE + item_height,
    };
    sgl_obj_update_area(&select);
}

static void sgl_filebrowser_visible_range(const sgl_filebrowser_t *fb, sgl_obj_t *obj, int item_height,
                                          int16_t *out_first, int16_t *out_last)
{
    const int list_y1 = obj->coords.y1 + item_height;
    const int list_h  = obj->coords.y2 - list_y1 + 1;

    int first = fb->sc.offset > 0 ? (int)(fb->sc.offset / item_height) : 0;
    if (first < 0) first = 0;
    if (first >= (int)fb->item_num) first = (int)fb->item_num - 1;

    int visible_rows = (list_h + item_height - 1) / item_height + 1;
    int last = first + visible_rows - 1;
    if (last >= (int)fb->item_num) last = (int)fb->item_num - 1;

    *out_first = (int16_t)first;
    *out_last  = (int16_t)last;
}

static uint16_t sgl_filebrowser_target_capacity(const sgl_obj_t *obj, int item_height)
{
    const int list_y1 = obj->coords.y1 + item_height;
    const int list_h  = obj->coords.y2 - list_y1 + 1;
    int visible_rows = (list_h + item_height - 1) / item_height + 1;
    if (visible_rows < 2) visible_rows = 2;
    int cap = visible_rows * SGL_FILEBROWSER_CACHE_MULT;
    if (cap < SGL_FILEBROWSER_CACHE_MIN) cap = SGL_FILEBROWSER_CACHE_MIN;
    if (cap > 0xFFFF)                    cap = 0xFFFF;
    return (uint16_t)cap;
}

static void sgl_filebrowser_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);

    const int item_height = sgl_font_get_height(fb->font) + 2 * SGL_FILEBROWSER_ITEM_SPACE;
    const int item_pad    = sgl_max(obj->radius, obj->border + SGL_FILEBROWSER_ITEM_PAD);
    const int16_t icon_col_w = fb->icons != NULL
                               ? (fb->icon_width == 0 ? (int16_t)item_height : (int16_t)fb->icon_width)
                               : (int16_t)0;
    const int16_t text_x  = obj->coords.x1 + item_pad + icon_col_w;
    const int list_y1     = obj->coords.y1 + item_height;

    sgl_area_t list_area = {
        .x1 = obj->area.x1,
        .y1 = list_y1 + 1,
        .x2 = obj->area.x2,
        .y2 = obj->area.y2 - obj->border,
    };

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN: {
        sgl_draw_rect_t bg_desc = {
            .alpha        = fb->alpha,
            .color        = fb->bg_color,
            .border       = obj->border,
            .border_alpha = fb->alpha,
            .border_color = fb->border_color,
            .radius       = obj->radius,
            .pixmap       = fb->pixmap,
        };

        sgl_draw_rect(surf, &obj->area, &obj->coords, &bg_desc);
        sgl_filebrowser_draw_path_bar(surf, fb, obj, item_height);

        /* Scrollbar viewport = list area below the path bar */
        const int32_t max_scroll = sgl_filebrowser_max_scroll(fb, obj, item_height);
        sgl_area_t viewport = {
            .x1 = obj->coords.x1,
            .y1 = (int16_t)list_y1,
            .x2 = obj->coords.x2,
            .y2 = obj->coords.y2,
        };

        if (fb->item_num == 0) {
            sgl_scroll_draw_bar(surf, obj, &fb->sc, max_scroll, &viewport);
            break;
        }

        /* --- Ensure the sliding window covers what we're about to draw. --- */
        uint16_t target_cap = sgl_filebrowser_target_capacity(obj, item_height);
        sgl_filebrowser_ensure_cache(fb, target_cap);

        int16_t first, last;
        sgl_filebrowser_visible_range(fb, obj, item_height, &first, &last);
        sgl_filebrowser_ensure_visible(fb, first, last);

        if (fb->cache_items == NULL || fb->cache_count == 0) {
            sgl_scroll_draw_bar(surf, obj, &fb->sc, max_scroll, &viewport);
            break;
        }

        const int16_t list_y2 = obj->coords.y2 - obj->border;

        /* Iterate over the intersection of [first,last] and the current window. */
        int16_t draw_first = first;
        int16_t draw_last  = last;
        if (draw_first < fb->cache_start_index) draw_first = fb->cache_start_index;
        if (draw_last  > fb->cache_start_index + (int16_t)fb->cache_count - 1)
            draw_last  = fb->cache_start_index + (int16_t)fb->cache_count - 1;

        for (int idx = draw_first; idx <= draw_last; ++idx) {
            const int16_t text_y = list_y1 + SGL_FILEBROWSER_ITEM_SPACE
                                 - (int16_t)fb->sc.offset + idx * item_height;
            if (text_y > list_y2) break;

            sgl_filebrowser_item_t *item = &fb->cache_items[idx - fb->cache_start_index];

            if (idx == fb->item_selected) {
                sgl_area_t select = {
                    .x1 = obj->coords.x1 + obj->border,
                    .y1 = text_y - SGL_FILEBROWSER_ITEM_SPACE,
                    .x2 = obj->coords.x2 - obj->border,
                    .y2 = text_y - SGL_FILEBROWSER_ITEM_SPACE + item_height,
                };
                if (select.y2 >= (list_y2 - obj->radius)) {
                    sgl_area_t select_coords = select;
                    select_coords.y1 = select.y1 - item_height - obj->radius - 1;
                    select_coords.y2 = list_y2;
                    sgl_draw_fill_rect(surf, &select, &select_coords, obj->radius,
                                       fb->item_selected_color, fb->alpha);
                } else {
                    sgl_draw_fill_rect(surf, &list_area, &select, 0,
                                       fb->item_selected_color, fb->alpha);
                }
            }

            if (icon_col_w > 0 && item->icon != NULL) {
                const int16_t icon_x = obj->coords.x1 + item_pad
                                     + (icon_col_w - item->icon_w) / 2;
                sgl_draw_string(surf, &list_area, icon_x, text_y,
                                item->icon, fb->icon_color, fb->alpha, fb->font);
            }
            sgl_draw_string(surf, &list_area, text_x, text_y,
                            item->text, fb->item_text_color, fb->alpha, fb->font);
        }

        sgl_scroll_draw_bar(surf, obj, &fb->sc, max_scroll, &viewport);
    }
    break;

    case SGL_EVENT_PRESSED:
        sgl_scroll_press(&fb->sc, evt->pos.y);
        sgl_scroll_bar_wake(&fb->sc);
        if (evt->pos.x >= (obj->coords.x2 - item_height) && evt->pos.y <= (obj->coords.y1 + item_height)) {
            sgl_obj_set_destroyed(obj);
        }
        break;

    case SGL_EVENT_MOVE_UP:
    case SGL_EVENT_MOVE_DOWN: {
        const int32_t max_scroll = sgl_filebrowser_max_scroll(fb, obj, item_height);
        if (sgl_scroll_stay(&fb->sc, evt->pos.y, max_scroll)) {
            sgl_obj_set_dirty(obj);
        }
    }
    break;

    case SGL_EVENT_RELEASED: {
        const int32_t max_scroll = sgl_filebrowser_max_scroll(fb, obj, item_height);
        fb->sc.range  = max_scroll;
        fb->sc.commit = sgl_filebrowser_scroll_commit;
        if (sgl_scroll_release(&fb->sc, max_scroll)) {
            sgl_scroll_anim_start(&fb->sc);
        }
        sgl_obj_set_dirty(obj);
    }
    break;

    case SGL_EVENT_CLICKED: {
        const int16_t item_area_top = list_y1 + SGL_FILEBROWSER_ITEM_SPACE - (int16_t)fb->sc.offset;
        if (evt->pos.y < item_area_top) break;

        int16_t clicked_index = (int16_t)((evt->pos.y - item_area_top) / item_height);
        if (clicked_index < 0 || clicked_index >= (int16_t)fb->item_num) break;

        const int16_t old_selected = fb->item_selected;
        sgl_filebrowser_select_index(fb, clicked_index);

        if (fb->selected != NULL &&
            strcmp(fb->selected->text, SGL_FILEBROWSER_PARENT_NAME) == 0) {
            fb->pending_dir_index = -1;
            char parent_path[SGL_FILEBROWSER_PATH_MAX_LEN];
            strncpy(parent_path, fb->current_path, sizeof(parent_path) - 1);
            parent_path[sizeof(parent_path) - 1] = '\0';
            sgl_filebrowser_trim_trailing_slash(parent_path);

            char *slash = strrchr(parent_path, '/');
            if (slash == NULL)              strcpy(parent_path, "/");
            else if (slash == parent_path)  parent_path[1] = '\0';
            else                            *slash = '\0';

            sgl_filebrowser_load_dir(fb, parent_path);
            sgl_obj_set_dirty(obj);
        }
        else if (fb->selected != NULL && fb->selected->type == SGL_S_IFDIR) {
            if (fb->pending_dir_index == clicked_index) {
                char next_path[SGL_FILEBROWSER_PATH_MAX_LEN];
                sgl_filebrowser_join_path(next_path, sizeof(next_path),
                                          fb->current_path, fb->selected->text);
                sgl_filebrowser_load_dir(fb, next_path);
                sgl_obj_set_dirty(obj);
            } else {
                fb->pending_dir_index = clicked_index;
                sgl_obj_set_dirty(obj);
            }
        }
        else {
            fb->pending_dir_index = -1;
            const int16_t item_y = list_y1 + SGL_FILEBROWSER_ITEM_SPACE
                                 - (int16_t)fb->sc.offset + clicked_index * item_height;
            if (old_selected >= 0) {
                const int16_t old_item_y = list_y1 + SGL_FILEBROWSER_ITEM_SPACE
                                         - (int16_t)fb->sc.offset + old_selected * item_height;
                sgl_filebrowser_update_selection_area(obj, old_item_y, item_height);
            }
            sgl_filebrowser_update_selection_area(obj, item_y, item_height);
        }
    }
    break;

    case SGL_EVENT_KEY_DOWN:
        if (fb->item_selected < (int16_t)fb->item_num - 1) {
            sgl_filebrowser_select_index(fb, fb->item_selected + 1);
            sgl_scroll_anim_stop(&fb->sc);
            sgl_filebrowser_ensure_selected_visible(fb, obj, item_height);
            sgl_scroll_bar_wake(&fb->sc);
            sgl_obj_set_dirty(obj);
        }
        break;

    case SGL_EVENT_KEY_UP:
        if (fb->item_selected > 0) {
            sgl_filebrowser_select_index(fb, fb->item_selected - 1);
            sgl_scroll_anim_stop(&fb->sc);
            sgl_filebrowser_ensure_selected_visible(fb, obj, item_height);
            sgl_scroll_bar_wake(&fb->sc);
            sgl_obj_set_dirty(obj);
        }
        break;

    case SGL_EVENT_DESTROYED:
        sgl_scroll_anim_stop(&fb->sc);
        sgl_filebrowser_clear_items(fb);
        break;

    default:
        break;
    }
}

/**
 * @brief Create a file browser object.
 * @param parent: parent object
 * @return file browser object
 */
sgl_obj_t* sgl_filebrowser_create(sgl_obj_t *parent)
{
    sgl_filebrowser_t *fb = sgl_malloc(sizeof(sgl_filebrowser_t));
    if (fb == NULL) {
        SGL_LOG_ERROR("sgl_filebrowser_create: malloc failed");
        return NULL;
    }
    memset(fb, 0, sizeof(*fb));

    sgl_obj_init(&fb->obj, parent);
    fb->obj.construct_fn = sgl_filebrowser_construct_cb;
    sgl_obj_set_clickable(&fb->obj);
    sgl_obj_set_movable(&fb->obj);
    sgl_obj_set_editable(&fb->obj);
    sgl_obj_set_border_width(&fb->obj, 1);

    fb->selected             = &fb->selected_item;
    fb->font                 = sgl_get_system_font();
    fb->alpha                = SGL_THEME_ALPHA;
    fb->bg_color             = SGL_THEME_COLOR;
    fb->item_selected_color  = sgl_color_mixer(SGL_THEME_COLOR, SGL_THEME_BORDER_COLOR, 128);
    fb->border_color         = SGL_THEME_BORDER_COLOR;
    fb->item_text_color      = SGL_THEME_TEXT_COLOR;
    fb->icon_color           = sgl_rgb(121, 177, 254);
    fb->path_color           = SGL_COLOR_WHEAT;
    fb->path_prefix          = "";
    fb->pending_dir_index    = -1;
    fb->item_selected        = -1;
    fb->cache_start_index    = -1;
    fb->dir_handle           = -1;
    sgl_scroll_reset(&fb->sc);
    strcpy(fb->current_path, "/");
    sgl_filebrowser_update_full_path(fb);
    sgl_filebrowser_load_dir(fb, "/");
    return &fb->obj;
}

/**
 * @brief Set the file browser object path.
 * @param obj: file browser object
 * @param path: path to set
 */
void sgl_filebrowser_set_dir(sgl_obj_t *obj, const char *path)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    sgl_filebrowser_load_dir(fb, path);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the text icons of the file browser object.
 * @param obj: file browser object
 * @param icons: icons to set
 * @return none
 */
void sgl_filebrowser_set_icons(sgl_obj_t *obj, sgl_filebrowser_icon_t *icons)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    fb->icons = icons;
    if (fb->cache_items != NULL) {
        for (uint16_t i = 0; i < fb->cache_count; ++i) {
            sgl_filebrowser_resolve_item_icon(fb, &fb->cache_items[i]);
        }
    }

    if (fb->selected_item.text[0] != '\0') {
        sgl_filebrowser_resolve_item_icon(fb, &fb->selected_item);
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the icon width of the file browser object.
 * @param obj: file browser object
 * @param width: icon width to set
 * @return none
 */
void sgl_filebrowser_set_icon_width(sgl_obj_t *obj, uint16_t width)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    fb->icon_width = (uint8_t)width;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Get the selected path of the file browser object.
 * @param obj: file browser object
 * @param buf: buffer to store the path
 * @param buf_size: size of the buffer
 * @return selected path
 */
int sgl_filebrowser_get_selected_path(sgl_obj_t *obj, char *buf, size_t buf_size)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    if (buf == NULL || buf_size == 0 || fb->selected == NULL) {
        return SGL_FS_INVALID_ARGUMENT;
    }
    if (strcmp(fb->selected->text, SGL_FILEBROWSER_PARENT_NAME) == 0) {
        return SGL_FS_INVALID_ARGUMENT;
    }
    if (fb->current_path[0] == '/' && fb->current_path[1] == '\0') {
        return sgl_snprintf(buf, buf_size, "/%s", fb->selected->text) >= 0
               ? SGL_FS_OK : SGL_FS_ERROR;
    }
    return sgl_snprintf(buf, buf_size, "%s/%s",
                        fb->current_path, fb->selected->text) >= 0
           ? SGL_FS_OK : SGL_FS_ERROR;
}

/**
 * @brief Get the selected name of the file browser object.
 * @param obj: file browser object
 * @return selected name
 */
const char* sgl_filebrowser_get_selected_name(sgl_obj_t *obj)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    return fb->selected != NULL ? fb->selected->text : NULL;
}

/**
 * @brief Get the selected type of the file browser object.
 * @param obj: file browser object
 * @return selected type
 */
uint32_t sgl_filebrowser_get_selected_type(sgl_obj_t *obj)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    return fb->selected != NULL ? fb->selected->type : 0;
}

/**
 * @brief Refresh the file browser object.
 * @param obj: file browser object
 * @return none
 */
void sgl_filebrowser_refresh(sgl_obj_t *obj)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    sgl_filebrowser_load_dir(fb, fb->current_path);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the radius of the file browser object.
 * @param obj: file browser object
 * @param radius: radius to set
 * @return none
 */
void sgl_filebrowser_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    if (obj->radius > 0) {
        sgl_obj_size_zoom(obj, radius - obj->radius);
    }
    obj->radius = radius;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the text color of the file browser object.
 * @param obj: file browser object
 * @param color: text color to set
 * @return none
 */
void sgl_filebrowser_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    fb->item_text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the icon color of the file browser object.
 * @param obj: file browser object
 * @param color: icon color to set
 * @return none
 */
void sgl_filebrowser_set_icon_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    fb->icon_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the selected color of the file browser object.
 * @param obj: file browser object
 * @param color: selected color to set
 * @return none
 */
void sgl_filebrowser_set_selected_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    fb->item_selected_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the border color of the file browser object.
 * @param obj: file browser object
 * @param color: border color to set
 * @return none
 */
void sgl_filebrowser_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    fb->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the background color of the file browser object.
 * @param obj: file browser object
 * @param color: background color to set
 * @return none
 */
void sgl_filebrowser_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    fb->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the path color of the file browser object.
 * @param obj: file browser object
 * @param color: path color to set
 * @return none
 */
void sgl_filebrowser_set_path_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    fb->path_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the text font of the file browser object.
 * @param obj: file browser object
 * @param font: font to set
 * @return none
 */
void sgl_filebrowser_set_text_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    fb->font = font;
    /* Icon widths depend on font. */
    if (fb->cache_items != NULL) {
        for (uint16_t i = 0; i < fb->cache_count; ++i) {
            sgl_filebrowser_item_t *it = &fb->cache_items[i];
            it->icon_w = (it->icon != NULL)
                         ? (int16_t)sgl_font_get_string_width(it->icon, fb->font)
                         : (int16_t)0;
        }
    }
    if (fb->selected_item.icon != NULL) {
        fb->selected_item.icon_w =
            (int16_t)sgl_font_get_string_width(fb->selected_item.icon, fb->font);
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the pixmap of the file browser object.
 * @param obj: file browser object
 * @param pixmap: pixmap to set
 * @return none
 */
void sgl_filebrowser_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    fb->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the alpha of the file browser object.
 * @param obj: file browser object
 * @param alpha: alpha to set
 * @return none
 */
void sgl_filebrowser_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    fb->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the border width of the file browser object.
 * @param obj: file browser object
 * @param width: border width to set
 * @return none
 */
void sgl_filebrowser_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief Set the path prefix of the file browser object.
 * @param obj: file browser object
 * @param prefix: path prefix to set
 * @return none
 */
void sgl_filebrowser_set_path_prefix(sgl_obj_t *obj, const char *prefix)
{
    sgl_filebrowser_t *fb = sgl_container_of(obj, sgl_filebrowser_t, obj);
    fb->path_prefix = (prefix != NULL) ? prefix : "";
    sgl_filebrowser_update_full_path(fb);
    sgl_obj_set_dirty(obj);
}
