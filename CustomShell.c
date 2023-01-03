#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <stdbool.h>
#define HISTORY_MAX 100
#define POSIX_C_SOURCE 200809L
 
extern char* strdup(const char*); 
extern ssize_t getline (char **restrict lineptr, size_t *restrict n, FILE *restrict stream); 
extern char* strtok_r(char *restrict str, const char *restrict delim, char **restrict saveptr); 
extern pid_t waitpid(pid_t pid, int *status, int options); 
extern pid_t wait(int *wstatus);

typedef struct cmdhist {
    int offset;  // offset in the history list
    bool isPiped; // this is a flag for every command to handle piped ones
    int numArgs; // this is the number of arguments
    char* enteredCommand; // anything the user types is stored here as a literal string
    char* command[]; // these are parsed commands for passing to execvp when required
} history;
history h[HISTORY_MAX];

void err_exit(char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

void printHistory(int index) {
    for (int i = 0; i <= index; i++) 
        printf ("%d %s \n", h[i].offset, h[i].enteredCommand);
}

void execute(int offset) {
    pid_t  pid;

    if ((pid = fork()) < 0) {     /* fork a child process           */
          perror("*** ERROR: forking child process failed\n");
          exit(1);
     }
     else if (pid == 0) {          /* for the child process:         */
        
        char* myargs[h[offset].numArgs];
        for (int i = 0; i < h[offset].numArgs; i++)
            myargs[i] = strdup(h[offset].command[i]);
        myargs[h[offset].numArgs] = NULL;

        if (execvp(myargs[0], myargs) < 0) {    /* execute the command  */
           perror("*** ERROR: exec failed\n");
           exit(1);
        }
     }
     else {                           /* for the parent:      */
           waitpid(pid, NULL, 0);     /* wait for completion  */
     }
}

int executeHistory(int offset) {
    int fd[2];
    if (pipe(fd) == -1) {
        return 1;
    }
    
    int pid = fork();
    if (pid == -1) {
        return 2;
    }
    if (pid == 0) {
        close(fd[1]);
        char str[200];
        int n;
        if (read(fd[0], &n, sizeof(int)) < 0) {
            perror("read n failure in child \n");
        }
        if (read(fd[0], &str, sizeof(int) * n) < 0) {
            perror("read str failure in child \n");
        }
        close(fd[0]);     
        // data from parent to child is received
        // parse the string and build args
        char* words; char* myargs[25]; int q = 0;
        char* newArray = strdup(str);
        while ((words = strtok_r(newArray, " ", &newArray)) != NULL) {
            if (strcmp(words, "\n") == 0) {                 
                    continue;                                           
            } else {
                myargs[q] = (char *) malloc(strlen(words) * sizeof(char));
                strcpy(myargs[q], words);
                q++;
            }
        }  
        myargs[q] = NULL;
        execvp(myargs[0], myargs);
    } else {
        close(fd[0]);
        char str[200];
        strcpy(str, h[offset].enteredCommand);
        int n =  strlen(h[offset].enteredCommand) + 1;
        if (write(fd[1], &n, sizeof(int)) < 0) {
            perror("write n failure in parent \n");
        }
        if (write(fd[1], &str, sizeof(char) * n) < 0) {
            perror("write str failure in parent \n");
        }
        close(fd[1]);
        wait(NULL);
    }
    return 0;
}

int processPipedCommand (const char * commandStr) {
    char* item; char* strCmds[50]; int counter = 0; // everything for outer loop (parsing the pipes)
    char* pipeCmd = strdup(commandStr);             // strdup the input string which has all the info in one string
    while ((item = strtok_r(pipeCmd, "|", &pipeCmd)) != NULL) { 
        int num = strlen(item);
        strCmds[counter] = (char *) malloc((num + 1)* sizeof (char));
        strcpy(strCmds[counter], item);
        counter++;
    }
    if (counter <= 1) {
        perror ("This is an invalid condition! \n");
        return -1;
    }

    int p[2];    pid_t pid;    int fd_in = 0;   // everything for fork/exec processing
    for (int i = 0; i < counter; i++) {
        pipe(p);
        if ((pid = fork()) == -1) {
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            dup2(fd_in, STDIN_FILENO);          // change the input according to the old one
            if (i < counter-1)
                dup2(p[1], STDOUT_FILENO);
            
            close(p[0]);
            // run the command
            char* words; char* myargs[25]; int q = 0;
            char* newArray = strdup(strCmds[i]);
            while ((words = strtok_r(newArray, " ", &newArray)) != NULL) {
                if (strcmp(words, "\n") == 0) {                 
                        continue;                                           
                } else {
                    myargs[q] = (char *) malloc(strlen(words) * sizeof(char));
                    strcpy(myargs[q], words);
                    q++;
                }
            }  
            myargs[q] = NULL;
            execvp(myargs[0], myargs);
            exit(EXIT_FAILURE);
        } else {
            wait(NULL);
            close(p[1]);
            fd_in = p[0]; //save the input for the next command
        }
    }
    return 0;
}

int main(int argc, char* argv[])
{
    int numWords = 0;
    int index = 0;

    while (1) {
        size_t buf_size = 100000;
        char buffer[buf_size];
        memset(buffer, '\0', buf_size);
        char *pbuffer = buffer;
        printf("sish> ");                                               // show the prompt
        size_t numCharacters = getline(&pbuffer, &buf_size, stdin);         // this takes the entire user input
        if (feof(stdin)) {
            // perror("Reached eof for input redirected file .. exiting to reset my stdin");
            int fd = open ("testfile", O_WRONLY | O_CREAT, 0777);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
            exit(EXIT_FAILURE); // should not happen
        }
        if (numCharacters == 1) { continue; }
        char* parseCmd = strdup(buffer);                                   // dont touch the input buffer; duplicated into another char* variable
        char* pipeCmd = strdup(buffer);                                    // check if there are pipes
        char* word;
        if (index > HISTORY_MAX - 1) {
            index = 0;
        }
        h[index].offset = index;                                        // history has a set of 100 structures; filling each index
        buffer[numCharacters-1] = '\0';                                    // the last character is a \n replaced with \0
        parseCmd[numCharacters-1] = '\0';
        h[index].enteredCommand = (char *) malloc(numCharacters * sizeof(char));    // doing this to avoid writing inefficient static arrays
        h[index].isPiped = false;
        strcpy(h[index].enteredCommand, parseCmd);                      // copy the command entered by the user as it is to the history array at current index
        // check if the entered command has pipes
        char* item; char* strCmds[50]; int counter = 0;
        while ((item = strtok_r(pipeCmd, "|", &pipeCmd)) != NULL) { 
            // parse into multiple strings
            int num = strlen(item);
            strCmds[counter] = (char *) malloc((num + 1)* sizeof (char));
            strcpy(strCmds[counter], item);
            counter++;
        }
        if  (counter > 1) {
            h[index].isPiped = true;
            processPipedCommand(h[index].enteredCommand);  
            numWords = 0;
            counter = 0;
            index++;
            continue;
        }

        for (int k = 0; k < counter; k++)
            free(strCmds[k]);

        while ((word = strtok_r(parseCmd, " ", &parseCmd)) != NULL) {   // delimit by each word
            if (strncmp("exit", word, 4) == 0) {                        // did the user enter exit?  then leave the shell
                    exit(EXIT_SUCCESS);
            } else if (strcmp(word, "\n") == 0) {                       // does the tokenizer see a \n at the end?
                    continue;                                           // continue goes to next iteration
            } else {
                h[index].command[numWords] = (char *) malloc(strlen(word) * sizeof(char));
                strcpy(h[index].command[numWords], word);               // putting the arguments into the command array inside history structure
                numWords++;                                             // this just increments the counter for number of tokens parsed
            }
        } 
        h[index].numArgs = numWords;                                    // store the number of arguments
                    
        if (strcmp(h[index].command[0], "history") == 0) {
            if (h[index].numArgs > 2) {                                 // history command with too many arguments is error condition
                        perror("This is not correct. History command takes only 1 or no argument");
                        numWords = 0;
                        index++;
                        continue;
            }
            if (h[index].numArgs == 1) {                                // command to display a list of all history commands
                        printHistory(index);
                        numWords = 0;
                        index++;
                        continue;
            }
            if (h[index].numArgs == 2) {
                if (strcmp(h[index].command[1], "-c") == 0) {           // history command with -c option clears history
                            memset(h, 0, sizeof(h));   
                            index = 0;
                            numWords = 0;
                            continue;
                } else { // using offset
                    int cmdoffset = atoi(h[index].command[1]);
                    if ((cmdoffset < 0) || (cmdoffset > index)) 
                    {   // history command with invalid index is checked and exception provided
                        perror ("history offset error : This offset is incorrect or invalid \n");
                        numWords = 0;
                        index++;
                        continue;
                    } else  {   // valid offset provided to run command from history     
                        if (h[cmdoffset].isPiped == true)
                            processPipedCommand(h[cmdoffset].enteredCommand);
                        else                        
                            executeHistory(cmdoffset);
                        numWords = 0;
                        index++;
                        continue;
                    } // executing the offset command
                } // else part
            } // args == 2
        } // history
        if ((strcmp(h[index].command[0], "cd") == 0) && (h[index].numArgs == 2)) {
            if (chdir(h[index].command[1]) != 0) {
                perror ("Unable to change directory");
                index++;
                numWords = 0;
                continue;
            }
            numWords = 0;
            index++;
            continue;
        } // doing all the cd error stuff here
        else if ((strcmp(h[index].command[0], "cd") == 0) && (h[index].numArgs == 1)) {
            perror("This is an error.  Please provide directory after cd .. \n");
            numWords = 0;
            index++;
            continue;
        }
        execute(index);
        numWords = 0;
        index++;
        continue;
    } // while (1) ends here
}
