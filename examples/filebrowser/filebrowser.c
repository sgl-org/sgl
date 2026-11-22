/* sgl/examples/filebrowser.c
 *
 * File browser example.
 *
 * Two simulated block devices are mounted with RAMFS ("/flash" and "/sd"),
 * seeded with sample files, then a single sgl_filebrowser widget opens at the
 * virtual "/" root, which lists both devices by name; clicking one enters it.
 */
#include <sgl.h>
#include <string.h>

/* Simulated block devices. RAMFS ignores the IO callbacks; it only needs a
 * distinct, stable pointer to identify each "disk". The info->name is what the
 * file browser shows in its device list -- it can differ from the "/flash" /
 * "/sd" mount paths, just like a product name differs from an internal path. */
static const sgl_block_dev_info_t g_flash_info = { .name = "Flash" };
static const sgl_block_dev_info_t g_sd_info    = { .name = "SD Card" };
static sgl_block_dev_t g_flash_dev = { .info = &g_flash_info };
static sgl_block_dev_t g_sd_dev    = { .info = &g_sd_info };

/* Give each RAMFS a capacity so the browser's device list can show the
 * total / available space next to each device name. */
static sgl_ramfs_config_t g_flash_cfg = { .max_size =  2 * 1024 * 1024 }; /* 2 MB  */
static sgl_ramfs_config_t g_sd_cfg     = { .max_size = 16 * 1024 * 1024 }; /* 16 MB */

/* Key group holding the browser.  Unlike the file dialog the browser is not
 * modal and never destroys itself, so it can stay registered for the whole
 * demo -- no stale pointer to worry about. */
static sgl_key_group_t *g_filebrowser_keys = NULL;

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

/* Row icons: pattern -> glyph, using the SGL_ICON_* strings provided by the
 * file browser header (FontAwesome codepoints drawn with the text font, which
 * therefore must contain the icon glyphs). */
static const sgl_filebrowser_icon_t g_filebrowser_icons[] = {
    { "*",     SGL_ICON_NORMAL_FILE },
    { "..",    SGL_ICON_RETURN_FILE },
    { "dir",   SGL_ICON_DIR_FILE },
    { ".txt",  SGL_ICON_TEXT_FILE },
    { ".md",   SGL_ICON_TEXT_FILE },
    { ".ini",  SGL_ICON_TEXT_FILE },
    { ".log",  SGL_ICON_TEXT_FILE },
    { ".pdf",  SGL_ICON_PDF_FILE },
    { ".doc",  SGL_ICON_WORD_FILE },
    { ".docx", SGL_ICON_WORD_FILE },
    { ".xls",  SGL_ICON_EXCEL_FILE },
    { ".xlsx", SGL_ICON_EXCEL_FILE },
    { ".ppt",  SGL_ICON_PPT_FILE },
    { ".pptx", SGL_ICON_PPT_FILE },
    { ".png",  SGL_ICON_IMAGE_FILE },
    { ".jpg",  SGL_ICON_IMAGE_FILE },
    { ".bmp",  SGL_ICON_IMAGE_FILE },
    { ".zip",  SGL_ICON_ZIP_FILE },
    { ".rar",  SGL_ICON_ZIP_FILE },
    { ".mp3",  SGL_ICON_MUSIC_FILE },
    { ".wav",  SGL_ICON_MUSIC_FILE },
    { ".mp4",  SGL_ICON_VIDEO_FILE },
    { ".avi",  SGL_ICON_VIDEO_FILE },
    { ".c",    SGL_ICON_CPP_FILE },
    { ".h",    SGL_ICON_CPP_FILE },
    { ".cpp",  SGL_ICON_CPP_FILE },
    { NULL,    NULL },
};

void sgl_filebrowser_examples(sgl_obj_t *parent)
{
    /* 1. mount one filesystem per block device */
    sgl_ramfs_register();   /* ignore "already registered" */
    if (sgl_fs_mount("/flash", "ramfs", &g_flash_dev, &g_flash_cfg) != 0) {
        SGL_LOG_ERROR("filebrowser example: mount /flash failed");
        return;
    }

    if (sgl_fs_mount("/sd", "ramfs", &g_sd_dev, &g_sd_cfg) != 0) {
        SGL_LOG_ERROR("filebrowser example: mount /sd failed");
        return;
    }
    seed_demo_content();

    /* 2. one browser opens at the "/" device list; the user clicks a device
     *    ("flash" / "sd") to enter its file system. Placed on the right half
     *    so the avi demo can keep the left. */
    sgl_obj_t *fb = sgl_filebrowser_create(parent);
    sgl_obj_set_pos(fb, 380, 20);
    sgl_obj_set_size(fb, 405, 260);
    sgl_filebrowser_set_text_font(fb, &consolas14);
    sgl_filebrowser_set_icons(fb, g_filebrowser_icons);
    sgl_filebrowser_set_root_text(fb, "Devices");

    /* 3. make the browser reachable by the physical keys: arrow keys move the
     *    highlight, ENTER activates a row (a directory needs two presses: the
     *    first highlights it, the second enters it) */
    g_filebrowser_keys = sgl_key_group_create(1);
    if (g_filebrowser_keys != NULL) {
        sgl_key_group_add_obj(g_filebrowser_keys, fb);
        sgl_key_group_load(g_filebrowser_keys);
    }

    /* 4. on-screen hints for the manual test */
    sgl_obj_t *hint1 = sgl_label_create(parent);
    sgl_obj_set_pos(hint1, 20, 320);
    sgl_obj_set_size(hint1, 340, 20);
    sgl_label_set_font(hint1, &consolas14);
    sgl_label_set_text_color(hint1, sgl_rgb(120, 120, 120));
    sgl_label_set_text(hint1, "mouse: click a device to enter it");

    sgl_obj_t *hint2 = sgl_label_create(parent);
    sgl_obj_set_pos(hint2, 20, 344);
    sgl_obj_set_size(hint2, 340, 20);
    sgl_label_set_font(hint2, &consolas14);
    sgl_label_set_text_color(hint2, sgl_rgb(120, 120, 120));
    sgl_label_set_text(hint2, "keys: arrows move, ENTER activates");
}
