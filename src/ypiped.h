#ifndef _YPIPED_H_
#define _YPIPED_H_

#include <limits.h>
#include "../include/config.h"

#define MAX_BUF_SIZE 1024

typedef struct {
    char data[MAX_BUF_SIZE+1];
    int filled;
} buffer;

typedef struct {
    int     fifo_fd;
    FILE   *output_file_fd;
    buffer  buf;
    int     terminate;
} YpipeState;

typedef struct {
    char fifo_path[PATH_MAX];
    int  output;
    char output_file_path[PATH_MAX];
    int  append;
} YpipeConfig;

/* global variables */
extern YpipeState  g_yp_state;
extern YpipeConfig g_yp_config; 

#endif
