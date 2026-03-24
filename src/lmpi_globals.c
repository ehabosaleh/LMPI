#include <stdio.h>
#include <stdarg.h>
#include "internal/lmpi_globals.h"

MPI_Comm LMPI_COMM_WORLD=MPI_COMM_NULL;
MPI_Comm LMPI_COMM_PROGRESS=MPI_COMM_NULL;
LMPI_Request *shared_queue=NULL;
int *shared_queue_tail=NULL;
MPI_Win tail_win=MPI_WIN_NULL;

int *shared_index=NULL;
MPI_Win shared_index_win=MPI_WIN_NULL;

int progress_rank = -1;
int *progress_per_rank = NULL;
int *progress_ranks=NULL;
int local_rank = 0;
int local_size = 0;
MPI_Win win=MPI_WIN_NULL;

MPI_Win shm_win_send = MPI_WIN_NULL;
MPI_Win shm_win_recv = MPI_WIN_NULL;
void **shm_buf_send = NULL;      /* allocated to [local_size] */
void **shm_buf_recv = NULL;
void *my_send_base = NULL;
void *my_recv_base = NULL;

MPI_Win shm_queue_win = MPI_WIN_NULL;

MPI_Comm shm_comm = MPI_COMM_NULL;
MPI_Comm app_proxy_comm = MPI_COMM_NULL;

size_t shm_offset_send = 0;
size_t shm_offset_recv = 0;



