rm -f minishell
gcc -pedantic -Wall -ansi -O4 -o minishell minishell.c helpers.c proc_clock.c
./minishell
