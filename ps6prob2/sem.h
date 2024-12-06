#ifndef SEM_H
#define NUM_TASKS 6

#include "spinlock.h"

struct sem {
    //initial count
    int sem_id;
    int sem_count;
    int tasks_waiting_count;
    int tasks_waiting[NUM_TASKS];

    int sleep_count[NUM_TASKS];
    int wake_count[NUM_TASKS];

    volatile char *lock;
};

void sem_init(struct sem *s, int count, int sem_id);
int sem_try(struct sem *s);
void sem_wait(struct sem *s, int whichtask);
void sem_inc(struct sem *s, int whichtask);
#endif