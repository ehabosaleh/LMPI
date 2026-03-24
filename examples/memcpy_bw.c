
// gcc -O3 -march=native memcpy_bw.c -o memcpy_bw
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
static inline uint64_t ns() {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec*1000000000ull + t.tv_nsec;
}
static inline size_t align_up(size_t x, size_t a){ return (x + a - 1) & ~(a - 1); }

int main(int argc,char**argv){
  size_t N = (argc>1)? strtoull(argv[1],0,0) : (8ull<<20); // bytes
  const size_t ALIGN = 64;  // cache-line alignment

  void *src = NULL, *dst = NULL;
  if (posix_memalign(&src, ALIGN, align_up(N, ALIGN)) != 0) return 1;
  if (posix_memalign(&dst, ALIGN, align_up(N, ALIGN)) != 0) return 1;

  // Optional: advise huge pages (ignore if it fails)
  madvise(src, align_up(N, ALIGN), MADV_HUGEPAGE);
  madvise(dst, align_up(N, ALIGN), MADV_HUGEPAGE);

  // First-touch both buffers (place pages on this NUMA node)
  memset(src, 0xA5, N);
  memset(dst, 0x00, N);
  
  for(int i=0;i<100;i++)
  	memcpy(dst, src, N);

  sleep(60);
  
  uint64_t t0 = ns();
  memcpy(dst, src, N);
  uint64_t t1 = ns();

  double sec = (t1 - t0) / 1e9;
  printf("N=%zu bytes  time=%.3f ms  BW=%.2f GB/s\n",
         N, 1e3*sec, (double)N/sec/1e9);

  
  free(dst);
  free(src);
  return 0;
}

