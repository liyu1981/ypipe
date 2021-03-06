/*                               -*- Mode: C -*- 
 * Filename: ypipedDaemon.c
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

#include "ypiped.h"

#define BUFFER_EMPTY_SIZE(bref) (MAX_BUF_SIZE - (bref).filled)
#define BUFFER_FREE_PTR(bref) ((bref).data + (bref).filled)

/* for daemon */
void init();
void signal_term(int signum);
void signal_usr1(int signum);
void reaper();
void terminate();
void readAndProcess();
void outputBufferAll();
void outputBufferUntilLineBreak();
void printBuffer(const char *data);

void ypipeDaemon()
{
    init();
    reaper();
    terminate();
}

void init()
{     
    g_yp_state.terminate = 0;

    memset(g_yp_state.buf.data, 0, MAX_BUF_SIZE+1);
    g_yp_state.buf.filled = 0;

    g_yp_state.fifo_fd = open(g_yp_config.fifo_path, O_RDONLY | O_NONBLOCK);
    if (g_yp_state.fifo_fd <0) {
        printf("Open named pipe %s error with code %d!\n", g_yp_config.fifo_path, errno);
        exit(errno);
    }
 
    if (!g_yp_config.output) {
        g_yp_state.output_file_fd = 0;
    }
    else {
        g_yp_state.output_file_fd = fopen(g_yp_config.output_file_path, "w+");
        if (!g_yp_state.output_file_fd) {
            printf("Open output file %s failed!\n", g_yp_config.output_file_path);
            exit(errno);
        }
    }

    if (signal(SIGTERM, signal_term) == SIG_ERR) {
        printf("Error setting up catching signal SIGTERM.\n");
        exit(errno);
    }

    if (signal(SIGUSR1, signal_usr1) == SIG_ERR) {
        printf("Error setting up catching signal SIGUSR1.\n");
        exit(errno);
    }
}

void reaper()
{
    int      ret;
    int      fd_max;
    fd_set   fdset;
    sigset_t sigmask;
    sigset_t sigmask2;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGTERM);
    sigaddset(&sigmask, SIGUSR1);

    sigprocmask(SIG_BLOCK, &sigmask, &sigmask2);

    for (;;) {
        FD_ZERO(&fdset);
        FD_SET(g_yp_state.fifo_fd, &fdset);
        fd_max = g_yp_state.fifo_fd + 1;

        ret = pselect(fd_max, &fdset, NULL, NULL, NULL, &sigmask2);

        if (ret > 0) {
            if (FD_ISSET(g_yp_state.fifo_fd, &fdset) != 0) {
                readAndProcess();
            }
        }

        if (g_yp_state.terminate == 1)
            break;
    }
}

void terminate()
{
    if (g_yp_state.output_file_fd) {
        if(g_yp_state.buf.filled > 0) {
            outputBufferAll();
        }
    }

    printf("Ypipe on %s now terminated, bye!:)\n", g_yp_config.fifo_path);
}

void readAndProcess()
{
    int need;
    int numread;
    int last_filled;
    char *b;

    need = BUFFER_EMPTY_SIZE(g_yp_state.buf);
    b = BUFFER_FREE_PTR(g_yp_state.buf);

    numread = read(g_yp_state.fifo_fd, b, need);

    if (numread == 0)
        return;
    else if (numread == need) {
        outputBufferAll();
    }
    else {
        last_filled = g_yp_state.buf.filled;
        g_yp_state.buf.filled += numread;
        outputBufferUntilLineBreak(last_filled);
    }
}

void outputBufferAll()
{
    printBuffer(g_yp_state.buf.data);
    memset(g_yp_state.buf.data, '\0', MAX_BUF_SIZE+1);
    g_yp_state.buf.filled = 0;
}

void outputBufferUntilLineBreak(int last_filled)
{
    int i,j;
    char tmp;

    for (i=g_yp_state.buf.filled; i>last_filled; --i)
        if (g_yp_state.buf.data[i-1] == '\n')
            break;

    if (i == last_filled)
        return;
    else {
        tmp = g_yp_state.buf.data[i];
        g_yp_state.buf.data[i] = '\0';
        printBuffer(g_yp_state.buf.data);
        g_yp_state.buf.data[i] = tmp;
        
        g_yp_state.buf.filled = g_yp_state.buf.filled - i;
        j = i;
        for (i=0; i<g_yp_state.buf.filled; ++i) {
            g_yp_state.buf.data[i] = g_yp_state.buf.data[j];
        }
        g_yp_state.buf.data[g_yp_state.buf.filled] = '\0';
    }
}

void printBuffer(const char *data)
{
    /* first write to std* streams */
    fprintf(stdout, "%s", data);
    fflush(stdout);

    /* then write to user specified streams */
    if (g_yp_config.output && g_yp_state.output_file_fd) {
        fprintf(g_yp_state.output_file_fd, "%s", data);
        fflush(g_yp_state.output_file_fd);
    }
}

void signal_term(int signum)
{
    g_yp_state.terminate = 1;
}

void signal_usr1(int signum)
{
    if (g_yp_state.output_file_fd) {
        if(g_yp_state.buf.filled > 0) {
            outputBufferAll();
        }
        fclose(g_yp_state.output_file_fd);
        g_yp_state.output_file_fd = fopen(g_yp_config.output_file_path, "w+");
        if (g_yp_state.output_file_fd < 0) {
            printf("Open output file %s failed!\n", g_yp_config.output_file_path);
            exit(errno);
        }
        printf("Ypipe on %s, output file %s cleared!\n",
               g_yp_config.fifo_path, g_yp_config.output_file_path);
    }
}
