//
//  main.c
//  myShell
//
//  Created by Zhihao Wang on 10/21/19.
//  Copyright © 2019 Zhihao Wang. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/time.h>
#define TOKSEP " \n"

void splitLine(FILE *input, int *errcode);
int mycd(char **arg);
int mypwd(void);
int myexit(char **arg, int *errcode);
int execLine(char **arg, char **redir, int *errcode);
int runLine(char **arg, char **redir, int *errcode);
void ioRedir(char **redir);
int redirect(char *path, int newfd, int flags, mode_t mode);



int main(int argc, const char **argv)
{
    FILE * fdin;
    int errcode =0;
    
    if(argc>1){
        if((fdin = fopen(argv[1], "r")) == NULL){
            fprintf(stderr, "Error:Can't open file %s in read mode: %s\n", argv[1], strerror(errno));
            exit(1);
        }
        else
            splitLine(fdin,&errcode);
    }
    else if(argc == 1){
        fdin = stdin;
        splitLine(fdin, &errcode);
    }
    
    return errcode;
}


void splitLine(FILE *input, int *errcode){
    char *line;
    char **arg;
    char **iodir;
    int estatus, linenum=0;
    size_t buffsize = 0;
    
    do{
        int dirpos = 0, argpos = 0;
        char *token;
        char **args = malloc(BUFSIZ);
        char **redir = malloc(BUFSIZ);
        char *templine = malloc(BUFSIZ);
        if (getline(&line, &buffsize, input) == -1){
            if (errno!=0){
                fprintf(stderr, "Error: Can't getline: %s", strerror(errno));
                exit(errno);
            }
            else{
                fprintf(stderr, "End of file read, exiting shell with exit code:%i\n",*errcode);
                exit(*errcode);
            }
            
        }
        strcpy(templine ,line);
        token = strtok(templine, TOKSEP);
        while (token != NULL){
            //check if token is io redirection
            if ((token[0]== '<')||(token[0]=='>')||((token[0]=='2')&&(token[1]=='>'))){
                redir[dirpos] = token;
                dirpos ++;
            }
            else{
                args[argpos] = token;
                argpos++;
                
            }
            token = strtok(NULL, TOKSEP);
        }
        redir[dirpos] = NULL;
        args[argpos] = NULL;
        
        arg = args;
        iodir = redir;
        estatus = execLine(arg, iodir, errcode);
        free(args);
        free(redir);
        free(templine);
    }
    while(estatus);
    
}


int mycd(char **arg){
    if(arg[1] == NULL){
        if(chdir(getenv("HOME")) < 0){
            fprintf(stderr, "Error:Can't change directory to %s: %s\n", getenv("HOME"), strerror(errno));
        }
    }
    else{
        if (chdir(arg[1]) < 0){
            fprintf(stderr, "Error:Can't change directory to %s: %s\n", arg[1], strerror(errno));
        }
    }
    return 1;
}

int mypwd(void){
    char cwd[BUFSIZ];
    if ((getcwd(cwd, sizeof(cwd))) == NULL)
        fprintf(stderr, "Error: Can't retrive current working directory: %s\n", strerror(errno));
    else
        printf("%s\n", cwd);
    return 1;
}

int myexit(char **arg, int *errcode){
    if(arg[1] != NULL)
        *errcode = atoi(arg[1]);
    exit(*errcode);
}



int execLine(char **arg, char **redir, int *errcode){
    if(arg[0] == NULL)
        return 1;
    
    //ignore if #
    if(strstr(arg[0], "#"))
        return 1;
    
    if(strcmp(arg[0], "cd")==0)
        return(mycd(arg));
    
    if(strcmp(arg[0], "pwd")==0)
        return(mypwd());
    
    if(strcmp(arg[0], "exit") ==0)
        return(myexit(arg, errcode));
    
    //if command not built-in, exec with ioredirection
    return(runLine(arg, redir, errcode));
}



int runLine(char **arg, char **redir, int *errcode){
    pid_t pid, wpid;
    int wstat, tmdiff;
    struct timeval tmbegin, tmend;
    struct rusage stats;
    
    //start timing
    gettimeofday(&tmbegin, NULL);
    pid = fork();
    if (pid == 0){
        //In child process
        ioRedir(redir);
        if(execvp(arg[0], arg) == -1){
            fprintf(stderr, "Error: Can't execute command %s: %s\n", arg[0], strerror(errno));
            exit(127);
        }
        exit(EXIT_FAILURE);
    }
    else if(pid<0)
        fprintf(stderr, "Error:Can't fork the process %s: %s\n", arg[0], strerror(errno));
    else{
        //wait and get wait status
        if((wpid = wait3(&wstat, WUNTRACED, &stats))==-1)
            fprintf(stderr, "Error: wait3 failed: %s\n", strerror(errno));
        else{
            gettimeofday(&tmend, NULL);
            if(wstat!=0){
                if(WIFSIGNALED(wstat)){
                    fprintf(stderr, "Child process %d exited with signal: %d\n",wpid, WTERMSIG(wstat));
                }
                else{
                    fprintf(stderr, "Child process %d exited with return val: %d\n",wpid, WEXITSTATUS(wstat));
                    *errcode = WEXITSTATUS(wstat);
                }
            }
            else
                fprintf(stderr, "Child process %d exited normally\n", wpid);
            tmdiff = tmend.tv_sec*1000000 +tmend.tv_usec - (tmbegin.tv_sec*1000000 +tmbegin.tv_usec);
            fprintf(stderr, "Real: %d.%04ds\t", tmdiff/1000000, tmdiff%1000000);
            fprintf(stderr, "User: %ld.%04ds\t", stats.ru_utime.tv_sec, stats.ru_utime.tv_usec);
            fprintf(stderr, "System: %ld.%04ds\n", stats.ru_stime.tv_sec, stats.ru_stime.tv_usec);
        }
    }
        
    return 1;
}

void ioRedir(char **redir){
    char *curdir;
    for(int i=0; redir[i] != NULL; i++){
        curdir = redir[i];
        if(strstr(curdir, ">")){
            curdir+=1;
            //child exit 1 when fail to redirect
            if(redirect(curdir, 1, O_RDWR|O_CREAT|O_APPEND, 0666))
                exit(1);
        }
        else if(strstr(curdir, "<")){
            curdir+=1;
            if(redirect(curdir, 0, O_RDONLY, 0666))
                exit(1);
        }
        else if(strstr(curdir, "2>")){
            curdir+=2;
            if(redirect(curdir, 2, O_RDWR|O_CREAT|O_TRUNC, 0666))
                exit(1);
        }
        else if(strstr(curdir, ">>")){
            curdir+=2;
            if(redirect(curdir, 1, O_RDWR|O_CREAT|O_APPEND, 0666))
                exit(1);
        }
        else if(strstr(curdir, "2>>")){
            curdir+=3;
            if(redirect(curdir, 2, O_RDWR|O_CREAT|O_APPEND, 0666))
                exit(1);
        }
    }
}

//redirect file and report errors
int redirect(char *path, int newfd, int flags, mode_t mode){
    int fd;
    if((fd = open(path, flags, mode))<0){
        fprintf(stderr, "Error: Can't open file %s: %s\n", path, strerror(errno));
        return 1;
    }
    if((dup2(fd, newfd)<0)){
        fprintf(stderr, "Error: Can't duplicate file %s: %s\n", path, strerror(errno));
        return 1;
    }
    if((close(fd)<0)){
        fprintf(stderr, "Error: Can't close file %s: %s\n", path, strerror(errno));
        return 1;
    }
    //if IO redirection is successful
    return 0;
    
}
