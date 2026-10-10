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

/* ramfs ignores the block device internals, but mounting with a non-NULL
 * device registers the mount point in the block device registry, which is
 * exactly what the file dialog enumerates */
static sgl_block_dev_t g_filedialog_dev_sd;
static sgl_block_dev_t g_filedialog_dev_flash;

static sgl_obj_t *g_filedialog_status;
static char g_filedialog_status_text[160];

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
    SGL_UNUSED(dialog);
    SGL_UNUSED(user_data);

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

static void sgl_filedialog_example_open_file_cb(sgl_event_t *event)
{
    if (event->type != SGL_EVENT_CLICKED) return;

    /* dev_name = NULL: the dialog shows the registered block devices first */
    sgl_obj_t *dlg = sgl_filedialog_open_file(NULL, NULL);
    if (dlg == NULL) {
        sgl_filedialog_example_status("dialog alloc failed");
        return;
    }
    sgl_filedialog_set_title(dlg, "Open file - select device");
    sgl_filedialog_set_text_font(dlg, &consolas14);
    sgl_filedialog_set_confirm_cb(dlg, sgl_filedialog_example_confirm, NULL);
}

static void sgl_filedialog_example_open_dir_cb(sgl_event_t *event)
{
    if (event->type != SGL_EVENT_CLICKED) return;

    sgl_obj_t *dlg = sgl_filedialog_open_dir(NULL, NULL);
    if (dlg == NULL) {
        sgl_filedialog_example_status("dialog alloc failed");
        return;
    }
    sgl_filedialog_set_title(dlg, "Select directory - select device");
    sgl_filedialog_set_text_font(dlg, &consolas14);
    sgl_filedialog_set_confirm_cb(dlg, sgl_filedialog_example_confirm, NULL);
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
    sgl_filedialog_set_text_font(dlg, &consolas14);
    sgl_filedialog_set_confirm_cb(dlg, sgl_filedialog_example_confirm, NULL);
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
}
