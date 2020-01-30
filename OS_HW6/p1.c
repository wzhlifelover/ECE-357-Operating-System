#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include "tas.h"
#define core 2

void spin_lock(char *lock);
void spin_unlock(char *lock);

void spin_lock(char *lock)
{
    while(tas(lock))
        ;
    
}

void spin_unlock(char *lock)
{
    *lock = 0;
}

int main()
{
    int *map1 = mmap(NULL, sizeof(int *), PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_SHARED, -1, 0);
    if(map1 == MAP_FAILED)
    {
        fprintf(stderr, "Error:Can't mmap for map1: %s\n", strerror(errno));
        return(EXIT_FAILURE);
    }

    char *lock = mmap(NULL, sizeof(char *), PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_SHARED, -1, 0);
    if(lock == MAP_FAILED)
    {
        fprintf(stderr, "Error:Can't mmap for map2: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    //test without mutex lock
    printf("test without lock\n");
    *map1 = 0;
    for(int i =0; i<core; i++)
    {
        pid_t pid = fork();
        if(pid ==-1)
        {
            fprintf(stderr, "Error: Can't fork: %s\n", strerror(errno));
            continue;
        }
        else if(pid == 0)
        {
            for(int j=0; j<1000000; j++)
                *map1 = *map1+1;
            exit(EXIT_SUCCESS);
        }
    }
    for(int k=0; k<core; k++)
        wait(NULL);
    
    fprintf(stderr, "Map1 = %d\n", *map1);

    printf("test with lock\n");
    *map1 = 0;
    for(int i =0; i<core; i++)
    {
        pid_t pid = fork();
        if(pid ==-1)
        {
            fprintf(stderr, "Error: Can't fork: %s\n", strerror(errno));
            continue;
        }
        else if(pid == 0)
        {
            for(int j=0; j<1000000; j++)
            {
                spin_lock(lock);
                *map1 = *map1+1;
                spin_unlock(lock);
            }
            exit(EXIT_SUCCESS);
        }
    }
    for(int k=0; k<core; k++)
        wait(NULL);
    fprintf(stderr, "Map1 = %d\n", *map1);
    printf("result should be 2000000\n");
}