/* This is a simple shell with built-in commands cd and exit */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <poll.h>
#include "helpers.h"

#define CMD_MAX_LEN 80
#define TRUE 1
#define FALSE 0
#define READ_SIDE 0
#define WRITE_SIDE 1

/* TODO Bug: run 'cat', terminate with Ctrl-C. This will also terminate minishell. Sighandler? */

/* This struct is used to send timing stats when process finished */
typedef struct ptt {
    pid_t   pid;
    int     delta_millis;
} proc_time_t ;

/* A pipe to send proc_time_t through when processes finish */
int proc_time_pipe[2];

void interpret(char **, int);

int main() {
    char *input_string;
    char **args;
    size_t linecap;
    int read_length;
    /* int return_val; */
    int is_background;
    struct pollfd time_poll;
    proc_time_t proc_time;

    linecap = CMD_MAX_LEN;

    /* Set up timing stats pipe */
    if(-1 == pipe(proc_time_pipe)) {
        perror("pipe");
        exit(-1);
    }

    /* Set up polling of proc_time_pipe */
    proc_time.pid = 0;
    proc_time.delta_millis = 0;
    time_poll.fd = proc_time_pipe[READ_SIDE];
    time_poll.events = POLLIN;

    while(1) {
        /* Read input */
        printf("> ");
        read_length = getline(&input_string, &linecap, stdin);
        input_string[read_length - 1] = '\0';
        switch (read_length) {
            case -1:
                /* If eof was entered or something went wrong, quietly quit
                 * (eof is bash behavior) */
                return 0;
            case 1:
                /* Empty line, new prompt */
                break;
            default:
                /* Handle '&' first. Might not be space separated, so easier to do here */
                if('&' == input_string[read_length-2]) {
                    /* '&' acknowledged, change to null */
                    input_string[read_length-2] = '\0';
                    is_background = TRUE;
                } else {
                    is_background = FALSE;
                }

                /* Get array of input tokens */
                args = args_tokenized(input_string);
                interpret(args, is_background); 
                break;
        }

        /* Check if any processes finished */
        while(poll(&time_poll, 1, 0)) { /* maybe &time_poll is wrong? */
            read(proc_time_pipe[READ_SIDE], &proc_time, sizeof(proc_time));
            /* Print stats */
            printf("Process %d terminated, %d milliseconds.\n",
                    proc_time.pid,
                    proc_time.delta_millis);
        }
    }
    return 0;
}

/* Tries to execute the given commands. If is_background is set to TRUE, the
 * prompt will immediately return. If false, prompt will return when command is
 * done executing */
void interpret(char **args, int is_background) {
    int pid; 
    struct timeval start;
    struct timeval stop;
    proc_time_t proc_time;

    /* TODO Figure out how to separate background/foreground */
    /* Parent: fork, wait only if foreground
     *   |    
     * Child: start time, fork, waitpid, stop time, send proc_time through pipe to parent
     *   |        
     * Baby: execute      
     */
    pid = fork();
    if(0 == pid) {
        pid = fork();
        if(0 == pid) {

            /* in child, execute the command */
            execvp(args[0], args);

            /* If we get here, execlp returned -1 */
            perror("exec");
            exit(-1);

        } else {
            /* Start timer here? */
            gettimeofday(&start, NULL);
            waitpid(pid, NULL, 0);
            /* Stop timer here? */
            gettimeofday(&stop, NULL);
            proc_time.pid = pid;
            proc_time.delta_millis = (stop.tv_sec - start.tv_sec)*1000 +
                                     (stop.tv_usec - start.tv_usec)/1000;
            close(proc_time_pipe[READ_SIDE]);
            write(proc_time_pipe[WRITE_SIDE], &proc_time, sizeof(proc_time));
            close(proc_time_pipe[WRITE_SIDE]);
            free_args(args);
            exit(0);
        }
    } else {
        waitpid(pid, NULL, 0);
    }
}
