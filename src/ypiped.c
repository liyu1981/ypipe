#include "ypiped.h"

YpipeState  g_yp_state;
YpipeConfig g_yp_config;

const char *g_homedir;

const char *yp_dir = "%s/.ypipe";
const char *yp_pid_file_format = "%s/.ypipe/%d.pid";
const char *yp_fifo_file_format = "%s/.ypipe/%d";

#define CurrentUserHome ((getpwuid(getuid()))->pw_dir)

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
    char   tmpath[MAX_BUF_SIZE];

    g_homedir = CurrentUserHome;

    if ((ret = chdir(g_homedir)) < 0) {
        printf("Change to your home directory failed, oops.\n");
        exit(errno);
    }

    sprintf(tmpath, yp_dir, g_homedir);
    d = opendir(tmpath);
    if (d == NULL) {
        ret = mkdir(tmpath, S_IRWXU);
        if (ret != 0) {
            printf("Create dir %s failed with code %d, oops.\n", yp_dir, ret);
            exit(errno);
        }
    }
    else
        closedir(d);

    slot = findSlot();

    if ((pid = fork()) == 0) {
        /* in child now */
        umask(0);

        sid = setsid();
        if (sid < 0) {
            printf("Create a new SID for ypipe daemon failed, oops.\n");
            exit(errno);
        }

        close(STDIN_FILENO);
        /* close(STDOUT_FILENO); */
        close(STDERR_FILENO);

        sprintf(g_yp_config.fifo_path, "%s/.ypipe/%d", g_homedir, slot);

        /* now the real guts... */
        ypipeDaemon();
    }
    else {
        if (pid < 0) {
            printf("Fork ypipe daemon failed, oops.\n");
            exit(errno);
        }

        writePid(pid, slot);
        printf("Ypipe started with id=%d.\n", slot);
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
    int    i;
    char   tmpath[PATH_MAX];
    int    f;
    mode_t mask;

    for (i=1; ; ++i) {
        sprintf(tmpath, yp_fifo_file_format, g_homedir, i);

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
            mask = umask(0);
            if (mkfifo(tmpath, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) < 0) {
                printf("Create named pipe %s failed, oops.\n", tmpath);
                exit(errno);
            }
            umask(mask);
            break;
        }
    }

    return i;
}

void writePid(pid_t pid, int slot)
{
    char  tmpath[MAX_BUF_SIZE]; 
    FILE *f;

    sprintf(tmpath, yp_pid_file_format, g_homedir, slot);
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
     
    memset(g_yp_config.fifo_path, '\0', PATH_MAX);
}
