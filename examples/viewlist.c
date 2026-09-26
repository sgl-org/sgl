/**
 * @file sgl/examples/viewlist.c
 * @brief ViewList (virtual list) widget example: a scrollable music list
 *
 * The list holds 200 virtual songs, but only a small sliding window of
 * items is cached in RAM (see SGL_VIEWLIST_CACHE_MULT in sgl_viewlist.h).
 * Item content is fully user-defined through the get/draw/click callbacks.
 */

#include <sgl.h>
#include <sgl_log.h>

#define MUSIC_SONG_NUM   200

static const char *music_titles[] = {
    "Night Drive", "Blue Horizon", "Midnight Sun", "Silver Rain", "Lost Echoes",
    "Neon Skyline", "Golden Hour", "Quiet Storm", "Aurora", "Starfall",
};

static const char *music_artists[] = {
    "The Wanderers", "Luna Park", "Echo Valley", "Nova Beat", "Solar Twins",
    "Crystal Lake", "Paper Planes", "Velvet Moon", "Amber Waves", "Zero Gravity",
};

/* palette for the generated album covers */
static const uint16_t music_cover_colors[] = {
    0x3A7D, 0x5B2E, 0x7417, 0x2D9F, 0x8C71, 0xD4AC, 0x03EF, 0x9B13,
};

/* duration in seconds, derived from the index so it is stable */
static int music_duration_sec(int32_t index)
{
    return 150 + (int)(index * 37 % 180); /* 2:30 .. 5:30 */
}

/**
 * @brief item data provider: fill the cached item slot for a song index.
 *        Called lazily by the widget, only for items near the visible area.
 */
static void music_get_item(sgl_obj_t *list, int32_t index, sgl_viewlist_item_t *item)
{
    SGL_UNUSED(list);
    sgl_snprintf(item->text, sizeof(item->text), "%s",
                 music_titles[index % (sizeof(music_titles) / sizeof(music_titles[0]))]);
    sgl_snprintf(item->subtext, sizeof(item->subtext), "%s",
                 music_artists[index % (sizeof(music_artists) / sizeof(music_artists[0]))]);
}

/**
 * @brief custom item renderer: album cover + title + artist + duration.
 *        This replaces the widget's default item style completely.
 */
static void music_draw_item(sgl_obj_t *list, sgl_surf_t *surf, sgl_area_t *clip,
                            sgl_area_t *coords, const sgl_viewlist_item_t *item, bool selected)
{
    SGL_UNUSED(list);
    const int16_t pad     = 6;
    const int16_t cover   = coords->y2 - coords->y1 + 1 - 2 * pad;
    const int16_t cover_x = coords->x1 + pad;
    const int16_t cover_y = coords->y1 + pad;

    /* album cover: rounded square with a per-song color and an index number */
    sgl_area_t cover_rect = {
        .x1 = cover_x, .y1 = cover_y,
        .x2 = cover_x + cover - 1, .y2 = cover_y + cover - 1,
    };
    sgl_color_t cover_color = sgl_rgb565_to_color(music_cover_colors[item->index %
                              (sizeof(music_cover_colors) / sizeof(music_cover_colors[0]))]);
    sgl_draw_fill_rect(surf, clip, &cover_rect, 6, cover_color, 255);

    char num[8];
    sgl_snprintf(num, sizeof(num), "%d", (int)(item->index + 1));
    const int16_t num_w = (int16_t)sgl_font_get_string_width(num, &consolas14);
    sgl_draw_string(surf, clip,
                    cover_x + (cover - num_w) / 2,
                    cover_y + (cover - sgl_font_get_height(&consolas14)) / 2,
                    num, SGL_COLOR_WHITE, 255, &consolas14);

    /* title and artist */
    const int16_t text_x = cover_rect.x2 + 10;
    sgl_draw_string(surf, clip, text_x, coords->y1 + 7, item->text,
                    SGL_COLOR_WHITE, 255, &consolas23);
    sgl_draw_string(surf, clip, text_x, coords->y1 + 32, item->subtext,
                    SGL_COLOR_GRAY, 255, &consolas14);

    /* right-aligned duration */
    char duration[8];
    const int sec = music_duration_sec(item->index);
    sgl_snprintf(duration, sizeof(duration), "%d:%02d", sec / 60, sec % 60);
    const int16_t dur_w = (int16_t)sgl_font_get_string_width(duration, &consolas14);
    sgl_draw_string(surf, clip, coords->x2 - dur_w - 20,
                    coords->y1 + (coords->y2 - coords->y1 + 1 - sgl_font_get_height(&consolas14)) / 2,
                    duration, SGL_COLOR_GRAY, 255, &consolas14);
}

/**
 * @brief item click: in a real player this would start playback
 */
static void music_click_item(sgl_obj_t *list, int32_t index, const sgl_viewlist_item_t *item)
{
    SGL_UNUSED(list);
    SGL_LOG_INFO("Play song %d: %s - %s", (int)index,
                 item ? item->text : "?", item ? item->subtext : "?");
}

/**
 * @brief create the viewlist music list example
 * @param parent parent object, NULL creates the list on the active screen
 * @return none
 */
void sgl_viewlist_examples(sgl_obj_t *parent)
{
    sgl_obj_t *viewlist;

    /* Create the virtual list widget */
    viewlist = sgl_viewlist_create(parent);
    if (viewlist == NULL) return;

    /* Set position and size */
    sgl_obj_set_pos(viewlist, 340, 40);
    sgl_obj_set_size(viewlist, 400, 400);

    /* Configure appearance */
    sgl_viewlist_set_bg_color(viewlist, sgl_rgb(0x18, 0x1C, 0x24));
    sgl_viewlist_set_border_color(viewlist, sgl_rgb(0x2F, 0x4F, 0x4F));
    sgl_viewlist_set_border_width(viewlist, 2);
    sgl_viewlist_set_radius(viewlist, 8);
    sgl_viewlist_set_selected_color(viewlist, sgl_rgb(0x24, 0x40, 0x5A));
    sgl_viewlist_set_alpha(viewlist, 255);

    /* Item geometry */
    sgl_viewlist_set_item_height(viewlist, 56);
    sgl_viewlist_set_item_margin(viewlist, 4, 10);

    /* Virtual list data source and callbacks */
    sgl_viewlist_set_item_num(viewlist, MUSIC_SONG_NUM);
    sgl_viewlist_set_item_get_cb(viewlist, music_get_item);
    sgl_viewlist_set_item_draw_cb(viewlist, music_draw_item);
    sgl_viewlist_set_item_click_cb(viewlist, music_click_item);
}
