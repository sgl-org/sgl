/* source/fs/ramfs/sgl_ramfs.c
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
#include "sgl_ramfs.h"
#include <string.h>
#include <sgl_mm.h>

#define RAMFS_FS_NAME               "ramfs"

#define RAMFS_OK                    SGL_FS_OK
#define RAMFS_ERR_NOT_FOUND         SGL_FS_NOT_FOUND
#define RAMFS_ERR_INVALID           SGL_FS_INVALID_ARGUMENT
#define RAMFS_ERR_DENIED            SGL_FS_PERMISSION_DENIED
#define RAMFS_ERR_NO_MEM            SGL_FS_NO_MEMORY
#define RAMFS_ERR_NO_SPACE          SGL_FS_NO_SPACE
#define RAMFS_ERR_EXIST             SGL_FS_EXISTS
#define RAMFS_ERR_BADF              SGL_FS_INVALID_ARGUMENT
#define RAMFS_ERR_NOT_EMPTY         SGL_FS_ERROR

#define RAMFS_MAX_OPEN_FILES        8
#define RAMFS_MAX_OPEN_DIRS         4

typedef struct ramfs_node {
    struct ramfs_node *parent;
    struct ramfs_node *child;
    struct ramfs_node *next;
    char name[SGL_RAMFS_MAX_NAME_LEN + 1];
    uint32_t type;
    uint8_t *data;
    uint32_t size;
    uint32_t capacity;
} ramfs_node_t;

typedef struct {
    uint8_t used;
    uint32_t pos;
    uint32_t flags;
    ramfs_node_t *node;
} ramfs_file_t;

typedef struct {
    uint8_t used;
    ramfs_node_t *dir;
    uint32_t index;
} ramfs_dir_t;

typedef struct {
    ramfs_node_t root;
    uint32_t max_size;
    uint32_t used_size;
    ramfs_file_t files[RAMFS_MAX_OPEN_FILES];
    ramfs_dir_t dirs[RAMFS_MAX_OPEN_DIRS];
} ramfs_ctx_t;

static const char *ramfs_skip_slash(const char *path)
{
    while (path && *path == '/') path++;
    return path;
}

static uint32_t ramfs_name_len(const char *path)
{
    uint32_t len = 0;
    while (path[len] != '\0' && path[len] != '/') len++;
    return len;
}

static int ramfs_name_equal(const char *name, const char *path, uint32_t len)
{
    return strlen(name) == len && strncmp(name, path, len) == 0;
}

static ramfs_node_t *ramfs_find_child(ramfs_node_t *dir, const char *name, uint32_t len)
{
    if (!dir || dir->type != SGL_S_IFDIR) return NULL;
    ramfs_node_t *node = dir->child;
    while (node) {
        if (ramfs_name_equal(node->name, name, len)) return node;
        node = node->next;
    }
    return NULL;
}

static int ramfs_lookup(ramfs_ctx_t *ctx, const char *path, ramfs_node_t **node)
{
    if (!ctx || !path || !node) return RAMFS_ERR_INVALID;

    path = ramfs_skip_slash(path);
    if (*path == '\0') {
        *node = &ctx->root;
        return RAMFS_OK;
    }

    ramfs_node_t *cur = &ctx->root;
    while (*path != '\0') {
        uint32_t len = ramfs_name_len(path);
        if (len == 0 || len > SGL_RAMFS_MAX_NAME_LEN) return RAMFS_ERR_INVALID;

        cur = ramfs_find_child(cur, path, len);
        if (!cur) return RAMFS_ERR_NOT_FOUND;

        path += len;
        path = ramfs_skip_slash(path);
    }

    *node = cur;
    return RAMFS_OK;
}

static int ramfs_lookup_parent(ramfs_ctx_t *ctx, const char *path, ramfs_node_t **parent,
                              const char **name, uint32_t *name_len)
{
    if (!ctx || !path || !parent || !name || !name_len) return RAMFS_ERR_INVALID;

    path = ramfs_skip_slash(path);
    if (*path == '\0') return RAMFS_ERR_INVALID;

    ramfs_node_t *cur = &ctx->root;
    while (1) {
        uint32_t len = ramfs_name_len(path);
        if (len == 0 || len > SGL_RAMFS_MAX_NAME_LEN) return RAMFS_ERR_INVALID;

        const char *next = ramfs_skip_slash(path + len);
        if (*next == '\0') {
            *parent = cur;
            *name = path;
            *name_len = len;
            return RAMFS_OK;
        }

        cur = ramfs_find_child(cur, path, len);
        if (!cur) return RAMFS_ERR_NOT_FOUND;
        if (cur->type != SGL_S_IFDIR) return RAMFS_ERR_INVALID;
        path = next;
    }
}

static ramfs_node_t *ramfs_create_node(ramfs_node_t *parent, const char *name, uint32_t name_len, uint32_t type)
{
    ramfs_node_t *node = (ramfs_node_t *)sgl_malloc(sizeof(ramfs_node_t));
    if (!node) return NULL;

    memset(node, 0, sizeof(*node));
    node->parent = parent;
    node->type = type;
    memcpy(node->name, name, name_len);
    node->name[name_len] = '\0';

    if (parent->child == NULL) {
        parent->child = node;
    }
    else {
        ramfs_node_t *tail = parent->child;
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = node;
    }
    return node;
}

static ramfs_node_t *ramfs_get_child_by_index(ramfs_node_t *dir, uint32_t index)
{
    if (!dir || dir->type != SGL_S_IFDIR) return NULL;

    ramfs_node_t *node = dir->child;
    while (node && index > 0) {
        node = node->next;
        index--;
    }
    return node;
}

static int ramfs_is_ancestor(ramfs_node_t *ancestor, ramfs_node_t *node)
{
    while (node) {
        if (node == ancestor) return 1;
        node = node->parent;
    }
    return 0;
}

static int ramfs_has_open_dir(ramfs_ctx_t *ctx, ramfs_node_t *dir)
{
    for (int i = 0; i < RAMFS_MAX_OPEN_DIRS; i++) {
        if (ctx->dirs[i].used && ctx->dirs[i].dir == dir) return 1;
    }
    return 0;
}

static void ramfs_unlink_node(ramfs_node_t *node)
{
    ramfs_node_t **link = &node->parent->child;
    while (*link) {
        if (*link == node) {
            *link = node->next;
            node->next = NULL;
            node->parent = NULL;
            return;
        }
        link = &(*link)->next;
    }
}

static void ramfs_free_node(ramfs_node_t *node)
{
    if (!node) return;

    ramfs_node_t *child = node->child;
    while (child) {
        ramfs_node_t *next = child->next;
        ramfs_free_node(child);
        child = next;
    }

    if (node->data) sgl_free(node->data);
    sgl_free(node);
}

static int ramfs_resize_file(ramfs_ctx_t *ctx, ramfs_node_t *node, uint32_t new_size)
{
    if (!ctx || !node || node->type != SGL_S_IFREG) return RAMFS_ERR_INVALID;
    if (ctx->used_size < node->size) return RAMFS_ERR_INVALID;

    uint32_t used_without_file = ctx->used_size - node->size;
    if (new_size > 0xFFFFFFFFUL - used_without_file) return RAMFS_ERR_NO_SPACE;

    uint32_t new_used = used_without_file + new_size;
    if (ctx->max_size != 0 && new_used > ctx->max_size) return RAMFS_ERR_NO_SPACE;

    if (new_size <= node->capacity) {
        if (new_size > node->size) memset(node->data + node->size, 0, new_size - node->size);
        node->size = new_size;
        ctx->used_size = new_used;
        return RAMFS_OK;
    }

    uint32_t new_capacity = node->capacity ? node->capacity : 16;
    while (new_capacity < new_size) {
        if (new_capacity > 0x80000000UL) return RAMFS_ERR_NO_SPACE;
        new_capacity *= 2;
    }

    uint8_t *data = (uint8_t *)sgl_realloc(node->data, new_capacity);
    if (!data) return RAMFS_ERR_NO_MEM;

    memset(data + node->size, 0, new_size - node->size);
    node->data = data;
    node->capacity = new_capacity;
    node->size = new_size;
    ctx->used_size = new_used;
    return RAMFS_OK;
}

static int ramfs_mount(void **fs, sgl_block_dev_t *dev, const char *mount_point, void *fs_config)
{
    (void)dev;
    (void)mount_point;

    if (!fs) return RAMFS_ERR_INVALID;

    ramfs_ctx_t *ctx = (ramfs_ctx_t *)sgl_malloc(sizeof(ramfs_ctx_t));
    if (!ctx) return RAMFS_ERR_NO_MEM;

    memset(ctx, 0, sizeof(*ctx));
    strcpy(ctx->root.name, "/");
    ctx->root.type = SGL_S_IFDIR;

    if (fs_config) {
        sgl_ramfs_config_t *cfg = (sgl_ramfs_config_t *)fs_config;
        ctx->max_size = cfg->max_size;
    }

    *fs = ctx;
    return RAMFS_OK;
}

static int ramfs_unmount(void *fs, const char *mount_point)
{
    (void)mount_point;
    if (!fs) return RAMFS_ERR_INVALID;

    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;
    ramfs_node_t *child = ctx->root.child;
    while (child) {
        ramfs_node_t *next = child->next;
        ramfs_free_node(child);
        child = next;
    }
    sgl_free(ctx);
    return RAMFS_OK;
}

static int ramfs_open(void *fs, const char *path, uint32_t flags)
{
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;
    if (!ctx || !path) return RAMFS_ERR_INVALID;

    int slot = -1;
    for (int i = 0; i < RAMFS_MAX_OPEN_FILES; i++) {
        if (!ctx->files[i].used) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return RAMFS_ERR_BADF;

    ramfs_node_t *node = NULL;
    int ret = ramfs_lookup(ctx, path, &node);
    if (ret == RAMFS_ERR_NOT_FOUND && (flags & SGL_O_CREAT)) {
        ramfs_node_t *parent;
        const char *name;
        uint32_t name_len;
        ret = ramfs_lookup_parent(ctx, path, &parent, &name, &name_len);
        if (ret != RAMFS_OK) return ret;
        if (ramfs_find_child(parent, name, name_len)) return RAMFS_ERR_EXIST;
        node = ramfs_create_node(parent, name, name_len, SGL_S_IFREG);
        if (!node) return RAMFS_ERR_NO_MEM;
        ret = RAMFS_OK;
    }

    if (ret != RAMFS_OK) return ret;
    if (node->type != SGL_S_IFREG) return RAMFS_ERR_INVALID;
    if ((flags & SGL_O_TRUNC) && ((flags & 0x03) != SGL_O_RDONLY)) {
        ret = ramfs_resize_file(ctx, node, 0);
        if (ret != RAMFS_OK) return ret;
    }

    ctx->files[slot].used = 1;
    ctx->files[slot].node = node;
    ctx->files[slot].flags = flags;
    ctx->files[slot].pos = (flags & SGL_O_APPEND) ? node->size : 0;
    return slot;
}

static int ramfs_close(void *fs, int fd)
{
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;
    if (!ctx || fd < 0 || fd >= RAMFS_MAX_OPEN_FILES || !ctx->files[fd].used) return RAMFS_ERR_BADF;

    ctx->files[fd].used = 0;
    ctx->files[fd].node = NULL;
    ctx->files[fd].pos = 0;
    ctx->files[fd].flags = 0;
    return RAMFS_OK;
}

static int ramfs_read(void *fs, int fd, void *buffer, uint32_t count)
{
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;
    if (!ctx || fd < 0 || fd >= RAMFS_MAX_OPEN_FILES || !ctx->files[fd].used) return RAMFS_ERR_BADF;
    if (!buffer || count == 0) return 0;

    ramfs_file_t *file = &ctx->files[fd];
    if ((file->flags & 0x03) == SGL_O_WRONLY) return RAMFS_ERR_DENIED;

    ramfs_node_t *node = file->node;
    if (file->pos >= node->size) return 0;

    uint32_t left = node->size - file->pos;
    uint32_t read_size = count < left ? count : left;
    memcpy(buffer, node->data + file->pos, read_size);
    file->pos += read_size;
    return (int)read_size;
}

static int ramfs_write(void *fs, int fd, const void *buffer, uint32_t count)
{
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;
    if (!ctx || fd < 0 || fd >= RAMFS_MAX_OPEN_FILES || !ctx->files[fd].used) return RAMFS_ERR_BADF;
    if (!buffer || count == 0) return 0;

    ramfs_file_t *file = &ctx->files[fd];
    if ((file->flags & SGL_O_WRONLY) == 0 && (file->flags & SGL_O_RDWR) == 0) return RAMFS_ERR_DENIED;

    ramfs_node_t *node = file->node;
    if (file->flags & SGL_O_APPEND) file->pos = node->size;
    if (file->pos > 0xFFFFFFFFUL - count) return RAMFS_ERR_NO_SPACE;

    uint32_t end = file->pos + count;
    int ret = ramfs_resize_file(ctx, node, end);
    if (ret != RAMFS_OK) return ret;

    memcpy(node->data + file->pos, buffer, count);
    file->pos = end;
    return (int)count;
}

static int ramfs_seek(void *fs, int fd, int32_t offset, uint8_t whence)
{
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;
    if (!ctx || fd < 0 || fd >= RAMFS_MAX_OPEN_FILES || !ctx->files[fd].used) return RAMFS_ERR_BADF;

    ramfs_file_t *file = &ctx->files[fd];
    ramfs_node_t *node = file->node;
    int64_t new_pos;

    switch (whence) {
    case SGL_SEEK_SET:
        new_pos = offset;
        break;
    case SGL_SEEK_CUR:
        new_pos = (int64_t)file->pos + offset;
        break;
    case SGL_SEEK_END:
        new_pos = (int64_t)node->size + offset;
        break;
    default:
        return RAMFS_ERR_INVALID;
    }

    if (new_pos < 0) return RAMFS_ERR_INVALID;

    file->pos = (uint32_t)new_pos;
    return (int)file->pos;
}

static int ramfs_opendir(void *fs, const char *path, int *dd)
{
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;
    if (!ctx || !path || !dd) return RAMFS_ERR_INVALID;

    ramfs_node_t *node;
    int ret = ramfs_lookup(ctx, path, &node);
    if (ret != RAMFS_OK) return ret;
    if (node->type != SGL_S_IFDIR) return RAMFS_ERR_INVALID;

    for (int i = 0; i < RAMFS_MAX_OPEN_DIRS; i++) {
        if (!ctx->dirs[i].used) {
            ctx->dirs[i].used = 1;
            ctx->dirs[i].dir = node;
            ctx->dirs[i].index = 0;
            *dd = i;
            return RAMFS_OK;
        }
    }

    return RAMFS_ERR_BADF;
}

static int ramfs_readdir(void *fs, int dd, char *name, uint32_t name_size, uint32_t *type)
{
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;
    if (!ctx || dd < 0 || dd >= RAMFS_MAX_OPEN_DIRS || !ctx->dirs[dd].used) return RAMFS_ERR_BADF;

    ramfs_node_t *node = ramfs_get_child_by_index(ctx->dirs[dd].dir, ctx->dirs[dd].index);
    if (!node) return 0;

    if (name && name_size > 0) {
        uint32_t len = (uint32_t)strlen(node->name);
        uint32_t copy = len < name_size - 1 ? len : name_size - 1;
        memcpy(name, node->name, copy);
        name[copy] = '\0';
    }
    if (type) *type = node->type;
    ctx->dirs[dd].index++;
    return 1;
}

static int ramfs_closedir(void *fs, int dd)
{
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;
    if (!ctx || dd < 0 || dd >= RAMFS_MAX_OPEN_DIRS || !ctx->dirs[dd].used) return RAMFS_ERR_BADF;

    ctx->dirs[dd].used = 0;
    ctx->dirs[dd].dir = NULL;
    ctx->dirs[dd].index = 0;
    return RAMFS_OK;
}

static int ramfs_sync(void *fs)
{
    (void)fs;
    return RAMFS_OK;
}

static int ramfs_format(void *fs)
{
    if (!fs) return RAMFS_ERR_INVALID;
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;

    ramfs_node_t *child = ctx->root.child;
    while (child) {
        ramfs_node_t *next = child->next;
        ramfs_free_node(child);
        child = next;
    }
    ctx->root.child = NULL;
    ctx->used_size = 0;
    memset(ctx->files, 0, sizeof(ctx->files));
    memset(ctx->dirs, 0, sizeof(ctx->dirs));
    return RAMFS_OK;
}

static int ramfs_remove(void *fs, const char *path)
{
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;
    if (!ctx || !path) return RAMFS_ERR_INVALID;

    ramfs_node_t *node;
    int ret = ramfs_lookup(ctx, path, &node);
    if (ret != RAMFS_OK) return ret;
    if (node == &ctx->root) return RAMFS_ERR_DENIED;
    if (node->type == SGL_S_IFDIR && node->child) return RAMFS_ERR_NOT_EMPTY;
    if (node->type == SGL_S_IFDIR && ramfs_has_open_dir(ctx, node)) return RAMFS_ERR_DENIED;

    for (int i = 0; i < RAMFS_MAX_OPEN_FILES; i++) {
        if (ctx->files[i].used && ctx->files[i].node == node) return RAMFS_ERR_DENIED;
    }

    ctx->used_size -= node->size;
    ramfs_unlink_node(node);
    ramfs_free_node(node);
    return RAMFS_OK;
}

static int ramfs_mkdir(void *fs, const char *path)
{
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;
    if (!ctx || !path) return RAMFS_ERR_INVALID;

    ramfs_node_t *parent;
    const char *name;
    uint32_t name_len;
    int ret = ramfs_lookup_parent(ctx, path, &parent, &name, &name_len);
    if (ret != RAMFS_OK) return ret;
    if (ramfs_find_child(parent, name, name_len)) return RAMFS_ERR_EXIST;

    ramfs_node_t *node = ramfs_create_node(parent, name, name_len, SGL_S_IFDIR);
    return node ? RAMFS_OK : RAMFS_ERR_NO_MEM;
}

static int ramfs_stat(void *fs, const char *path, sgl_stat_t *st)
{
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;
    if (!ctx || !path || !st) return RAMFS_ERR_INVALID;

    ramfs_node_t *node;
    int ret = ramfs_lookup(ctx, path, &node);
    if (ret != RAMFS_OK) return ret;

    st->st_mode = node->type;
    st->st_size = node->size;
    st->st_mtime = 0;
    return RAMFS_OK;
}

static int ramfs_rename(void *fs, const char *old_path, const char *new_path)
{
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;
    if (!ctx || !old_path || !new_path) return RAMFS_ERR_INVALID;

    ramfs_node_t *node;
    int ret = ramfs_lookup(ctx, old_path, &node);
    if (ret != RAMFS_OK) return ret;
    if (node == &ctx->root) return RAMFS_ERR_DENIED;

    ramfs_node_t *new_parent;
    const char *new_name;
    uint32_t new_name_len;
    ret = ramfs_lookup_parent(ctx, new_path, &new_parent, &new_name, &new_name_len);
    if (ret != RAMFS_OK) return ret;
    if (ramfs_find_child(new_parent, new_name, new_name_len)) return RAMFS_ERR_EXIST;
    if (node->type == SGL_S_IFDIR && ramfs_is_ancestor(node, new_parent)) return RAMFS_ERR_DENIED;

    ramfs_unlink_node(node);
    node->parent = new_parent;
    node->next = new_parent->child;
    new_parent->child = node;
    memcpy(node->name, new_name, new_name_len);
    node->name[new_name_len] = '\0';
    return RAMFS_OK;
}

static int ramfs_statvfs(void *fs, sgl_statvfs_t *info)
{
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs;
    if (!ctx || !info) return RAMFS_ERR_INVALID;

    memset(info, 0, sizeof(*info));
    /* RAMFS uses 1-byte blocks for simplicity */
    info->f_bsize = 1;
    if (ctx->max_size == 0) {
        info->f_blocks = 0;
        info->f_bfree = 0;
    } else {
        info->f_blocks = ctx->max_size;
        info->f_bfree = ctx->max_size > ctx->used_size ? ctx->max_size - ctx->used_size : 0;
    }
    info->f_bavail = info->f_bfree;
    info->f_files = 0;
    info->f_ffree = 0;

    return RAMFS_OK;
}

static sgl_fs_ops_t ramfs_ops = {
    .mount = ramfs_mount,
    .unmount = ramfs_unmount,
    .open = ramfs_open,
    .close = ramfs_close,
    .read = ramfs_read,
    .write = ramfs_write,
    .seek = ramfs_seek,
    .opendir = ramfs_opendir,
    .readdir = ramfs_readdir,
    .closedir = ramfs_closedir,
    .sync = ramfs_sync,
    .format = ramfs_format,
    .remove = ramfs_remove,
    .mkdir = ramfs_mkdir,
    .stat = ramfs_stat,
    .rename = ramfs_rename,
    .statvfs = ramfs_statvfs,
};

static sgl_fs_type_t ramfs_type = {
    .name = RAMFS_FS_NAME,
    .ops = &ramfs_ops,
};

/**
 * @brief Register RAMFS file system type with sgl_fs VFS.
 *        Call this once before sgl_fs_mount("ramfs", ...).
 * @return 0 on success, -1 on failure
 */
int sgl_ramfs_register(void)
{
    return sgl_fs_register(&ramfs_type);
}

/* Default RAMFS configuration */
sgl_ramfs_config_t ramfs_cfg = {
    .max_size = 1024 * 1024 * 10, // 10 MB
};
