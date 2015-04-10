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
    pid_t           pid;
    struct timeval  time;
} proc_time_t ;

/* A pipe to send proc_time_t through when processes finish */
int proc_time_pipe[2];

void interpret(char **, int);

/* Without these, it says implicit declaration even though stdio.h is included? */
int getline(char**, size_t*, FILE*);
int vfork(void);

int main() {
    char *input_string;
    char **args;
    size_t linecap;
    int read_length;
    /* int return_val; */
    int is_background;
    struct pollfd proc_time_pipe_poll;
    proc_time_t proc_time;
    linecap = CMD_MAX_LEN;

    /* Set up timing stats pipe */
    if(-1 == pipe(proc_time_pipe)) {
        perror("pipe");
        exit(-1);
    }

    /* Set up polling of proc_time_pipe */
    proc_time_pipe_poll.fd = proc_time_pipe[READ_SIDE];
    proc_time_pipe_poll.events = POLLIN;

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
        while(poll(&proc_time_pipe_poll, 1, 0) != 0) {
            if(-1 == read(proc_time_pipe[READ_SIDE], &proc_time, sizeof(proc_time_t))) {
                perror("read");
                exit(-1);
            }
            /* Print stats */
        }
    }
    return 0;
}

/* Tries to execute the given commands. If is_background is set to TRUE, the
 * prompt will immediately return. If false, prompt will return when command is
 * done executing */
void interpret(char **args, int is_background) {
    int pid; 

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
            waitpid(pid, NULL, 0);
            free_args(args);
            exit(0);
        }
    } else {
        waitpid(pid, NULL, 0);
    }
}
