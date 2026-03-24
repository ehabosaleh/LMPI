#ifndef LMPI_DEBUG_H
#define LMPI_DEBUG_H

#include <stdio.h>
#include <stdarg.h>
#include<string.h>
static inline void debug_log(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, "[LMPI DEBUG] ");
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    fflush(stdout);
    va_end(args);
}
static inline int hash_hostname(const char *hostname) {
    int hash = 0;
    for (size_t i = 0; i < strlen(hostname); i++) {
        hash = (hash * 31 + hostname[i]) % 100000;
    }
    return hash;
}
#endif

