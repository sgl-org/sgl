/* source/core/sgl_fs.c
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
#include <sgl_fs.h>
#include <string.h>
#include <sgl_mm.h>

/**
 * @brief File system context data
 */
static SGL_LIST_HEAD(g_fs_type_list);
static SGL_LIST_HEAD(g_mount_list);
static sgl_fd_ctrl_t g_fd_table[SGL_MAX_FD];
static sgl_dd_ctrl_t g_dd_table[SGL_MAX_DD];

#define FS_LOG_TAG    "fs: "

/**
 * @brief Find a file system type by name
 * @param name File system type name
 * @return File system type pointer, or NULL if not found
 */
static sgl_fs_type_t* find_fs_type(const char *name)
{
    sgl_fs_type_t *pos, *tmp;
    sgl_list_for_each_entry_safe (pos, tmp, &g_fs_type_list, sgl_fs_type_t, node) {
        if (strcmp(pos->name, name) == 0) {
            return pos;
        }
    }
    return NULL;
}

/**
 * @brief Register a file system type
 * @param fs_type File system type pointer
 * @return 0 on success, -1 on failure
 */
int sgl_fs_register(sgl_fs_type_t *fs_type)
{
    if (!fs_type || !fs_type->name || !fs_type->ops) {
        SGL_LOG_ERROR(FS_LOG_TAG"invalid fs type");
        return -1;
    }

    if (find_fs_type(fs_type->name) != NULL) {
        SGL_LOG_ERROR(FS_LOG_TAG"fs type %s already registered", fs_type->name);
        return -1;
    }

    sgl_list_add_node_at_tail(&g_fs_type_list, &fs_type->node);
    return 0;
}

/**
 * @brief Mount a file system
 * @param mount_point Mount point path
 * @param fs_name File system name
 * @param dev Block device pointer
 * @param fs_config File system configuration pointer
 * @return 0 on success, -1 on failure
 */
int sgl_fs_mount(const char *mount_point, const char *fs_name, sgl_block_dev_t *dev, void *fs_config)
{
    sgl_fs_type_t *type = find_fs_type(fs_name);
    if (!type) {
        SGL_LOG_ERROR(FS_LOG_TAG"fs type %s not registered", fs_name);
        return -1;
    }

    sgl_mount_point_t *mp = (sgl_mount_point_t *)sgl_malloc(sizeof(sgl_mount_point_t));
    if (!mp) {
        SGL_LOG_ERROR(FS_LOG_TAG"failed to allocate memory for mount point");
        return -1;
    }

    mp->path = mount_point;
    mp->dev = dev;
    mp->fs_type = type;
    mp->fs_data = NULL;

    void *fs = NULL;
    int ret = type->ops->mount(&fs, dev, mount_point, fs_config);
    if (ret != 0) {
        sgl_free(mp);
        return ret;
    }
    mp->fs_data = fs;

    sgl_list_add_node_at_tail(&g_mount_list, &mp->node);
    SGL_LOG_INFO(FS_LOG_TAG"mounted %s at '%s' successfully", fs_name, mount_point);
    return 0;
}

/**
 * @brief Resolve a path to a mount point
 * @param path Path to resolve
 * @param rel_path Pointer to store the relative path
 * @return Mount point pointer, or NULL if not found
 */
static sgl_mount_point_t* resolve_path(const char *path, const char **rel_path)
{
    sgl_mount_point_t *best_mp = NULL;
    size_t best_len = 0;
    sgl_mount_point_t *pos, *tmp;

    if (!path || !rel_path) return NULL;

    sgl_list_for_each_entry_safe(pos, tmp, &g_mount_list, sgl_mount_point_t, node) {
        size_t len = strlen(pos->path);
        if (strncmp(path, pos->path, len) == 0) {
            if (path[len] == '\0' || path[len] == '/' || pos->path[len-1] == '/') {
                if (len > best_len) {
                    best_len = len;
                    best_mp = pos;
                }
            }
        }
    }

    if (best_mp && rel_path) {
        *rel_path = path + best_len;
        if (**rel_path == '\0') *rel_path = "/";
    }
    return best_mp;
}

