#include "ypiped.h"

#define BUFFER_EMPTY_SIZE(bref) (MAX_BUF_SIZE - (bref).filled)
#define BUFFER_FREE_PTR(bref) ((bref).data + (bref).filled)

/* for daemon */
void reaper();
void terminate();
void readAndProcess();
void outputBufferAll();
void outputBufferUntilLineBreak();
void printBuffer(const char *data);

void ypipeDaemon()
{
    reaper();
    terminate();
}

void reaper()
{
    int      ret;
    int      fd_max;
    fd_set   fdset;
    sigset_t sigmask;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGTERM);

    for (;;) {
        FD_ZERO(&fdset);
        FD_SET(g_yp_state.fifo_fd, &fdset);
        fd_max = g_yp_state.fifo_fd + 1;

        ret = pselect(fd_max, &fdset, NULL, NULL, NULL, &sigmask);
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
    if(g_yp_state.buf.filled > 0) {
        outputBufferAll();
    }
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

void signal_term(int signum)
{
    g_yp_state.terminate = 1;
}

void signal_usr2(int signum)
{
    outputBufferAll();
    fclose(g_yp_state.output_file_fd);
    g_yp_state.output_file_fd = fopen(g_yp_config.output_file_path, "w+");
    if (!g_yp_state.output_file_fd) {
        printf("Open output file %s failed!\n", g_yp_config.output_file_path);
        exit(1);
    }
    printf("ypipe on %s, output cleared!\n", g_yp_config.output_file_path);
}
