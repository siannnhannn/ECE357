#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>

void handler1(int signo);
void handler2(int signo);

int file_count = 0;
int read_bytes = 0;

int written = 0;
int processed_bytes = 0;

sigjmp_buf jump_buf;

int p1[2];
int p2[2];
pid_t grep_pid;
pid_t more_pid;
int ws1, ws2;

int main(int argc, char* argv[]) {
    if (argc < 3){
        fprintf(stderr, "too few arguments");
        return 1;
    }

    char *pattern = argv[1];
    char read_buf[4096];
    int in_file;

    struct sigaction s1, s2;

    s1.sa_handler = handler1;
    sigemptyset(&s1.sa_mask);
    s1.sa_flags = 0; //resume
    sigaction(SIGUSR1, &s1, NULL);

    s2.sa_handler = handler2;
    sigemptyset(&s2.sa_mask);
    s2.sa_flags = 0;
    sigaction(SIGUSR2, &s2, NULL);
    sigaction(SIGPIPE, &s2, NULL);


    for(int i = 2; i < argc; i++) {
        if(sigsetjmp(jump_buf, 1)!=0) {
            if(in_file != -1) close(in_file);
            continue;
        }

        //create pipes
        if(pipe(p1) < 0 || pipe(p2) < 0) {
            perror("pipe failed");
            exit(1);
        }

        //grepping
        switch (grep_pid = fork()) {
            case 0:
                close(p1[1]);
                p1[1] = -1;
                close(p2[0]);
                p2[0] = -1;

                dup2(p1[0], STDIN_FILENO);
                dup2(p2[1], STDOUT_FILENO);

                execlp("grep", "grep", pattern, NULL);
                perror("execing grep failed");
                exit(1);

            case -1:
                perror("fork failed");
                break;
        }

        close(p1[0]);
        p1[0] = -1;
        close(p2[1]);
        p2[1] = -1;

        //moreing
        switch (more_pid = fork()) {
            case 0:
                close(p1[1]);
                dup2(p2[0], STDIN_FILENO);
                
                execlp("more", "more", NULL);
                perror("execing more failed");
                exit(1);

            case -1:
                perror("fork failed");
                break;
        }
        
        close(p2[0]);
        p2[0] = -1;

        //processing file
        in_file = open(argv[i], O_RDONLY);
        file_count++;

        if(in_file == -1) {
            fprintf(stderr, "Failed to open file %s: %s\n", argv[i], strerror(errno));
            continue;
        }

        while((read_bytes = read(in_file, read_buf, sizeof(read_buf))) > 0) {
            written = write(p1[1], read_buf, read_bytes);
            processed_bytes += read_bytes;

            while(written < read_bytes) {
                written += write(p1[1], read_buf + written, read_bytes - written);
                fprintf(stderr, "%d", processed_bytes);
	        }

            if(written == -1) {
                perror("failed writing to pipe");
                exit(1);
            }
        }

        close(in_file);
        close(p1[1]);
        p1[1] = -1;

        //make sure all file descriptors are closed
        for(int j=0; j<2; j++) {
            if(p1[j] != -1) {
                close(p1[j]);
            }
            if(p2[j] != -1) {
                close(p2[j]);
            }
        }

        waitpid(grep_pid, &ws1, 0);
        waitpid(more_pid, &ws1, 0);
    }
}

void handler1(int signo) {
    char msg[100];
    snprintf(msg, sizeof(msg), "processed files: %d, processed bytes: %d\n", file_count, processed_bytes);
    write(STDERR_FILENO, msg, strlen(msg));
}

void handler2(int signo) {
    if (signo == SIGPIPE) {
        fprintf(stderr, "broken pipe");
    } else {
        fprintf(stderr, "SIGUSR2 received moving onto next file\n");
    }

    for(int j=0; j<2; j++) {
        if(p1[j] != -1) {
            close(p1[j]);
        }
        if(p2[j] != -1) {
            close(p2[j]);
        }
    }

    kill(grep_pid, SIGINT);
    kill(more_pid, SIGINT);

    siglongjmp(jump_buf, 1);
}

