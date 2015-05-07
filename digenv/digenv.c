/*
 * This program lists environment variables. The results can be filtered to
 * only show lines containing parameter regex. The listing is done using 'less'
 * unless environment variable PAGER is set to another pager. If less is not
 * existing, more is used.
 *
 * Usage: digenv [parameter list]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#define READ_SIDE 0
#define WRITE_SIDE 1

/* 
 * Redirects listen_pipe to stdin, send_pipe to stdout
 * If an argument set to null, that argument will be ignored
 */
void setup_pipe(int *listen_pipe, int *send_pipe);

/* Does 'waitpid(pid, ...)' then checks exit status and quits if abnormal */
void checkpid(char *name, pid_t pid);

int main(int argc, char **argv, char **envp) {
    /* Declaration of all variables to be used */
    int return_value;
    int printenv_grep_pipe[2], grep_sort_pipe[2], sort_pager_pipe[2];  
    int printenv_pid, grep_pid, sort_pid, pager_pid;
    int status;
    char *pager;

    /* 
     * Let environment variable 'PAGER' decide how results should be listed. If
     * not set, use 'less' 
     */
    pager = getenv("PAGER");
    if (pager == NULL) {
        pager = "less";
    }

    /* Set up all pipes */
    return_value = pipe(printenv_grep_pipe);
    if(return_value == -1) { return -1; }
    return_value = pipe(grep_sort_pipe);
    if(return_value == -1) { return -1; }
    return_value = pipe(sort_pager_pipe);
    if(return_value == -1) { return -1; }

    /* Create all childs */
    /* Run printenv and pipe output to grep */
    printenv_pid = fork();
    if(printenv_pid == 0) {
         
        /* Pipe output to grep process */
        setup_pipe(NULL, printenv_grep_pipe);

        /* Change process image */
        execlp("printenv", "printenv", NULL); 
        
        /* We only get here if something went wrong */
        exit(0);
    } 
    else if(printenv_pid == -1) {
        exit(-1);
    }

    /* Listen to printenv, grep, and pipe output to sort */
    grep_pid = fork();
    if(grep_pid == 0) {

        /* pipe to stdin */
        setup_pipe(printenv_grep_pipe, grep_sort_pipe);

        /* Change process image */
        if(argc == 1) {
            /* If no arguments, send '.' to grep */ 
            execlp("grep", "grep", ".", NULL);
        } else {
            char arg1[] = "grep";
            argv[0] = arg1;
            execvp("grep", argv);
        }
        
        /* We only get here if something went wrong */
        exit(-1);
    } 
    else if(grep_pid == -1) {
        exit(-1);
    }

    /* Close first pipe to let sort start */
    close(printenv_grep_pipe[READ_SIDE]);
    close(printenv_grep_pipe[WRITE_SIDE]);

    /* Listen to grep, sort, and pipe output to pager */
    sort_pid = fork();
    if(sort_pid == 0) {

        /* pipe to stdin */
        setup_pipe(grep_sort_pipe, sort_pager_pipe);

        /* Change process image */
        execlp("sort", "sort", NULL);
        
        /* We only get here if something went wrong */
        exit(-1);
    } 
    else if(sort_pid == -1) {
        exit(-1);
    }

    close(grep_sort_pipe[READ_SIDE]);
    close(grep_sort_pipe[WRITE_SIDE]);

    /* Listen to grep, sort, and pipe output to pager */
    pager_pid = fork();
    if(pager_pid == 0) {

        /* pipe to stdin */
        setup_pipe(sort_pager_pipe, NULL);

        /* Change process image */
        execlp(pager, pager, NULL);

        /* If 'less' fails, try 'more' */
        execlp("more", "more", NULL);
        
        /* We only get here if something went wrong */
        exit(-1);
    } 
    else if(pager_pid == -1) {
        exit(-1);
    }
   
    close(sort_pager_pipe[READ_SIDE]);
    close(sort_pager_pipe[WRITE_SIDE]);

    /* Wait for all processes and examine the nature of their exit */
    checkpid("printenv", printenv_pid); 

    /* Special case: grep returns with 1 if no lines matched */
    if(waitpid(grep_pid, &status, 0) == -1) {
        perror("waitpid");
        exit(-1);
    }
    if(!WIFEXITED(status)) { 
        fputs("grep process did not exit normally", stderr);
        exit(-1);
    } else {
        switch(WEXITSTATUS(status)) {
            case 0:
                /* One or more lines was selected */
                break;
            case 1:
                /* No lines were selected */
                break;
            default: 
                fputs("grep exited with error", stderr);
                exit(-1);
        }
    }

    checkpid("sort", sort_pid);
    checkpid("pager", pager_pid);

    return 0;
}

/* Does 'waitpid(pid, ...)' then checks exit status and quits if abnormal */
void checkpid(char *name, pid_t pid) {
    int status;
    if(waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        exit(-1);
    }
    if(!WIFEXITED(status)) { 
        fputs(name, stderr);
        fputs(": process did not exit normally", stderr);
        exit(-1);
    } else {
        if(WEXITSTATUS(status)) {
            fputs(name, stderr);
            fputs(": did not exit with status 0", stderr);
            exit(-1);
        }
    }
}

/* 
 * Redirects listen_pipe to stdin, send_pipe to stdout
 * If an argument set to null, that argument will be ignored
 */
void setup_pipe(int *listen_pipe, int *send_pipe) {
     /* listen_pipe to stdin */
     if(listen_pipe != NULL) {
         close(listen_pipe[WRITE_SIDE]);     
         if(dup2(listen_pipe[READ_SIDE], STDIN_FILENO) == -1) {
             fputs("Pipe couldn't be redirected", stderr);
             exit(-1);
         }
         close(listen_pipe[READ_SIDE]);     
     }

     /* send_pipe to stdout */
     if(send_pipe != NULL) {
         close(send_pipe[READ_SIDE]);     
         if(dup2(send_pipe[WRITE_SIDE], STDOUT_FILENO) == -1) {
             fputs("Pipe couldn't be redirected", stderr);
             exit(-1);
         }
         close(send_pipe[WRITE_SIDE]);     
     }
}
