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

void *fifo_write_thread(void *ptr)
{
    return NULL;
}

void *fifo_read_thread(void *ptr)
{
    return NULL;
}

int fifo_write_setup(conf_t *conf)
{
    out_fifo_fd = fopen(conf->out_fifo, "wb");
    out_channels = conf->out_channels;
    return 0;
}

int fifo_read_setup(conf_t *conf)
{
    in_fifo_fd = fopen(conf->in_fifo, "rb");
    in_channels = conf->in_channels;
    return 0;
}

int fifo_write(void *buf, size_t frames)
{
    if (feof(out_fifo_fd)) {
        return -1;
    }
    fwrite(buf, sizeof(int16_t), frames * out_channels, out_fifo_fd);
    return 0;
}

int fifo_read(void *buf, size_t frames, int timeout_ms)
{
    if (feof(in_fifo_fd)) {
        return -1;
    }
    fread(buf, sizeof(int16_t), frames * in_channels, in_fifo_fd);
    return 0;
}
