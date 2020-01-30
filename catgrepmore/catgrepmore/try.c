#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

static int MAX_BUFSIZ = 1024;
int bytesRead, totalBytesRead, filesRead = 0;

void handler()
{
    fprintf(stderr, "Files read: %d\nBytes read: %d\n", filesRead, totalBytesRead);
    exit(EXIT_FAILURE);
}

const struct sigaction sig_ignore = {
        .sa_handler = SIG_IGN,
        .sa_mask = 0,
        .sa_flags = 0
};

const struct sigaction sig_interrupt = {
        .sa_handler = handler,
        .sa_mask = 0,
        .sa_flags = 0
};

int main(int argc, char* argv[]) {

    if(argc < 3 || strlen(argv[1]) == 0) {
        fprintf(stderr,"Error: Please use proper usage: ./catgrepmore pattern infile1 [...infile2...]\n");
        return -1;
    }

    if(sigaction(SIGINT, &sig_ignore, 0) == -1) {
        fprintf(stderr, "Error: Failed to handle signal ignore\n%s\n", strerror(errno));
    }
    if(sigaction(SIGINT, &sig_interrupt, 0) == -1) {
        fprintf(stderr, "Error: Failed to handle signal interrupt\n%s\n", strerror(errno));
    }


    int g_stat, m_stat;

    char * pattern = malloc(MAX_BUFSIZ);
    char * buffer = malloc(MAX_BUFSIZ);
    //check malloc failure
    if(!pattern || !buffer) {
        fprintf(stderr, "Error: malloc() failed.\n%s\n", strerror(errno));
        return -1;
    }

    pattern = argv[1];

    int m_pipe[2];
    int g_pipe[2];
    int fd;

    for(int i = 2; i < argc; i++) {

        pid_t m_pid, g_pid;

        if(pipe(m_pipe) == -1) {
            fprintf(stderr, "Error: Could not successfully pipe m_pipe\n%s\n", strerror(errno));
        }

        if(pipe(g_pipe) == -1) {
            fprintf(stderr, "Error: Could not successfully pipe g_pipe\n%s\n", strerror(errno));
        }

        g_pid = fork();
        switch(g_pid = fork()) {
            case -1:
                fprintf(stderr, "Error: fork() failed for grep().\n%s\n", strerror(errno));
                return -1;

            case 0:
                if(close(g_pipe[1]) < 0){
                    fprintf(stderr, "Error: Could not successfully close the read end of g_pipe\n%s\n", strerror(errno));
                    return -1;
                }
                if(close(m_pipe[0]) < 0) {
                    fprintf(stderr, "Error: Could not successfully close the write end of m_pipe\n%s\n", strerror(errno));
                    return -1;
                }
                if(dup2(g_pipe[0], STDIN_FILENO) < 0 ) {
                    fprintf(stderr, "Error: Could not successfully dup2() the write end of g_pipe\n%s\n", strerror(errno));
                    return -1;
                }
                if(dup2(m_pipe[1], STDOUT_FILENO) < 0) {
                    fprintf(stderr, "Error: Could not successfully dup2() the read end of m_pipe\n%s\n", strerror(errno));
                    return -1;
                }
                execlp("grep", "grep", pattern, NULL);
                return -1;
            default:
                if(close(g_pipe[0]) < 0 || close(m_pipe[1]) < 0) {
                    fprintf(stderr, "Error: Could not close unused pipes\n%s\n", strerror(errno));
                    return -1;
                }
                break;

        }
        m_pid = fork();
        if(m_pid == 0) {
            if (close(g_pipe[1]) < 0) {
                fprintf(stderr, "Error: Could not close write side of g_pipe\n%s\n", strerror(errno));
                exit(-1);
            }
            if (dup2(m_pipe[0], STDIN_FILENO) < 0) {
                fprintf(stderr, "Error: Could not redirect stdin to more().\n%s\n", strerror(errno));
                exit(-1);
            }
            execlp("more", "more", NULL);
            exit(-1);

        }
        else if (m_pid > 0) {
            if (close(m_pipe[0]) < 0) {
                fprintf(stderr, "Error: Could not close read side of m_pipe\n%s\n", strerror(errno));
                exit(-1);
            }
            if((fd = open(argv[i], O_RDONLY)) < 0) {
                fprintf(stderr, "Error: Could not open file %s\n%s\n", argv[i], strerror(errno));
                exit(-1);
            }

            filesRead++;

            while(1) {
                totalBytesRead += bytesRead = read(fd, buffer, MAX_BUFSIZ);
                if(bytesRead < 0) {
                    perror("Error reading input");
                    exit(-1);
                }
                else {
                    int wbytes = write(g_pipe[1], buffer, bytesRead);
                    if(wbytes < 0) {
                        perror("Error writing to output");
                        exit(-1);
                    }
                    if(bytesRead < MAX_BUFSIZ) { //reached EOF
                        if(close(g_pipe[1]) < 0) {
                            perror("Cannot close write pipe");
                        }
                        break;
                    }
                }
            }

            waitpid(g_pid, &g_stat, 0);
            waitpid(m_pid, &m_stat, 0);
            if(close(fd) < 0) {
                fprintf(stderr, "Error: Could not successfully close file\n%s\n", strerror(errno));
                exit(-1);
            }
        }
        else {
            fprintf(stderr, "Error: fork() for more() failed.\n%s\n", strerror(errno));
            exit(-1);
        }
    }

    handler();
    return 0;
}
