//  ECE357
//  OS_HW4
//  catgrepmore
//  Created by Zhihao Wang
 
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <setjmp.h>
 
#define BUFFSIZE 4096
 
void handler1(int signo);
void handler2(int signo);
 
unsigned byteProcessed, fileProcessed =0;
jmp_buf int_jb;
 
int main(int argc, char * argv[])
{
 
    if (argc<3 || strlen(argv[1]) == 0)
    {
        fprintf(stderr, "Error: Correct Format is ./catgrepmore pattern infile1 [..infile2..]\n");
        return -1;
    }
    
    struct sigaction sa1 =
    {
        .sa_handler = handler1,
        .sa_mask = 0,
        .sa_flags = 0
    };
    struct sigaction sa2 =
    {
        .sa_handler = handler2,
        .sa_mask = 0,
        .sa_flags = 0
    };
    
    if (sigaction(SIGINT, &sa1, 0) == -1)
        fprintf(stderr, "Error: Can't handle SIGINT: %s\n", strerror(errno));
    if (sigaction(SIGPIPE, &sa2, 0) == -1)
        fprintf(stderr, "Error: Can't handle SIGPIPE: %s\n", strerror(errno));
 
    for (int i=2; i<argc; i++)
    {
        if(setjmp(int_jb) != 0)
        {
            continue;
        }
        pid_t pid1, pid2;
        int stat1, stat2;
        int pipe1[2], pipe2[2];
        if (pipe(pipe1) ==-1 || pipe(pipe2) == -1)
            fprintf(stderr, "Error: Can't create pipes: %s\n", strerror(errno));
        
        switch(pid1 = fork())
        {
            case -1:
                fprintf(stderr, "Error: Can't fork for grep(): %s", strerror(errno));
                return -1;
            
            case 0:
                if (dup2(pipe1[0], STDIN_FILENO)<0)
                {
                    fprintf(stderr, "Error: Can't duplicate write end of grep pipe: %s\n", strerror(errno));
                    return -1;
                }
                
                if (dup2(pipe2[1], STDOUT_FILENO)<0)
                {
                    fprintf(stderr, "Error: Can't duplicate read end of more pipe: %s\n", strerror(errno));
                    return -1;
                }
                
                if (close(pipe1[1])<0)
                {
                    fprintf(stderr, "Error: Can't close read end of grep pipe: %s\n", strerror(errno));
                    return -1;
                }
                
                if (close(pipe2[0])<0)
                {
                    fprintf(stderr, "Error: Can't close write end of more pipe: %s\n", strerror(errno));
                    return -1;
                }
                
                if (execlp("grep", "grep", argv[1],NULL) <0)
                {
                    fprintf(stderr, "Error: Can't exec grep: %s\n", strerror(errno));
                    return -1;
                }
                return -1;
                
            default:
                if(close(pipe1[0])<0 || close(pipe2[1])<0)
                {
                    fprintf(stderr, "Error: Can't close pipes: %s\n", strerror(errno));
                    return -1;
                }
                break;
        }
        
        pid2 = fork();
        if (pid2<0)
        {
            fprintf(stderr, "Error: Can't fork for more(): %s", strerror(errno));
            return -1;
        }
        if(pid2 == 0)
        {
            if(dup2(pipe2[0], STDIN_FILENO)<0){
                fprintf(stderr, "Error: Can't duplicate stdin to more pipe: %s\n", strerror(errno));
                exit(-1);
            }
            if(close(pipe1[1])<0)
            {
                fprintf(stderr, "Error: Can't close write end of grep pipe: %s\n", strerror(errno));
                exit(-1);
            }
            if(execlp("more", "more", NULL)<0)
            {
                fprintf(stderr, "Error: Can't exec more: %s\n", strerror(errno));
                exit(-1);
            }
        }
        else
        {
            //in parent
            int fd, readNum, writeNum, tempNum;
            char *buffer = malloc(BUFFSIZE);
            if (!buffer)
            {
                fprintf(stderr, "Error: Can't malloc: %s\n", strerror(errno));
                exit(-1);
            }
                
            if((fd = open(argv[i], O_RDONLY))<0)
            {
                fprintf(stderr, "Error:Can't open file %s for reading: %s\n", argv[i], strerror(errno));
                exit(-1);
            }
            fileProcessed ++;
            
            while((readNum = read(fd, buffer, BUFFSIZE)))
            {
                if(readNum<0)
                {
                    fprintf(stderr, "Error: Can't read input file %s: %s\n",argv[i], strerror(errno));
                    exit(-1);
                }
                byteProcessed += readNum;
                writeNum = 0;
                //handle partial write
                while (writeNum < readNum)
                {
                    if ((tempNum = write(pipe1[1], buffer + writeNum, readNum - writeNum))<=0)
                    {
                        fprintf(stderr, "Error: Can't write from buffer: %s\n", strerror(errno));
                        exit(-1);
                    }
                    writeNum += tempNum;
                }
            }
            
            if(close(pipe1[1])<0)
            {
                fprintf(stderr, "Error: Can't close write pipe: %s\n", strerror(errno));
                exit(-1);
            }
            if(close(fd)<0)
            {
                fprintf(stderr, "Error: Can't close file %s: %s\n", argv[i], strerror(errno));
                return -1;
            }
 
            waitpid(pid1, &stat1, 0);
            waitpid(pid2, &stat2, 0);
        }
    }
    return 0;
}
 
void handler1(int signo)
{
    fprintf(stderr, "\nTotal files processed: %u\nTotal bytes processed: %u\n", fileProcessed, byteProcessed);
    longjmp(int_jb, 1);
    
}
 
void handler2(int signo)
{

    longjmp(int_jb, 1);
    
}
 


