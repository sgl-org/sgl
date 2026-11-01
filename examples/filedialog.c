/* examples/filedialog.c
 *
 * File dialog widget demo: mounts two ramfs volumes as virtual block
 * devices (/sd and /flash), seeds them with a few directories and files,
 * then lets the user pick a file or a directory through sgl_filedialog.
 *
 * The picked path is shown on the status label at the bottom.
 */
#include <sgl.h>
#include <stdio.h>
#include <string.h>

/* Simulated block devices. ramfs ignores the block device internals, but
 * mounting with a non-NULL device registers the mount point in the block
 * device registry, which is exactly what the file dialog enumerates.
 *
 * The dialog's device list shows info->name (not the mount point), so the
 * label can differ from the "/sd" / "/flash" mount paths -- useful when the
 * mount point is an internal path while the device has a product name. */
static const sgl_block_dev_info_t g_filedialog_sd_info    = { .name = "SD Card" };
static const sgl_block_dev_info_t g_filedialog_flash_info = { .name = "Flash" };

static sgl_block_dev_t g_filedialog_dev_sd    = { .info = &g_filedialog_sd_info };
static sgl_block_dev_t g_filedialog_dev_flash = { .info = &g_filedialog_flash_info };

static sgl_obj_t *g_filedialog_status;
static char g_filedialog_status_text[160];

/* Key group for the physical keys.  The dialog is the only member: it is added
 * when it opens and removed again from the confirm callback, which runs right
 * before the widget destroys itself -- leaving it in the group would mean
 * keeping a pointer to freed memory.  With no dialog open the group is empty
 * and the arrow keys are simply inert. */
static sgl_key_group_t *g_filedialog_keys = NULL;

static void sgl_filedialog_example_status(const char *text)
{
    sgl_snprintf(g_filedialog_status_text, sizeof(g_filedialog_status_text),
                 "%s", text != NULL ? text : "");
    sgl_label_set_text(g_filedialog_status, g_filedialog_status_text);
}

static void sgl_filedialog_example_seed(const char *path, const char *content)
{
    int fd = sgl_fs_open(path, SGL_O_WRONLY | SGL_O_CREAT);
    if (fd < 0) {
        SGL_LOG_WARN("filedialog example: cannot create %s", path);
        return;
    }
    sgl_fs_write(fd, content, (uint32_t)strlen(content));
    sgl_fs_close(fd);
}

static void sgl_filedialog_example_confirm(sgl_obj_t *dialog, const char *path, void *user_data)
{
    SGL_UNUSED(user_data);

    /* drop the dialog from the key group before it destroys itself */
    if (g_filedialog_keys != NULL) {
        sgl_key_group_remove_obj(g_filedialog_keys, dialog);
    }

    if (path != NULL) {
        char text[160];
        sgl_snprintf(text, sizeof(text), "picked: %s", path);
        sgl_filedialog_example_status(text);
        SGL_LOG_INFO("filedialog example: picked %s", path);
    } else {
        sgl_filedialog_example_status("cancelled");
        SGL_LOG_INFO("filedialog example: cancelled");
    }
}

/* Row icons: the pattern -> glyph pairs, using the SGL_ICON_* strings that the
 * file browser / file dialog headers provide (FontAwesome codepoints). The
 * glyphs are drawn with the text font, so that font must contain them. */
static const sgl_filebrowser_icon_t g_filedialog_icons[] = {
    { "*",     SGL_ICON_NORMAL_FILE },
    { "..",    SGL_ICON_RETURN_FILE },
    { "dir",   SGL_ICON_DIR_FILE },
    { ".txt",  SGL_ICON_TEXT_FILE },
    { ".ini",  SGL_ICON_TEXT_FILE },
    { ".cfg",  SGL_ICON_TEXT_FILE },
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
    { ".jpeg", SGL_ICON_IMAGE_FILE },
    { ".bmp",  SGL_ICON_IMAGE_FILE },
    { ".zip",  SGL_ICON_ZIP_FILE },
    { ".rar",  SGL_ICON_ZIP_FILE },
    { ".7z",   SGL_ICON_ZIP_FILE },
    { ".mp3",  SGL_ICON_MUSIC_FILE },
    { ".wav",  SGL_ICON_MUSIC_FILE },
    { ".mp4",  SGL_ICON_VIDEO_FILE },
    { ".avi",  SGL_ICON_VIDEO_FILE },
    { ".c",    SGL_ICON_CPP_FILE },
    { ".h",    SGL_ICON_CPP_FILE },
    { ".cpp",  SGL_ICON_CPP_FILE },
    { NULL,    NULL },
};

