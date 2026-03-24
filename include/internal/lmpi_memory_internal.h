#pragma once
#include <stddef.h>

static inline size_t align_up(size_t x, size_t a) { return (x + a - 1) & ~(a - 1); }
MPI_Aint LMPI_Alloc(int count, MPI_Datatype datatype);
