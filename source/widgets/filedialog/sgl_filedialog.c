/* source/widgets/sgl_filedialog.c
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
#include "sgl_filedialog.h"

#define SGL_FILEDIALOG_ITEM_PAD        (3)
#define SGL_FILEDIALOG_ITEM_SPACE      (3)
#define SGL_FILEDIALOG_PARENT_NAME     ".."
#define SGL_FILEDIALOG_OK_TEXT         "OK"

static inline bool sgl_filedialog_is_valid_dir_entry(const char *name)
{
    if (name[0] == '\0') return false;
    if (name[0] == '.' && name[1] == '\0') return false;
    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') return false;
    return true;
}

static void sgl_filedialog_trim_trailing_slash(char *path)
{
    size_t len = strlen(path);
    while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        path[--len] = '\0';
    }
}

static void sgl_filedialog_join_path(char *dst, size_t dst_size,
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

static bool sgl_filedialog_at_dev_root(const sgl_filedialog_t *fd)
{
    return fd->dev_root[0] != '\0' && strcmp(fd->current_path, fd->dev_root) == 0;
}

/* ".." is shown while browsing files, except at the root of a device the
 * dialog was opened on directly (no device list to go back to) */
static bool sgl_filedialog_has_parent(const sgl_filedialog_t *fd)
{
    if (fd->at_devlist) return false;
    if (fd->from_devlist) return true;
    return !sgl_filedialog_at_dev_root(fd);
}

static const char *sgl_filedialog_match_icon(const sgl_filedialog_t *fd,
                                             const sgl_filebrowser_item_t *item)
{
    if (fd->icons == NULL || item == NULL) return NULL;

    const char *default_icon = NULL;
    const char *dir_icon     = NULL;
    const char *parent_icon  = NULL;
    size_t item_len = strlen(item->text);

    for (const sgl_filebrowser_icon_t *e = fd->icons; e->pattern != NULL; ++e) {
        const char *p  = e->pattern;
        const char *ic = e->icon;
        if (p == NULL || ic == NULL) continue;

        if (p[0] == '*' && p[1] == '\0') { default_icon = ic; continue; }
        if (p[0] == '.' && p[1] == '.' && p[2] == '\0') { parent_icon = ic; continue; }
        if (p[0] == 'd' && p[1] == 'i' && p[2] == 'r' && p[3] == '\0') { dir_icon = ic; continue; }

        size_t plen = strlen(p);
        if (plen == 0 || plen > item_len) continue;

        /* case-insensitive suffix match, same as the file browser */
        const char *a = item->text + item_len - plen;
        const char *b = p;
        for (;;) {
            unsigned char ca = (unsigned char)*a++;
            unsigned char cb = (unsigned char)*b++;
            if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca + 32);
            if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb + 32);
            if (ca != cb) break;
            if (ca == 0)  return ic;
        }
    }
    if (item->text[0] == '.' && item->text[1] == '.' && item->text[2] == '\0' && parent_icon)
        return parent_icon;
    if (item->type == SGL_S_IFDIR && dir_icon != NULL)
        return dir_icon;
    return default_icon;
}

static void sgl_filedialog_resolve_item_icon(sgl_filedialog_t *fd,
                                             sgl_filebrowser_item_t *item)
{
    item->icon   = sgl_filedialog_match_icon(fd, item);
    item->icon_w = (item->icon != NULL)
                   ? (int16_t)sgl_font_get_string_width(item->icon, fd->font)
                   : (int16_t)0;
}

static void sgl_filedialog_close_dir(sgl_filedialog_t *fd)
{
    if (fd->dir_handle >= 0) {
        sgl_fs_closedir(fd->dir_handle);
        fd->dir_handle = -1;
    }
    fd->dir_cursor = 0;
}

/* Same persistent-handle readdir windowing as the file browser: rewind costs
 * one closedir + opendir because the FS abstraction has no seekdir. */
static bool sgl_filedialog_seek_dir(sgl_filedialog_t *fd, int16_t target)
{
    if (target < 0) target = 0;

    if (fd->dir_handle < 0 || fd->dir_cursor > target) {
        sgl_filedialog_close_dir(fd);
        if (sgl_fs_opendir(fd->current_path, &fd->dir_handle) != SGL_FS_OK) {
            fd->dir_handle = -1;
            fd->dir_cursor = 0;
            return false;
        }
        fd->dir_cursor = 0;
    }

    char name[SGL_FILEDIALOG_NAME_MAX_LEN];
    uint32_t type;
    while (fd->dir_cursor < target) {
        int r = sgl_fs_readdir(fd->dir_handle, name, sizeof(name), &type);
        if (r <= 0) {
            sgl_filedialog_close_dir(fd);
            return false;
        }
        if (sgl_filedialog_is_valid_dir_entry(name)) {
            fd->dir_cursor++;
        }
    }
    return true;
}

static int sgl_filedialog_read_next(sgl_filedialog_t *fd,
                                    char *name, size_t name_size, uint32_t *type)
{
    if (fd->dir_handle < 0) return 0;
    for (;;) {
        int r = sgl_fs_readdir(fd->dir_handle, name, name_size, type);
        if (r <= 0) return 0;
        if (sgl_filedialog_is_valid_dir_entry(name)) {
            fd->dir_cursor++;
            return 1;
        }
    }
}

