/* This is a simple shell with built-in commands cd and exit. 'digenv' is
 * assumed to be in the PATH variable since this is an external program. */
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <poll.h>
#include "helpers.h"
#include <readline/readline.h>
#include <readline/history.h>

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
    int read_length;
    /* int return_val; */
    int is_background, status, pid;
    struct pollfd time_poll;
    proc_time_t proc_time;
    char prompt[] = "> ";

    /* Set up timing stats pipe */
    if(-1 == pipe(proc_time_pipe)) {
        perror("pipe");
        exit(-1);
    }

    /* Set up signal handling */
    if(SIGDET) {
        if(-1 == (long)sigset(SIGCHLD, child_listener)) {
            perror("sigset");
            exit(-1);
        }
    }

    if(-1 == sigignore(SIGINT)) {
        perror("sigignore");
    }

    /* Set up polling of proc_time_pipe */
    proc_time.pid = 0;
    proc_time.delta_millis = 0;
    time_poll.fd = proc_time_pipe[READ_SIDE];
    time_poll.events = POLLIN;

    printf("           _       _ __ _          _ _ \n"); 
    printf(" _ __ ___ (_)_ __ (_) _\\ |__   ___| | |\n"); 
    printf("| '_ ` _ \\| | '_ \\| \\ \\| '_ \\ / _ \\ | |\n"); 
    printf("| | | | | | | | | | |\\ \\ | | |  __/ | |\n"); 
    printf("|_| |_| |_|_|_| |_|_\\__/_| |_|\\___|_|_| 2.0\n"); 
    printf("Author: Isac Arnekvist\n");

    while(1) {
        /* Read input */
        input_string = NULL;
        input_string = readline(prompt);
        if(input_string == NULL) {
            read_length = -1;
        } else {
            read_length = strlen(input_string);
        }

        switch (read_length) {
            case -1:
                /* If only eof was entered or something went wrong, quietly quit
                 * (this is bash behavior) */
                exit(0);
                break;
                return 0;
            case 0:
                /* Empty line, new prompt */
                break;
            default:
                /* Handle '&' first. Might not be space separated, so easier to
                 * do here */
                if('&' == input_string[read_length-1]) {
                    /* '&' acknowledged, change to null */
                    input_string[read_length-1] = '\0';
                    is_background = TRUE;
                } else {
                    is_background = FALSE;
                }

                /* Get array of input tokens */
                args = args_tokenized(input_string);
                interpret(args, is_background); 
        }

        /* Check if any process sent stats over pipe after they terminated */
        while(poll(&time_poll, 1, 0)) { 
            if(-1 == read(proc_time_pipe[READ_SIDE], &proc_time, sizeof(proc_time))) {
                perror("read");
                exit(-1);
            }
            /* Print stats */
            printf("Process %d terminated.\n", proc_time.pid);
        }
        if(NULL != input_string) free(input_string);

        /* If polling (SIGDET = 0), check if any processes finished running in
         * the background */
        if (1 != SIGDET) {
            while((pid = waitpid(-1, &status, WNOHANG)) > 1) {
                if(!WIFSIGNALED(status)) {
                    printf("Process %d terminated.\n", pid);
                }
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
        /* No built in command was entered, so now we try to execute the command */
        if(SIGDET) {

            /* Do not wait for background processes, let child_listener handle
             * them */

            if(!is_background) {
                /* Block any signals from background processes during
                 * foreground process */
                sighold(SIGCHLD);
            }

            pid = fork();
            if(0 == pid) {

                /* Set background processes to a new process group id to make
                 * them not react to Ctrl-C but still to SIGINT via pkill */
                if(is_background) {
                    if(-1 == setsid()) {
                        perror("setsid");
                        exit(-1);
                    } 
                }
                /* Reset SIGINT handling */
                if(-1 == (long)sigset(SIGINT, SIG_DFL)) {
                    perror("sigset");
                    exit(-1);
                }
                execvp(args[0], args);

                /* If we get here, execlp returned -1 */
                perror("exec");
                exit(-1);

            } else {
                /* In parent */
                if(!is_background) {
                    /* In parent of executing process */
                    /* Start timer */
                    gettimeofday(&start_time, NULL);
                    if(-1 == waitpid(pid, NULL, 0)) {
                        perror("waitpid");
                        exit(-1);
                    }

                    /* Reset signal handling */
                    sigrelse(SIGCHLD);

                    /* Stop timer */
                    gettimeofday(&stop_time, NULL);

                    printf("Process %d terminated, %ld milliseconds.\n", 
                            pid,
                            (stop_time.tv_sec - start_time.tv_sec)*1000 +
                            (stop_time.tv_usec - start_time.tv_usec)/1000
                          );
                } else {
                    /* Process was background and signal when terminated should be catched
                     * by child_listener */
                     printf("Process %d started in background\n", pid);
                }
            }

        } else {
            /* Try to execute given command with polling */
            pid = fork();
            if(0 == pid) {

                /* in child of shell, execute the command */

                /* Set background processes to a new process group id to make
                 * them not react to Ctrl-C but still to SIGINT via pkill */
                if(is_background) {
                    if(-1 == setsid()) {
                        perror("setsid");
                        exit(-1);
                    } 
                }
                /* Reset SIGINT handling */
                if(-1 == (long)sigset(SIGINT, SIG_DFL)) {
                    perror("sigset");
                    exit(-1);
                }
                execvp(args[0], args);

                /* If we get here, execlp returned -1 */
                perror("exec");
                exit(-1);

            } else {
                /* In parent */
                /* Start timer */
                if(!is_background) {
                    gettimeofday(&start_time, NULL);
                    waitpid(pid, NULL, 0);
                    /* Stop timer */
                    gettimeofday(&stop_time, NULL);
                    
                    /* Prepare stats to send */
                    proc_time.pid = pid;
                    printf("Process %d terminated, %ld milliseconds.\n", 
                            pid,
                            (stop_time.tv_sec - start_time.tv_sec)*1000 +
                            (stop_time.tv_usec - start_time.tv_usec)/1000
                          );

                } else {
                    /* Continue to prompt and poll from there for finished
                     * processes */
                }
            }
        }
    }
    /* The array of arguments was allocated and must be freed */
    free_args(args);
}

/* Signal handler for SIGCHLD. Checks if the signal was due to a terminated
 * process. If it was, acknowledge entry in process tabel via wait() */
void child_listener(int sig) {
    pid_t pid;
    int status;
    proc_time_t proc_time;

    /* TODO report once per process in process table */
    while((pid = waitpid(-1, &status, WNOHANG)) > 1) {
        if(!WIFSIGNALED(status)) {
            /* Prepare stats to send */
            proc_time.pid = pid;
            proc_time.delta_millis = 0;
            proc_time.was_background = TRUE;

            /* Report! */
            if(-1 == write(proc_time_pipe[WRITE_SIDE], &proc_time, sizeof(proc_time))) {
                perror("write");
                exit(-1);
            }
        } else {
            /* Nothing to do, maybe next time!? */
            printf("Signal %d terminated process %d\n", WTERMSIG(status), pid);
        }
    }
    return;

    /* Reset signal handler, might not be needed? */
    if(SIG_ERR == signal(SIGCHLD, child_listener)) {
        perror("signal");
        exit(-1);
    }
}
