#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
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

void sem_init(struct sem *s, int count, int sem_id) {
    s->sem_id = sem_id;
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

/*
// Non-blocking P operation
int sem_try(struct sem *s) {
    spin_lock(s->lock);
    if (s->sem_count > 0) {
        s->sem_count--;
        spin_unlock(s->lock);
        return 0; // Success
    }
    spin_unlock(s->lock);
    return -1; // Failure
}
*/

//blocking P operation
void sem_wait(struct sem *s, int whichtask) {
    sigset_t waitmask, oldmask;
    pid_t pid = getpid();

    //block SIGUSR1 and save the old mask
    sigemptyset(&waitmask);
    sigaddset(&waitmask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &waitmask, &oldmask);

    spin_lock(s->lock);
    fprintf(stderr, "locked1 %d, sem_count: %d\n", s->sem_id, s->sem_count);

    while (s->sem_count <= 0) {
        if (s->tasks_waiting_count < NUM_TASKS && !already_waiting(pid, s)) {
            s->tasks_waiting[s->tasks_waiting_count++] = pid;
            s->sleep_count[whichtask]++;
        }
        spin_unlock(s->lock);
        fprintf(stderr, "unlocked1 %d, sem_count: %d\n", s->sem_id, s->sem_count);

        //wait for wakeup signal
        sigsuspend(&oldmask);
        spin_lock(s->lock);
        fprintf(stderr, "locked2 %d, sem_count: %d\n", s->sem_id, s->sem_count);
    }

    s->sem_count--;
    spin_unlock(s->lock);
    fprintf(stderr, "unlocked2 %d, sem_count: %d\n", s->sem_id, s->sem_count);

    sigprocmask(SIG_UNBLOCK, &oldmask, NULL);
}

//V operation
void sem_inc(struct sem *s, int whichtask) {
    spin_lock(s->lock);
    s->sem_count++;
    fprintf(stderr, "locked3 %d, sem_count: %d\n", s->sem_id, s->sem_count);

    if (s->tasks_waiting_count > 0) {
        for (int i = 0; i < s->tasks_waiting_count; i++) {
            kill(s->tasks_waiting[i], SIGUSR1);
            s->wake_count[whichtask]++;
        }
        s->tasks_waiting_count = 0;
    }

    spin_unlock(s->lock);
}