static void sgl_filedialog_init_item(sgl_filebrowser_item_t *item, const char *name, uint32_t type)
{
    if (item == NULL || name == NULL) return;
    sgl_snprintf(item->text, sizeof(item->text), "%s", name);
    item->type   = type;
    item->icon   = NULL;
    item->icon_w = 0;
}

static void sgl_filedialog_release_cache(sgl_filedialog_t *fd)
{
    if (fd->cache_items != NULL) {
        sgl_free(fd->cache_items);
        fd->cache_items = NULL;
    }
    fd->item_capacity     = 0;
    fd->cache_start_index = -1;
    fd->cache_count       = 0;
}

static bool sgl_filedialog_ensure_cache(sgl_filedialog_t *fd, uint16_t capacity)
{
    if (capacity < SGL_FILEDIALOG_CACHE_MIN) {
        capacity = SGL_FILEDIALOG_CACHE_MIN;
    }
    if (fd->cache_items != NULL && fd->item_capacity == capacity) {
        return true;
    }

    sgl_filedialog_release_cache(fd);
    fd->cache_items = sgl_malloc(sizeof(sgl_filebrowser_item_t) * capacity);
    if (fd->cache_items == NULL) {
        SGL_LOG_ERROR("sgl_filedialog: cache alloc failed (%u items)", (unsigned)capacity);
        return false;
    }
    fd->item_capacity     = capacity;
    fd->cache_start_index = -1;
    fd->cache_count       = 0;
    return true;
}

static inline bool sgl_filedialog_cache_covers(const sgl_filedialog_t *fd,
                                               int16_t first, int16_t last /* inclusive */)
{
    if (fd->cache_items == NULL || fd->cache_count == 0 || fd->cache_start_index < 0)
        return false;
    return first >= fd->cache_start_index &&
           last  <  fd->cache_start_index + (int16_t)fd->cache_count;
}

static void sgl_filedialog_fill_window(sgl_filedialog_t *fd, int16_t start_index)
{
    if (fd->cache_items == NULL || fd->item_capacity == 0) return;
    if (fd->item_num == 0) {
        fd->cache_start_index = 0;
        fd->cache_count       = 0;
        return;
    }

    if (start_index < 0) start_index = 0;
    if (start_index > (int16_t)fd->item_num - 1) start_index = (int16_t)fd->item_num - 1;

    uint16_t want = fd->item_capacity;
    if ((int)want > (int)fd->item_num - start_index) {
        want = (uint16_t)(fd->item_num - start_index);
    }
    if (fd->cache_start_index == start_index && fd->cache_count == want) {
        return;
    }

    fd->cache_start_index = start_index;
    fd->cache_count       = 0;
    uint16_t written = 0;

    if (fd->at_devlist) {
        /* device list level: rows come straight from the device-backed
         * mount points (enumerated live so hot-plugged mounts show up) */
        int count = sgl_fs_mount_count();
        for (int i = start_index; i < count && written < want; ++i) {
            const char *mp = sgl_fs_mount_get_path((uint32_t)i);
            if (mp == NULL) continue;
            sgl_filedialog_init_item(&fd->cache_items[written], mp, SGL_S_IFDIR);
            sgl_filedialog_resolve_item_icon(fd, &fd->cache_items[written]);
            written++;
        }
        fd->cache_count = written;
        return;
    }

    const bool has_parent = sgl_filedialog_has_parent(fd);

    /* 1. emit ".." if it falls inside the window */
    if (has_parent && start_index == 0 && written < want) {
        sgl_filedialog_init_item(&fd->cache_items[written],
                                 SGL_FILEDIALOG_PARENT_NAME, SGL_S_IFDIR);
        sgl_filedialog_resolve_item_icon(fd, &fd->cache_items[written]);
        written++;
    }

    int16_t content_start = start_index - (has_parent ? 1 : 0);
    if (content_start < 0) content_start = 0;

    if (!sgl_filedialog_seek_dir(fd, content_start)) {
        fd->cache_count = written;
        return;
    }

    char name[SGL_FILEDIALOG_NAME_MAX_LEN];
    uint32_t type = 0;
    while (written < want &&
           sgl_filedialog_read_next(fd, name, sizeof(name), &type)) {
        sgl_filedialog_init_item(&fd->cache_items[written], name, type);
        sgl_filedialog_resolve_item_icon(fd, &fd->cache_items[written]);
        written++;
    }
    fd->cache_count = written;
}

static void sgl_filedialog_ensure_visible(sgl_filedialog_t *fd, int16_t first, int16_t last)
{
    if (fd->cache_items == NULL || fd->item_capacity == 0 || fd->item_num == 0)
        return;
    if (first < 0) first = 0;
    if (last  > (int16_t)fd->item_num - 1) last = (int16_t)fd->item_num - 1;
    if (first > last) first = last;

    if (sgl_filedialog_cache_covers(fd, first, last)) return;

    const int16_t cap    = (int16_t)fd->item_capacity;
    const int16_t total  = (int16_t)fd->item_num;
    int16_t new_start;

    if (fd->cache_start_index < 0) {
        new_start = first;
    } else if (first >= fd->cache_start_index + fd->cache_count) {
        new_start = first;
    } else if (last < fd->cache_start_index) {
        new_start = last - cap + 1;
    } else {
        int16_t range_mid = (int16_t)((first + last) / 2);
        new_start = (int16_t)(range_mid - cap / 2);
    }

    if (new_start < 0)              new_start = 0;
    if (new_start > total - cap)    new_start = (int16_t)(total - cap);
    if (new_start < 0)              new_start = 0;

    sgl_filedialog_fill_window(fd, new_start);
}

