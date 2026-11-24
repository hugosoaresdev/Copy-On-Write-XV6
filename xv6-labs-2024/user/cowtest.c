#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  char *p;
  int pid, status;

  p = sbrk(4096);
  if(p == (char*)-1){
    printf("cowtest: sbrk falhou\n");
    exit(1);
  }

  *p = 1;

  pid = fork();
  if(pid < 0){
    printf("cowtest: fork falhou\n");
    exit(1);
  }

  if(pid == 0){
    if(*p != 1)
      exit(1);
    *p = 2;
    if(*p != 2)
      exit(1);
    exit(0);
  }

  if(wait(&status) != pid || status != 0){
    printf("cowtest: filho falhou\n");
    exit(1);
  }

  if(*p != 1){
    printf("cowtest: escrita do filho alterou a página do pai\n");
    exit(1);
  }

  printf("cowtest: COW passou\n");
  exit(0);
}