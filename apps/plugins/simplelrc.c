/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

/* simplelrc - display current song info and lyrics from .lrc file */

#include "plugin.h"
#include "../../lib/rbcodec/metadata/metadata.h"

static void log(const char *fmt, ...) ATTRIBUTE_PRINTF(1, 2);
static void log(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    rb->vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    DEBUGF("simplelrc: %s", buf);
}

#define LRC_BUF_SIZE 8192
#define LRC_MAX_LINES 512
#define ALBUM_ART_DARKEN_FACTOR 0.4f
#define SCROLL_PADDING 0

struct Point {
    int x;
    int y;
};

static struct {
    struct {
        struct bitmap bmp;
        unsigned char *buf;
        size_t buf_size;
        bool loaded;
        struct Point position;
    } album_art;
    struct {
        char buf[LRC_BUF_SIZE];
        char *lines[LRC_MAX_LINES];
        int line_count;
        int scroll_offset;
        bool loaded;
    } lyrics;
    char current_track[MAX_PATH];
    int font_h;
} state;

static void lrc_get_path(const char *track_path, char *lrc_path, size_t size)
{
    rb->strlcpy(lrc_path, track_path, size);
    char *dot = rb->strrchr(lrc_path, '.');
    if (dot)
        rb->strlcpy(dot, ".lrc", size - (dot - lrc_path));
}

static bool lrc_read_file(const char *path, char *buf, size_t buf_size)
{
    int fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    ssize_t bytes = rb->read(fd, buf, buf_size - 1);
    rb->close(fd);
    if (bytes <= 0)
        return false;
    buf[bytes] = '\0';
    return true;
}

static char *lrc_skip_timestamp(char *line)
{
    char *p = line;
    while (*p == '[') {
        char *end = rb->strchr(p, ']');
        if (!end) return line;
        p = end + 1;
    }
    return p;
}

static int lrc_wrap_line(char *line, char **lines, int max_lines, int count)
{
    char *seg = line;
    char *p = line;

    while (count < max_lines)
    {
        while (*p == ' ') p++;
        if (!*p) break;

        char *word = p;
        while (*p && *p != ' ') p++;

        char save = *p;
        *p = '\0';
        int w;
        rb->font_getstringsize(seg, &w, NULL, FONT_UI);
        *p = save;

        if (w > LCD_WIDTH && word > seg)
        {
            *(word - 1) = '\0'; /* split: null-terminate the space before the overflowing word */
            lines[count++] = seg;
            seg = word;
            p = word;
            continue;
        }

        if (!*p) break;
    }

    if (count < max_lines)
        lines[count++] = seg;

    return count;
}

static int lrc_split_lines(char *buf, char **lines, int max_lines)
{
    int count = 0;
    char *p = buf;
    while (*p && count < max_lines)
    {
        char *nl = rb->strchr(p, '\n');
        if (nl) *nl = '\0';
        count = lrc_wrap_line(lrc_skip_timestamp(p), lines, max_lines, count);
        if (!nl) break;
        p = nl + 1;
    }
    return count;
}

static void lrc_load(const char *track_path)
{
    state.lyrics.loaded = false;
    state.lyrics.line_count = 0;
    state.lyrics.scroll_offset = 0;
    char lrc_path[MAX_PATH];
    lrc_get_path(track_path, lrc_path, sizeof(lrc_path));
    DEBUGF("simplelrc: looking for lyrics at %s\n", lrc_path);
    if (!lrc_read_file(lrc_path, state.lyrics.buf, sizeof(state.lyrics.buf)))
        return;
    state.lyrics.line_count = lrc_split_lines(state.lyrics.buf, state.lyrics.lines, LRC_MAX_LINES);
    state.lyrics.loaded = true;
    DEBUGF("simplelrc: lyrics loaded, %d lines\n", state.lyrics.line_count);
}

/* Darken all pixels in loaded album art in-place.
 * For RGB565: extract each channel, scale by ALBUM_ART_DARKEN_FACTOR, recombine. */
static void album_art_darken(void)
{
    fb_data *pixels = (fb_data *)state.album_art.bmp.data;
    int count = state.album_art.bmp.width * state.album_art.bmp.height;
    for (int i = 0; i < count; i++) {
        fb_data p = pixels[i];
        int r = (int)(((p >> 11) & 0x1F) * ALBUM_ART_DARKEN_FACTOR);
        int g = (int)(((p >>  5) & 0x3F) * ALBUM_ART_DARKEN_FACTOR);
        int b = (int)(( p        & 0x1F) * ALBUM_ART_DARKEN_FACTOR);
        pixels[i] = (fb_data)((r << 11) | (g << 5) | b);
    }
}

