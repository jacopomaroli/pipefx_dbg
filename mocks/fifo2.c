#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdint.h>

#include "conf.h"
#include "util.h"

FILE *in_fifo_fd, *out_fifo_fd;
unsigned in_channels;
unsigned out_channels;

extern int g_is_quit;

void *fifo_write_thread(void *ptr)
{
    return NULL;
}

void *fifo_read_thread(void *ptr)
{
    return NULL;
}

int fifo_setup(conf_t *conf)
{
    char* out_fifo = "C:\\Users\\Jacopo\\Desktop\\ec\\out_dbg.raw";
    out_fifo_fd = fopen(out_fifo, "wb");
    out_channels = conf->out_channels;
    return 0;
}

int fifo_write(void *buf, size_t frames)
{
    if (feof(out_fifo_fd)) {
        g_is_quit = 1;
        return -1;
    }
    fwrite(buf, sizeof(int16_t), frames * out_channels, out_fifo_fd);
    return 0;
}

int capture_start(conf_t* conf)
{
    // char* playback_fifo = "D:\\development\\domotics\\speex\\audiofiles\\recording.raw";
    char* playback_fifo = "C:\\Users\\Jacopo\\Desktop\\ec\\recording.raw";
    // char* playback_fifo = "C:\\Users\\Jacopo\\Desktop\\ec\\test_order.raw";
    in_fifo_fd = fopen(playback_fifo, "rb");
    in_channels = conf->in_fifo;
    return 0;
}

int capture_read(void *buf, size_t frames, int timeout_ms)
{
    if (feof(in_fifo_fd)) {
        g_is_quit = 1;
        return -1;
    }
    fread(buf, sizeof(int16_t), frames * in_channels, in_fifo_fd);
    return 0;
}

int capture_stop()
{
    return 0;
}
