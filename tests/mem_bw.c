#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
    size_t bytes=(argc>1)? strtoull(argv[1], NULL,0):(size_t)1024*1024*1024; // 256 MiB
    int iters=(argc>2)?atoi(argv[2]):10;

    void *src=NULL,*dst=NULL;
    if (posix_memalign(&src, 64, bytes) || posix_memalign(&dst, 64, bytes)) {
        perror("posix_memalign");
        return EXIT_FAILURE;
    }

    memset(src, 0xA5, bytes);
    memset(dst, 0x00, bytes);

    memcpy(dst, src, bytes);

    double t0 = now_sec();
    for (int i = 0; i < iters; i++) {
        memcpy(dst, src, bytes);
        // Prevent compiler from optimizing away the copy:
        // read one byte from dst and fold into a volatile sink
        volatile unsigned char sink = ((unsigned char*)dst)[(i * 4096) % bytes];
        (void)sink;
    }
    double t1 = now_sec();
    double elapsed = t1 - t0;

    // Compute bandwidths
    double total_bytes = (double)bytes * (double)iters;
    double bw_payload_GBps = total_bytes / elapsed / 1e9;         // GB/s (10^9)
    double bw_memory_GBps  = (2.0 * total_bytes) / elapsed / 1e9; // read+write

    printf("Copied: %.2f MiB x %d in %.3f s\n",
           (double)bytes / (1024.0*1024.0), iters, elapsed);
    printf("Payload bandwidth : %.2f GB/s\n", bw_payload_GBps);
    printf("Memory bandwidth  : %.2f GB/s (counts read+write)\n", bw_memory_GBps);

    free(src);
    free(dst);
    return 0;
}

