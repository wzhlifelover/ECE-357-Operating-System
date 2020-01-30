//Zhihao Wang
//OS HW5

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>



void sigHandler(int sig)
{
    fprintf(stderr, "Signal %s received\n",strsignal(sig));
    exit(sig);
}

int createFile(size_t length)
{
    int fd;
    char buff[1];
    if((fd = open("test.txt", O_RDWR|O_CREAT|O_TRUNC, 0666))<0)
    {
        fprintf(stderr, "Error: Can't open file test.txt for reading: %s\n", strerror(errno));
        exit(255);
    }


    //Used this method to randomly generate "A to Z" reference: https://www.dreamincode.net/forums/topic/172322-generate-random-letters-from-a-to-z/
    srand (time(NULL));
    for(int i = 0; i < length; i++)
    {
        buff[0] = rand() % 26 + 65;
        if(write(fd, buff, 1) < 0)
        {
            fprintf(stderr, "Error: Can't write to test.txt file: %s\n", strerror(errno));
            exit(255);
         }
    }

    return fd;

}

void mtest1()
{
    char *map;
    size_t length = 4096;
    int fd = createFile(length);
    printf("Executing Test #1 (write to r/o mmap):\n");

    if((map = mmap(NULL, length, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED)
    {
        fprintf(stderr, "Error: Can't mmap file to memory: %s\n", strerror(errno));
        exit(255);
    }
  
    printf("map[3]==\'%c\'\n", map[3]);
    printf("Writing a \'1\'\n");

    map[3] = '1';

    if(munmap(map, length)<0)
    {
        fprintf(stderr, "Error: Can't unmap file to memory: %s\n", strerror(errno));
        exit(255);
    }
    
    
    if(close(fd)<0)
    {
        fprintf(stderr, "Error: Can't close file test.txt: %s\n", strerror(errno));
        exit(255);

    }
    exit(0);
}

void mtest2()
{
    char *map;
    char buff[1];
    size_t length = 4096;
    int fd = createFile(length);
    int offset = 10;
    int visible;

    printf("Executing Test #2 (write to MAP_SHARED mmap):\n");

    if((map = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0))== MAP_FAILED)
    {
        fprintf(stderr, "Error: Can't mmap file to memory: %s\n", strerror(errno));
        exit(255);

    }

    printf("map[%d]== \'%c\'\n", offset, map[offset]);
    printf("writing a \'2\'\n");
    map[offset] = '2';
    printf("new map[%d]== \'%c\'\n", offset, map[offset]);

    if(lseek(fd, offset, SEEK_SET)<0)
        fprintf(stderr, "Can't lseek file test.txt: %s\n", strerror(errno));

    if(read(fd, buff, 1)<0)
        fprintf(stderr, "Can't read file test.txt: %s\n", strerror(errno));
    
    printf("read == \'%c\'\n", buff[0]);
    if(buff[0] == '2')
    {
        visible = 0;
        printf("Update to MAP_SHARED map is visible\n");
    }
    else
    {
        visible = 1;
        printf("Update to MAP_SHARED map is not visible\n");
    }

    if(munmap(map, length)<0)
    {
        fprintf(stderr, "Error: Can't unmap file to memory: %s\n", strerror(errno));
        exit(255);
    }
    
    if(close(fd)<0)
    {
        fprintf(stderr, "Error: Can't close file test.txt: %s\n", strerror(errno));
        exit(255);
    }

    exit(visible);
}

//mtest3 exactly the same as mtest2 except changing MAP_SHARED to MAP_PRIVATE
void mtest3()
{
    char *map;
    char buff[1];
    size_t length = 4096;
    int fd = createFile(length);
    int offset = 10;
    int visible;

    printf("Executing Test #3 (write to MAP_PRIVATE mmap):\n");

    if((map = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0))== MAP_FAILED)
    {
        fprintf(stderr, "Error: Can't mmap file to memory: %s\n", strerror(errno));
        exit(255);

    }

    printf("map[%d]== \'%c\'\n", offset, map[offset]);
    printf("writing a \'3\'\n");
    map[offset] = '3';
    printf("new map[%d]== \'%c\'\n", offset, map[offset]);

    if(lseek(fd, offset, SEEK_SET)<0)
        fprintf(stderr, "Can't lseek file test.txt: %s\n", strerror(errno));

    if(read(fd, buff, 1)<0)
        fprintf(stderr, "Can't read file test.txt: %s\n", strerror(errno));
    
    printf("read == \'%c\'\n", buff[0]);
    if(buff[0] == '3')
    {
        visible = 0;
        printf("Update to MAP_PRIVATE map is visible\n");
    }
    else
    {
        visible = 1;
        printf("Update to MAP_PRIVATE map is not visible\n");
    }

    if(munmap(map, length)<0)
    {
        fprintf(stderr, "Error: Can't unmap file to memory: %s\n", strerror(errno));
        exit(255);
    }
    
    if(close(fd)<0)
    {
        fprintf(stderr, "Error: Can't close file test.txt: %s\n", strerror(errno));
        exit(255);
    }

    exit(visible);
}

void mtest4()
{
    char *map;
    char buff[1];
    char readbuff[1];
    size_t length = getpagesize()+16;
    int fd = createFile(length);
    int visible;
    
    printf("Executing Test #4 (write to hole):\n");
    if((map = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0))== MAP_FAILED)
    {
        fprintf(stderr, "Error: Can't mmap file to memory: %s\n", strerror(errno));
        exit(255);
    }

    int offset = length;
    printf("writing \'X\' to offset\n");
    map[offset] = 'X';

    if(lseek(fd, 16, SEEK_END) <0)
    {
        fprintf(stderr, "Error: Can't lseek on test.txt: %s\n", strerror(errno));
        exit(255);
    }

    buff[0] = '4';
    printf("writing \'4\' to create a hole\n");
    if(write(fd, buff, 1)<0)
    {
        fprintf(stderr, "Error: Can't write to test.txt: %s\n", strerror(errno));
        exit(255);
    }

    if(lseek(fd, length, SEEK_SET)<0)
    {
        fprintf(stderr, "Error: Can't lseek on test.txt to %d: %s\n",length, strerror(errno));
        exit(255);
    }

    if(read(fd, readbuff, 1)<0)
    {
        fprintf(stderr, "Error: Can't read from test.txt at %d: %s\n",length, strerror(errno));
        exit(255);
    }

    printf("read == \'%c\'\n", readbuff[0]);
    if(readbuff[0] == 'X')
    {
        visible = 0;
        printf("Writing into a hole is visible\n");
    }
    else
    {
        visible = 1;
        printf("Writing into a hole is not visible\n");
    }

    if(munmap(map, length)<0)
    {
        fprintf(stderr, "Error: Can't unmap file to memory: %s\n", strerror(errno));
        exit(255);
    }
    
    if(close(fd)<0)
    {
        fprintf(stderr, "Error: Can't close file test.txt: %s\n", strerror(errno));
        exit(255);
    }

    exit(visible);
    

}

int main(int argc, char **argv)
{
    for(int i = 1; i<32; i++)
        signal(i, sigHandler);
    
    if(argc<2)
    {
        fprintf(stderr, "wrong input\n");
        exit(255);
    }

    switch(atoi(argv[1]))
    {
        case 1:
            mtest1();
            break;
        case 2:
            mtest2();
            break;
        case 3:
            mtest3();
            break;
        case 4:
            mtest4();
            break;
    }

}