/* This is a simple shell with built-in commands cd and exit. 'digenv' is
 * assumed to be in the PATH variable since this is an external program. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
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

/* If SIGDET = 1, then program will listen for signals from child processes
 * to determine that they finished running.
 * If SIGDET = 0, program will use waitpid */
#ifndef SIGDET
#define SIGDET 0
#endif

/* TODO Bug: run 'cat', terminate with Ctrl-C. This will also terminate minishell. Sighandler? */

/* This struct is used to send timing stats when process finished */
typedef struct ptt {
    pid_t   pid;
    int     delta_millis;
    int     was_background;
} proc_time_t ;

/* A pipe to send proc_time_t through when processes finish */
int proc_time_pipe[2];

void interpret(char **, int);

/* Sets everything up, loops for checking input and printing stats */
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

    printf("This is miniShell 2.0\n");
    printf("Author: Isac Arnekvist\n");

    while(1) {
        /* Read input */
        printf("> ");
        read_length = getline(&input_string, &linecap, stdin);
        input_string[read_length - 1] = '\0';
        switch (read_length) {
            case -1:
                /* If only eof was entered or something went wrong, quietly quit
                 * (this is bash behavior) */
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

        /* 1: Check if any process sent stats over pipe after they terminated 
         * 2: If it was backgrond, clean up process table */
        while(poll(&time_poll, 1, 0)) { /* maybe &time_poll is wrong? */
            /* This line has problems when compiled on u-shell.csc.kth.se.
             * Gives segmentation error even when inside of loop is not run */
            if(-1 == read(proc_time_pipe[READ_SIDE], &proc_time, sizeof(proc_time))) {
                perror("read");
                exit(-1);
            }
            /* Print stats */
            printf("Process %d terminated, %d milliseconds.\n",
                    proc_time.pid,
                    proc_time.delta_millis);

            if(proc_time.was_background) {
                /* Acknowledge from process table */
                wait(NULL);
            }
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
    char *home;

    /* Check for built in commands 
     * - minor bug: 'cd .. foo' is interpreted as 'cd ..' */
    if(strcmp(args[0], "cd") == 0) {

        /* cd to given directory, if fail, go to $HOME */
        if(-1 == chdir(args[1])) {
            home = getenv("HOME");
            chdir(home);
        }

    } else if(strcmp(args[0], "exit") == 0) {

        printf("Goodbye!\n");
        /* TODO Keep track of started processes and kill any non-terminated
         * processes before quitting */
        exit(0);

    } else {
        /* ## Try to execute given command ##
         * Parent: fork, wait only if foreground
         *   |    
         * Child: start time, fork, waitpid, stop time, send proc_time through pipe to parent
         *   |        
         * Baby: execute      
         */
        pid = fork();
        if(0 == pid) {
            pid = vfork();
            if(0 == pid) {

                /* in child, execute the command */
                execvp(args[0], args);

                /* If we get here, execlp returned -1 */
                perror("exec");
                exit(-1);

            } else {
                /* In parent of executing process */
                /* Start timer */
                gettimeofday(&start, NULL);
                waitpid(pid, NULL, 0);
                /* Stop timer */
                gettimeofday(&stop, NULL);
                
                /* Prepare stats to send */
                proc_time.pid = pid;
                proc_time.delta_millis = (stop.tv_sec - start.tv_sec)*1000 +
                                         (stop.tv_usec - start.tv_usec)/1000;
                proc_time.was_background = is_background;

                /* Send */
                close(proc_time_pipe[READ_SIDE]);
                write(proc_time_pipe[WRITE_SIDE], &proc_time, sizeof(proc_time));
                close(proc_time_pipe[WRITE_SIDE]);

                /* The array of arguments was allocated and must be freed */
                free_args(args);
                exit(0);
            }
        } else {
            /* If background process, process table is cleared in main()
             * when that process finishes */
            if(!is_background) {
                waitpid(pid, NULL, 0);
            } else {
                printf("Process %d started in background\n", pid);
            }
        }
    }
}
