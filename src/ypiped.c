#include "ypiped.h"

YpipeState  g_yp_state;
YpipeConfig g_yp_config;

const char *yp_dir = "~/.ypipe";
const char *yp_pid_file_format = "~/.ypipe/%d.pid";
const char *yp_fifo_file_format = "~/.ypipe/%d";

/* function prototypes */
/* for luncher */
void usage();
void readConfig(int argc, char *argv[]);
void ypipeCmdOpen();
void ypipeCmdKill();
void ypipeCmdClear();
int  findSlot();
void writePid(pid_t pid, int slot);

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
    DIR   *d;
    int    slot;
    int    ret;

    if (chdir("~/") < 0) {
        printf("Change to your home directory failed, oops.\n");
        exit(1);
    }

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

    slot = findSlot();

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

        sprintf(g_yp_config.fifo_path, "~/.ypipe/%d", slot);

        /* now the real guts... */
        ypipeDaemon();
    }
    else {
        writePid(pid, slot);
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

int findSlot()
{
    int   i;
    char  tmpath[PATH_MAX];
    int   f;

    for (i=1; ; ++i) {
        sprintf(tmpath, yp_fifo_file_format, i);

        f = open(tmpath, O_RDONLY);

        if (f >= 0) {
            if (flock(f, LOCK_EX | LOCK_NB) != 0) {
                close(f);
                continue;
            }
            else {
                /* we can get a lock on this fifo, so use it */
                flock(f, LOCK_UN | LOCK_NB);
                close(f);
                break;
            }
        }
        else {
            /* this slot does not exist */
            break;
        }
    }

    return i;
}

void writePid(pid_t pid, int slot)
{
    char  tmpath[MAX_BUF_SIZE]; 
    FILE *f;

    sprintf(tmpath, yp_pid_file_format, slot);
    f = fopen(tmpath, "w+");
    fprintf(f, "%d\n", pid);
    fclose(f);
}

void usage()
{
    printf("usage:\n");
    printf("      ypiped -o <output_file>\n");
    printf("          open a ypipe.\n");
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
    /* strcpy(g_yp_config.fifo_path, argv[optind]); */
}
