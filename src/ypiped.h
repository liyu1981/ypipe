/*                               -*- Mode: C -*- 
 * Filename: ypiped.h
 */

/* This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth
 * Floor, Boston, MA 02110-1301, USA.
 */

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

void ypipeDaemon();

#endif