struct BitmapResize {
    struct Point resize;
    struct Point position;
};

static struct BitmapResize resize_to_cover(int width, int height)
{
    const int display_width = LCD_WIDTH;
    const int display_height = LCD_HEIGHT;
    float aspect_ratio = (float)width / height;

    const bool wide = aspect_ratio >= 1;
    const int target_width = wide ? display_width : (int)(display_height * aspect_ratio);
    const int target_height = wide ? (int)(display_width / aspect_ratio) : display_height;
    const int pos_x = (display_width - target_width) / 2;
    const int pos_y = (display_height - target_height) / 2;

    log("target album art resolution: %dx%d\n", target_width, target_height);
    log("album art position: (%d, %d)\n", pos_x, pos_y);

    return (struct BitmapResize){
        .resize = { target_width, target_height },
        .position = { pos_x, pos_y }
    };
}

static void album_art_load(const struct mp3entry *id3)
{
    char path[MAX_PATH];

    state.album_art.loaded = false;

    if (!id3 || !state.album_art.buf || state.album_art.buf_size == 0)
        return;

    if (!rb->search_albumart_files(id3, "", path, MAX_PATH))
        return;

    struct bitmap dim_bmp;
    rb->memset(&dim_bmp, 0, sizeof(dim_bmp));
    dim_bmp.data = state.album_art.buf;
    rb->read_jpeg_file(path, &dim_bmp, state.album_art.buf_size,
                       FORMAT_NATIVE | FORMAT_RETURN_SIZE, NULL);
    log("album art native resolution: %dx%d\n",
        dim_bmp.width, dim_bmp.height);

    struct BitmapResize resize = resize_to_cover(dim_bmp.width, dim_bmp.height);

    state.album_art.bmp.data   = state.album_art.buf;
    state.album_art.bmp.width  = resize.resize.x;
    state.album_art.bmp.height = resize.resize.y;
    state.album_art.bmp.format = FORMAT_NATIVE;

    int rc = rb->read_jpeg_file(path, &state.album_art.bmp, state.album_art.buf_size,
                                FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT,
                                NULL);
    if (rc > 0)
    {
        state.album_art.position = resize.position;
        state.album_art.loaded = true;
        album_art_darken();
    }
}

static void draw_centered(const char *str, int y)
{
    int w;
    rb->font_getstringsize(str, &w, NULL, FONT_UI);
    rb->lcd_putsxy((LCD_WIDTH - w) / 2, y, str);
}

static void draw_lyrics(int y_start)
{
    int y_relative = (state.lyrics.scroll_offset * state.font_h) * -1;

    for (int i = 0; i < state.lyrics.line_count; i++)
    {
        int y_absolute = y_start + y_relative;

        if (y_absolute > LCD_HEIGHT)
            return;

        if (y_absolute >= y_start)
            draw_centered(state.lyrics.lines[i], y_absolute);

        y_relative += state.font_h;
    }
}

static void adjust_scroll_offset(int delta)
{
    int new_offset = state.lyrics.scroll_offset + delta;

    int lyrics_y = 5 + state.font_h + 5 + SCROLL_PADDING;
    int visible_lines = (LCD_HEIGHT - lyrics_y + SCROLL_PADDING) / state.font_h;
    int max_offset = state.lyrics.line_count - visible_lines;
    if (max_offset < 0)
        max_offset = 0;

    if (new_offset >= 0 && new_offset <= max_offset)
        state.lyrics.scroll_offset = new_offset;
}

static void update_display(const struct mp3entry *id3)
{
    char display[512];

    if (!id3 || !id3->artist || !id3->title)
        rb->snprintf(display, sizeof(display), "Nothing playing");
    else
        rb->snprintf(display, sizeof(display), "%s - %s", id3->artist, id3->title);

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    if (state.album_art.loaded)
        rb->lcd_bmp_part(&state.album_art.bmp, 0, 0, state.album_art.position.x, state.album_art.position.y,
                         state.album_art.bmp.width, state.album_art.bmp.height);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_putsxy(5, 5, display);
    draw_lyrics(5 + state.font_h + 5);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_update();
}

