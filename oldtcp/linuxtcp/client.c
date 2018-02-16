#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include "server.h"

#ifndef PACKET_WORDS
#define PACKET_WORDS	2
#endif
#ifndef NTRIES
#define NTRIES		5
#endif

#define PACKS_PER_TRY	1
#define SZ		(PACKET_WORDS * sizeof(int))

#define TIME1		gettimeofday(&tv_before, 0)
#define TIME2		gettimeofday(&tv_after, 0)
#define SUB_TIME(result, a, b) do { \
	(result).tv_sec = (a).tv_sec - (b).tv_sec; \
	if (((result).tv_usec = (a).tv_usec - (b).tv_usec) < 0) { \
		(result).tv_sec--; \
		(result).tv_usec += 1000000; \
	} } while (0)
#define TIME_DIFF_USEC(a, b) (((a).tv_sec - (b).tv_sec) * 1000000 + (a).tv_usec - (b).tv_usec)


int snd[PACKET_WORDS];
int rcv[PACKET_WORDS];
int is_server = 0;
char *program_name = "client";


int
main(int argc, char *argv[])
{
  int i, j, k;
  int r;
  void *handle;
  struct timeval tv_before, tv_after, tv1, tv2;
  unsigned int m;
  
  printf("client\n");
  
  // Set up 'snd' buffer.
  for (i = 0; i < PACKET_WORDS; i++) snd[i] = i;
  
  init__Tcp_Interface(1);
  
  if ((handle = (void *)connect__Tcp_Interface(SERVER)) == 0) {
    fprintf(stderr, "connect failed %d\n", r);
    exit(-1);
  }
  
  printf("%s: connected\n", program_name);
  
  if (gettimeofday(&tv_before, 0)) {
    perror("gettimeofday");
    exit(-1);
  }
  
  for (i = 0; i < NTRIES / PACKS_PER_TRY; i++) {
    
    // printf("client: presend %d\n", i);
    for (j = 0; j < PACKS_PER_TRY; j++) {
      for (k = 0; k < PACKET_WORDS; k++) assert(snd[k] == k);
      if ((r = write__Tcp_Interface(handle, snd, SZ)) != SZ) {
	fprintf(stderr, "%s: write failed %d\n", program_name, r);
	exit(-1);
      }
    }
    
    // printf("client: sendone %d\n", i);
    tcp_send_all_penders();
    // printf("client: sent %d\n", i);

    printf("client: trying to read\n");
    for (j = 0; j < PACKS_PER_TRY; j++) {
      if ((r = read__Tcp_Interface(handle, rcv, SZ)) != SZ) {
	fprintf(stderr, "%s: read failed %d\n", program_name, r);
	exit(-1);
      }
      // printf("client: read %d\n", i * PACKS_PER_TRY + j);
    }
  }
  
  if (gettimeofday(&tv_after, 0)) {
    perror("gettimeofday");
    exit(-1);
  } 
  
  printf("%d roundtrips in %d usec\n", NTRIES,
	 TIME_DIFF_USEC(tv_after, tv_before));
  
  if ((r = close__Tcp_Interface(handle)) != 0) {
    fprintf(stderr, "%s: close failed %d\n", program_name, r);
    exit(-1);
  }
  
  printf("%s: close succeeded\n", program_name);
  
  {
    extern int tv_poll_count;
    extern double tv_poll_sum, tv_poll_sqsum;
    double average = tv_poll_sum / tv_poll_count;
    printf("%s: REPORT\n\
    POLLS   AVERAGE TIME  STANDARD DEV\n\
%8d    %8g us   %8g us\n",
	   program_name,
	   tv_poll_count,
	   average,
	   sqrt(tv_poll_sqsum / tv_poll_count - average*average));
  }
  
  wrapup__Tcp_Interface(handle);
  
  printf("%s: exit\n", program_name);
  
  fflush(stdout);
}
