#ifndef _PROC_CLOCK_H
#define _PROC_CLOCK_H

/* Starts a timer for the given pid */
void start(pid_t pid);
/* Stops the timer for the given pid and returns time in milliseconds */
int stop(pid_t pid);

#endif