static sgl_filebrowser_item_t *sgl_filedialog_get_item(sgl_filedialog_t *fd, int16_t index)
{
    if (index < 0 || index >= (int16_t)fd->item_num) return NULL;
    if (!sgl_filedialog_cache_covers(fd, index, index)) {
        sgl_filedialog_ensure_visible(fd, index, index);
    }
    if (!sgl_filedialog_cache_covers(fd, index, index)) return NULL;
    return &fd->cache_items[index - fd->cache_start_index];
}

static void sgl_filedialog_clear_items(sgl_filedialog_t *fd)
{
    sgl_filedialog_close_dir(fd);
    sgl_filedialog_release_cache(fd);

    fd->selected              = &fd->selected_item;
    fd->selected_item.text[0] = '\0';
    fd->selected_item.type    = 0;
    fd->selected_item.icon    = NULL;
    fd->selected_item.icon_w  = 0;
    fd->item_num              = 0;
    fd->item_selected         = -1;
    sgl_scroll_anim_stop(&fd->sc);
    sgl_scroll_reset(&fd->sc);
}

static void sgl_filedialog_select_index(sgl_filedialog_t *fd, int16_t index)
{
    if (index < 0 || index >= (int16_t)fd->item_num) {
        fd->selected              = &fd->selected_item;
        fd->selected_item.text[0] = '\0';
        fd->selected_item.type    = 0;
        fd->selected_item.icon    = NULL;
        fd->selected_item.icon_w  = 0;
        fd->item_selected         = -1;
        return;
    }
    if (index == fd->item_selected) return;

    sgl_filebrowser_item_t *it = sgl_filedialog_get_item(fd, index);
    if (it == NULL) {
        fd->selected              = &fd->selected_item;
        fd->selected_item.text[0] = '\0';
        fd->selected_item.type    = 0;
        fd->selected_item.icon    = NULL;
        fd->selected_item.icon_w  = 0;
        fd->item_selected         = -1;
        return;
    }
    fd->selected_item = *it;
    fd->selected      = &fd->selected_item;
    fd->item_selected = index;
}

/* Show the registered block devices as the top level */
static void sgl_filedialog_show_devices(sgl_filedialog_t *fd)
{
    sgl_filedialog_clear_items(fd);
    fd->at_devlist      = 1;
    fd->dev_root[0]     = '\0';
    fd->current_path[0] = '\0';
    fd->item_num        = (uint16_t)sgl_max(0, sgl_fs_mount_count());
    if (fd->item_num > 0) {
        sgl_filedialog_select_index(fd, 0);
    }
}

static void sgl_filedialog_load_dir(sgl_filedialog_t *fd, const char *path)
{
    sgl_filedialog_clear_items(fd);
    fd->at_devlist = 0;

    if (path == NULL || path[0] == '\0') {
        strncpy(fd->current_path, "/", sizeof(fd->current_path) - 1);
    } else {
        strncpy(fd->current_path, path, sizeof(fd->current_path) - 1);
    }
    fd->current_path[sizeof(fd->current_path) - 1] = '\0';
    sgl_filedialog_trim_trailing_slash(fd->current_path);

    /* count entries once; the cache window is filled lazily on draw */
    uint16_t count = sgl_filedialog_has_parent(fd) ? 1 : 0;
    int dd = -1;
    if (sgl_fs_opendir(fd->current_path, &dd) == SGL_FS_OK) {
        char name[SGL_FILEDIALOG_NAME_MAX_LEN];
        uint32_t type;
        while (sgl_fs_readdir(dd, name, sizeof(name), &type) > 0) {
            if (sgl_filedialog_is_valid_dir_entry(name)) count++;
        }
        sgl_fs_closedir(dd);
    } else {
        SGL_LOG_WARN("sgl_filedialog: cannot open dir '%s'", fd->current_path);
    }
    fd->item_num = count;

    if (count > 0) {
        sgl_filedialog_select_index(fd, 0);
    }
}

static void sgl_filedialog_enter_device(sgl_filedialog_t *fd, const char *name)
{
    if (name == NULL || name[0] == '\0') return;
    sgl_snprintf(fd->dev_root, sizeof(fd->dev_root), "%s", name);
    sgl_filedialog_trim_trailing_slash(fd->dev_root);
    if (fd->dev_root[0] == '\0') {
        strcpy(fd->dev_root, "/");
    }
    sgl_filedialog_load_dir(fd, fd->dev_root);
}

/* ------------------------------------------------------------------------- */
/* Confirmation                                                              */
/* ------------------------------------------------------------------------- */

static bool sgl_filedialog_can_confirm(const sgl_filedialog_t *fd)
{
    if (fd->at_devlist) return false;
    if (fd->mode == SGL_FILEDIALOG_OPEN_DIR) return true; /* current dir at least */
    return fd->selected != NULL &&
           fd->selected->text[0] != '\0' &&
           fd->selected->type == SGL_S_IFREG;
}

