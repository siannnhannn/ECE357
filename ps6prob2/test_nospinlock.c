#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

//12 cores on laptop
#define NUM_PROCESSES 12
#define NUM_ITERATIONS 1000000

int main() {
    int* shared_variable = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(shared_variable == MAP_FAILED) {
        perror("Failed to mmap");
        exit(1);
    }

    *shared_variable = 0;

    for(int i = 0; i < NUM_PROCESSES; i++) {
        int pid = fork();
        if(pid == 0) {
            for(int j = 0; j < NUM_ITERATIONS; j++) {
                (*shared_variable)++;
                //fprintf(stderr, "am adding to shared variable: %d\n", *shared_variable);
            }
            return *shared_variable;
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
