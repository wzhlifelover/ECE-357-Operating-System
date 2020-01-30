#include <stdlib.h> 
#include <stdio.h> 
#include <unistd.h> 
#include <sys/signal.h> 
#include <errno.h>

int p[2];

void f()
{
    return;
}

main()
{
int r,w;
struct sigaction sa;
signal(SIGCHLD,SIG_IGN); // Question (d)
sa.sa_handler=f;
sigemptyset(&sa.sa_mask);
sa.sa_flags=SA_RESTART; // Change this to 0 for part (c) sigaction(SIGUSR1,&sa,0);
pipe(p);
switch(w=fork())
{
case 0:
                  writer();
                  exit(2);
                  break;
}
switch(r=fork())
{
case 0:
                  reader();
                  exit(3);
                  break;
         }
         close(p[0]);
         close(p[1]);
for(;;)
{

int i;

if (kill(w,SIGUSR1)<0){
          perror("kill");
          exit(1);
}
    }
}
          /*This is just to delay a little*/;

reader()
{
  char buf[65536];
  int n;
  close(p[1]);
  for(;;)
  {
  if ((n=read(p[0],buf,555))<0)
  {
      perror("pipe read");
      exit(-1);
  }
  if (n==0)
  {
  fprintf(stderr,"Read returned EOF\n");
  exit(0);
  }
  }
}

    
writer() {

char buf[65536];
int  n,x;
    
    
close(p[0]);
for(x=0;x<1<<10;x++)
{
    n=write(p[1],buf,65536);
    if (n<0)
    {
    if (errno==EINTR)
              fprintf(stderr,"EINTR\n");
    else {
        perror("pipe write");
        exit(-1);
    }
    continue;
    }
    if (n<65536)
        fprintf(stderr,"pipe short write %d\n ",n);
}
}
    




