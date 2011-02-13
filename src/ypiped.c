#include "ypiped.h"

YpipeState  g_yp_state;
YpipeConfig g_yp_config;

const char *yp_dir = "~/.ypipe";
const char *yp_pid_file_format = "~/.ypipe/%d.pid";

/* function prototypes */
/* for luncher */
void usage();
void readConfig(int argc, char *argv[]);
void ypipeCmdOpen();
void ypipeCmdKill();
void ypipeCmdClear();
void init();
void signal_term(int signum);
void signal_usr2(int signum);
int  writePid(pid_t pid);

/* implementations */
int main(int argc, char *argv[])
{
    readConfig(argc, argv);

    switch(g_yp_config.cmd) {
    case YP_OPEN:
        ypipeCmdOpen();
        break;
    case YP_CLEAR:
        ypipeCmdClear();
        break;
    case YP_KILL:
        ypipeCmdKill();
        break;
    }

    return 0;
}

void ypipeCmdOpen()
{
    pid_t  pid, sid;
    FILE  *d;
    int    ret;

    init();

    if (chdir("~/") < 0) {
        printf("Change to your home directory failed, oops.\n");
        exit(1);
    }

    if ((pid = fork()) != 0) {
        /* in child now */
        if (pid < 0) {
            printf("Fork ypipe daemon failed, oops.\n");
            exit(1);
        }

        umask(0);

        sid = setsid();
        if (sid < 0) {
            printf("Create a new SID for ypipe daemon failed, oops.\n");
            exit(1);
        }

        close(STDIN_FILENO);
        /* close(STDOUT_FILENO); */
        close(STDERR_FILENO);

        /* now the real guts... */
        ypipeDaemon();
    }
    else {
        d = opendir(yp_dir);
        if (d == NULL) {
            ret = mkdir(yp_dir, S_IREAD | S_IWRITE);
            if (ret != 0) {
                printf("Create dir %s failed with code %d, oops.\n", yp_dir, ret);
                exit(ret);
            }
        }
        else
            closedir(d);

        ret = writePid(pid);
        printf("Ypipe started with id=%d.\n", ret);
    }
}

void ypipeCmdKill()
{
    kill(g_yp_config.pid_to_kill, SIGTERM);
    printf("SIGTERM sent to process %d.\n", g_yp_config.pid_to_kill);
}

void ypipeCmdClear()
{
    kill(g_yp_config.pid_to_kill, SIGUSR2);
}

int writePid(pid_t pid)
{
    int   i;
    char  tmpath[PATH_MAX];
    char  tmpstr[MAX_BUF_SIZE];
    pid_t p;
    FILE *f;

    for (i=1; ; ++i) {
        sprintf(tmpath, yp_pid_file_format, i);

        f = fopen(tmpath, "r");
        if (f == NULL)
            continue;

        if (fgets(tmpstr, MAX_BUF_SIZE, f) == NULL) {
            /* empty pid file => no process associated => we can use it */
            fclose(f);
            break;
        }
        else {
            if (sscanf(tmpstr, "%d", &p) <= 0) {
                /* no valid pid => no process associated => we can use it */
                fclose(f);
                break;
            }
            else {
                if (kill(p, SIGUSR1) < 0) {
                    /* this process no longer exist, we can use it */
                    fclose(f);
                    break;
                }
                else {
                    /* There is a process running, skip to next */
                    fclose(f);
                    continue;
                }
            }
        }
    }

    sprintf(tmpath, yp_pid_file_format, i);
    f = fopen(tmpath, "w+");
    fprintf(f, "%d\n", pid);
    fclose(f);

    return i;
}

void usage()
{
    printf("usage:\n");
    printf("      ypiped -o <output_file> <fifo>\n");
    printf("          open a ypipe based on <fifo>.\n");
    printf("      ypiped -k <id>\n");
    printf("          kill ypipe with <id>.\n");
    printf("      ypiped -c <id>\n");
    printf("          clear the output of ypipe with <id>.\n");
}

void readConfig(int argc, char *argv[])
{
    int c;

    opterr = 0;
     
    while ((c = getopt (argc, argv, "c:o:k:")) != -1) {
        switch (c)
        {
        case 'c':
            g_yp_config.cmd = YP_CLEAR;
            g_yp_config.pid_to_kill = atoi(optarg);
            break;
        case 'o':
            g_yp_config.cmd = YP_OPEN;
            memset(g_yp_config.output_file_path, '\0', PATH_MAX);
            g_yp_config.output = 1;
            strcpy(g_yp_config.output_file_path, optarg);
            break;
        case 'k':
            g_yp_config.cmd = YP_KILL;
            g_yp_config.pid_to_kill = atoi(optarg);
            break;
        case '?':
        default:
            usage();
        exit(1);
        }
    }
     
    if (argc - optind < 1) {
        usage();
        exit(1);
    }

    memset(g_yp_config.fifo_path, '\0', PATH_MAX);
    strcpy(g_yp_config.fifo_path, argv[optind]);
}

void init()
{     
    g_yp_state.terminate = 0;

    g_yp_state.fifo_fd = open(g_yp_config.fifo_path, O_RDONLY);
    if (!g_yp_state.fifo_fd) {
        printf("Open named pipe %s error!\n", g_yp_config.fifo_path);
        exit(1);
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
            exit(1);
        }
    }

    if (signal(SIGTERM, signal_term) == SIG_ERR) {
        printf("Error setting up catching signal SIGTERM.\n");
        exit(1);
    }

    if (signal(SIGUSR2, signal_usr2) == SIG_ERR) {
        printf("Error setting up catching signal SIGUSR2.\n");
        exit(1);
    }
}

