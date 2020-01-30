/*
 * ECE 357: Computer Operating Systems 
 * Problem Set 4: Problem 3 
 * By: Guy Bar Yosef 
 *
 * catgrepmore - For each infile specified, this program 
 * pipes the infile into a 'grep' program with the specified
 * pattern and then pipes the output into the 'more' program.
 * 
 * version: 1.0
 * 
 * To run: 
 *  % ./catgrepmore pattern infline1 [...infile2...]
 */


#include <signal.h>
#include <stdio.h>
#include <string.h> 
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>   
#include <unistd.h>
#include <sys/wait.h>

#define BUFSIZE 4096 /* reading from files buffer size */

void handlerINT (int signo);
int openInput(const char *file);
int pipeIO(int pipe[], const int fd_to_redirect);
int closePipe(int pipe[]);


/* global vars used for SIGINT handler */
unsigned long long bytesProcessed = 0; /* total bytes processed */
unsigned filesProcessed = 0;           /* total files processed */


int main(int argc, char **argv) {
    
    /* confirm correct usage */
    if (argc < 3) {
        fprintf(stderr, "Correct usage: ./catgrepmore pattern infline1 [...infile2...]\n");
        return -1;
    }
    
    /* handle the SIGINT signal appropriately */
    struct sigaction sa;
    sa.sa_handler=handlerINT;
    sa.sa_flags=0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, 0) == -1) {
        fprintf(stderr, "Unable to provide a handler as the" 
                        "disposition of SIGINT: %s\n", strerror(errno));
    }
    
    /* iterate through inputted files */
    for (int i = 2 ; i < argc ; ++i) { 
        printf("In file: %s\n\n", argv[i]);

        /* constructing pipes from 'cat' to 'grep' and from 'grep' to 'more' */
        int p1[2], p2[2];
        if (pipe(p1) == -1 || pipe(p2) == -1) {
            fprintf(stderr, "Unable to create pipes: %s\n", strerror(errno));
            return -1;
        }

        /* forking and exec-uting */
        int pid1, pid2, status1, status2;
        pid1 = fork();
        if (pid1 == -1) { /* error forking */
            fprintf(stderr, "Error during process forking: %s\n", strerror(errno));
            return -1;
        }    
        else if (pid1 == 0) { /* in 'grep' */

            /* connect stdIO to pipes and close both pipes' file descriptors */
            if (pipeIO(p1, STDIN_FILENO) < 0 || pipeIO(p2, STDOUT_FILENO) < 0)
                return -1;

            if (closePipe(p1) == -1 || closePipe(p2) == -1)
                return -1;

            /* run grep with the inputted pattern (argv[1]) */
            if (execlp("grep", "grep", argv[1], (char *)0) == -1) {
                fprintf(stderr, "Error running 'grep' program: %s\n", strerror(errno));
                return -1;
            }
        }
        else {
            pid2 = fork();
            if (pid2 == -1) { /* error forking */
                fprintf(stderr, "Error during process forking: %s\n", strerror(errno));
                return -1;
            }  
            else if (pid2 == 0) { /* in 'more' */
                
                /* connect stdIO to pipe2 and close both pipes' file descriptors */
                if (pipeIO(p2, STDIN_FILENO) < 0)
                    return -1;
                
                if (closePipe(p1) == -1 || closePipe(p2) == -1)
                    return -1;

                if (execlp("more", "more", (char *)0) == -1) {
                    fprintf(stderr, "Error running 'more' program: %s\n", strerror(errno));
                    return -1;
                }
            }
            else { /* in catgrepmore */
                
                /* opening input file */
                int input;
                if ((input = openInput(argv[i])) == -1)
                    return -1; 
                ++filesProcessed; /* update total files processed */

                /* the reading buffer */
                char *buffer;
                if (!(buffer = malloc(sizeof(char)*BUFSIZE)) ) {
                    fprintf(stderr, "Unable to allocate memory for default buffer size: %s\n", 
                                                                                    strerror(errno));
                    return -1;
                }

                int nwt, nrd, tmp; /*num of bytes writen, num of bytes read, temp var */
                while (nrd = read(input, buffer, BUFSIZE*sizeof(char))) { /* while not at EOF */
                    if (nrd < 0) { /* if unsuccessful read */
                        fprintf(stderr, "An error occurred while trying to "
                                        "read from pipe1: %s\n", strerror(errno));
                        return -1;
                    }
    
                    bytesProcessed += nrd; /* update total bytes processed */

                    nwt = 0;
                    while (nwt < nrd) { /* accounts for partial writes */

                        /* write to first pipe's input */
                        if ( (tmp = write(p1[1], buffer + nwt, nrd - nwt)) <= 0 ) {  
                            fprintf(stderr, "Unable to write buffer to output: %s", 
                                                                        strerror(errno));
                            return -1;
                        }
                        nwt += tmp;
                    }
                }

                /* close pipes and wait for 'grep' and 'more' programs to finish */
                if (closePipe(p1) == -1 || closePipe(p2) == -1)
                    return -1;

                if (waitpid(pid1, &status1, 0) < 0 || waitpid(pid2, &status2, 0) < 0) 
                    return -1;
                else if (!WIFEXITED(status1) || !WIFEXITED(status2)) {
                    perror("The 'grep' or 'more' programs exited abnormally.\n");
                    return -1;
                }
            }
        } 
    }
    return 0;
}


/*
 * handlerINT - a signal handler for SIGINT.
 * 
 * Writes the total number of files and 
 * bytes processed to stder and then exits the program.
 */
void handlerINT (int signo) {
    fprintf(stderr, "Files Processed: %u, Bytes Processed: %llu.\n",
                                        filesProcessed, bytesProcessed);
    exit(0);
}


/*
 * processInput - Opens the input file.
 * 
 * Returns file descriptor on success, -1 on error.
 */
int openInput(const char *file) {
    int input;
    if ( (input = open(file, O_RDONLY)) == -1) { 
        fprintf(stderr, "Unable to open %s for reading: %s\n", 
                                            file, strerror(errno));
        return -1;
    }
    return input;
}


/* 
 * pipeIO - Depending on whether fd_to_redirect is
 * stdin or stdout, this function redirects the appropriate
 * stdIO to the correct end of the pipe specified in the
 * array 'pipe'. This function then closes the file
 * descriptors of the pipe.
 * 
 * Returns:
 *    0 - Success
 *   -1 - Error with IO redirection using the dup2 system call.
 *   -2 - Error closing the file descriptors of the pipe.
 */
int pipeIO(int pipe[], const int fd_to_redirect) {
    /* connect stding to pipe and close the pipe's file descriptors */
    if (fd_to_redirect == STDIN_FILENO) {
        if (dup2(pipe[0], STDIN_FILENO) == -1) {
            fprintf(stderr, "Error redirecting input: %s\n", strerror(errno));
            return -1;
        }
    }
    else {
        if (dup2(pipe[1], STDOUT_FILENO) == -1) {
            fprintf(stderr, "Error redirecting output: %s\n", strerror(errno));
            return -1;
        }
    }
    return 0;
}


/*
 * closePipe - Closes pipe.
 * 
 * Returns 0 upon success, -1 on error.
 */
int closePipe(int pipe[]) {
    if (close(pipe[0]) == -1 || close(pipe[1]) == -1) {
        fprintf(stderr, "Error closing file descriptors of pipe: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}