#ifndef LMPI_TYPES_H
#define LMPI_TYPES_H

#include<mpi.h>
#include<stdint.h>
#include <stdatomic.h>

typedef MPI_Comm	LMPI_Comm;
typedef MPI_Datatype	LMPI_Datatype;
typedef MPI_Win		LMPI_Win;

//MPI_Comm LMPI_COMM_WORLD;

/* 
#define  LMPI_COMM_WORLD MPI_COMM_WORLD
*/

#define LMPI_INT	MPI_INT
#define LMPI_CHAR	MPI_CHAR
#define LMPI_BYTE	MPI_BYTE
#define LMPI_SHORT	MPI_SHORT
#define LMPI_LONG	MPI_LONG
#define LMPI_FLOAT	MPI_FLOAT
#define LMPI_DOUBLE	MPI_DOUBLE
#define LMPI_Aint	MPI_Aint

typedef struct{
        int src;
        int dst;
        int tag;
        long index;
        LMPI_Datatype datatype;
        int count;
        int size;
        _Atomic int valid;
        int opcode;
        int corresponding_progress_rank;
        LMPI_Comm communicator;
        LMPI_Aint mem_offset;
	MPI_Aint remote_win_addr;
        int request_status;
        void *send_buffer;
        void *recv_buffer;
        LMPI_Win shm_win;
        MPI_Request request;
        MPI_Request rts_send_request;
        MPI_Request rts_recv_request;
        MPI_Request fin_send_request;
        MPI_Request fin_recv_request;
        int rts_complete;
        int rget_complete;
        int fin_complete;
        int local_rank;
	int shm_queue_sender_id;
        int world_src;
        uint64_t request_id;
}LMPI_Request;

typedef struct{
        void *dst_mem_addr;
        void *src_mem_addr;
        LMPI_Request *src_request;
        LMPI_Request *dst_request;

}LMPI_Mem;

#endif
