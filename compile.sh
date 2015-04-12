# -O3 skall vara 04 enligt peket
rm minishell
gcc -pedantic -Wall -ansi -O3 -o minishell minishell.c helpers.c
./minishell
