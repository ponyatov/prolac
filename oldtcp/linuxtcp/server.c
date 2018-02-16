#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include "server.h"

#ifndef PACKET_WORDS
#define PACKET_WORDS	2
#endif

//#undef PACKET_WORDS
//#define PACKET_WORDS	1

#define SZ	(PACKET_WORDS * sizeof(int))

int buf[PACKET_WORDS];
int is_server = 1;
char *program_name = "server";

int
main (int argc, char *argv[])
{
  int i, r;
  int count = 0;
  void *handle;
  
  printf("server\n");
  
  init__Tcp_Interface(0);
  
  handle = (void *)listen__Tcp_Interface(SERVER);
  if (handle == 0) {
    fprintf(stderr, "listen failed %d\n", r);
    exit(-1);
  }
  
  printf("%s: listen done\n", program_name);
  
  if ((handle = (void *)accept__Tcp_Interface(handle)) == 0) {
    fprintf(stderr, "accept failed %d\n", r);
    exit(-1);
  }
  
  printf("%s: accept succeeded\n", program_name);
  
  do {

    printf("%s: trying to read.\n", program_name);
    if ((r = read__Tcp_Interface(handle, buf, SZ)) < 0) {
      fprintf(stderr, "%s: read failed %d\n", argv[0], r);
      exit(-1);
    }
    printf("%s: read returned %d.\n", program_name, r);
    
    printf("server GOT %d\n", count);
    count++;
    
    if (r < SZ) break;
    
    for (i = 0; i < PACKET_WORDS; i++) {
      if (buf[i] != i) {
	printf("%s: buf[%d] contains %d\n", program_name, i, buf[i]);
	assert(0);
      }
    }

    printf("%s: Trying to do write.\n", program_name);
    if ((r = write__Tcp_Interface(handle, buf, SZ)) < 0) {
      fprintf(stderr, "%s: read failed %d\n", program_name, r);
      exit(-1);
    }
    
    printf("%s: write returned %d\n", program_name, r);
    
    // drop__Tcp_Interface(handle, DROP);
    
  } while (1) ;
  
  printf("%s: trying to close...\n", program_name);
  if ((r = close__Tcp_Interface(handle)) != 0) {
    fprintf(stderr, "%s: close failed %d\n", program_name, r);
    exit(-1);
  }
  printf("%s: close succeeded\n", program_name);
  
  wrapup__Tcp_Interface(handle);
  
  printf("%s: exit\n", program_name);
  fflush(stdout);
}