static void sgl_filedialog_finish(sgl_filedialog_t *fd, const char *path)
{
    sgl_filedialog_cb_t cb = fd->confirm_cb;
    void *user_data        = fd->cb_user_data;
    sgl_obj_t *obj         = &fd->obj;

    if (cb != NULL) {
        cb(obj, path, user_data);
    }
    sgl_obj_set_destroyed(obj);
}

static void sgl_filedialog_confirm(sgl_filedialog_t *fd)
{
    if (!sgl_filedialog_can_confirm(fd)) return;

    fd->result_path[0] = '\0';
    if (fd->mode == SGL_FILEDIALOG_OPEN_FILE) {
        sgl_filedialog_join_path(fd->result_path, sizeof(fd->result_path),
                                 fd->current_path, fd->selected->text);
    } else {
        /* open-dir: a highlighted directory wins, otherwise the directory
         * currently being browsed is the answer */
        if (fd->selected != NULL && fd->selected->type == SGL_S_IFDIR &&
            strcmp(fd->selected->text, SGL_FILEDIALOG_PARENT_NAME) != 0) {
            sgl_filedialog_join_path(fd->result_path, sizeof(fd->result_path),
                                     fd->current_path, fd->selected->text);
        } else {
            sgl_snprintf(fd->result_path, sizeof(fd->result_path), "%s", fd->current_path);
        }
    }
    if (fd->result_path[0] == '\0') return;
    sgl_filedialog_finish(fd, fd->result_path);
}

/* ------------------------------------------------------------------------- */
/* Geometry                                                                  */
/* ------------------------------------------------------------------------- */

static int32_t sgl_filedialog_max_scroll(const sgl_filedialog_t *fd, const sgl_obj_t *obj,
                                         int item_height, int list_h)
{
    SGL_UNUSED(obj);
    const int content_h = (int)fd->item_num * item_height;
    return sgl_max(0, content_h - list_h);
}

static void sgl_filedialog_scroll_commit(sgl_scroll_t *sc)
{
    sgl_filedialog_t *fd = sgl_container_of(sc, sgl_filedialog_t, sc);
    sgl_scroll_bar_wake(sc);
    sgl_obj_set_dirty(&fd->obj);
}

static void sgl_filedialog_ensure_selected_visible(sgl_filedialog_t *fd, sgl_obj_t *obj,
                                                   int item_height, int list_h)
{
    if (fd->item_selected < 0) return;
    const int selected_y     = fd->item_selected * item_height;
    const int view_top       = (int)fd->sc.offset;
    const int view_bottom    = view_top + list_h;
    const int32_t max_scroll = sgl_filedialog_max_scroll(fd, obj, item_height, list_h);

    if (selected_y < view_top) {
        fd->sc.offset = selected_y;
    }
    else if (selected_y + item_height > view_bottom) {
        fd->sc.offset = selected_y + item_height - list_h;
    }

    if (fd->sc.offset < 0)
        fd->sc.offset = 0;
    if (fd->sc.offset > max_scroll)
        fd->sc.offset = max_scroll;
}

static void sgl_filedialog_visible_range(const sgl_filedialog_t *fd, int item_height, int list_h,
                                         int16_t *out_first, int16_t *out_last)
{
    int first = fd->sc.offset > 0 ? (int)(fd->sc.offset / item_height) : 0;
    if (first < 0) first = 0;
    if (first >= (int)fd->item_num) first = (int)fd->item_num - 1;

    int visible_rows = (list_h + item_height - 1) / item_height + 1;
    int last = first + visible_rows - 1;
    if (last >= (int)fd->item_num) last = (int)fd->item_num - 1;

    *out_first = (int16_t)first;
    *out_last  = (int16_t)last;
}

static uint16_t sgl_filedialog_target_capacity(int item_height, int list_h)
{
    int visible_rows = (list_h + item_height - 1) / item_height + 1;
    if (visible_rows < 2) visible_rows = 2;
    int cap = visible_rows * SGL_FILEDIALOG_CACHE_MULT;
    if (cap < SGL_FILEDIALOG_CACHE_MIN) cap = SGL_FILEDIALOG_CACHE_MIN;
    if (cap > 0xFFFF)                   cap = 0xFFFF;
    return (uint16_t)cap;
}

/* OK button rectangle inside the bottom bar */
static void sgl_filedialog_ok_rect(const sgl_filedialog_t *fd, const sgl_obj_t *obj,
                                   int item_height, sgl_area_t *out)
{
    const int text_w = sgl_font_get_string_width(SGL_FILEDIALOG_OK_TEXT, fd->font);
    const int w = text_w + 2 * (item_height / 2);
    out->x2 = obj->coords.x2 - obj->border - SGL_FILEDIALOG_ITEM_PAD - 2;
    out->x1 = out->x2 - w;
    out->y1 = obj->coords.y2 - obj->border - item_height + SGL_FILEDIALOG_ITEM_SPACE;
    out->y2 = obj->coords.y2 - obj->border - SGL_FILEDIALOG_ITEM_SPACE;
}

static bool sgl_filedialog_pos_in_area(const sgl_area_t *area, int16_t x, int16_t y)
{
    return x >= area->x1 && x <= area->x2 && y >= area->y1 && y <= area->y2;
}

/* ------------------------------------------------------------------------- */
/* Activate (click / enter key) one row                                      */
/* ------------------------------------------------------------------------- */