enum button_action {
    BUTTON_CONTINUE,
    BUTTON_EXIT,
    BUTTON_GOTO_WPS,
    BUTTON_SCROLL_UP,
    BUTTON_SCROLL_DOWN,
};

/* Custom button mapping: std scroll actions first, then chain to WPS */
static const struct button_mapping simplelrc_context[] = {
#ifdef HAVE_SCROLLWHEEL
    { ACTION_STD_PREV,       BUTTON_SCROLL_BACK,               BUTTON_NONE },
    { ACTION_STD_PREVREPEAT, BUTTON_SCROLL_BACK|BUTTON_REPEAT, BUTTON_NONE },
    { ACTION_STD_NEXT,       BUTTON_SCROLL_FWD,                BUTTON_NONE },
    { ACTION_STD_NEXTREPEAT, BUTTON_SCROLL_FWD|BUTTON_REPEAT,  BUTTON_NONE },
#endif
    LAST_ITEM_IN_LIST__NEXTLIST(CONTEXT_WPS)
};

static const struct button_mapping* simplelrc_get_context_map(int context)
{
    (void)context;
    return simplelrc_context;
}

static enum button_action handle_button(void)
{
    switch (rb->get_custom_action(CONTEXT_PLUGIN, HZ / 10,
                                  simplelrc_get_context_map))
    {
        case ACTION_STD_PREV:
        case ACTION_STD_PREVREPEAT:
            return BUTTON_SCROLL_UP;
        case ACTION_STD_NEXT:
        case ACTION_STD_NEXTREPEAT:
            return BUTTON_SCROLL_DOWN;

        case ACTION_STD_MENU:
            /* Go to WPS if something is playing; fall back to root menu if not. */
            return (rb->audio_status() & AUDIO_STATUS_PLAY)
                   ? BUTTON_GOTO_WPS : BUTTON_EXIT;

        case ACTION_STD_CANCEL:
        case ACTION_WPS_BROWSE:
            return BUTTON_EXIT;

        case ACTION_WPS_PLAY:
            if (rb->audio_status() & AUDIO_STATUS_PAUSE)
                rb->audio_resume();
            else
                rb->audio_pause();
            break;

        case ACTION_WPS_SKIPNEXT:
            rb->audio_next();
            break;

        case ACTION_WPS_SKIPPREV:
            rb->audio_prev();
            break;

        case ACTION_WPS_VOLUP:
            return BUTTON_SCROLL_UP;

        case ACTION_WPS_VOLDOWN:
            return BUTTON_SCROLL_DOWN;
    }
    return BUTTON_CONTINUE;
}

static bool has_track_changed(const struct mp3entry *id3)
{
    if (id3 == NULL || id3->artist == NULL || id3->title == NULL)
        return false;

    char current_track[512];
    rb->snprintf(current_track, sizeof(current_track), "%s - %s", id3->artist, id3->title);

    if (rb->strcmp(current_track, state.current_track) == 0)
        return false;

    rb->strcpy(state.current_track, current_track);
    return true;
}

enum plugin_status plugin_start(const void* parameter)
{
    (void)parameter;

    state.current_track[0] = '\0';
    state.album_art.buf = rb->plugin_get_buffer(&state.album_art.buf_size);
    state.album_art.loaded = false;

    enum button_action action;
    while (1) {
        action = handle_button();
        if (action == BUTTON_EXIT || action == BUTTON_GOTO_WPS)
            break;

        struct mp3entry *id3 = rb->audio_current_track();
        if (has_track_changed(id3))
        {
            log("track changed, loading new album art and updating display %s - %s\n", id3 ? id3->title : "", id3 ? id3->artist : "");
            rb->font_getstringsize("X", NULL, &state.font_h, FONT_UI);
            album_art_load(id3);
            lrc_load(id3->path);
            update_display(id3);
        }
        else if ((action == BUTTON_SCROLL_UP || action == BUTTON_SCROLL_DOWN)
                 && state.lyrics.loaded)
        {
            adjust_scroll_offset((action == BUTTON_SCROLL_UP) ? -1 : 1);
            update_display(rb->audio_current_track());
        }
    }

    /* Restore default colors */
    rb->lcd_set_foreground(LCD_DEFAULT_FG);
    rb->lcd_set_background(LCD_DEFAULT_BG);

    return (action == BUTTON_GOTO_WPS) ? PLUGIN_GOTO_WPS : PLUGIN_OK;
}