/**
 * @brief Open a file
 * @param path Path to open
 * @param flags Open flags
 * @return File descriptor, or -1 on failure
 */
int sgl_fs_open(const char *path, uint32_t flags)
{
    const char *rel;
    sgl_mount_point_t *mp = resolve_path(path, &rel);
    if (!mp) {
        SGL_LOG_ERROR(FS_LOG_TAG"no mount point found for path: %s", path);
        return -1;
    }

    int local_fd = mp->fs_type->ops->open(mp->fs_data, rel, flags);
    if (local_fd < 0) {
        SGL_LOG_ERROR(FS_LOG_TAG"failed to open file: %s", path);
        return local_fd;
    }

    for (int i = 0; i < SGL_MAX_FD; i++) {
        if (!g_fd_table[i].used) {
            g_fd_table[i].used = 1;
            g_fd_table[i].mp = mp;
            g_fd_table[i].local_fd = local_fd;
            return i + SGL_FD_OFFSET;
        }
    }

    mp->fs_type->ops->close(mp->fs_data, local_fd);
    SGL_LOG_ERROR(FS_LOG_TAG"fd table full, max=%d", SGL_MAX_FD);
    return -1;
}

/**
 * @brief Close a file
 * @param fd File descriptor
 * @return 0 on success, -1 on failure
 */
int sgl_fs_close(int fd)
{
    int ret;
    sgl_fd_ctrl_t *ctrl;
    int table_idx = fd - SGL_FD_OFFSET;
    if (table_idx < 0 || table_idx >= SGL_MAX_FD || !g_fd_table[table_idx].used) {
        SGL_LOG_ERROR(FS_LOG_TAG"invalid file descriptor: %d", fd);
        return -1;
    }

    ctrl = &g_fd_table[table_idx];
    ret = ctrl->mp->fs_type->ops->close(ctrl->mp->fs_data, ctrl->local_fd);

    ctrl->used = 0;
    ctrl->mp = NULL;
    ctrl->local_fd = -1;

    return ret;
}

/**
 * @brief Read from a file
 * @param fd File descriptor
 * @param buffer Buffer to read into
 * @param count Number of bytes to read
 * @return Number of bytes read, or -1 on failure
 */
int sgl_fs_read(int fd, void *buffer, uint32_t count)
{
    int table_idx = fd - SGL_FD_OFFSET;
    if (table_idx < 0 || table_idx >= SGL_MAX_FD || !g_fd_table[table_idx].used) {
        SGL_LOG_ERROR(FS_LOG_TAG"invalid file descriptor: %d", fd);
        return -1;
    }

    sgl_fd_ctrl_t *ctrl = &g_fd_table[table_idx];
    return ctrl->mp->fs_type->ops->read(
        ctrl->mp->fs_data,
        ctrl->local_fd, 
        buffer, 
        count
    );
}

/**
 * @brief Write to a file
 * @param fd File descriptor
 * @param buffer Buffer to write from
 * @param count Number of bytes to write
 * @return Number of bytes written, or -1 on failure
 */
int sgl_fs_write(int fd, const void *buffer, uint32_t count)
{
    int table_idx = fd - SGL_FD_OFFSET;
    if (table_idx < 0 || table_idx >= SGL_MAX_FD || !g_fd_table[table_idx].used) {
        SGL_LOG_ERROR(FS_LOG_TAG"invalid file descriptor: %d", fd);
        return -1;
    }

    sgl_fd_ctrl_t *ctrl = &g_fd_table[table_idx];
    return ctrl->mp->fs_type->ops->write(ctrl->mp->fs_data,
                                          ctrl->local_fd, 
                                          buffer, 
                                          count
                                        );
}

/**
 * @brief Get file status
 * @param path Path to get status for
 * @param st Pointer to store status
 * @return 0 on success, -1 on failure
 */
int sgl_fs_stat(const char *path, sgl_stat_t *st)
{
    const char *rel;
    sgl_mount_point_t *mp = resolve_path(path, &rel);
    if (!mp) {
        SGL_LOG_ERROR(FS_LOG_TAG"no mount point found for path: %s", path);
        return -1;
    }
    return mp->fs_type->ops->stat(mp->fs_data, rel, st);
}

