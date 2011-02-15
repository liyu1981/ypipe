#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/select.h>

#include "ypiped.h"

#define BUFFER_EMPTY_SIZE(bref) (MAX_BUF_SIZE - (bref).filled)
#define BUFFER_FREE_PTR(bref) ((bref).data + (bref).filled)

YpipeState  g_yp_state;
YpipeConfig g_yp_config;

/* function prototypes */
void usage();
void readConfig(int argc, char *argv[]);
void init();
void reaper();
void terminate();
void readAndProcess();
void outputBufferAll();
void outputBufferUntilLineBreak();
void printBuffer(const char *data);

void signal_term(int signum);
void signal_usr1(int signum);

int main(int argc, char *argv[])
{
    readConfig(argc, argv);
    init();
    reaper();
    terminate();
    return 0;
}

void signal_term(int signum)
{
    g_yp_state.terminate = 1;
}

void signal_usr1(int signum)
{
    if (g_yp_state.output_file_fd) {
        outputBufferAll();
        fclose(g_yp_state.output_file_fd);
        g_yp_state.output_file_fd = fopen(g_yp_config.output_file_path, "w+");
        if (!g_yp_state.output_file_fd) {
            printf("Open output file %s failed!\n", g_yp_config.output_file_path);
            exit(errno);
        }
        printf("File %s is cleared.\n", g_yp_config.output_file_path);
    }
}

void usage()
{
    printf("usage: -o <outputfile> ypipe <fifo> &\n");
}

void readConfig(int argc, char *argv[])
{
    int c;

    memset(g_yp_config.fifo_path, '\0', PATH_MAX);
    g_yp_config.output = 0;
    memset(g_yp_config.output_file_path, '\0', PATH_MAX);
    g_yp_config.append = 0;

    opterr = 0;
     
    while ((c = getopt (argc, argv, "o:")) != -1) {
        switch (c)
        {
        case 'o':
            g_yp_config.output = 1;
            strcpy(g_yp_config.output_file_path, optarg);
            break;
        case '?':
        default:
            usage();
            exit(1);
        }
    }
     
    if (argc - optind < 1) {
        usage();
        exit(errno);
    }

    strcpy(g_yp_config.fifo_path, argv[optind]);
}

void init()
{     
    g_yp_state.terminate = 0;

    g_yp_state.fifo_fd = open(g_yp_config.fifo_path, O_RDONLY | O_NONBLOCK);
    if (!g_yp_state.fifo_fd) {
        printf("Open named pipe %s error!\n", g_yp_config.fifo_path);
        exit(errno);
    }

    memset(g_yp_state.buf.data, 0, MAX_BUF_SIZE+1);
    g_yp_state.buf.filled = 0;

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
        if (g_yp_state.buf.filled > 0) {
            outputBufferAll();
        }
    }
    printf("Ypipe on %s terminate now, bye:)\n", g_yp_config.fifo_path);
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
    if (g_yp_config.output) {
        fprintf(g_yp_state.output_file_fd, "%s", data);
        fflush(g_yp_state.output_file_fd);
    }
}