static void sgl_filedialog_activate_index(sgl_filedialog_t *fd, sgl_obj_t *obj, int16_t index)
{
    if (index < 0 || index >= (int16_t)fd->item_num) return;

    const int16_t old_selected = fd->item_selected;
    const bool reclick = (index == old_selected);
    sgl_filedialog_select_index(fd, index);
    if (fd->selected == NULL) return;

    if (fd->at_devlist) {
        /* single click highlights, second click enters the device */
        if (reclick) {
            const char *mp = sgl_fs_mount_get_path((uint32_t)index);
            if (mp != NULL) {
                sgl_filedialog_enter_device(fd, mp);
            }
        }
        sgl_obj_set_dirty(obj);
        return;
    }

    if (strcmp(fd->selected->text, SGL_FILEDIALOG_PARENT_NAME) == 0) {
        /* single click highlights, second click goes up to the parent */
        if (reclick) {
            if (fd->from_devlist && sgl_filedialog_at_dev_root(fd)) {
                sgl_filedialog_show_devices(fd);
            } else {
                char parent_path[SGL_FILEDIALOG_PATH_MAX_LEN];
                strncpy(parent_path, fd->current_path, sizeof(parent_path) - 1);
                parent_path[sizeof(parent_path) - 1] = '\0';
                sgl_filedialog_trim_trailing_slash(parent_path);

                char *slash = strrchr(parent_path, '/');
                if (slash == NULL)              strcpy(parent_path, "/");
                else if (slash == parent_path)  parent_path[1] = '\0';
                else                            *slash = '\0';

                sgl_filedialog_load_dir(fd, parent_path);
            }
        }
        sgl_obj_set_dirty(obj);
        return;
    }

    if (fd->selected->type == SGL_S_IFDIR) {
        if (reclick) {
            char next_path[SGL_FILEDIALOG_PATH_MAX_LEN];
            sgl_filedialog_join_path(next_path, sizeof(next_path),
                                     fd->current_path, fd->selected->text);
            sgl_filedialog_load_dir(fd, next_path);
        }
        sgl_obj_set_dirty(obj);
        return;
    }

    /* regular file: in open-file mode a second click confirms right away */
    if (fd->mode == SGL_FILEDIALOG_OPEN_FILE && reclick) {
        sgl_filedialog_confirm(fd);
        return;
    }
    sgl_obj_set_dirty(obj);
}

/* ------------------------------------------------------------------------- */
/* Construct / event callback                                                */
/* ------------------------------------------------------------------------- */

