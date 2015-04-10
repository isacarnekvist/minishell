/* These are a collection of helper function to minishell. Explanations can be
 * read before each function definition */
#include "helpers.h"
#include <stdlib.h>
#include <string.h>

char *strtok(char*, const char*);

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

