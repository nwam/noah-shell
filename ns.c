/* file: ns.c -- Noah Shell
 * Author: Noah Murad
 * Date: February 1, 2016
*/

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#define MAX_INPUT_SIZE 1024
#define HISTORY_SIZE 10
#define CMD_MAX 64

/* This function takes as input the following:
     buf: This represents the string for which the tokens are to be determined
          for

     tokens: This represents the array that the found tokens are then put into

   The function strtok() is used to find the tokens.  The delimiter used
   to distinguish tokens is a space
*/
int make_tokenlist(char *buff, char *tokens[]){
    char buf[MAX_INPUT_SIZE];
    strcpy(buf, buff);
    char input_line[MAX_INPUT_SIZE];
    char *line;
    int i,n;

    i = 0;

    line =   buf;
    tokens[i] = strtok(line, " ");
    do  {
       i++;
       line = NULL;
       tokens[i] = strtok(line, " ");
    } while(tokens[i] != NULL);

    return i;
}

/* gets and returns the username of the user using the system */
const char *get_username(){
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    if(pw){
        return pw->pw_name;
    }
    return "";
}

/*removes newline character at the end of strings 
 * received from fgets from stdin */
void format_input(char* c){
    int i=0;
    while(c[i]!='\n') i++;
    c[i] = '\0';
}

/* checks if any of the tokens in the token list contain
 * a specified token.
 * Returns the index of the token if found,
 * -1 otherwise */
int contains_token(char *tokens[], int size, char* token){
    for(int i=0; i<size; i++){
        if(tokens[i] && strcmp(tokens[i], token) == 0) return i;
    }
    return -1;
}

/* handles exiting the shell */
void handle_sigint(){
   printf("Attempting to exit...\n");
   exit(EXIT_SUCCESS);
}

/* creates a subset of tokens from a list of tokens */
void extract_tokens(char *tokens[], char *extracted_tokens[], int first, int last){
    int j = 0;
    for(int i=first; i<last; i++){
            if(tokens[i]){
                extracted_tokens[j] = tokens[i];
                j++;
            }
    }
}

/* main */
int main(){

    bool    exit_flag = false;
    int     oldest_history = HISTORY_SIZE - 1,
            status;
    char    *username,
            input[MAX_INPUT_SIZE],
            history[HISTORY_SIZE][MAX_INPUT_SIZE] = {'\0'},
            *tokens[CMD_MAX];

    signal(SIGINT, handle_sigint);
    //shell loop
    while(!exit_flag){

        //print the prompt and get the input
        printf("%s> " , get_username());
        fgets(input, MAX_INPUT_SIZE, stdin); 
        format_input(input);

        //built-in exit function
        if(!strcmp(input, "exit")) exit_flag = true;

        //built-in history function
        else if(!strcmp(input, "history")){
            int i;
            for(i = oldest_history+1; i!=oldest_history; i = (i+1)%HISTORY_SIZE){
                if(history[i][0] != '\0') printf("%s\n", history[i]);
            }
        }

        //non-built-in function
        else if(strcmp(input, "")){
            int     token_index,
                    num_tokens;
            pid_t   pid;
            char   *tokens[CMD_MAX];

            //get the token list and size
            num_tokens = make_tokenlist(input, tokens);

            //fork a child process
            pid = fork();

            //parent: wait for child to finish
            if(pid >  0){
                pid = wait(&status);
            }

            //child: execute command
            if(pid == 0){

                //redirect output
                token_index = contains_token(tokens, num_tokens, ">");
                if(token_index >= 0 && token_index+1 < num_tokens){
                    int fd_o = open(tokens[token_index+1], O_CREAT|O_TRUNC|O_WRONLY, 0644);
                    if(dup2(fd_o, STDOUT_FILENO)<0){
                        perror("dup2 error");
                        exit(EXIT_FAILURE);
                    }
                    close(fd_o);

                    //remove ">" and the output file from the list of arguments
                    tokens[token_index  ] = '\0';
                    tokens[token_index+1] = '\0';
                }

                //redirect input
                token_index = contains_token(tokens, num_tokens, "<");
                if(token_index >= 0 && token_index+1 < num_tokens){
                    int fd_i = open(tokens[token_index+1], O_RDONLY);
                    dup2(fd_i, STDIN_FILENO);
                    close(fd_i);

                    //remove "<" and the input file from the list of arguments
                    tokens[token_index  ] = '\0';
                    tokens[token_index+1] = '\0';
                }

                //handle pipe
                token_index = contains_token(tokens, num_tokens, "|");
                if(token_index >= 0 && token_index+1 < num_tokens){
                   
                    //open a pipe
                    int fds[2];
                    if(pipe(fds)<0){
                        perror("Pipe fail");
                        exit(EXIT_FAILURE);
                    }

                    //create another process
                    pid_t pid2 = fork();
                    if(pid2 == 0){      
                        //child process

                        //redirect output to the pipe
                        close(fds[0]);
                        dup2(fds[1], STDOUT_FILENO);

                        //extract the input command (command to the left of the pipe)
                        char *left_pipe[CMD_MAX];
                        extract_tokens(tokens, left_pipe, 0, token_index);

                        //execute the command
                        execvp(left_pipe[0], left_pipe);
                        perror("Exec error");
                        exit(EXIT_FAILURE);
                    }

                    else if(pid2 > 0){ 
                        //parent process (ie child of the parent shell)
                       
                        //redirect input to from the pipe
                        close(fds[1]);
                        dup2(fds[0], STDIN_FILENO);

                        //extract the output command (command to the right of the pipe)
                        char* right_pipe[CMD_MAX];
                        extract_tokens(tokens, right_pipe, token_index+1, CMD_MAX);

                        //execute the command
                        execvp(right_pipe[0], right_pipe);
                        perror("Exec error");
                        exit(EXIT_FAILURE);
                    }
                }
 
                //execute command
                execvp(tokens[0], tokens);
                printf("%s: invalid command\n", tokens[0]);
                exit(EXIT_FAILURE);
            }
        }

        //update history
        printf("%d\n", status);
        if(strcmp(input, "") && status==0){
            strcpy( history[oldest_history], input );
            oldest_history--;
            if(oldest_history < 0) oldest_history = HISTORY_SIZE - 1;
        }

        //clean up

    }

}
