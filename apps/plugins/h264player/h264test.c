/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Simulator-only unattended test runner. Enabled when /h264player.test
 * exists on the simulated disk. Plays the file with pacing and frame
 * dropping disabled, logging an MD5 of every drawn frame to
 * /h264player_test.log, saves an LCD screenshot, then seeks to the middle
 * and logs the frames after the seek.
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
#include "plugin.h"
#include "h264player.h"
#include "lib/md5.h"
#include "mpeg_settings.h"
#include "stream_mgr.h"
#include "h264test.h"

#define LOG_NAME   "/h264player_test.log"
#define SHOT_NAME  "/h264player_shot.bmp"
#define SHOT_FRAME 30
#define STALL_TICKS (3*HZ)
#define TIMEOUT_TICKS (180*HZ)

static int log_fd = -1;
static volatile int frame_count;
static volatile long last_frame_tick;
static bool test_active;
static bool shot_enabled;
static bool audio_mode;      /* marker file says "audio": capture PCM too */
static bool pcm_capture;
static int pcm_fd = -1;
#define PCM_NAME "/h264player_pcm.raw"

static void log_line(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = rb->vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (log_fd >= 0 && n > 0)
        rb->write(log_fd, buf, n);
}

#if defined(HAVE_LCD_COLOR) && LCD_DEPTH == 16
static void write_bmp(const char *name)
{
    static const unsigned char hdr[54] = {
        'B','M', 0,0,0,0, 0,0,0,0, 54,0,0,0, 40,0,0,0,
        0,0,0,0, 0,0,0,0, 1,0, 24,0, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 };
    unsigned char h[54], row[LCD_WIDTH * 3 + 3];
    struct viewport *vp = *(rb->screens[SCREEN_MAIN]->current_viewport);
    const fb_data *fb = vp->buffer->fb_ptr;
    size_t stride = vp->buffer->stride;
    int fd = rb->open(name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    int rowbytes = (LCD_WIDTH * 3 + 3) & ~3, x, y;
    long size = 54 + rowbytes * LCD_HEIGHT;

    if (fd < 0)
        return;
    rb->memcpy(h, hdr, 54);
    h[2] = size; h[3] = size >> 8; h[4] = size >> 16; h[5] = size >> 24;
    h[18] = LCD_WIDTH; h[19] = LCD_WIDTH >> 8;
    h[22] = LCD_HEIGHT; h[23] = LCD_HEIGHT >> 8;
    rb->write(fd, h, 54);
    rb->memset(row, 0, sizeof row);
    for (y = LCD_HEIGHT - 1; y >= 0; y--)
    {
        for (x = 0; x < LCD_WIDTH; x++)
        {
            fb_data p = fb[y * stride + x];
            row[x*3 + 0] = (FB_UNPACK_BLUE(p));
            row[x*3 + 1] = (FB_UNPACK_GREEN(p));
            row[x*3 + 2] = (FB_UNPACK_RED(p));
        }
        rb->write(fd, row, rowbytes);
    }
    rb->close(fd);
}
#else
static void write_bmp(const char *name) { (void)name; }
#endif

/* Called from the audio thread with what is about to be played (stereo,
 * 16 bit, at the output clock rate) */
void h264test_pcm(const int16_t *stereo, int frames)
{
    if (pcm_capture && pcm_fd >= 0)
    {
        rb->write(pcm_fd, stereo, frames * 2 * sizeof (int16_t));
        last_frame_tick = *rb->current_tick;   /* audio still running */
    }
}

void h264test_frame(const mpeg2_sequence_t *seq, uint8_t *const *buf,
                    uint32_t tag)
{
    struct md5_s m;
    char hex[40];
    unsigned y, w = seq->display_width, h = seq->display_height;
    unsigned stride = seq->width;

    if (!test_active)
        return;

    InitMD5(&m);
    for (y = 0; y < h; y++)
        AddMD5(&m, buf[0] + y * stride, w);
    for (int p = 1; p < 3; p++)
        for (y = 0; y < h / 2; y++)
            AddMD5(&m, buf[p] + y * (stride / 2), w / 2);
    EndMD5(&m);
    psz_md5_hash(hex, &m);

    log_line("F %d %s %ld\n", frame_count, hex,
             tag == H264DEC_NO_TAG ? -1L : (long)tag);

    if (shot_enabled && frame_count == SHOT_FRAME)
        write_bmp(SHOT_NAME);

    frame_count++;
    last_frame_tick = *rb->current_tick;
}

/* Wait until frames stop arriving (end of stream) */
static void wait_finished(void)
{
    long start = *rb->current_tick;
    last_frame_tick = start;

    while (TIME_BEFORE(*rb->current_tick, start + TIMEOUT_TICKS))
    {
        rb->sleep(HZ/10);
        if (frame_count > 0 &&
            TIME_AFTER(*rb->current_tick, last_frame_tick + STALL_TICKS))
            return;
        if (frame_count == 0 &&
            TIME_AFTER(*rb->current_tick, start + STALL_TICKS*3))
            return;
    }
    log_line("TIMEOUT\n");
}

int h264test_run(const char *file)
{
    int r;
    uint32_t duration;
    int first_pass;

    log_fd = rb->open(LOG_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (log_fd < 0)
        return PLUGIN_ERROR;

    {
        char mode[16] = "";
        int mfd = rb->open("/h264player.test", O_RDONLY);
        if (mfd >= 0)
        {
            rb->read(mfd, mode, sizeof mode - 1);
            rb->close(mfd);
        }
        audio_mode = rb->strncmp(mode, "audio", 5) == 0;
    }

    init_settings(file);
    settings.limitfps = 0;
    settings.skipframes = 0;

    if (stream_init() < STREAM_OK)
    {
        log_line("ERROR stream_init\n");
        goto out;
    }

    r = stream_open(file);
    log_line("OPEN %d\n", r);
    if (r < STREAM_OK)
        goto out_exit;

    duration = stream_get_duration();
    log_line("DURATION %lu\n", (unsigned long)duration);

    if (audio_mode)
    {
        pcm_fd = rb->open(PCM_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        pcm_capture = pcm_fd >= 0;
    }

    test_active = true;
    shot_enabled = true;
    frame_count = 0;

    stream_show_vo(true);
    r = stream_seek(0, SEEK_SET);
    log_line("SEEK0 %d\n", r);
    r = stream_play();
    log_line("PLAY %d\n", r);
    wait_finished();
    first_pass = frame_count;
    log_line("PASS1 frames=%d\n", first_pass);

    pcm_capture = false;
    if (pcm_fd >= 0)
    {
        rb->close(pcm_fd);
        pcm_fd = -1;
    }

    stream_stop();

    /* Seek tests: jump to several points and play out the rest each time */
    shot_enabled = false;
    {
        static const int seek_pct[] = { 50, 90, 10 };
        unsigned i;

        for (i = 0; i < sizeof seek_pct / sizeof seek_pct[0]; i++)
        {
            stream_stop();
            frame_count = 0;
            log_line("PHASE seek %d\n", seek_pct[i]);
            r = stream_seek(muldiv_uint32(duration, seek_pct[i], 100), SEEK_SET);
            log_line("SEEK %d\n", r);
            r = stream_play();
            log_line("PLAY %d\n", r);
            wait_finished();
            log_line("PASS%u frames=%d\n", i + 2, frame_count);
        }
    }

    test_active = false;
    stream_stop();
    stream_close();
out_exit:
    stream_exit();
out:
    log_line("END\n");
    rb->close(log_fd);
    log_fd = -1;
    return PLUGIN_OK;
}
