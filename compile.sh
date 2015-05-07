rm -f minishell
gcc -lreadline -ltermcap -pedantic -Wall -ansi -O4 -o minishell minishell.c helpers.c
