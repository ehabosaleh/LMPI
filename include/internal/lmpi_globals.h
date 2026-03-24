#pragma once
#include <mpi.h>
#include <stddef.h>
#include <stdint.h>
#include "../lmpi_types.h"

extern LMPI_Request *shared_queue;
extern MPI_Comm LMPI_COMM_WORLD;
extern MPI_Comm LMPI_COMM_PROGRESS;
extern int *shared_queue_tail;     
extern MPI_Win tail_win;

extern int *shared_index;          
extern MPI_Win shared_index_win;

extern int progress_rank;
extern int *progress_ranks;
extern int *progress_per_rank;

extern int local_rank;
extern int local_size;

extern MPI_Win shm_win_send;
extern MPI_Win shm_win_recv;
extern void **shm_buf_send;        
extern void **shm_buf_recv;        
extern void *my_send_base;
extern void *my_recv_base;

extern MPI_Win shm_queue_win;
extern MPI_Win win;

extern MPI_Comm shm_comm;
extern MPI_Comm app_proxy_comm;


extern size_t shm_offset_send;
extern size_t shm_offset_recv;







