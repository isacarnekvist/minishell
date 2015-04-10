# missing -ANSI arg
rm minishell
gcc -pedantic -Wall -ansi -O4 -o minishell minishell.c helpers.c
./minishell