/**
 * @brief Open a directory
 * @param path Path to open
 * @param dd Pointer to store directory descriptor
 * @return 0 on success, -1 on failure
 */
int sgl_fs_opendir(const char *path, int *dd)
{
    const char *rel;
    sgl_mount_point_t *mp = resolve_path(path, &rel);
    if (!mp) {
        SGL_LOG_ERROR(FS_LOG_TAG"no mount point found for path: %s", path);
        return -1;
    }
    if (!dd) return -1;

    int local_dd = -1;
    int ret = mp->fs_type->ops->opendir(mp->fs_data, rel, &local_dd);
    if (ret != 0 || local_dd < 0) {
        SGL_LOG_ERROR(FS_LOG_TAG"failed to opendir: %s", path);
        return ret;
    }

    for (int i = 0; i < SGL_MAX_DD; i++) {
        if (!g_dd_table[i].used) {
            g_dd_table[i].used = 1;
            g_dd_table[i].mp = mp;
            g_dd_table[i].local_dd = (int16_t)local_dd;
            *dd = i + SGL_FD_OFFSET;
            return 0;
        }
    }

    mp->fs_type->ops->closedir(mp->fs_data, local_dd);
    SGL_LOG_ERROR(FS_LOG_TAG"directory descriptor table full, max=%d", SGL_MAX_DD);
    return -1;
}

/**
 * @brief Read a directory entry
 * @param dd Directory descriptor
 * @param name Buffer to store name
 * @param name_size Size of name buffer
 * @param type Pointer to store type
 * @return 0 on success, -1 on failure
 */
int sgl_fs_readdir(int dd, char *name, uint32_t name_size, uint32_t *type)
{
    int table_idx = dd - SGL_FD_OFFSET;
    if (table_idx < 0 || table_idx >= SGL_MAX_DD || !g_dd_table[table_idx].used) {
        SGL_LOG_ERROR(FS_LOG_TAG"invalid directory descriptor: %d", dd);
        return -1;
    }

    sgl_dd_ctrl_t *ctrl = &g_dd_table[table_idx];
    return ctrl->mp->fs_type->ops->readdir(
        ctrl->mp->fs_data,
        ctrl->local_dd,
        name,
        name_size,
        type
    );
}

/**
 * @brief Close a directory
 * @param dd Directory descriptor
 * @return 0 on success, -1 on failure
 */
int sgl_fs_closedir(int dd)
{
    int table_idx = dd - SGL_FD_OFFSET;
    if (table_idx < 0 || table_idx >= SGL_MAX_DD || !g_dd_table[table_idx].used) {
        SGL_LOG_ERROR(FS_LOG_TAG"invalid directory descriptor: %d", dd);
        return -1;
    }

    sgl_dd_ctrl_t *ctrl = &g_dd_table[table_idx];
    int ret = ctrl->mp->fs_type->ops->closedir(ctrl->mp->fs_data, ctrl->local_dd);

    ctrl->used = 0;
    ctrl->mp = NULL;
    ctrl->local_dd = -1;

    return ret;
}

/**
 * @brief Unmount a filesystem
 * @param mount_point Mount point to unmount
 * @return 0 on success, -1 on failure
 */
int sgl_fs_unmount(const char *mount_point)
{
    int ret;
    sgl_mount_point_t *pos, *tmp;
    sgl_list_for_each_entry_safe(pos, tmp, &g_mount_list, sgl_mount_point_t, node) {
        if (strcmp(pos->path, mount_point) == 0) {
            for (int i = 0; i < SGL_MAX_FD; i++) {
                if (g_fd_table[i].used && g_fd_table[i].mp == pos) {
                    SGL_LOG_ERROR(FS_LOG_TAG"cannot unmount: fd %d still open on %s", i + SGL_FD_OFFSET, mount_point);
                    return -1;
                }
            }

            ret = pos->fs_type->ops->unmount(pos->fs_data, mount_point);
            sgl_list_del_node(&pos->node);
            sgl_free(pos);
            return ret;
        }
    }
    SGL_LOG_ERROR(FS_LOG_TAG"mount point not found: %s", mount_point);
    return -1;
}

