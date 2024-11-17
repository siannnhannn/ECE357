#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>

#define TOKEN_DELIMITERS "\t \n"
#define MAX_LINES 100

typedef struct {
    int op_index;
    char op_buf[10];
    char **redir_array;
    int redir_index;
    int command_index;
    char **command_array;
    char **args;
    int args_index;
}line;

void io_redir(char *op, char *out_filename, int *status);
void parse_line(char *input_buf, line *ld);
void execute(line *ld, int *status);
void print_array(char **array, int array_size, const char *array_name);

int main(int argc, char **argv) {
    int status = 0;
    int ws;
    int i = 0;
    char line_buf[256];
    FILE* input_file;
    line line_data = (line) {0, {0}, NULL, 0, 0, NULL, NULL, 0};

    if(argc == 2) {
        //input file provided
        input_file = fopen(argv[1], "r");
    } else {
        //read from standard in
        input_file = stdin;
    }                                                 

    while ((fgets(line_buf, sizeof(line_buf), input_file)) != NULL) {

        if (line_buf[0] == '#' || line_buf[0] == '\n' || line_buf[0] == '\0') {
            continue; 
        }

        line_data = (line){0, {0}, NULL, 0, 0, NULL, NULL, 0};
        parse_line(line_buf, &line_data);

        execute(&line_data, &status);
/*
        fprintf(stderr, "%s\n", "worked");
        print_array(line_data.command_array, line_data.command_index, "command_array");
        print_array(line_data.args, line_data.args_index, "args");
        print_array(line_data.redir_array, line_data.redir_index, "redir_array");
*/

        //free allocations
        for (int i = 0; i < line_data.command_index; i++) free(line_data.command_array[i]);
        free(line_data.command_array);
        for (int i = 0; i < line_data.args_index; i++) free(line_data.args[i]);
        free(line_data.args);
        for (int i = 0; i < line_data.redir_index; i++) free(line_data.redir_array[i]);
        free(line_data.redir_array);
    }

    fprintf(stderr, "%s", "I am reaching here");

    //exiting and returning from main are the same
    exit(status);
    return status;
}
 
void parse_line(char *input_buf, line *ld) {
    const char* token_delimiters = TOKEN_DELIMITERS;
    char* token;
    token = strtok(input_buf, token_delimiters);
    int is_command = 1;

    //check for redir ops
    while(token != NULL) {

        if (token[0] == '<' || token[0] == '>' || token[0] == '2') {
            ld->op_buf[ld->op_index] = token[0];
            ld->op_index++;
            memmove(token, token + 1, strlen(token));

            while (strlen(token) > 0 && token[0] == '>' && ld->op_index < 3) {
                ld->op_buf[ld->op_index] = token[0];
                ld->op_index++;
                memmove(token, token + 1, strlen(token));
            }
            
            ld->op_buf[ld->op_index] = '\0';

            //reset op_index
            ld->op_index = 0;
            
            ld->redir_array = (char **)realloc(ld->redir_array, (ld->redir_index+2) * sizeof(char*));
            if (!ld->redir_array) {
                perror("realloc failed");
            }
            ld->redir_array[ld->redir_index++] = strdup(ld->op_buf);
            ld->redir_array[ld->redir_index++] = strdup(token);

            token = strtok(NULL, token_delimiters);
            continue;
        }
        
        if(is_command) {
            //commands
            ld->command_array = (char **)realloc(ld->command_array, (ld->command_index + 1) * sizeof(char*));
            if (!ld->command_array) {
                perror("realloc failed");
            }

            ld->command_array[ld->command_index] = strdup(token);
            ld->command_index++;

            is_command = 0;
            token = strtok(NULL, token_delimiters);

        } else {
            //arguments
            ld->args = (char **)realloc(ld->args, (ld->args_index + 1) * sizeof(char*));
            
            if (!ld->args) {
                perror("realloc failed");
            }
                
            ld->args[ld->args_index] = strdup(token);
            ld->args_index++;
            token = strtok(NULL, token_delimiters);
            
            continue;
        }
    }
}