static void sgl_filedialog_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_filedialog_t *fd = sgl_container_of(obj, sgl_filedialog_t, obj);

    const int item_height    = sgl_font_get_height(fd->font) + 2 * SGL_FILEDIALOG_ITEM_SPACE;
    const int item_pad       = sgl_max(obj->radius, obj->border + SGL_FILEDIALOG_ITEM_PAD);
    const int16_t icon_col_w = fd->icons != NULL
                               ? (fd->icon_width == 0 ? (int16_t)item_height : (int16_t)fd->icon_width)
                               : (int16_t)0;
    const int16_t text_x = obj->coords.x1 + item_pad + icon_col_w;
    const int list_y1    = obj->coords.y1 + item_height;          /* below the title bar */
    const int list_y2    = obj->coords.y2 - item_height;          /* above the bottom bar */
    const int list_h     = sgl_max(0, list_y2 - list_y1 + 1);

    sgl_area_t list_area = {
        .x1 = obj->area.x1,
        .y1 = list_y1 + 1,
        .x2 = obj->area.x2,
        .y2 = obj->area.y2 - obj->border - item_height,
    };

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN: {
        sgl_draw_rect_t bg_desc = {
            .alpha        = fd->alpha,
            .color        = fd->bg_color,
            .border       = obj->border,
            .border_alpha = fd->alpha,
            .border_color = fd->border_color,
            .radius       = obj->radius,
            .pixmap       = NULL,
        };
        sgl_draw_rect(surf, &obj->area, &obj->coords, &bg_desc);

        /* --- title bar: device list title or current path --- */
        {
            const int16_t title_y1 = obj->coords.y1 + obj->border;
            const int16_t title_y2 = title_y1 + item_height - 1;
            sgl_area_t title_area = {
                .x1 = obj->area.x1 + obj->border,  .y1 = title_y1,
                .x2 = obj->area.x2 - obj->border,  .y2 = title_y2,
            };
            sgl_area_t title_rect = {
                .x1 = obj->coords.x1 + obj->border, .y1 = title_y1,
                .x2 = obj->coords.x2 - obj->border, .y2 = title_y2 + obj->radius + obj->border,
            };
            sgl_draw_fill_rect(surf, &title_area, &title_rect, obj->radius,
                               fd->path_color, fd->alpha);

            const char *title_text;
            if (fd->at_devlist) {
                title_text = fd->title != NULL ? fd->title : "Select device";
            } else {
                title_text = fd->current_path;
            }
            sgl_draw_string(surf, &obj->area,
                            title_rect.x1 + obj->radius + SGL_FILEDIALOG_ITEM_PAD,
                            title_rect.y1 + SGL_FILEDIALOG_ITEM_PAD,
                            title_text, fd->item_text_color, fd->alpha, fd->font);

            /* close box at the right end of the title bar */
            const int16_t cx2 = obj->coords.x2 - obj->border - SGL_FILEDIALOG_ITEM_PAD;
            const int16_t cx1 = cx2 - item_height + 2 * SGL_FILEDIALOG_ITEM_PAD;
            const int16_t cx  = cx1 + (cx2 - cx1 -
                                 sgl_font_get_string_width("X", fd->font)) / 2;
            sgl_draw_string(surf, &obj->area, cx,
                            title_rect.y1 + SGL_FILEDIALOG_ITEM_PAD,
                            "X", fd->item_text_color, fd->alpha, fd->font);
        }

        /* --- bottom bar: selection hint + OK button --- */
        {
            const int16_t bar_y1 = list_y2 + 1;
            sgl_area_t bar_area = {
                .x1 = obj->area.x1 + obj->border,  .y1 = bar_y1,
                .x2 = obj->area.x2 - obj->border,  .y2 = obj->area.y2 - obj->border,
            };
            sgl_area_t bar_rect = {
                .x1 = obj->coords.x1 + obj->border, .y1 = bar_y1 - obj->radius - obj->border,
                .x2 = obj->coords.x2 - obj->border, .y2 = obj->coords.y2 - obj->border,
            };
            sgl_draw_fill_rect(surf, &bar_area, &bar_rect, obj->radius,
                               fd->path_color, fd->alpha);

            if (!fd->at_devlist && fd->selected != NULL && fd->selected->text[0] != '\0') {
                sgl_draw_string(surf, &bar_area,
                                bar_area.x1 + obj->radius + SGL_FILEDIALOG_ITEM_PAD,
                                bar_y1 + SGL_FILEDIALOG_ITEM_SPACE,
                                fd->selected->text, fd->item_text_color, fd->alpha, fd->font);
            }

            sgl_area_t ok_rect;
            sgl_filedialog_ok_rect(fd, obj, item_height, &ok_rect);
            const bool can_ok = sgl_filedialog_can_confirm(fd);
            sgl_draw_fill_rect(surf, &bar_area, &ok_rect, (ok_rect.y2 - ok_rect.y1) / 2,
                               can_ok ? fd->btn_color : fd->btn_disabled_color, fd->alpha);
            {
                const int tw = sgl_font_get_string_width(SGL_FILEDIALOG_OK_TEXT, fd->font);
                const int th = sgl_font_get_height(fd->font);
                sgl_draw_string(surf, &bar_area,
                                ok_rect.x1 + (ok_rect.x2 - ok_rect.x1 - tw) / 2,
                                ok_rect.y1 + (ok_rect.y2 - ok_rect.y1 - th) / 2,
                                SGL_FILEDIALOG_OK_TEXT, fd->item_text_color, fd->alpha, fd->font);
            }
        }

        /* --- entry list --- */
        const int32_t max_scroll = sgl_filedialog_max_scroll(fd, obj, item_height, list_h);
        sgl_area_t viewport = {
            .x1 = obj->coords.x1, .y1 = (int16_t)list_y1,
            .x2 = obj->coords.x2, .y2 = (int16_t)list_y2,
        };

        if (fd->item_num == 0) {
            const char *hint = fd->at_devlist ? "No block device registered" : "Empty directory";
            sgl_draw_string(surf, &list_area,
                            obj->coords.x1 + item_pad,
                            list_y1 + SGL_FILEDIALOG_ITEM_SPACE,
                            hint, fd->item_text_color, fd->alpha, fd->font);
            sgl_scroll_draw_bar(surf, obj, &fd->sc, max_scroll, &viewport,
                                sgl_color_invert(fd->bg_color));
            break;
        }

        uint16_t target_cap = sgl_filedialog_target_capacity(item_height, list_h);
        sgl_filedialog_ensure_cache(fd, target_cap);

        int16_t first, last;
        sgl_filedialog_visible_range(fd, item_height, list_h, &first, &last);
        sgl_filedialog_ensure_visible(fd, first, last);

        if (fd->cache_items == NULL || fd->cache_count == 0) {
            sgl_scroll_draw_bar(surf, obj, &fd->sc, max_scroll, &viewport,
                                sgl_color_invert(fd->bg_color));
            break;
        }

        int16_t draw_first = first;
        int16_t draw_last  = last;
        if (draw_first < fd->cache_start_index) draw_first = fd->cache_start_index;
        if (draw_last  > fd->cache_start_index + (int16_t)fd->cache_count - 1)
            draw_last  = fd->cache_start_index + (int16_t)fd->cache_count - 1;

        for (int idx = draw_first; idx <= draw_last; ++idx) {
            const int16_t text_y = list_y1 + SGL_FILEDIALOG_ITEM_SPACE
                                 - (int16_t)fd->sc.offset + idx * item_height;
            if (text_y > list_y2) break;

            sgl_filebrowser_item_t *item = &fd->cache_items[idx - fd->cache_start_index];

            if (idx == fd->item_selected) {
                sgl_area_t select = {
                    .x1 = obj->coords.x1 + obj->border,
                    .y1 = text_y - SGL_FILEDIALOG_ITEM_SPACE,
                    .x2 = obj->coords.x2 - obj->border,
                    .y2 = text_y - SGL_FILEDIALOG_ITEM_SPACE + item_height,
                };
                sgl_draw_fill_rect(surf, &list_area, &select, 0,
                                   fd->item_selected_color, fd->alpha);
            }

            if (icon_col_w > 0 && item->icon != NULL) {
                const int16_t icon_x = obj->coords.x1 + item_pad
                                     + (icon_col_w - item->icon_w) / 2;
                sgl_draw_string(surf, &list_area, icon_x, text_y,
                                item->icon, fd->icon_color, fd->alpha, fd->font);
            }

            /* in open-dir mode regular files are visible but dimmed */
            sgl_color_t text_color = fd->item_text_color;
            if (fd->mode == SGL_FILEDIALOG_OPEN_DIR && item->type != SGL_S_IFDIR &&
                strcmp(item->text, SGL_FILEDIALOG_PARENT_NAME) != 0) {
                text_color = sgl_color_mixer(fd->item_text_color, fd->bg_color, 128);
            }
            sgl_draw_string(surf, &list_area, text_x, text_y,
                            item->text, text_color, fd->alpha, fd->font);
        }

        sgl_scroll_draw_bar(surf, obj, &fd->sc, max_scroll, &viewport,
                            sgl_color_invert(fd->bg_color));
    }
    break;

    case SGL_EVENT_PRESSED:
        sgl_scroll_press(&fd->sc, evt->pos.y);
        sgl_scroll_bar_wake(&fd->sc);
        break;

    case SGL_EVENT_MOVE_UP:
    case SGL_EVENT_MOVE_DOWN: {
        const int32_t max_scroll = sgl_filedialog_max_scroll(fd, obj, item_height, list_h);
        if (sgl_scroll_stay(&fd->sc, evt->pos.y, max_scroll)) {
            sgl_obj_set_dirty(obj);
        }
    }
    break;

    case SGL_EVENT_RELEASED: {
        const int32_t max_scroll = sgl_filedialog_max_scroll(fd, obj, item_height, list_h);
        fd->sc.range  = max_scroll;
        fd->sc.commit = sgl_filedialog_scroll_commit;
        if (sgl_scroll_release(&fd->sc, max_scroll)) {
            sgl_scroll_anim_start(&fd->sc);
        }
        sgl_obj_set_dirty(obj);
    }
    break;

    case SGL_EVENT_CLICKED: {
        /* keyboard activation (enter) */
        if (evt->pos.x == SGL_POS_MIN && evt->pos.y == SGL_POS_MIN) {
            sgl_filedialog_activate_index(fd, obj, fd->item_selected);
            break;
        }

        /* close box on the title bar */
        if (evt->pos.y <= (obj->coords.y1 + item_height) &&
            evt->pos.x >= (obj->coords.x2 - obj->border - item_height)) {
            sgl_filedialog_finish(fd, NULL);
            break;
        }

        /* OK button on the bottom bar */
        sgl_area_t ok_rect;
        sgl_filedialog_ok_rect(fd, obj, item_height, &ok_rect);
        if (sgl_filedialog_pos_in_area(&ok_rect, evt->pos.x, evt->pos.y)) {
            sgl_filedialog_confirm(fd);
            break;
        }

        /* entry rows */
        if (evt->pos.y <= list_y1 || evt->pos.y > list_y2) break;
        const int16_t item_area_top = list_y1 + SGL_FILEDIALOG_ITEM_SPACE - (int16_t)fd->sc.offset;
        if (evt->pos.y < item_area_top) break;

        int16_t clicked_index = (int16_t)((evt->pos.y - item_area_top) / item_height);
        sgl_filedialog_activate_index(fd, obj, clicked_index);
    }
    break;

    case SGL_EVENT_KEY_DOWN:
        if (fd->item_selected < (int16_t)fd->item_num - 1) {
            sgl_filedialog_select_index(fd, fd->item_selected + 1);
            sgl_scroll_anim_stop(&fd->sc);
            sgl_filedialog_ensure_selected_visible(fd, obj, item_height, list_h);
            sgl_scroll_bar_wake(&fd->sc);
            sgl_obj_set_dirty(obj);
        }
        break;

    case SGL_EVENT_KEY_UP:
        if (fd->item_selected > 0) {
            sgl_filedialog_select_index(fd, fd->item_selected - 1);
            sgl_scroll_anim_stop(&fd->sc);
            sgl_filedialog_ensure_selected_visible(fd, obj, item_height, list_h);
            sgl_scroll_bar_wake(&fd->sc);
            sgl_obj_set_dirty(obj);
        }
        break;

    case SGL_EVENT_KEY_ESC:
        sgl_filedialog_finish(fd, NULL);
        break;

    case SGL_EVENT_DESTROYED:
        sgl_scroll_anim_stop(&fd->sc);
        sgl_filedialog_clear_items(fd);
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

