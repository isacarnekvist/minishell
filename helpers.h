#ifndef _HELPERS_H
#define _HELPERS_H

/* Takes the string the user inputted and creates an array of the tokens in it */
char **args_tokenized(char *input_string);

/* Frees all tokens in argument and finally the array itself */
void free_args(char **args);

#endif
