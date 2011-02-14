#ifndef _YPIPED_H_
#define _YPIPED_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <pwd.h>

#include <sys/file.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/select.h>

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

enum YpipeCmdType {
    YP_OPEN = 0,
    YP_KILL,
    YP_CLEAR
};

typedef struct {
    enum YpipeCmdType cmd;
    pid_t        pid_to_kill;
    char         fifo_path[PATH_MAX];
    int          output;
    char         output_file_path[PATH_MAX];
} YpipeConfig;

/* global variables */
extern YpipeState  g_yp_state;
extern YpipeConfig g_yp_config;

extern const char *yp_dir;
extern const char *yp_pid_file_format;
extern const char *yp_fifo_file_format;

void ypipeDaemon();

#endif
