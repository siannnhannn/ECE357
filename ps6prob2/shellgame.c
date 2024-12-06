#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sched.h>
#include <signal.h>
#include "sem.h"
#include "spinlock.h"

#define NUM_TASKS 6

typedef struct shared {
    struct sem sem_a;
    struct sem sem_b;
    struct sem sem_c;
} shared;

int whichtask;
int sig_count[NUM_TASKS];
shared *shared_sems;

void setup_signal_handler();
void handler(int signo);
void print_info(const char *sem_name, struct sem *sem);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <initial_pebbles> <initial_moves>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int init_pebbles = atoi(argv[1]);
    int init_moves = atoi(argv[2]);

    if (init_pebbles <= 0 || init_moves <= 0) {
        fprintf(stderr, "Initial pebbles and moves must be greater than 0.\n");
        exit(EXIT_FAILURE);
    }

    //allocate shared memory
    shared_sems = mmap(NULL, sizeof(shared), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_sems == MAP_FAILED) {
        perror("Failed to create shared memory with mmap");
        exit(EXIT_FAILURE);
    }

    sem_init(&shared_sems->sem_a, init_pebbles);
    sem_init(&shared_sems->sem_b, init_pebbles);
    sem_init(&shared_sems->sem_c, init_pebbles);

    setup_signal_handler();

    int pids[NUM_TASKS];

    for (int i = 0; i < NUM_TASKS; i++) {
        pids[i] = fork();

        if (pids[i] < 0) {
            perror("Failed fork");
            exit(EXIT_FAILURE);
        }

        if (pids[i] == 0) {
            //child process

            struct sem *sem_from = NULL;
            struct sem *sem_to = NULL;

            switch (i) {
                case 0: sem_from = &shared_sems->sem_a; sem_to = &shared_sems->sem_b; break;
                case 1: sem_from = &shared_sems->sem_b; sem_to = &shared_sems->sem_c; break;
                case 2: sem_from = &shared_sems->sem_c; sem_to = &shared_sems->sem_a; break;
                case 3: sem_from = &shared_sems->sem_b; sem_to = &shared_sems->sem_a; break;
                case 4: sem_from = &shared_sems->sem_c; sem_to = &shared_sems->sem_b; break;
                case 5: sem_from = &shared_sems->sem_a; sem_to = &shared_sems->sem_c; break;
                default:
                    fprintf(stderr, "Invalid task ID\n");
                    exit(EXIT_FAILURE);
            }

            whichtask = i;
            printf("Child process %d started: task %d, sem_from: %p, sem_to: %p\n", getpid(), whichtask, sem_from, sem_to);

            for (int j = 0; j < init_moves; j++) {
                printf("Task %d (child %d): iteration %d - waiting on sem_from\n", whichtask, getpid(), j);
                sem_wait(sem_from, whichtask);
                printf("Task %d (child %d): iteration %d - incrementing sem_to\n", whichtask, getpid(), j);
                sem_inc(sem_to, whichtask);
                sched_yield();
            }
            printf("Task %d (child %d): completed all iterations\n", whichtask, getpid());
            exit(EXIT_SUCCESS);
        }
    }

    //wait for all children
    for (int i = 0; i < NUM_TASKS; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        printf("VCPU %d done\n", i);
        if (WIFEXITED(status)) {
            printf("Child pid %d exited with status %d\n", pids[i], WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Child pid %d exited due to signal %d\n", pids[i], WTERMSIG(status));
        }
    }

    //print semaphore info
    print_info("sem_a", &shared_sems->sem_a);
    print_info("sem_b", &shared_sems->sem_b);
    print_info("sem_c", &shared_sems->sem_c);

    munmap(shared_sems, sizeof(shared));
    
    return 0;
}

void print_info(const char *sem_name, struct sem *sem) {
    fprintf(stderr, "Semaphore: %s\n", sem_name);
    for (int i = 0; i < NUM_TASKS; i++) {
        fprintf(stderr, "Task %d Sleep Count: %d\n", i, sem->sleep_count[i]);
        fprintf(stderr, "Task %d Wake Count: %d\n", i, sem->wake_count[i]);
    }
}

void setup_signal_handler() {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("Failed to set up signal handler");
        exit(EXIT_FAILURE);
    }
}

void handler(int signo) {
    if (signo == SIGUSR1) {
        sig_count[whichtask]++;
    }
}
