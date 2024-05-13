#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>



#define MAX_ARGS 10
#define MAX_LENGTH 100
#define HISTORY_LENGTH 10

void update_history(char history[HISTORY_LENGTH][100], char *argv, int *k, int *full);
int is_background_process(char **args); 
void execute_piped_commands(char **args, int pipe_index);
int get_pipe_index(char **args, int i);
void execute_and_commands(char **args, int and_operator_index);
int get_pipe_index(char **args, int i);
int get_and_operator_index(char **args, int i);



int main() {
    char history[HISTORY_LENGTH][MAX_LENGTH]; //history array and variables
    int k = 0; //fifo index
    int full = 0; // check if fifo full
    while(1){
        char argv[MAX_LENGTH]; //input
        char *args[MAX_ARGS]; // tokens
        int i = 0;

        //input command
        printf("myshell>");
        fgets(argv, sizeof(argv), stdin);
        
        //create fifo structure for history
        update_history(history, argv, &k, &full);

        //tokenize input string
        char *token = strtok(argv, " \n");

        while (token != NULL && i < MAX_ARGS -1) {
            args[i++] = token;
            token = strtok(NULL, " \n");
        }
        args[i] = NULL; // mark last element as null
        
        int pipe_index = get_pipe_index(args, i);
        int and_operator_index = get_and_operator_index(args, i);
        int background_process = is_background_process(args);

        char cwd[1024];    
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("getcwd() error");
            return 1;
    }    
        if(strcmp(args[0], "pwd") == 0){
            if(getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            } else {
                perror("getcwd() error");
                return 1;
            }

        }
        else if (strcmp(args[0], "cd") == 0) 
        {
           if (args[1] == NULL)
           {
            chdir(getenv("HOME"));
            if(setenv("PWD", cwd, 1) == -1) {
                perror("setenv() error");
                return 1;
            }
           }
           
            else if (chdir(args[1]) == 0) 
            {

            }
            else {
                perror("chdir() error");
                return 1;
            }
             
        }
        else if (strcmp(args[0], "history") == 0)
        {
            for (int index = 0; index < k + 1; index++){
                printf("history[%d]:%s\n", index, history[index]);
            }
        }
        else if (strcmp(args[0], "exit") == 0) 
        {
            return 0;
        }
        else{

            pid_t pid;

            
            if(pipe_index){
                    execute_piped_commands(args, pipe_index);
                    }
            else if(and_operator_index ){
                execute_and_commands(args, and_operator_index);
            }
            else {
                if((pid =fork()) == 0) { //child process
                   
                if(execvp(args[0], args) == -1 && !pipe_index) { 
                    printf("An error occurred.\n"); 
                }
                }
                 else { //wait if not background
                    if (!background_process && !pipe_index)
                    {
                        wait(NULL);
                    }
                    else if(background_process && !pipe_index) {
                        printf("%d\n", pid);
                    }

  
                    }          
            } //else end of non pipe process 

            } // else end
        } // while end

     
    return 0;
} //main end

int is_background_process(char **args) {
    int background_process = 0;
    int last_index = 0;
    
    // Find the last index of the arguments array
    while (args[last_index] != NULL) {
        last_index++;
    }
    last_index--; // Adjust for array index starting from 0
    
    // Check if the last argument is "&"
    if (strcmp(args[last_index], "&") == 0) {
        background_process = 1;
    }
    
    
    return background_process;
}

void execute_piped_commands(char **args, int pipe_index) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid1, pid2;
    pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid1 == 0) { 
        close(pipe_fd[0]); 
        dup2(pipe_fd[1], STDOUT_FILENO); 
        close(pipe_fd[1]); 
        args[pipe_index] = NULL; // Null-terminate args for exec
        if(execvp(args[0], args) == -1){
            printf("an error occured");
            exit(EXIT_FAILURE);
        };

    } else { // Parent process
        pid2 = fork();
        if (pid2 == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid2 == 0) { // Second child process
            close(pipe_fd[1]); // Close writing end
            dup2(pipe_fd[0], STDIN_FILENO); // Redirect stdin to pipe
            close(pipe_fd[0]); // Close unused reading end
            if(execvp(args[pipe_index + 1], &args[pipe_index + 1]) == -1){
            printf("an error occured");
            exit(EXIT_FAILURE);
            };

        } else { // Parent process
            close(pipe_fd[0]); // Close unused pipe ends
            close(pipe_fd[1]);
            waitpid(pid1, NULL, 0); // Wait for child processes to finish
            waitpid(pid2, NULL, 0);
        }
    }
}
void execute_and_commands(char **args, int and_operator_index) {
    pid_t pid1 = fork();
    if (pid1 == 0) { // First child process
        args[and_operator_index] = NULL; // Null-terminate args for exec
        if (execvp(args[0], args) == -1) {
            printf("An error occurred.\n");
            exit(EXIT_FAILURE);
        }
    } else if (pid1 == -1) { // Error forking
        perror("fork");
        exit(EXIT_FAILURE);
    } else { // Parent process
        int status;
        if (waitpid(pid1, &status, 0) == -1) { // Wait for the first child process to finish
            perror("waitpid");
            exit(EXIT_FAILURE);
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) { // If the first child process succeeded
            pid_t pid2 = fork();
            if (pid2 == 0) { // Second child process
                if (execvp(args[and_operator_index + 1], &args[and_operator_index + 1]) == -1) {
                    printf("An error occurred.\n");
                    exit(EXIT_FAILURE);
                }
            } else if (pid2 == -1) { // Error forking
                perror("fork");
                exit(EXIT_FAILURE);
            } else { // Parent process
                // Wait for the second child process to finish
                if (waitpid(pid2, &status, 0) == -1) {
                    perror("waitpid");
                    exit(EXIT_FAILURE);
                }
            }
        } else { // If the first child process failed
            printf("The first command failed.\n");
        }
    }
}



int get_pipe_index(char **args, int i){
        int pipe_index = 0;
        for (int j = 0; j < i; j++) {
            if (strcmp(args[j], "|") == 0) {
                pipe_index = j;
                break;
            }
        }
        return pipe_index;
}
int get_and_operator_index(char **args, int i){
        int and_index = 0;
        for (int j = 0; j < i; j++) {
            if (strcmp(args[j], "&&") == 0) {
                and_index = j;
                break;
            }
        }
        return and_index;
}
void update_history(char history[HISTORY_LENGTH][100], char *argv, int *k, int *full) {
    if (!(*full)) {
        strcpy(history[*k], argv);
        (*k)++;
        if (*k == HISTORY_LENGTH - 1) {
            *full = 1;
        }
    } else {
        for (int d = 0; d < HISTORY_LENGTH - 1; d++) {
            strcpy(history[d], history[d + 1]);
        }
        strcpy(history[HISTORY_LENGTH - 1], argv);
    }
}