/* Text font (also used for the icons) + icon table for a freshly opened dialog */
static void sgl_filedialog_example_setup(sgl_obj_t *dlg)
{
    sgl_filedialog_set_text_font(dlg, &consolas14);
    sgl_filedialog_set_icons(dlg, g_filedialog_icons);
    sgl_filedialog_set_confirm_cb(dlg, sgl_filedialog_example_confirm, NULL);

    /* the virtual "/" device list gets a custom title; without this call it
     * would show the built-in default, "DISK" */
    sgl_filedialog_set_root_text(dlg, "Storage");

    /* make the new dialog reachable by the physical keys */
    if (g_filedialog_keys == NULL) {
        g_filedialog_keys = sgl_key_group_create(1);
    }
    if (g_filedialog_keys != NULL) {
        sgl_key_group_add_obj(g_filedialog_keys, dlg);
        sgl_key_group_load(g_filedialog_keys);
    }
}

static void sgl_filedialog_example_open_file_cb(sgl_event_t *event)
{
    if (event->type != SGL_EVENT_CLICKED) return;

    /* dev_name = NULL: the dialog opens on "/", listing the block devices */
    sgl_obj_t *dlg = sgl_filedialog_open_file(NULL, NULL);
    if (dlg == NULL) {
        sgl_filedialog_example_status("dialog alloc failed");
        return;
    }
    sgl_filedialog_example_setup(dlg);
}

static void sgl_filedialog_example_open_dir_cb(sgl_event_t *event)
{
    if (event->type != SGL_EVENT_CLICKED) return;

    sgl_obj_t *dlg = sgl_filedialog_open_dir(NULL, NULL);
    if (dlg == NULL) {
        sgl_filedialog_example_status("dialog alloc failed");
        return;
    }
    sgl_filedialog_example_setup(dlg);
}

static void sgl_filedialog_example_open_sd_cb(sgl_event_t *event)
{
    if (event->type != SGL_EVENT_CLICKED) return;

    /* dev_name given: browse the "sd" device directly, skipping the device list */
    sgl_obj_t *dlg = sgl_filedialog_open_file(NULL, "sd");
    if (dlg == NULL) {
        sgl_filedialog_example_status("dialog alloc failed");
        return;
    }
    sgl_filedialog_example_setup(dlg);
}