static sgl_obj_t *sgl_filedialog_create(sgl_obj_t *parent, const char *dev_name, uint8_t mode)
{
    sgl_filedialog_t *fd = sgl_malloc(sizeof(sgl_filedialog_t));
    if (fd == NULL) {
        SGL_LOG_ERROR("sgl_filedialog_create: malloc failed");
        return NULL;
    }
    memset(fd, 0, sizeof(*fd));

    sgl_obj_init(&fd->obj, parent);
    fd->obj.construct_fn = sgl_filedialog_construct_cb;
    sgl_obj_set_clickable(&fd->obj);
    sgl_obj_set_movable(&fd->obj);
    sgl_obj_set_editable(&fd->obj);
    sgl_obj_set_border_width(&fd->obj, 1);

    fd->mode                = mode;
    fd->selected            = &fd->selected_item;
    fd->font                = sgl_get_system_font();
    fd->alpha               = SGL_THEME_ALPHA;
    fd->bg_color            = SGL_THEME_COLOR;
    fd->item_selected_color = sgl_color_mixer(SGL_THEME_COLOR, SGL_THEME_BORDER_COLOR, 128);
    fd->border_color        = SGL_THEME_BORDER_COLOR;
    fd->item_text_color     = SGL_THEME_TEXT_COLOR;
    fd->icon_color          = sgl_rgb(121, 177, 254);
    fd->path_color          = SGL_COLOR_WHEAT;
    fd->btn_color           = sgl_rgb(86, 156, 214);
    fd->btn_disabled_color  = sgl_color_mixer(SGL_THEME_COLOR, SGL_THEME_TEXT_COLOR, 64);
    fd->item_selected       = -1;
    fd->cache_start_index   = -1;
    fd->dir_handle          = -1;
    fd->sc.bar_alpha        = 200;
    sgl_scroll_reset(&fd->sc);

    /* default geometry: centered 3/4 of the screen; the caller may override
     * with sgl_obj_set_pos()/sgl_obj_set_size() */
    {
        int16_t w = (int16_t)(SGL_SCREEN_WIDTH * 3 / 4);
        int16_t h = (int16_t)(SGL_SCREEN_HEIGHT * 3 / 4);
        sgl_obj_set_size(&fd->obj, w, h);
        sgl_obj_set_pos(&fd->obj,
                        (int16_t)((SGL_SCREEN_WIDTH - w) / 2),
                        (int16_t)((SGL_SCREEN_HEIGHT - h) / 2));
    }

    if (dev_name != NULL) {
        const char *mp = sgl_fs_mount_find(dev_name);
        if (mp != NULL) {
            fd->from_devlist = 0;
            sgl_filedialog_enter_device(fd, mp);
        } else {
            SGL_LOG_WARN("sgl_filedialog: device '%s' not mounted, showing device list",
                         dev_name);
            fd->from_devlist = 1;
            sgl_filedialog_show_devices(fd);
        }
    } else {
        fd->from_devlist = 1;
        sgl_filedialog_show_devices(fd);
    }

    return &fd->obj;
}

