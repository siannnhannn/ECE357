#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int kit(int argc, char* argv[], int outfildes, int startIndex, int buffsize);

int main(int argc, char* argv[]) {
    int buffsize=4096;
    int startindex = 1;
    int outfildes = 1;
    int byeoutfildes;

    if(argc < 2) {
        write(2, "No Arguments \n", 30);
        return -1;
    }

    for(int i = 1; i < argc; i++) {
        if(((strcmp(argv[i], "-o")) == 0) && i + 1 < argc) {
            outfildes = open(argv[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            startindex = i+2;

            if(outfildes < 0) {
                write(2, "Could Not Open Output File \n", 30); 
                return -1;
            }
        }
        if(((strcmp(argv[i], "-b")) == 0) && i + 1 < argc) {
            buffsize = atoi(argv[i+1]);
            startindex = startindex + 2;
        }
    }

    if(startindex == argc) {
        kit(argc, (char*[]){"-"}, 1, 0, buffsize);
    } else {
        kit(argc, argv, outfildes, startindex, buffsize);
    }
    
    if(outfildes != 1) {
        byeoutfildes = close(outfildes);
        if(byeoutfildes<0) {
            write(2, "Could Not Close Output File \n", 29);
            return -1;
        }
    }

    return 0;
}

int kit(int argc, char* argv[], int outfildes, int startindex, int buffsize) {
    char *buff = malloc(buffsize);

    if (buff == NULL) {
        write(2, "Memory Allocation Failed \n", 27);
        return -1;
    }

    ssize_t bytesRead;
    int infildes;
    int byeinfildes;

    for(int i = startindex; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) {
            infildes = 0;
        } else {
            infildes = open(argv[i], O_RDONLY);
            if(infildes<0) {
                write(2, "Could Not Open Input File \n", 28);
                return -1;
            }
        }
        while((bytesRead = read(infildes, buff, sizeof(buff))) > 0) {
            write(outfildes, buff, bytesRead);
        }
        if (infildes != 0) {
            byeinfildes = close(infildes);
            if(byeinfildes<0) {
                write(2, "Could Not Close Input File \n", 28);
                return -1;
            }
        } 
    }

    free(buff);
    return 0;
}