void sgl_filedialog_examples(sgl_obj_t *parent)
{
    SGL_UNUSED(parent);

    if (sgl_ramfs_register() != 0) {
        SGL_LOG_WARN("filedialog example: ramfs already registered");
    }
    if (sgl_fs_mount("/sd", "ramfs", &g_filedialog_dev_sd, &ramfs_cfg) != 0) {
        SGL_LOG_WARN("filedialog example: mount /sd failed (already mounted?)");
    }

    if (sgl_fs_mount("/flash", "ramfs", &g_filedialog_dev_flash, &ramfs_cfg) != 0) {
        SGL_LOG_WARN("filedialog example: mount /flash failed (already mounted?)");
    }

    /* seed /sd */
    sgl_fs_mkdir("/sd/music");
    sgl_fs_mkdir("/sd/video");
    sgl_fs_mkdir("/sd/photos");
    sgl_filedialog_example_seed("/sd/readme.txt", "sd card root readme\n");
    sgl_filedialog_example_seed("/sd/movie.avi", "fake avi payload\n");
    sgl_filedialog_example_seed("/sd/music/song1.mp3", "id3 fake\n");
    sgl_filedialog_example_seed("/sd/music/song2.mp3", "id3 fake\n");
    sgl_filedialog_example_seed("/sd/video/clip1.avi", "fake avi payload\n");

    /* seed /flash */
    sgl_fs_mkdir("/flash/config");
    sgl_fs_mkdir("/flash/logs");
    sgl_filedialog_example_seed("/flash/system.ini", "[system]\nboot=fast\n");
    sgl_filedialog_example_seed("/flash/boot.bin", "BOOT");
    sgl_filedialog_example_seed("/flash/config/wifi.cfg", "ssid=demo\n");
    sgl_filedialog_example_seed("/flash/logs/boot.log", "boot ok\n");

    /* buttons */
    sgl_obj_t *btn_file = sgl_button_create(NULL);
    sgl_obj_set_pos(btn_file, 20, 20);
    sgl_obj_set_size(btn_file, 150, 40);
    sgl_button_set_text(btn_file, "Open File...");
    sgl_button_set_font(btn_file, &consolas14);
    sgl_button_set_color(btn_file, sgl_rgb(86, 156, 214));
    sgl_button_set_text_color(btn_file, SGL_COLOR_WHITE);
    sgl_button_set_radius(btn_file, 8);
    sgl_obj_set_event_cb(btn_file, sgl_filedialog_example_open_file_cb, NULL);

    sgl_obj_t *btn_dir = sgl_button_create(NULL);
    sgl_obj_set_pos(btn_dir, 190, 20);
    sgl_obj_set_size(btn_dir, 150, 40);
    sgl_button_set_text(btn_dir, "Open Dir...");
    sgl_button_set_font(btn_dir, &consolas14);
    sgl_button_set_color(btn_dir, sgl_rgb(78, 154, 110));
    sgl_button_set_text_color(btn_dir, SGL_COLOR_WHITE);
    sgl_button_set_radius(btn_dir, 8);
    sgl_obj_set_event_cb(btn_dir, sgl_filedialog_example_open_dir_cb, NULL);

    sgl_obj_t *btn_sd = sgl_button_create(NULL);
    sgl_obj_set_pos(btn_sd, 360, 20);
    sgl_obj_set_size(btn_sd, 100, 40);
    sgl_button_set_text(btn_sd, "Open /sd");
    sgl_button_set_font(btn_sd, &consolas14);
    sgl_button_set_color(btn_sd, sgl_rgb(120, 120, 120));
    sgl_button_set_text_color(btn_sd, SGL_COLOR_WHITE);
    sgl_button_set_radius(btn_sd, 8);
    sgl_obj_set_event_cb(btn_sd, sgl_filedialog_example_open_sd_cb, NULL);

    /* status label */
    g_filedialog_status = sgl_label_create(NULL);
    sgl_obj_set_pos(g_filedialog_status, 20, 285);
    sgl_obj_set_size(g_filedialog_status, 440, 20);
    sgl_label_set_font(g_filedialog_status, &consolas14);
    sgl_label_set_text_color(g_filedialog_status, sgl_rgb(40, 40, 40));
    sgl_filedialog_example_status("click a button to open the file dialog");

    /* on-screen hints for the manual test */
    sgl_obj_t *hint1 = sgl_label_create(NULL);
    sgl_obj_set_pos(hint1, 20, 320);
    sgl_obj_set_size(hint1, 700, 20);
    sgl_label_set_font(hint1, &consolas14);
    sgl_label_set_text_color(hint1, sgl_rgb(120, 120, 120));
    sgl_label_set_text(hint1, "mouse: click a button to open, click a row twice to enter / pick, X to close");

    sgl_obj_t *hint2 = sgl_label_create(NULL);
    sgl_obj_set_pos(hint2, 20, 344);
    sgl_obj_set_size(hint2, 700, 20);
    sgl_label_set_font(hint2, &consolas14);
    sgl_label_set_text_color(hint2, sgl_rgb(120, 120, 120));
    sgl_label_set_text(hint2, "keys: arrow keys move the selection, ENTER picks, ESC cancels");

    sgl_obj_t *hint3 = sgl_label_create(NULL);
    sgl_obj_set_pos(hint3, 20, 368);
    sgl_obj_set_size(hint3, 700, 20);
    sgl_label_set_font(hint3, &consolas14);
    sgl_label_set_text_color(hint3, sgl_rgb(120, 120, 120));
    sgl_label_set_text(hint3, "the virtual \"/\" device list title is set by sgl_filedialog_set_root_text()");
}