/**
 * @brief Open a "pick a file" dialog
 */
sgl_obj_t *sgl_filedialog_open_file(sgl_obj_t *parent, const char *dev_name)
{
    return sgl_filedialog_create(parent, dev_name, SGL_FILEDIALOG_OPEN_FILE);
}

/**
 * @brief Open a "pick a directory" dialog
 */
sgl_obj_t *sgl_filedialog_open_dir(sgl_obj_t *parent, const char *dev_name)
{
    return sgl_filedialog_create(parent, dev_name, SGL_FILEDIALOG_OPEN_DIR);
}

/**
 * @brief Set the result callback
 */
void sgl_filedialog_set_confirm_cb(sgl_obj_t *obj, sgl_filedialog_cb_t cb, void *user_data)
{
    sgl_filedialog_t *fd = sgl_container_of(obj, sgl_filedialog_t, obj);
    fd->confirm_cb  = cb;
    fd->cb_user_data = user_data;
}

/**
 * @brief Set the dialog title
 */
void sgl_filedialog_set_title(sgl_obj_t *obj, const char *title)
{
    sgl_filedialog_t *fd = sgl_container_of(obj, sgl_filedialog_t, obj);
    fd->title = title;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the text font
 */
void sgl_filedialog_set_text_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_filedialog_t *fd = sgl_container_of(obj, sgl_filedialog_t, obj);
    fd->font = font;
    if (fd->cache_items != NULL) {
        for (uint16_t i = 0; i < fd->cache_count; ++i) {
            sgl_filebrowser_item_t *it = &fd->cache_items[i];
            it->icon_w = (it->icon != NULL)
                         ? (int16_t)sgl_font_get_string_width(it->icon, fd->font)
                         : (int16_t)0;
        }
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the file type icons
 */
void sgl_filedialog_set_icons(sgl_obj_t *obj, const sgl_filebrowser_icon_t *icons)
{
    sgl_filedialog_t *fd = sgl_container_of(obj, sgl_filedialog_t, obj);
    fd->icons = icons;
    if (fd->cache_items != NULL) {
        for (uint16_t i = 0; i < fd->cache_count; ++i) {
            sgl_filedialog_resolve_item_icon(fd, &fd->cache_items[i]);
        }
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the icon column width
 */
void sgl_filedialog_set_icon_width(sgl_obj_t *obj, uint16_t width)
{
    sgl_filedialog_t *fd = sgl_container_of(obj, sgl_filedialog_t, obj);
    fd->icon_width = (uint8_t)width;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the background color
 */
void sgl_filedialog_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_filedialog_t *fd = sgl_container_of(obj, sgl_filedialog_t, obj);
    fd->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the entry text color
 */
void sgl_filedialog_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_filedialog_t *fd = sgl_container_of(obj, sgl_filedialog_t, obj);
    fd->item_text_color = color;
    sgl_obj_set_dirty(obj);
}
