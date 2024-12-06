#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "spinlock.h"

//12 cores on laptop
#define NUM_PROCESSES 12
#define NUM_ITERATIONS 1000000

int main() {
    volatile char* variable_lock = mmap(NULL, sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (variable_lock == MAP_FAILED) {
        perror("Failed to mmap for lock");
        exit(1);
    }

    *variable_lock = 0;

    int* shared_variable = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(shared_variable == MAP_FAILED) {
        perror("Failed to mmap");
        exit(1);
    }

    *shared_variable = 0;

    for(int i = 0; i < NUM_PROCESSES; i++) {
        int pid = fork();
        if(pid == 0) {
            spin_lock(variable_lock);
            for(int j = 0; j < NUM_ITERATIONS; j++) {
                (*shared_variable)++;
            }
            spin_unlock(variable_lock);
            exit(0);
        } else if (pid < 0) {
            perror("Failed fork");
            exit(1);
        }
    }

    for(int i = 0; i < NUM_PROCESSES; i++) {
        wait(NULL);
    }

    printf("Shared Variable: %d\n", *shared_variable);
    return 0;
}
