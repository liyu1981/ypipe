#include "ypiped.h"

YpipeState  g_yp_state;
YpipeConfig g_yp_config;

const char *g_homedir;

#define CurrentUserHome ((getpwuid(getuid()))->pw_dir)

/* function prototypes */
/* for luncher */
void usage();
void readConfig(int argc, char *argv[]);
void ypipeCmdOpen();
void ypipeCmdKill();
void ypipeCmdClear();

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

    if ((pid = fork()) == 0) {
        /* in child now */

        chdir("/");

        umask(0);

        sid = setsid();
        if (sid < 0) {
            printf("Create a new SID for ypipe daemon failed, oops.\n");
            exit(errno);
        }

        close(STDIN_FILENO);
        /* close(STDOUT_FILENO); */
        close(STDERR_FILENO);

        /* now the real guts... */
        ypipeDaemon();
    }
    else {
        if (pid < 0) {
            printf("Fork ypipe daemon failed, oops.\n");
            exit(errno);
        }

        printf("Ypipe started with pid=%d.\n", pid);
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

void usage()
{
    printf("usage:\n");
    printf("      ypiped -o <output_file> <fifo>\n");
    printf("          open a ypipe on <fifo>.\n");
    printf("      ypiped -k <pid>\n");
    printf("          kill ypipe with <pid>.\n");
    printf("      ypiped -c <pid>\n");
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

    if (g_yp_config.cmd == YP_OPEN) {
        if (argc - optind < 1) {
            usage();
            exit(1);
        }

        memset(g_yp_config.fifo_path, '\0', PATH_MAX);
        strcpy(g_yp_config.fifo_path, argv[optind]);
    }
}
