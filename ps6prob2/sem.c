#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sched.h>
#include "sem.h"
#include "spinlock.h"

bool already_waiting(pid_t pid, struct sem *s) {
    for (int i = 0; i < s->tasks_waiting_count; i++) {
        if (s->tasks_waiting[i] == pid) {
            return true;
        }
    }
    return false;
}

void sem_init(struct sem *s, int count) {
    s->sem_count = count;
    s->tasks_waiting_count = 0;

    s->lock = mmap(NULL, sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (s->lock == MAP_FAILED) {
        perror("Failed mmap");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUM_TASKS; i++) {
        s->tasks_waiting[i] = 0;
        s->sleep_count[i] = 0;
        s->wake_count[i] = 0;
    }

    *(s->lock) = 0;
}

// Non-blocking P operation
int sem_try(struct sem *s) {
    spin_lock(s->lock);
    if (s->sem_count > 0) {
        s->sem_count--;
        spin_unlock(s->lock);
        return 0;
    }
    spin_unlock(s->lock);
    return -1;
}

void sem_wait(struct sem *s, int whichtask) {
    sigset_t waitmask, oldmask;

    //block SIGUSR1 before adding to tasks_waiting
    sigemptyset(&waitmask);
    sigaddset(&waitmask, SIGUSR1);

    spin_lock(s->lock);

    while (1) {
        if (s->sem_count > 0) {
            s->sem_count--;
            spin_unlock(s->lock);
            return;
        }

        sigprocmask(SIG_BLOCK, &waitmask, &oldmask);

        pid_t pid = getpid();
        if (s->tasks_waiting_count < NUM_TASKS && !already_waiting(pid, s)) {
            s->tasks_waiting[s->tasks_waiting_count++] = pid;
            s->sleep_count[whichtask]++;
        }
        spin_unlock(s->lock);
        //sleep
        sigsuspend(&oldmask);

        spin_lock(s->lock);
        sigprocmask(SIG_UNBLOCK, &waitmask, NULL);

        sched_yield();
    }
}


void sem_inc(struct sem *s, int whichtask) { 
    spin_lock(s->lock);
    s->sem_count++;

    while (s->tasks_waiting_count > 0) {
        int woken_task = s->tasks_waiting[0];
        //wake
        kill(woken_task, SIGUSR1);
        s->wake_count[whichtask]++;
        
        //shift tasks in queue
        for (int i = 1; i < s->tasks_waiting_count; i++) {
            s->tasks_waiting[i - 1] = s->tasks_waiting[i];
        }
        s->tasks_waiting_count--;
    }

    spin_unlock(s->lock);
}



