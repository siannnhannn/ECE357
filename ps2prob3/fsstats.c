#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/errno.h>

#define SPECIAL_CHARS " !\"#$%&'()*+,/:;<=>?@[\\]^_`{|}~"

struct result {
    int dir_inode;
    int fil_inode;
    int sl_inode;
    int hl_inode;
    int total_size;
    int total_blocks;
    int problematic;
    int unresolved_sl;
};

void explorefs(char* start_dir, struct result* result_buf);
bool check_encountered(int *encountered, int count_encountered, ino_t st_ino);
bool check_problematic(const char *dir_name);
void print_results(struct result result_buf);
bool check_symlink(const char* path);
int* collect_inode(int* encountered, int* count_encountered, ino_t inode);
void dont_give_up(const char* path);

int main(int argc, char* argv[]) {
    struct result result_buf = {0};
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
        return 1;
    }
    
    explorefs(argv[1], &result_buf);
    print_results(result_buf);

    return 0;
}

void explorefs(char* start_dir, struct result* result_buf) {
    struct stat stat_buf;
    struct dirent *dir_buf;

    DIR *dir = opendir(start_dir);
    
    if (dir == NULL) {
        perror("opendir failed");
        return;
    }

    int *encountered = NULL;
    int count_encountered = 0;

    while ((dir_buf = readdir(dir)) != NULL) {
        if (strcmp(dir_buf->d_name, ".") == 0 || strcmp(dir_buf->d_name, "..") == 0) {
            continue;
        }

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", start_dir, dir_buf->d_name);

        if (lstat(path, &stat_buf) == -1) {
            dont_give_up(path);
            continue;
        }

        if (check_problematic(dir_buf->d_name)) {
            result_buf->problematic++;
        }

        result_buf->total_blocks += stat_buf.st_blocks;
        result_buf->total_size += stat_buf.st_size;
        
        if (S_ISDIR(stat_buf.st_mode)) {
            result_buf->dir_inode++;
            explorefs(path, result_buf);

        } else if (check_encountered(encountered, count_encountered, stat_buf.st_ino)) {
            // Skip if inode already encountered
        } else {
            // Add inode to encountered collection
            encountered = collect_inode(encountered, &count_encountered, stat_buf.st_ino);

            if (S_ISREG(stat_buf.st_mode)) {
                result_buf->fil_inode++;
            } else if (S_ISLNK(stat_buf.st_mode)) {
                result_buf->sl_inode++;
                if (!check_symlink(path)) {
                    result_buf->unresolved_sl++;
                }
            }

            // Increment hl_inode only if st_nlink > 1 and this is the first time the inode is encountered
            if (stat_buf.st_nlink > 1) {
                result_buf->hl_inode++;
            }
        }
    }

    free(encountered);
    closedir(dir);
}

void dont_give_up(const char* path) {
    fprintf(stderr, "Error accessing: %s - %s\n", path, strerror(errno));
}


bool check_encountered(int *encountered, int count_encountered, ino_t st_ino) {
    for (int i = 0; i < count_encountered; i++) {
        if (st_ino == encountered[i]) {
            return true;
        }
    }
    return false;
}

bool check_problematic(const char *dir_name) {
    if (dir_name[0] == '.') {
            return true;
    }

    for (int i = 0; dir_name[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)dir_name[i];
        if (ch < 32 || ch > 126) {
            return true;
        }

        if (strchr(SPECIAL_CHARS, ch) != NULL) {
            return true;
        }
    }
    return false;
}

bool check_symlink(const char* path) {
    char target[1024];
    ssize_t len = readlink(path, target, sizeof(target) - 1);
    if (len == -1) {
        return false;
    }
    target[len] = '\0';
    if (access(target, F_OK) == -1) {
        return false;    }
    return true;
}

int* collect_inode(int* encountered, int* count_encountered, ino_t inode) {
    (*count_encountered)++;
    encountered = (int *)realloc(encountered, (*count_encountered) * sizeof(int));
    if (encountered == NULL) {
        perror("realloc failed");
        exit(EXIT_FAILURE);
    }
    encountered[(*count_encountered) - 1] = inode;
    return encountered;
}

void print_results(struct result result_buf) {
    printf("Directory Inodes: %d\n", result_buf.dir_inode);
    printf("File Inodes: %d\n", result_buf.fil_inode);
    printf("Symbolic Link Inodes: %d\n", result_buf.sl_inode);
    printf("Unresolved Symbolic Links: %d\n", result_buf.unresolved_sl);
    printf("Hard Link Inodes: %d\n", result_buf.hl_inode);
    printf("Total Size: %d bytes\n", result_buf.total_size);
    printf("Total Blocks: %d\n", result_buf.total_blocks);
    printf("Problematic: %d\n", result_buf.problematic);
}

