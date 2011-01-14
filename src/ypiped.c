#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

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

/* function prototypes */
void readAndProcess();

int main(int argc, char *argv[])
{
    int    ret;
    int    fd_max;
    fd_set fdset;

    if (argc < 2) {
        printf("usage: ypipe <fifo> &\n");
        exit(1);
    }

    g_fd = open(argv[1], O_RDONLY);
    memset(g_buffer.data, 0, MAX_BUF_SIZE+1);
    g_buffer.filled = 0;

    if(!g_fd) {
        printf("Open named pipe error!\n");
        exit(1);
    }

    for(;;) {
        FD_ZERO(&fdset);
        FD_SET(g_fd, &fdset);
        fd_max = g_fd + 1;

        ret = select(fd_max, &fdset, NULL, NULL, NULL);
        if (ret > 0) {
            if (FD_ISSET(g_fd, &fdset) != 0) {
                readAndProcess();
            }
        }
    }

    return 0;
}

void readAndProcess()
{
    int i, j;
    int need;
    int numread;
    int last_filled;
    char *b;
    char tmp;

    need = BUFFER_EMPTY_SIZE(g_buffer);
    b = BUFFER_FREE_PTR(g_buffer);

    numread = read(g_fd, b, need);

    if (numread == 0)
        return;
    else if (numread == need) {
        printf("%s", g_buffer.data);
        memset(g_buffer.data, 0, MAX_BUF_SIZE+1);
        g_buffer.filled = 0;
    }
    else {
        last_filled = g_buffer.filled;
        g_buffer.filled += numread;

        for (i=last_filled; i<MAX_BUF_SIZE; ++i)
            if (g_buffer.data[i] == '\n')
                break;

        if (i == MAX_BUF_SIZE)
            return;
        else {
            tmp = g_buffer.data[i+1];
            g_buffer.data[i+1] = '\0';
            printf("%s", g_buffer.data);
            g_buffer.data[i+1] = tmp;
            
            g_buffer.filled = g_buffer.filled - i;
            j = i;
            for (i=0; i<g_buffer.filled; ++i) {
                g_buffer.data[i] = g_buffer.data[j];
            }
            g_buffer.data[g_buffer.filled] = '\0';
        }
    }
}
