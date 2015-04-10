/* This is a simple shell with built-in commands cd and exit */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define CMD_MAX_LEN 80

void exec_foreground(char **); 
void exec_background(char **); 
char **args_tokenized(char *);

/* Without these, it says implicit declaration even though stdio.h is included? */
int getline(char**, size_t*, FILE*);
int vfork(void);

int main() {
    char *input_string;
    char **args;
    size_t linecap;
    int return_value;
    linecap = CMD_MAX_LEN;

    while(1) {
        /* Read input */
        printf("> ");
        return_value = getline(&input_string, &linecap, stdin);
        input_string[return_value - 1] = '\0';
        switch (return_value) {
            case -1:
                /* If eof was entered or something went wrong, quietly quit
                 * (eof is bash behavior) */
                return 0;
            case 1:
                /* Empty line, new prompt */
                break;
            default:

                /* Check for '&', if '&' -> run in background, else foreground */
                if(input_string[return_value-2] == '&') {
                    /* Now ignore the '&' character */
                    input_string[return_value-2] = '\0';

                    /* Get array of input tokens (w/o '&') */
                    args = args_tokenized(input_string);
                    exec_background(args);
                } else {
                    
                    /* Get array of input tokens */
                    args = args_tokenized(input_string);
                    exec_foreground(args); 

                }
                break;
        }
    }
    return 0;
}

/* Takes the string the user inputted and creates an array of the tokens in it */
char **args_tokenized(char *input_string) {
    /* Assume no more than 32 space separated tokens was entered */
    char **args = malloc(sizeof(char**) * 32);
    char *token;
    int i;

    /* Ugly hack to be able to know which elements to free */
    for(i = 0; i < 32; i++) {
        args[i] = NULL;
    }

    /* Take one token at a time, allocate and add to 'args' */
    token = strtok(input_string, " \t");
    for(i = 0; token != NULL; i++) {
        args[i] = malloc(strlen(token) + 1);
        strcpy(args[i], token);
        token = strtok(NULL, " \t");
    }

    return args;
}

/* Frees all tokens in argument and finally the array itself */
void free_args(char **args) {
    int i;
    for(i = 0; args[i] != NULL; i++) {
       free(args[i]);
    }
    free(args);
}

void exec_background(char **args) {
    /* TODO Do in background */
    int pid; 
    pid = vfork();
    if(pid == 0) {

        /* in child, execute the command */
        execvp(args[0], args);

        /* If we get here, execlp returned -1 */
        perror("exec");
        exit(-1);

    } else {
        waitpid(pid, NULL, 0);
        free_args(args);
    }

}

void exec_foreground(char **args) {
    int pid; 
    pid = vfork();
    if(pid == 0) {

        /* in child, execute the command */
        execvp(args[0], args);

        /* If we get here, execlp returned -1 */
        perror("exec");
        exit(-1);

    } else {
        waitpid(pid, NULL, 0);
        free_args(args);
    }
}
