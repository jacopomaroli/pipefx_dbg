#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdint.h>
#include <getopt.h>
#include <unistd.h>

#include "conf.h"
#include "util.h"
#include "ws_server.h"

FILE *in_fifo_fd, *out_fifo_fd;
unsigned in_channels;
unsigned out_channels;
void* ws_server;

extern int g_is_quit;

unsigned interactive = 1;
unsigned end_of_input = 0;

#define INPUT_STREAM_ID 0
#define OUTPUT_STREAM_ID 1

void* fifo_read_thread(void* ptr)
{
    return NULL;
}

void *fifo_write_thread(void *ptr)
{
    return NULL;
}

int fifo_read_setup(conf_t *conf)
{
    in_fifo_fd = fopen(conf->in_fifo, "rb");
    in_channels = conf->in_channels;
    if (interactive) {
        ws_server = setup_ws_server();
        char* config_file_path = 0;
        int opt = 0;
        int argc = __argc;
        char** argv = __argv;
        optind = 1;
        while ((opt = getopt(argc, argv, "D:h:c:v")) != -1)
        {
            switch (opt)
            {
            case 'c':
                config_file_path = optarg;
                break;
            default:
                break;
            }
        }
        ws_server_load_config(ws_server, config_file_path, conf);
        int frame_size = conf->rate * 10 / 1000; // 10 ms
        ws_server_push_stream(ws_server, INPUT_STREAM_ID, in_channels, frame_size, "Input");
    }
    return 0;
}

int fifo_write_setup(conf_t* conf)
{
    out_fifo_fd = fopen(conf->out_fifo, "wb");
    out_channels = conf->out_channels;
    if (interactive) {
        int frame_size = conf->rate * 10 / 1000; // 10 ms
        ws_server_push_stream(ws_server, OUTPUT_STREAM_ID, out_channels, frame_size, "Output");
    }
    return 0;
}

int fifo_read(void* buf, size_t frames, int timeout_ms)
{
    fread(buf, sizeof(int16_t), frames * in_channels, in_fifo_fd);
    if (feof(in_fifo_fd)) {
        if (interactive) {
            fseek(in_fifo_fd, 0, SEEK_SET);
            end_of_input = 1;
        }
        else {
            g_is_quit = 1;
            return -1;
        }
    }
    if (interactive) {
        ws_server_send(buf, sizeof(int16_t) * frames * in_channels, ws_server, INPUT_STREAM_ID);
    }
    return 0;
}

int fifo_write(void *buf, size_t frames)
{
    if (interactive) {
        ws_server_send(buf, sizeof(int16_t) * frames * out_channels, ws_server, OUTPUT_STREAM_ID);
        if (end_of_input) {
            end_of_input = 0;
            ws_server_send_done(ws_server);
        }
    }
    else {
        fwrite(buf, sizeof(int16_t), frames * in_channels, out_fifo_fd);
    }
    return 0;
}
