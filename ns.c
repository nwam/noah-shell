/* file: ns.c -- noah'shell
 * Author: Noah Murad -- CS3305B, Department of Computer Science, Western University, 
 *      London, Ontario, Canada, Earth, Solar System, Milky Way, Local Group, Laniakea, Universe
 * Date: February 5, 2016
 *
 * Description: noah'shell (ns, Noah's Shell) is a shell created by Noah Murad which
 *      along with supporting system functions, supports the built-in functions
 *      history and exit. ns also supports IO redirection and multi-piping. 
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

/*counts the number of pipe tokens from a list of tokens */
int count_pipes(char *tokens[], int size){
    int count = 0;
    for(int i=0; i<size; i++){
        if(tokens[i] && strcmp(tokens[i],"|")==0) count++;
    }
    return count;
}

/* gets and stores (inside pipe_indices) the indices of pipe tokens found in tokens
 * pipe_indices[0] is reseved for the value -1, and 
 * pipe_indices[num_pipes+1] is reserved for the number of tokens in tokens */
void get_pipe_indices(char *tokens[], int size, int pipe_indices[]){
    pipe_indices[0] = -1;
    int j = 1;
    for(int i=0; i<size; i++){
        if(tokens[i] && strcmp(tokens[i], "|")==0){
            pipe_indices[j] = i;
            j++;
        }
    }
    pipe_indices[j] = CMD_MAX;
}

/* forks a new process and executes the command command with the new (child) process
 * redirects the input  to in
 * redirects the output to out */
void execute_piped_process(int in, int out, char *command[]){
    pid_t pid = fork();

    if(pid == 0){
        //child process

        //connect input to the specified input
        if(in != STDIN_FILENO){
            dup2(in, STDIN_FILENO);
            close(in);
        }

        //connect output to the specified output
        if(out != STDOUT_FILENO){
            dup2(out, STDOUT_FILENO);
            close(out);
        }

        //execute the command
        execvp(command[0], command);
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
                int num_pipes = count_pipes(tokens, num_tokens);
                if(num_pipes > 0){
                    
                    //get the indices of the pipes to extract the commands
                    int pipe_indices[CMD_MAX];
                    get_pipe_indices(tokens, num_tokens, pipe_indices);
                    
                    //extract all the commands
                    char *commands[CMD_MAX][CMD_MAX];
                    for(int i=0; i<=num_pipes; i++){
                        extract_tokens(tokens, commands[i], pipe_indices[i]+1, pipe_indices[i+1]);
                    }
                    
                    //connect and run all the pipe commands
                    int fds[2],
                        in = STDIN_FILENO;
                    for(int i=0; i<num_pipes; i++){
                        pipe(fds);
                        execute_piped_process(in, fds[1], commands[i]);
                        close(fds[1]);
                        in = fds[0];
                    }

                    //execute the last command
                    if(in != STDIN_FILENO) dup2(in, STDIN_FILENO);
                    execvp(commands[num_pipes][0], commands[num_pipes]);
                }

                //execute command
                execvp(tokens[0], tokens);
                printf("%s: invalid command\n", tokens[0]);
                exit(EXIT_FAILURE);
            }
        }

        //update history
        if(strcmp(input, "") && status==0){
            strcpy( history[oldest_history], input );
            oldest_history--;
            if(oldest_history < 0) oldest_history = HISTORY_SIZE - 1;
        }

    }

}