/**
 * @brief Synchronize a filesystem
 * @param path Path to synchronize, or NULL to synchronize all mounted filesystems
 * @return 0 on success, -1 on failure
 */
int sgl_fs_sync(const char *path)
{
    if (path) {
        const char *rel;
        sgl_mount_point_t *mp = resolve_path(path, &rel);
        if (!mp) return -1;
        return mp->fs_type->ops->sync(mp->fs_data);
    }
    else {
        sgl_mount_point_t *pos, *tmp;
        sgl_list_for_each_entry_safe(pos, tmp, &g_mount_list, sgl_mount_point_t, node) {
            pos->fs_type->ops->sync(pos->fs_data);
        }
        return 0;
    }
}

/**
 * @brief Format a filesystem
 * @param mount_point Mount point to format
 * @param fs_config Filesystem configuration
 * @return 0 on success, -1 on failure
 */
int sgl_fs_format(const char *mount_point, void *fs_config)
{
    sgl_mount_point_t *pos, *tmp;
    SGL_UNUSED(fs_config);
    sgl_list_for_each_entry_safe(pos, tmp, &g_mount_list, sgl_mount_point_t, node) {
        if (strcmp(pos->path, mount_point) == 0) {
            return pos->fs_type->ops->format(pos->fs_data);
        }
    }
    SGL_LOG_ERROR(FS_LOG_TAG"mount point not found for format: %s", mount_point);
    return -1;
}

/**
 * @brief Remove a file or directory
 * @param path Path to remove
 * @return 0 on success, -1 on failure
 */
int sgl_fs_remove(const char *path)
{
    const char *rel;
    sgl_mount_point_t *mp = resolve_path(path, &rel);
    if (!mp) {
        SGL_LOG_ERROR(FS_LOG_TAG"no mount point found for remove: %s", path);
        return -1;
    }
    return mp->fs_type->ops->remove(mp->fs_data, rel);
}

/**
 * @brief Create a directory
 * @param path Path to create
 * @return 0 on success, -1 on failure
 */
int sgl_fs_mkdir(const char *path)
{
    const char *rel;
    sgl_mount_point_t *mp = resolve_path(path, &rel);
    if (!mp) {
        SGL_LOG_ERROR(FS_LOG_TAG"no mount point found for mkdir: %s", path);
        return -1;
    }
    return mp->fs_type->ops->mkdir(mp->fs_data, rel);
}

/**
 * @brief Rename a file or directory
 * @param old_path Old path
 * @param new_path New path
 * @return 0 on success, -1 on failure
 */
int sgl_fs_rename(const char *old_path, const char *new_path)
{
    const char *old_rel, *new_rel;
    sgl_mount_point_t *old_mp = resolve_path(old_path, &old_rel);
    sgl_mount_point_t *new_mp = resolve_path(new_path, &new_rel);
    
    if (!old_mp || !new_mp) {
        SGL_LOG_ERROR(FS_LOG_TAG"mount point not found for rename");
        return -1;
    }

    if (old_mp != new_mp) {
        SGL_LOG_ERROR(FS_LOG_TAG"cross-fs rename not supported");
        return -1;
    }

    return old_mp->fs_type->ops->rename(old_mp->fs_data, old_rel, new_rel);
}

/**
 * @brief Get filesystem space information
 * @param path Path within the mounted filesystem
 * @param info Pointer to store filesystem space information
 * @return 0 on success, -1 on failure
 */
int sgl_fs_statvfs(const char *path, sgl_statvfs_t *info)
{
    const char *rel;
    sgl_mount_point_t *mp = resolve_path(path, &rel);
    if (!mp) {
        SGL_LOG_ERROR(FS_LOG_TAG"no mount point found for path: %s", path);
        return -1;
    }

    if (!mp->fs_type->ops->statvfs) {
        SGL_LOG_ERROR(FS_LOG_TAG"statvfs not supported for fs type: %s", mp->fs_type->name);
        return -1;
    }

    return mp->fs_type->ops->statvfs(mp->fs_data, info);
}
