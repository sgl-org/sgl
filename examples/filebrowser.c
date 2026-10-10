/* sgl/examples/filebrowser.c
 *
 * File browser example.
 *
 * Two simulated block devices are mounted with RAMFS ("/flash" and "/sd"),
 * seeded with sample files, then two sgl_filebrowser widgets each point at a
 * device's root via sgl_filebrowser_set_root().
 */
#include <sgl.h>
#include <string.h>

/* Simulated block devices. RAMFS ignores the IO callbacks; it only needs a
 * distinct, stable pointer to identify each "disk". */
static sgl_block_dev_t g_flash_dev;
static sgl_block_dev_t g_sd_dev;

static void seed_file(const char *path, const char *content)
{
    int fd = sgl_fs_open(path, SGL_O_RDWR | SGL_O_CREAT | SGL_O_TRUNC);
    if (fd < 0) {
        SGL_LOG_ERROR("filebrowser example: seed %s failed", path);
        return;
    }
    sgl_fs_write(fd, content, (uint32_t)strlen(content));
    sgl_fs_close(fd);
}

static void seed_demo_content(void)
{
    /* /flash: fake NOR flash */
    sgl_fs_mkdir("/flash/docs");
    seed_file("/flash/readme.txt", "SGL filebrowser demo on /flash");
    seed_file("/flash/boot.ini",  "log=0 level=info");
    seed_file("/flash/docs/note.md", "hello from the docs directory");

    /* /sd: fake SD card */
    sgl_fs_mkdir("/sd/music");
    seed_file("/sd/track01.mp3", "fake mp3 data 1");
    seed_file("/sd/track02.mp3", "fake mp3 data 2");
    seed_file("/sd/music/loop.wav", "fake wav data");
}

void sgl_filebrowser_examples(sgl_obj_t *parent)
{
    sgl_obj_t *fb_flash;
    sgl_obj_t *fb_sd;

    /* 1. mount one filesystem per block device */
    sgl_ramfs_register();   /* ignore "already registered" */
    if (sgl_fs_mount("/flash", "ramfs", &g_flash_dev, NULL) != 0) {
        SGL_LOG_ERROR("filebrowser example: mount /flash failed");
        return;
    }
    if (sgl_fs_mount("/sd", "ramfs", &g_sd_dev, NULL) != 0) {
        SGL_LOG_ERROR("filebrowser example: mount /sd failed");
        return;
    }
    seed_demo_content();

    /* 2. one browser per device, opened at the device's mount point.
     *    Placed on the right half so the avi demo can keep the left. */
    fb_flash = sgl_filebrowser_create(parent);
    sgl_obj_set_pos(fb_flash, 380, 20);
    sgl_obj_set_size(fb_flash, 200, 260);
    sgl_filebrowser_set_text_font(fb_flash, &consolas14);
    sgl_filebrowser_set_path_prefix(fb_flash, "FLASH:");
    sgl_filebrowser_set_root(fb_flash, "/flash");

    fb_sd = sgl_filebrowser_create(parent);
    sgl_obj_set_pos(fb_sd, 595, 20);
    sgl_obj_set_size(fb_sd, 190, 260);
    sgl_filebrowser_set_text_font(fb_sd, &consolas14);
    sgl_filebrowser_set_path_prefix(fb_sd, "SD:");
    sgl_filebrowser_set_root(fb_sd, "/sd");
}
