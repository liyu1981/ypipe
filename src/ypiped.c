#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/select.h>

#define MAX_BUF_SIZE 1024

typedef struct {
  char data[MAX_BUF_SIZE+1];
  int filled;
} buffer;

#define BUFFER_EMPTY_SIZE(bref) (MAX_BUF_SIZE - (bref).filled)
#define BUFFER_FREE_PTR(bref) ((bref).data + (bref).filled)

/* global variables */
int    g_fd;
buffer g_buffer;
int    g_terminate;


/* function prototypes */
void init();
void reaper();
void terminate();
void readAndProcess();
void outputBufferAll();
void outputBufferUntilLineBreak();

void signal_term(int signum);

int main(int argc, char *argv[])
{
    init(argc, argv);
    reaper();
    terminate();
    return 0;
}

void signal_term(int signum)
{
    g_terminate = 1;
}

void init(int argc, char *argv[])
{
    if (argc < 2) {
        printf("usage: ypipe <fifo> &\n");
        exit(1);
    }

    g_fd = open(argv[1], O_RDONLY);
    memset(g_buffer.data, 0, MAX_BUF_SIZE+1);
    g_buffer.filled = 0;
    g_terminate = 0;

    if (!g_fd) {
        printf("Open named pipe error!\n");
        exit(1);
    }

    if (signal(SIGTERM, signal_term) == SIG_ERR) {
        printf("Error setting up catching signal SIGTERM.\n");
        exit(1);
    }
}

void reaper()
{
    int    ret;
    int    fd_max;
    fd_set fdset;
    sigset_t sigmask;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGTERM);

    for (;;) {
        FD_ZERO(&fdset);
        FD_SET(g_fd, &fdset);
        fd_max = g_fd + 1;

        ret = pselect(fd_max, &fdset, NULL, NULL, NULL, &sigmask);
        if (ret > 0) {
            if (FD_ISSET(g_fd, &fdset) != 0) {
                readAndProcess();
            }
        }

        if (g_terminate == 1)
            break;
    }
}

void terminate()
{
    if(g_buffer.filled > 0) {
#ifdef DEBUG
        printf("Flush last %d chars ...\n", g_buffer.filled);
#endif
        outputBufferAll();
#ifdef DEBUG
        printf("Bye :)\n");
#endif
    }
}

void readAndProcess()
{
    int need;
    int numread;
    int last_filled;
    char *b;

    need = BUFFER_EMPTY_SIZE(g_buffer);
    b = BUFFER_FREE_PTR(g_buffer);

    numread = read(g_fd, b, need);

    if (numread == 0)
        return;
    else if (numread == need) {
        outputBufferAll();
    }
    else {
        last_filled = g_buffer.filled;
        g_buffer.filled += numread;
        outputBufferUntilLineBreak(last_filled);
    }
}

void outputBufferAll()
{
    printf("%s", g_buffer.data);
    memset(g_buffer.data, '\0', MAX_BUF_SIZE+1);
    g_buffer.filled = 0;
}

void outputBufferUntilLineBreak(int last_filled)
{
    int i,j;
    char tmp;

    for (i=g_buffer.filled; i>last_filled; --i)
        if (g_buffer.data[i-1] == '\n')
            break;

    if (i == last_filled)
        return;
    else {
        tmp = g_buffer.data[i];
        g_buffer.data[i] = '\0';
        printf("%s", g_buffer.data);
        g_buffer.data[i] = tmp;
        
        g_buffer.filled = g_buffer.filled - i;
        j = i;
        for (i=0; i<g_buffer.filled; ++i) {
            g_buffer.data[i] = g_buffer.data[j];
        }
        g_buffer.data[g_buffer.filled] = '\0';
    }
}
