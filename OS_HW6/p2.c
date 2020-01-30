#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include "tas.h"
#define core 2

#define NSLOTS 10

void spin_lock(char *lock)
{
    while(tas(lock))
        ; 
}

void spin_unlock(char *lock)
{
    *lock = 0;
}

struct dll {
    char lock;
    int value;
    struct dll *fwd, *rev;
};

struct slab {
    char lock;
    char freemap[NSLOTS];
    struct dll slots[NSLOTS];

};


void *slab_alloc(struct slab *slab)
{
    spin_lock(&(slab->lock));

    for(int i=0; i<NSLOTS; i++)
    {
        if(slab->freemap[i] == 0)
        {
            slab->freemap[i] = 1;
            spin_unlock(&(slab->lock));
            return &(slab->slots[i]);
        }
    }

    spin_unlock(&(slab->lock));
    return NULL;

}


int slab_dealloc(struct slab *slab, void *object)
{
    spin_lock(&(slab->lock));

    for (int i =0; i < NSLOTS; i++)
    {
        if(object == (&(slab->slots[i])))
        {
            if(slab->freemap[i] == 1)
            {   
                slab->freemap[i] = 0;
                spin_unlock(&(slab->lock));
                return 1;
            }
            else
            {
                spin_unlock(&(slab->lock));
                return -1;
            }
        }
    }
    spin_unlock(&(slab->lock));
    return -1;
}

struct dll *dll_insert(struct dll *anchor,int value,struct slab *slab)
{
    spin_lock(&(anchor->lock));

    //create a new node
    struct dll *newNode;
    if((newNode= slab_alloc(slab)) == NULL)
    {
        spin_unlock(&(anchor->lock));
        return NULL;
    }

    newNode->value = value;
    
    struct dll *current = anchor;
    while(current->fwd != anchor && current->fwd->value < value)
        current = current->fwd;
        
    newNode->fwd = current->fwd;
    newNode->fwd->rev = newNode;
    newNode->rev = current;
    current->fwd = newNode;

    spin_unlock(&(anchor->lock));
    return newNode;
}


void dll_delete(struct dll *anchor, struct dll *node,struct slab *slab)
{
    spin_lock(&(anchor->lock));

    if(node == NULL)
    {
        spin_unlock(&(anchor->lock));
        return;
    }

    if(slab_dealloc(slab, node) == -1)
    {
        spin_unlock(&(anchor->lock));
        return;
    }

    node->rev->fwd = node->fwd;
    node->fwd->rev = node->rev;

    spin_unlock(&(anchor->lock));
    return;
}

struct dll *dll_find(struct dll *anchor,int value)
{
    spin_lock(&(anchor->lock));

    struct dll *current = anchor;

    while(current->fwd != anchor)
    {
        
        if (current->fwd->value == value)
        {   
            spin_unlock(&(anchor->lock));
            return current->fwd;
        }
        else
        {
            current = current->fwd;
        }

    }
    spin_unlock(&(anchor->lock));
    return NULL;
}


int main()
{
    struct slab *slab = (struct slab *) mmap(NULL, sizeof(struct slab*), PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_SHARED, -1, 0);

    struct dll *anchor;

    if((anchor = slab_alloc(slab)) == NULL)
        exit(EXIT_FAILURE);

    anchor->lock = 0;
    anchor->fwd = anchor;
    anchor->rev = anchor;
    anchor->value = 0;
    
    for(int i=0; i<core; i++)
    {
        pid_t pid = fork();
        if(pid == -1)
            fprintf(stderr, "Error:Can't fork: %s\n", strerror(errno));
        else if(pid == 0)
        {
            srand(time(NULL));
            for(int j=0; j<1000000; j++)
            {
                //set value between 1-10
                int value = (rand() % NSLOTS)+1;
                int action = (rand() %3) +1;
                struct dll *delNode;
                switch(action)
                {
                    case 1:
                        dll_insert(anchor, value, slab);
                            //fprintf(stderr, "Error:Can't dll_insert in pid %d in iteration %d\n", getpid(), j);
                        break;
                    case 2:
                        //keep in slots 1-9
                        delNode = &(slab->slots[(value%9)+1]);
                        //fprintf(stderr, "start to delete in pid %d in iteration %d\n", getpid(), j);
                        dll_delete(anchor, delNode, slab);
                        break;
                    case 3:
                        dll_find(anchor, value);
                            //fprintf(stderr, "Error:Can't find in pid %d in iteration %d\n", getpid(), j);
                        break;
                }
            }
            exit(EXIT_SUCCESS);
        }

    }
    for(int k=0; k<core; k++)
        wait(NULL);

    //printf the dll
    struct dll *current = anchor;

    while(current->fwd!= anchor)
    {
        printf("%d\t", current->fwd->value);
        current = current->fwd;
    }
    printf("\n");

    if(munmap(slab, sizeof(struct slab*)) == -1)
    {
        fprintf(stderr, "Error:Can't unmap slab: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

}