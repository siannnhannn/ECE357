#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

void handler(int signo);
int test23(int flag);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "too few arguments");
        return -1;
    }

    int whichtest = *argv[1] - '0';

    switch (whichtest) {
        case 1: {
            size_t pagesize = sysconf(_SC_PAGESIZE);
            void *region = mmap(NULL, pagesize, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

            if (region == MAP_FAILED) {
                perror("Failed mmap");
                return 1;
            }

            struct sigaction sa;
            sa.sa_handler = handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = SA_RESTART;
            sigaction(SIGSEGV, &sa, NULL);

            //success; shouldn't happen
            char *char_region = (char *)region;
            char_region[0] = 'X';

            if (char_region[0] == 'X') {
                munmap(region, pagesize);
                return 0;
            }
            
            //failure without signal; shouldn't happen
            munmap(region, pagesize);
            return 255;
        }

        case 2: {
            return test23(MAP_SHARED);
        }
    
        case 3: {
            return test23(MAP_PRIVATE);
        }

        case 4: {
            char read_val;
            const char *file = "testfile.txt";

            int fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0666);
            if (fd == -1) {
                perror("Failed opening file");
                return -1;
            }

            int filesize = 4101;
            ftruncate(fd, filesize);

            void *region = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (region == MAP_FAILED) {
                perror("Failed mmap");
                close(fd);
                return -1;
            }

            char *char_region = (char *)region;
            char_region[4101] = 'X';

            filesize += 16;
            if (ftruncate(fd, filesize) == -1) {
                perror("Failed extending file");
                close(fd);
                return -1;
            }

            if (lseek(fd, 4101, SEEK_SET) == -1 || read(fd, &read_val, 1) != 1) {
                perror("Failed reading from file");
                close(fd);
                return 1;
            }

            int result = (read_val == 'X') ? 0 : 1;

            close(fd);
            munmap(region, filesize);
            return result;
        }

        default:
            fprintf(stderr, "Unknown test case: %d\n", whichtest);
            return 1;
    }
}

void handler(int signo) {
    fprintf(stderr, "Caught signal %d\n", signo);
    exit(signo);
}

int test23(int flag) {
    char initial_val;
    char read_val;
    const char *file = "testfile.txt";

    int fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        perror("Failed opening file");
        return -1;
    }

    struct stat st;
    fstat(fd, &st);
    size_t filesize = st.st_size;

    if (filesize == 0) {
        if (lseek(fd, 0, SEEK_SET) == -1 || write(fd, "\0", 1) != 1) {
            perror("Failed lseek");
            close(fd);
            return -1;
        }
        filesize = 1;
    }

    lseek(fd, 0, SEEK_SET);
    if (read(fd, &initial_val, 1) != 1) {
        perror("Failed read");
        close(fd);
        return -1;
    }

    void *region = mmap(0, filesize, PROT_READ | PROT_WRITE, flag, fd, 0);
    if (region == MAP_FAILED) {
        perror("Failed map");
        close(fd);
        return -1;
    }

    char *char_region = (char *)region;
    char_region[0] = 'C';

    lseek(fd, 0, SEEK_SET);

    if (read(fd, &read_val, 1) != 1) {
        perror("Failed read");
        munmap(region, filesize);
        close(fd);
        return -1;
    }

    //result should be 1 for MAP_PRIVATE and 0 for MAP_SHARED
    int result = (read_val == initial_val) ? 1 : 0;
    munmap(region, filesize);
    close(fd);

    return result;
}
