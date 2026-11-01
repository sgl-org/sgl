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
#include <sgl_cfgfix.h>
#include <sgl_fs.h>
#include <stdio.h>
#include <string.h>
#include "sgl_filedialog.h"

#define SGL_FILEDIALOG_ITEM_PAD        (3)
#define SGL_FILEDIALOG_ITEM_SPACE      (6)
#define SGL_FILEDIALOG_PARENT_NAME     ".."
#define SGL_FILEDIALOG_OK_TEXT         "OK"
#define SGL_FILEDIALOG_OK_MARGIN       (3)   /* OK pill inset inside the bottom bar */
#define SGL_FILEDIALOG_ROOT_TEXT       "DISK" /* default title on the "/" root */

/* rounded row card style, same scheme as sgl_menu */
#define SGL_FILEDIALOG_CARD_VMARGIN    (1)   /* gap between card and row edge  */
#define SGL_FILEDIALOG_CARD_HPAD       (3)   /* card inset from the widget edge */
#define SGL_FILEDIALOG_CARD_RADIUS     (4)   /* card corner radius             */
#define SGL_FILEDIALOG_CARD_BORDER_W   (2)   /* selected card border width     */
#define SGL_FILEDIALOG_LIST_TOP_PAD    (1)   /* extra top margin of the first row:
                                              * part of the content, it scrolls away */

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

/* The virtual "/" root lists the mounted block devices, exactly like the file
 * browser: no ".." there, and ".." at a device root returns to it */
static inline bool sgl_filedialog_is_root(const sgl_filedialog_t *fd)
{
    return fd->current_path[0] == '/' && fd->current_path[1] == '\0';
}

static inline const char *sgl_filedialog_title_text(const sgl_filedialog_t *fd)
{
    return sgl_filedialog_is_root(fd) ? fd->root_text : fd->current_path;
}

static const char *sgl_filedialog_match_icon(const sgl_filedialog_t *fd,
                                             const sgl_filebrowser_item_t *item)
{
    if (fd->icons == NULL || item == NULL) return NULL;

    /* The virtual "/" root lists block devices, not files: its rows are plain
     * device names, so no icon is matched (and none is drawn) for them. */
    if (sgl_filedialog_is_root(fd)) return NULL;

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

/* Decode the leading UTF-8 codepoint of an icon string. Icons are single
 * characters (e.g. the SGL_ICON_* font glyphs), so only the first one matters. */
static uint32_t sgl_filedialog_icon_unicode(const char *icon)
{
    const uint8_t c = (uint8_t)icon[0];
    uint32_t u;
    int n, i;

    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0)      { u = c & 0x1Fu; n = 1; }
    else if ((c & 0xF0) == 0xE0) { u = c & 0x0Fu; n = 2; }
    else if ((c & 0xF8) == 0xF0) { u = c & 0x07u; n = 3; }
    else return c;

    for (i = 1; i <= n && icon[i] != '\0'; ++i)
        u = (u << 6) | ((uint8_t)icon[i] & 0x3Fu);
    return u;
}

static void sgl_filedialog_draw_icon(sgl_filedialog_t *fd, sgl_surf_t *surf,
                                     sgl_area_t *area, int16_t x,
                                     int16_t card_y1, int16_t card_h,
                                     const char *icon)
{
    const sgl_font_t *ifont = fd->font;
    int16_t y = (int16_t)(card_y1 + (card_h - sgl_font_get_height(ifont)) / 2);

    if (ifont->table != NULL) {
        uint32_t idx = sgl_search_unicode_ch_index(ifont,
                                                   sgl_filedialog_icon_unicode(icon));
        if (idx != 0) {
            const sgl_font_table_t *dsc = &ifont->table[idx];
            y = (int16_t)(card_y1 + (card_h - dsc->box_h) / 2
                          + dsc->box_h + dsc->ofs_y + ifont->base_line
                          - ifont->font_height);
        }
    }
    sgl_draw_string(surf, area, x, y, icon, fd->icon_color, fd->alpha, ifont);
}

static void sgl_filedialog_resolve_item_icon(sgl_filedialog_t *fd,
                                             sgl_filebrowser_item_t *item)
{
    item->icon   = sgl_filedialog_match_icon(fd, item);
    item->icon_w = (item->icon != NULL)
                   ? (int16_t)sgl_font_get_string_width(item->icon, fd->font)
                   : (int16_t)0;
}

