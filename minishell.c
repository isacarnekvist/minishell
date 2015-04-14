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
#include "proc_clock.h"
/* #include <readline/readline.h>
 * #include <readline/history.h> */

#define CMD_MAX_LEN 80
#define TRUE 1
#define FALSE 0
#define READ_SIDE 0
#define WRITE_SIDE 1

#define SIGDET 1
/* If SIGDET = 1, then program will listen for signals from child processes
 * to determine that they finished running.
 * If SIGDET = 0, program will use waitpid */
#ifndef SIGDET
#define SIGDET 0
#endif

/* Signal handler for SIGCHLD */
void child_listener(int sig);
/* Catches SIGINT */
void sig_handler(int sig);

/* This struct is used to send timing stats when process finished */
typedef struct ptt {
    pid_t   pid;
    int     delta_millis;
    int     was_background;
} proc_time_t ;

/* A pipe to send proc_time_t through when processes finish */
int proc_time_pipe[2];

void interpret(char **, int);

/* Sets everything up, loop for checking input and printing stats */
int main() {
    char *input_string;
    char **args;
    size_t linecap;
    int read_length;
    /* int return_val; */
    int is_background;
    struct pollfd time_poll;
    proc_time_t proc_time;
    char prompt[] = "> ";

    linecap = CMD_MAX_LEN;

    /* Set up timing stats pipe */
    if(-1 == pipe(proc_time_pipe)) {
        perror("pipe");
        exit(-1);
    }

    /* Set up signal handling */
    if(SIGDET) {
        if(SIG_ERR == signal(SIGCHLD, child_listener)) {
            perror("signal");
            exit(-1);
        }
    }
    if(SIG_ERR == signal(SIGINT, sig_handler)) {
        perror("signal");
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
        input_string = NULL;
        /* input_string = readline(prompt); */
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
        while(poll(&time_poll, 1, 0)) { 
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
    struct timeval start_time;
    struct timeval stop_time;
    proc_time_t proc_time;
    char *home;

    /* Check for built in commands 
     * - minor bug: 'cd .. foo' is interpreted as 'cd ..' */
    if(strcmp(args[0], "cd") == 0) {

        /* cd to given directory, if fail, go to $HOME */
        if(-1 == chdir(args[1])) {
            home = getenv("HOME");
            if(-1 == chdir(home)) {
                perror("chdir");
                /* Not necessary to exit program here right? */
            }
        }

    } else if(strcmp(args[0], "exit") == 0) {

        printf("Goodbye!\n");
        /* TODO Keep track of started processes and kill any non-terminated
         * processes before quitting? */
        exit(0);

    } else {
        if(SIGDET) {
            /* Do not wait for background processes, let child_listener do this */
            pid = vfork();
            if(0 == pid) {

                /* in child, execute the command */
                start(getpid());
                execvp(args[0], args);

                /* If we get here, execlp returned -1 */
                perror("exec");
                exit(-1);

            } else {
                /* In parent */
                if(!is_background) {
                    waitpid(pid, NULL, 0);
                }
            }

        } else {
            /* ## Try to execute given command ##
             * Since SIGDET = 0, this 'version' waits for commands to finish executing
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
                    gettimeofday(&start_time, NULL);
                    /* This wait should not be done if SIGDET = 1 */
                    waitpid(pid, NULL, 0);
                    /* Stop timer */
                    gettimeofday(&stop_time, NULL);
                    
                    /* Prepare stats to send */
                    proc_time.pid = pid;
                    proc_time.delta_millis = (stop_time.tv_sec - start_time.tv_sec)*1000 +
                                             (stop_time.tv_usec - start_time.tv_usec)/1000;
                    proc_time.was_background = is_background;

                    /* Send */
                    if(-1 == close(proc_time_pipe[READ_SIDE])) {
                        perror("close"); exit(-1);
                    }
                    if(-1 == write(proc_time_pipe[WRITE_SIDE], &proc_time, sizeof(proc_time))) {
                        perror("write"); exit(-1);
                    }
                    if(-1 == close(proc_time_pipe[WRITE_SIDE])) {
                        perror("close"); exit(-1);
                    }

                    /* The array of arguments was allocated and must be freed */
                    free_args(args);
                    exit(0);
                }
            } else {
                /* If background process, process table is cleared in main()
                 * when that process finishes */
                if(!is_background) {
                    /* Wait for process if foreground */
                    waitpid(pid, NULL, 0);
                } else {
                    printf("Process %d started in background\n", pid);
                }
            }
        }
    }
}

/* Signal handler for SIGCHLD. Checks if the signal was due to a terminated
 * process. If it was, acknowledge entry in process tabel via wait() */
void child_listener(int sig) {
    pid_t pid;
    int status, delta;
    proc_time_t proc_time;

    /* See what process signaled */
    pid = wait(&status);
    /* TODO Check why it signaled
     * TODO Print stats */
    if(WIFEXITED(status)) {
        /* Prepare stats to send */
        delta = stop(pid);
        proc_time.pid = pid;
        proc_time.delta_millis = delta;
        /* The reason for FALSE is that it is no longer a zombie process */
        proc_time.was_background = FALSE;

        /* Report! */
        if(0 == write(proc_time_pipe[WRITE_SIDE], &proc_time, sizeof(proc_time))) {
            perror("write");
            exit(-1);
        }
    } else {
        /* Nothing to do, maybe next time! */
    }
}

/* Makes sure Ctrl-C in child process does not kill miniShell
 * at the moment also ignores if it happens in 'prompt' mode */
void sig_handler(int sig) {
    if(SIGINT == sig) {
        /* Do nothing, possible to know if sent during child process? */
    }
}
