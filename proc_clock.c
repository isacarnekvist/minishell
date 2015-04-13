/* This file is simply a linked list, but implemented so that a timer starts
 * when a pid is added, and a time delta in ms is returned when the element is
 * deleted */
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include "proc_clock.h"

typedef struct pt {
    pid_t           pid;
    struct timeval  time;
    struct pt       *prev;
    struct pt       *next;
} pid_time_t;

pid_time_t *root = NULL;

/* Starts a timer for the given pid */
void start(pid_t pid) {
   pid_time_t *new_p;

   new_p = malloc(sizeof(pid_time_t));
   new_p->pid = pid;
   new_p->prev = NULL;
   new_p->next = NULL;
   gettimeofday(&new_p->time, NULL);

   if(NULL == root) {
       root = new_p;
   } else {
       new_p->next = root;
       root->prev = new_p;
       root = new_p;
   }

}

/* Finds pid, frees memory, returns delta time */
int find_pid(pid_t pid, pid_time_t *pt) {
    int delta;
    struct timeval now;

    /* Non existing pid was requested, 'ugly hack'. The process might have terminated before
     * an entry was added */
    if(pt == NULL) {
        return -1;
    }

    if(pid == pt->pid) {
        /* Found link in chain, calculate time */
        gettimeofday(&now, NULL);
        delta = (now.tv_sec - pt->time.tv_sec)*1000 +
                (now.tv_usec - pt->time.tv_usec)/1000;

        /* Remove from list, fix linked list, free memory */
        /* Three cases EXE 0XE EX0 0X0 E=element, X=found element, 0 = NULL*/
        if(NULL == pt->prev) {
            /* 0** */
            if(NULL == pt->next) {
                /* 0X0 */
                root = NULL;
            } else {
                /* 0XE */
                root = pt->next;
                root->prev = NULL;
            }
        } else {
            /* E** */
            if(NULL == pt->next) {
                /* EX0 */
                pt->prev->next = NULL;
            } else {
                /* EXE */
                pt->prev->next = pt->next;
                pt->next->prev = pt->prev;
            }
        }

        free(pt);

        return delta;
    } else {

        /* Recurse down list */
        return find_pid(pid, pt->next);
    }
}

/* Stops the timer for the given pid and returns time in milliseconds */
int stop(pid_t pid) {
    return find_pid(pid, root);
}