/* Recompute the cached icon widths, e.g. after the text font changed */
static void sgl_filedialog_refresh_icon_widths(sgl_filedialog_t *fd)
{
    const sgl_font_t *font = fd->font;

    for (uint16_t i = 0; i < fd->cache_count; ++i) {
        sgl_filebrowser_item_t *it = &fd->cache_items[i];
        if (it->icon != NULL)
            it->icon_w = (int16_t)sgl_font_get_string_width(it->icon, font);
    }
    if (fd->selected_item.icon != NULL)
        fd->selected_item.icon_w =
            (int16_t)sgl_font_get_string_width(fd->selected_item.icon, font);
}

/* Format a byte count into a compact human string, e.g. 10485760 -> "10.0M",
 * 512 -> "512B". Uses binary (1024) units B/K/M/G with one fractional digit. */
static void sgl_filedialog_format_size(uint64_t bytes, char *out, size_t n)
{
    static const char units[] = "BKMG";
    int u = 0;
    uint64_t v = bytes;
    while (v >= 1024ULL && u < 3) { v >>= 10; u++; }

    if (u == 0) {
        sgl_snprintf(out, n, "%d%c", (int)bytes, units[0]);
    } else {
        uint64_t scale  = 1ULL << (u * 10);
        uint64_t whole  = bytes / scale;
        uint64_t frac10 = ((bytes % scale) * 10) / scale;
        sgl_snprintf(out, n, "%d.%d%c",
                     (int)whole, (int)frac10, units[u]);
    }
}

/* Draw the free/total capacity of the device at mount index `mnt_idx`,
 * right-aligned to `right_edge` (the card's right inner edge). No-op when
 * capacity is unknown. Computed on the fly so no per-item buffer is needed. */