void execute(line *ld, int *status) {
    char pwd_buf[256];

    //cd pwd exit
    if (strcmp(ld->command_array[0], "cd") == 0) {
        char *dir;
        if(ld->args_index != 0 && ld->args[0] != NULL) { //ensure the argument exists
            dir = ld->args[0];
        } else {
            dir = getenv("HOME");
        }

        chdir(dir);
        return;
    }

    if (strcmp(ld->command_array[0], "pwd") == 0) {
        getcwd(pwd_buf, sizeof(pwd_buf));
        fprintf(stderr, "%s\n", pwd_buf);
        return;
    }

    if (strcmp(ld->command_array[0], "exit") == 0) {
        if (ld->args_index > 0 && ld->args[0] != NULL) {
            *status = atoi(ld->args[0]);
        }
        exit(*status);
    }

    struct timeval start, end;
    struct rusage usage;
    gettimeofday(&start, NULL);

    switch(*status = fork()) {
        //if standard in
        case 0:
            for (int i = 0; i < ld->redir_index; i += 2) {
               io_redir(ld->redir_array[i], ld->redir_array[i + 1], status);
            }

            char **exec_args = (char **)malloc((ld->args_index + 2) * sizeof(char *));
            exec_args[0] = strdup(ld->command_array[0]);

            for (int i = 1; i <= ld->args_index; i++) {
                exec_args[i] = strdup(ld->args[i - 1]);
            }

            exec_args[ld->args_index + 1] = NULL;

            //finally actual execution!
            int exec_success = execvp(ld->command_array[0], exec_args);

            if (exec_success == -1) {
                fprintf(stderr, "executing %s failed %s:", ld->command_array[0], strerror(errno));
            }

            for (int i = 0; i <= ld->args_index; i++) {
                free(exec_args[i]);
            }

            free(exec_args);

            exit(127);

        case -1:
            perror("fork failed");
            break;

        default: //parent
            if (wait3(status, 1, &usage) == -1) {
                perror("wait3 failed");
                return;
            }

            gettimeofday(&end, NULL);

            double real_time = (end.tv_sec - start.tv_sec) * 1000.0;
            real_time += (end.tv_usec - start.tv_usec) / 1000.0;

            double user_cpu_time = usage.ru_utime.tv_sec * 1000.0 + usage.ru_utime.tv_usec / 1000.0;
            double system_cpu_time = usage.ru_stime.tv_sec * 1000.0 + usage.ru_stime.tv_usec / 1000.0;

            if (WIFEXITED(*status)) {
                *status = WEXITSTATUS(*status);
                printf("process exited status: %d\n", WEXITSTATUS(*status));
            } else if (WIFSIGNALED(status)) {
                *status = WTERMSIG(status);
                printf("process killed signal: %d\n", WTERMSIG(*status));
            }

            printf("\t real time elapsed: %.2f ms\n", real_time);
            printf("\t user CPU time: %.2f ms\n", user_cpu_time);
            printf("\t system CPU time: %.2f ms\n", system_cpu_time);

            break;
    }
}


void io_redir(char *op, char *dest_filename, int *status) {
    int init_fd;
    int dest_fd = -1;

    switch(op[0]) {
        case '<':
            init_fd = STDIN_FILENO;
            dest_fd = open(dest_filename, O_RDONLY);
            break;
        case '>':
            init_fd = STDOUT_FILENO;
            if (op[1] == '>') {
                dest_fd = open(dest_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
            } else {
                dest_fd = open(dest_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
            break;
        case '2':
            init_fd = STDERR_FILENO;
            if (op[1] == '>' && op[2] == '>') {
                dest_fd = open(dest_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
            } else if (op[1] == '>') {
                dest_fd = open(dest_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
            break;
        default:
            fprintf(stderr, "Invalid redirection operator: %s\n", op);
            *status = 1;
            return;
    }

    if (dest_fd == -1) {
        fprintf(stdout, "Failed to open %s for redirection: %s\n", dest_filename, strerror(errno));
        *status = 1;
        return;
    }

    if (dup2(dest_fd, init_fd) == -1) {
        perror("dup2 failed");
        close(dest_fd);
        *status = 1;
        return;
    }

    close(dest_fd);
}
                
void print_array(char **array, int array_size, const char *array_name) {
    printf("Contents of %s:\n", array_name);
    if (array == NULL) {
        printf("%s is NULL\n", array_name);
        return;
    }

    for (int i = 0; i < array_size; i++) {
        if (array[i] != NULL) {
            printf("%s[%d]: %s\n", array_name, i, array[i]);
        } else {
            printf("%s[%d]: NULL\n", array_name, i);
        }
    }
    printf("\n");
}