static void sgl_filedialog_draw_dev_capacity(sgl_filedialog_t *fd, sgl_surf_t *surf,
                                             sgl_area_t *list_area,
                                             int right_edge, int16_t text_y, int16_t mnt_idx)
{
    const char *mp = sgl_fs_mount_get_path((uint32_t)mnt_idx);
    sgl_statvfs_t sv;
    if (mp == NULL || sgl_fs_statvfs(mp, &sv) != SGL_FS_OK || sv.f_blocks == 0) return;

    uint64_t total = (uint64_t)sv.f_bsize * sv.f_blocks;
    uint64_t avail = (uint64_t)sv.f_bsize * sv.f_bfree;
    char ts[16], as[16], cap[24];
    sgl_filedialog_format_size(total, ts, sizeof(ts));
    sgl_filedialog_format_size(avail, as, sizeof(as));
    sgl_snprintf(cap, sizeof(cap), "%s/%s", as, ts);

    const int cap_w = (int)sgl_font_get_string_width(cap, fd->font);
    const int16_t cap_x = (int16_t)(right_edge - cap_w);
    sgl_draw_string(surf, list_area, cap_x, text_y,
                    cap, fd->item_text_color, fd->alpha, fd->font);
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

    /* At the virtual "/" root the list is the mounted block devices, which
     * are enumerated directly from the VFS mount table (no dir handle). */
    if (sgl_filedialog_is_root(fd)) {
        fd->dir_handle = -1;
        fd->dir_cursor = target;
        return true;
    }

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
    if (sgl_filedialog_is_root(fd)) {
        int cnt = sgl_fs_mount_count();
        if (fd->dir_cursor >= cnt) return 0;
        /* The row label is the block device's info->name; the mount point
         * basename is only the fallback for devices without info. */
        const char *nm = sgl_fs_mount_get_name((uint32_t)fd->dir_cursor);
        sgl_snprintf(name, name_size, "%s", nm ? nm : "?");
        *type = SGL_S_IFDIR;
        fd->dir_cursor++;
        return 1;
    }
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

    const bool has_parent = !sgl_filedialog_is_root(fd);

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

static void sgl_filedialog_load_dir(sgl_filedialog_t *fd, const char *path)
{
    sgl_filedialog_clear_items(fd);

    if (path == NULL || path[0] == '\0') {
        strncpy(fd->current_path, "/", sizeof(fd->current_path) - 1);
    } else {
        strncpy(fd->current_path, path, sizeof(fd->current_path) - 1);
    }
    fd->current_path[sizeof(fd->current_path) - 1] = '\0';
    sgl_filedialog_trim_trailing_slash(fd->current_path);

    /* count entries once; the cache window is filled lazily on draw.
     * The virtual "/" root lists mounted block devices, not a directory. */
    uint16_t count;
    if (sgl_filedialog_is_root(fd)) {
        count = (uint16_t)sgl_fs_mount_count();
    } else {
        count = 1; /* ".." */
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
    }
    fd->item_num = count;
    /* nothing is selected up front: the list opens with no highlight and the
     * first row is only selected once the user clicks it (or presses a key) */
}

static bool sgl_filedialog_can_confirm(const sgl_filedialog_t *fd)
{
    if (sgl_filedialog_is_root(fd)) return false;
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

static int32_t sgl_filedialog_max_scroll(const sgl_filedialog_t *fd, const sgl_obj_t *obj,
                                         int item_height, int list_h)
{
    SGL_UNUSED(obj);
    /* the content carries the first row's top margin, so it can scroll away */
    const int content_h = SGL_FILEDIALOG_LIST_TOP_PAD + (int)fd->item_num * item_height;
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
    const int selected_y     = SGL_FILEDIALOG_LIST_TOP_PAD + fd->item_selected * item_height;
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
    /* the first row starts below the top margin, so subtract it in content space */
    int first = fd->sc.offset > SGL_FILEDIALOG_LIST_TOP_PAD
                ? (int)((fd->sc.offset - SGL_FILEDIALOG_LIST_TOP_PAD) / item_height) : 0;
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

static void sgl_filedialog_ok_rect(const sgl_filedialog_t *fd, const sgl_obj_t *obj, int list_y2, sgl_area_t *out)
{
    const int font_h = sgl_font_get_height(fd->font);
    const int text_w = sgl_font_get_string_width(SGL_FILEDIALOG_OK_TEXT, fd->font);

    const int16_t bar_top = (int16_t)(list_y2 + 1);
    const int16_t bar_bot = (int16_t)(obj->coords.y2 - obj->border);
    const int bar_h = bar_bot - bar_top + 1;

    /* Fill the bar minus a small vertical margin, but never smaller than the text. */
    int btn_h = bar_h - 2 * SGL_FILEDIALOG_OK_MARGIN;
    if (btn_h < font_h) btn_h = font_h;
    const int w = text_w + 2 * (btn_h / 2);

    out->x2 = (int16_t)(obj->coords.x2 - obj->border - SGL_FILEDIALOG_ITEM_PAD - 2);
    out->x1 = (int16_t)(out->x2 - w);
    out->y1 = (int16_t)(bar_top + (bar_h - btn_h) / 2);
    out->y2 = (int16_t)(out->y1 + btn_h - 1);
}

static bool sgl_filedialog_pos_in_area(const sgl_area_t *area, int16_t x, int16_t y)
{
    return x >= area->x1 && x <= area->x2 && y >= area->y1 && y <= area->y2;
}

static void sgl_filedialog_activate_index(sgl_filedialog_t *fd, sgl_obj_t *obj, int16_t index)
{
    if (index < 0 || index >= (int16_t)fd->item_num) return;

    const int16_t old_selected = fd->item_selected;
    const bool reclick = (index == old_selected);
    sgl_filedialog_select_index(fd, index);
    if (fd->selected == NULL) return;

    if (strcmp(fd->selected->text, SGL_FILEDIALOG_PARENT_NAME) == 0) {
        /* single click highlights, second click goes up to the parent
         * (at a device root this steps back onto the device list) */
        if (reclick) {
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
        sgl_obj_set_dirty(obj);
        return;
    }

    if (fd->selected->type == SGL_S_IFDIR) {
        if (reclick) {
            char next_path[SGL_FILEDIALOG_PATH_MAX_LEN];
            if (sgl_filedialog_is_root(fd)) {
                /* device list: enter the clicked device's mount point */
                const char *mnt = sgl_fs_mount_get_path((uint32_t)index);
                if (mnt == NULL) {
                    sgl_obj_set_dirty(obj);
                    return;
                }
                strncpy(next_path, mnt, sizeof(next_path) - 1);
                next_path[sizeof(next_path) - 1] = '\0';
            } else {
                sgl_filedialog_join_path(next_path, sizeof(next_path),
                                         fd->current_path, fd->selected->text);
            }
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

static void sgl_filedialog_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_filedialog_t *fd = sgl_container_of(obj, sgl_filedialog_t, obj);

    const int item_height = sgl_font_get_height(fd->font) + 2 * SGL_FILEDIALOG_ITEM_SPACE;
    /* rounded row card geometry, same scheme as sgl_menu: the card radius is
     * clamped to the card body between the row gaps */
    int16_t card_radius = fd->card_radius;
    const int card_hpad = fd->card_hpad;
    /* no icon column at the "/" root: the device rows have no icons */
    const int16_t icon_col_w = (fd->icons != NULL && !sgl_filedialog_is_root(fd))
                               ? (fd->icon_width == 0 ? (int16_t)item_height : (int16_t)fd->icon_width)
                               : (int16_t)0;
    /* row content starts where the card corner arc ends, as in sgl_menu */
    const int16_t text_x = obj->coords.x1 + card_hpad + card_radius + icon_col_w;
    /* the top and bottom bars match the *visible* card height (a row is one
     * item tall, but the card is inset by CARD_VMARGIN top and bottom), so
     * bars and list rows look equally tall; the list spans the space between
     * them (inside the widget border) */
    const int bar_h      = item_height - 2 * SGL_FILEDIALOG_CARD_VMARGIN;
    const int list_y1    = obj->coords.y1 + obj->border + bar_h; /* below the title bar */
    const int list_y2    = obj->coords.y2 - obj->border - bar_h; /* above the bottom bar */
    const int list_h     = sgl_max(0, list_y2 - list_y1 + 1);
    /* the first row keeps an extra top margin: it is part of the content, so
     * the list viewport itself is unchanged and the gap scrolls away */
    const int16_t rows_y1 = (int16_t)(list_y1 + SGL_FILEDIALOG_LIST_TOP_PAD);

    sgl_area_t list_area = {
        .x1 = obj->area.x1,
        .y1 = list_y1 + 1,
        .x2 = obj->area.x2,
        .y2 = obj->area.y2 - obj->border - bar_h,
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
        /* title bar geometry */
        const char *title_text = sgl_filedialog_title_text(fd);
        const int16_t title_y1 = obj->coords.y1 + obj->border;
        const int16_t title_y2 = title_y1 + bar_h - 1;
        sgl_area_t title_area = {
            .x1 = obj->area.x1 + obj->border,  .y1 = title_y1,
            .x2 = obj->area.x2 - obj->border,  .y2 = title_y2,
        };
        sgl_area_t title_rect = {
            .x1 = obj->coords.x1 + obj->border, .y1 = title_y1,
            .x2 = obj->coords.x2 - obj->border, .y2 = title_y2 + obj->radius + obj->border,
        };
        /* close box at the right end of the title bar */
        const int16_t cx2   = obj->coords.x2 - obj->border - SGL_FILEDIALOG_ITEM_PAD;
        const int16_t cx1   = cx2 - bar_h + 2 * SGL_FILEDIALOG_ITEM_PAD;
        const int16_t ccx   = (int16_t)((cx1 + cx2) / 2);        /* box center x */
        const int16_t ccy   = (int16_t)(title_y1 + bar_h / 2);   /* bar center y */
        const int16_t csize = (int16_t)((bar_h - 4 * SGL_FILEDIALOG_ITEM_PAD) / 2);

        /* bottom bar geometry */
        const int16_t bar_y1 = list_y2 + 1;
        sgl_area_t bar_area = {
            .x1 = obj->area.x1 + obj->border,  .y1 = bar_y1,
            .x2 = obj->area.x2 - obj->border,  .y2 = obj->area.y2 - obj->border,
        };
        sgl_area_t bar_rect = {
            .x1 = obj->coords.x1 + obj->border, .y1 = bar_y1 - obj->radius - obj->border,
            .x2 = obj->coords.x2 - obj->border, .y2 = obj->coords.y2 - obj->border,
        };
        sgl_area_t ok_rect;
        const bool can_ok = sgl_filedialog_can_confirm(fd);
        const int ok_text_w = sgl_font_get_string_width(SGL_FILEDIALOG_OK_TEXT, fd->font);
        const int ok_text_h = sgl_font_get_height(fd->font);

        sgl_draw_rect(surf, &obj->area, &obj->coords, &bg_desc);

        /* --- title bar: current path, or the root label on the device list --- */
        sgl_draw_fill_rect(surf, &title_area, &title_rect, obj->radius,
                           fd->path_color, fd->alpha);
        sgl_draw_string(surf, &obj->area,
                        title_rect.x1 + obj->radius + 2 * SGL_FILEDIALOG_ITEM_PAD,
                        title_y1 + (bar_h - sgl_font_get_height(fd->font)) / 2,
                        title_text, fd->item_text_color, fd->alpha, fd->font);
        /* close box: draw an X (cross) at the right end of the title bar */
        sgl_draw_cross(surf, &obj->area, (int16_t)(ccx - csize), (int16_t)(ccy - csize),
                       csize, fd->item_text_color, 1, fd->alpha);

        /* --- bottom bar: selection hint + OK button --- */
        sgl_draw_fill_rect(surf, &bar_area, &bar_rect, obj->radius,
                           fd->path_color, fd->alpha);

        if (!sgl_filedialog_is_root(fd) && fd->selected != NULL &&
            fd->selected->text[0] != '\0') {
            sgl_draw_string(surf, &bar_area,
                            bar_area.x1 + obj->radius + SGL_FILEDIALOG_ITEM_PAD,
                            bar_y1 + (bar_h - sgl_font_get_height(fd->font)) / 2,
                            fd->selected->text, fd->item_text_color, fd->alpha, fd->font);
        }

        sgl_filedialog_ok_rect(fd, obj, list_y2, &ok_rect);
        sgl_draw_fill_rect(surf, &bar_area, &ok_rect, 4,
                           can_ok ? fd->btn_color : fd->btn_disabled_color, fd->alpha);
        sgl_draw_string(surf, &bar_area,
                        ok_rect.x1 + (ok_rect.x2 - ok_rect.x1 - ok_text_w) / 2,
                        ok_rect.y1 + (ok_rect.y2 - ok_rect.y1 - ok_text_h + 1) / 2,
                        SGL_FILEDIALOG_OK_TEXT, fd->item_text_color, fd->alpha, fd->font);

        /* --- entry list --- */
        const int32_t max_scroll = sgl_filedialog_max_scroll(fd, obj, item_height, list_h);
        sgl_area_t viewport = {
            .x1 = obj->coords.x1, .y1 = (int16_t)list_y1,
            .x2 = obj->coords.x2, .y2 = (int16_t)list_y2,
        };

        if (fd->item_num == 0) {
            const char *hint = sgl_filedialog_is_root(fd) ? "No block device registered" : "Empty directory";
            sgl_draw_string(surf, &list_area,
                            obj->coords.x1 + card_hpad + card_radius,
                            rows_y1 + SGL_FILEDIALOG_ITEM_SPACE,
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
            const int16_t row_y = rows_y1 - (int16_t)fd->sc.offset + idx * item_height;
            if (row_y > list_y2) break;
            if (row_y + item_height <= list_y1) continue;

            sgl_filebrowser_item_t *item = &fd->cache_items[idx - fd->cache_start_index];

            /* every row is a rounded card (sgl_menu style), the selected one
             * gets the accent border */
            sgl_area_t card = {
                .x1 = obj->coords.x1 + card_hpad,
                .y1 = row_y + SGL_FILEDIALOG_CARD_VMARGIN,
                .x2 = obj->coords.x2 - card_hpad,
                .y2 = row_y + item_height - 1 - SGL_FILEDIALOG_CARD_VMARGIN,
            };
            if (idx == fd->item_selected) {
                sgl_draw_fill_rect(surf, &list_area, &card, card_radius,
                                   fd->item_selected_color, fd->alpha);
                sgl_draw_fill_rect_border(surf, &list_area, &card, card_radius,
                                          fd->sel_border_color, fd->card_border_w,
                                          fd->alpha);
            }
            else {
                sgl_draw_fill_rect(surf, &list_area, &card, card_radius,
                                   fd->card_color, fd->alpha);
                sgl_draw_fill_rect_border(surf, &list_area, &card, card_radius,
                                          fd->card_border_color, 1, fd->alpha);
            }

            const int16_t text_y = row_y + SGL_FILEDIALOG_ITEM_SPACE;

            if (icon_col_w > 0 && item->icon != NULL) {
                /* center the icon between the card left edge and the text */
                const int16_t icon_x = obj->coords.x1 + card_hpad
                                     + (card_radius + icon_col_w - item->icon_w) / 2;
                sgl_filedialog_draw_icon(fd, surf, &list_area, icon_x,
                                         (int16_t)(row_y + SGL_FILEDIALOG_CARD_VMARGIN),
                                         (int16_t)(item_height - 2 * SGL_FILEDIALOG_CARD_VMARGIN),
                                         item->icon);
            }

            /* in open-dir mode regular files are visible but dimmed */
            sgl_color_t text_color = fd->item_text_color;
            if (fd->mode == SGL_FILEDIALOG_OPEN_DIR && item->type != SGL_S_IFDIR &&
                strcmp(item->text, SGL_FILEDIALOG_PARENT_NAME) != 0) {
                text_color = sgl_color_mixer(fd->item_text_color, fd->bg_color, 128);
            }
            sgl_draw_string(surf, &list_area, text_x, text_y,
                            item->text, text_color, fd->alpha, fd->font);

            /* device list (root): append free/total capacity, right-aligned */
            if (sgl_filedialog_is_root(fd)) {
                sgl_filedialog_draw_dev_capacity(fd, surf, &list_area,
                                                 obj->coords.x2 - card_hpad - card_radius,
                                                 text_y, idx);
            }
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
        if (evt->pos.y <= (obj->coords.y1 + bar_h) &&
            evt->pos.x >= (obj->coords.x2 - obj->border - bar_h)) {
            sgl_filedialog_finish(fd, NULL);
            break;
        }

        /* OK button on the bottom bar */
        sgl_area_t ok_rect;
        sgl_filedialog_ok_rect(fd, obj, list_y2, &ok_rect);
        if (sgl_filedialog_pos_in_area(&ok_rect, evt->pos.x, evt->pos.y)) {
            sgl_filedialog_confirm(fd);
            break;
        }

        /* entry rows: map the touch y to the row it lands on (rows are laid
         * out from rows_y1, offset by the scroll position) */
        if (evt->pos.y <= list_y1 || evt->pos.y > list_y2) break;
        const int16_t item_area_top = (int16_t)(rows_y1 - (int16_t)fd->sc.offset);
        if (evt->pos.y < item_area_top) break;

        int16_t clicked_index = (int16_t)((evt->pos.y - item_area_top) / item_height);
        sgl_filedialog_activate_index(fd, obj, clicked_index);
    }
    break;

    case SGL_EVENT_KEY_LEFT:
    case SGL_EVENT_KEY_DOWN: {
        /* with nothing selected yet, the first key press starts at the top */
        const int16_t target = (fd->item_selected < 0)
                               ? 0 : (int16_t)(fd->item_selected + 1);
        if (target < (int16_t)fd->item_num && target != fd->item_selected) {
            sgl_filedialog_select_index(fd, target);
            sgl_scroll_anim_stop(&fd->sc);
            sgl_filedialog_ensure_selected_visible(fd, obj, item_height, list_h);
            sgl_scroll_bar_wake(&fd->sc);
            sgl_obj_set_dirty(obj);
        }
    }
    break;

    case SGL_EVENT_KEY_RIGHT:
    case SGL_EVENT_KEY_UP: {
        const int16_t target = (fd->item_selected < 0)
                               ? 0 : (int16_t)(fd->item_selected - 1);
        if (target >= 0 && target != fd->item_selected) {
            sgl_filedialog_select_index(fd, target);
            sgl_scroll_anim_stop(&fd->sc);
            sgl_filedialog_ensure_selected_visible(fd, obj, item_height, list_h);
            sgl_scroll_bar_wake(&fd->sc);
            sgl_obj_set_dirty(obj);
        }
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

static sgl_obj_t *sgl_filedialog_create(sgl_obj_t *parent, const char *dev_name, uint8_t mode)
{
    sgl_filedialog_t *fd = sgl_malloc(sizeof(sgl_filedialog_t));
    int16_t w, h;
    const char *mount;

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
    sgl_snprintf(fd->root_text, sizeof(fd->root_text), "%s", SGL_FILEDIALOG_ROOT_TEXT);
    fd->font                = sgl_get_system_font();
    fd->alpha               = SGL_FILEDIALOG_ALPHA;
    fd->bg_color            = SGL_FILEDIALOG_COLOR;
    fd->item_selected_color = sgl_color_mixer(SGL_FILEDIALOG_COLOR, SGL_FILEDIALOG_BORDER_COLOR, 128);
    fd->border_color        = SGL_FILEDIALOG_BORDER_COLOR;
    fd->item_text_color     = SGL_FILEDIALOG_TEXT_COLOR;
    fd->icon_color          = SGL_FILEDIALOG_ICON_COLOR;
    fd->path_color          = SGL_COLOR_WHEAT;
    fd->btn_color           = SGL_FILEDIALOG_BTN_COLOR;
    fd->btn_disabled_color  = sgl_color_mixer(SGL_FILEDIALOG_COLOR, SGL_FILEDIALOG_TEXT_COLOR, 64);
    /* rounded row cards derived from the dialog accent (same scheme as sgl_menu) */
    fd->card_color          = sgl_color_mixer(fd->btn_color, fd->bg_color, 26);
    fd->card_border_color   = sgl_color_mixer(fd->btn_color, fd->bg_color, 80);
    fd->sel_border_color    = fd->btn_color;
    fd->card_radius         = SGL_FILEDIALOG_CARD_RADIUS;
    fd->card_border_w       = SGL_FILEDIALOG_CARD_BORDER_W;
    fd->card_hpad           = SGL_FILEDIALOG_CARD_HPAD;
    fd->item_selected       = -1;
    fd->cache_start_index   = -1;
    fd->dir_handle          = -1;
    fd->sc.bar_alpha        = 200;
    sgl_scroll_reset(&fd->sc);

    /* default geometry: centered 3/4 of the screen; the caller may override
     * with sgl_obj_set_pos()/sgl_obj_set_size() */
    w = (int16_t)(SGL_SCREEN_WIDTH * 3 / 4);
    h = (int16_t)(SGL_SCREEN_HEIGHT * 3 / 4);
    sgl_obj_set_size(&fd->obj, w, h);
    sgl_obj_set_pos(&fd->obj,
                    (int16_t)((SGL_SCREEN_WIDTH - w) / 2),
                    (int16_t)((SGL_SCREEN_HEIGHT - h) / 2));

    /* dev_name == NULL (or unmounted) opens on the virtual "/" root, which
     * lists every mounted block device, same as the file browser */
    mount = (dev_name != NULL) ? sgl_fs_mount_find(dev_name) : NULL;
    if (dev_name != NULL && mount == NULL) {
        SGL_LOG_WARN("sgl_filedialog: device '%s' not mounted, showing device list",
                     dev_name);
    }
    sgl_filedialog_load_dir(fd, mount != NULL ? mount : "/");

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
 * @brief Set the label shown in the title bar on the virtual "/" root
 */
void sgl_filedialog_set_root_text(sgl_obj_t *obj, const char *text)
{
    sgl_filedialog_t *fd = sgl_container_of(obj, sgl_filedialog_t, obj);

    if (text == NULL || text[0] == '\0') text = SGL_FILEDIALOG_ROOT_TEXT;
    strncpy(fd->root_text, text, sizeof(fd->root_text) - 1);
    fd->root_text[sizeof(fd->root_text) - 1] = '\0';
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the text font
 */
void sgl_filedialog_set_text_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_filedialog_t *fd = sgl_container_of(obj, sgl_filedialog_t, obj);
    fd->font = font;
    /* Icon widths depend on the font actually used to draw them. */
    sgl_filedialog_refresh_icon_widths(fd);
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
    if (fd->selected_item.text[0] != '\0') {
        sgl_filedialog_resolve_item_icon(fd, &fd->selected_item);
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

/**
 * @brief Set the rounded card style of the list rows (sgl_menu-like cards)
 */
void sgl_filedialog_set_card_style(sgl_obj_t *obj, sgl_color_t card_color, sgl_color_t card_border_color, int16_t radius)
{
    sgl_filedialog_t *fd = sgl_container_of(obj, sgl_filedialog_t, obj);
    fd->card_color        = card_color;
    fd->card_border_color = card_border_color;
    fd->card_radius       = radius;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the selection highlight style of the rounded cards
 */
void sgl_filedialog_set_sel_style(sgl_obj_t *obj, sgl_color_t sel_color, sgl_color_t sel_border_color, uint8_t border_width)
{
    sgl_filedialog_t *fd = sgl_container_of(obj, sgl_filedialog_t, obj);
    fd->item_selected_color = sel_color;
    fd->sel_border_color    = sel_border_color;
    fd->card_border_w       = border_width;
    sgl_obj_set_dirty(obj);
}
