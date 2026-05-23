#include <stdio.h>
#include "internal/lmpi_globals.h"
#include "internal/lmpi_memory_internal.h"
#include "lmpi.h"


void* LMPI_Register(LMPI_PoolKind pool,int count,MPI_Datatype datatype) {

        int typesize;
        MPI_Type_size(datatype, &typesize);
        size_t size=count*typesize;

        const size_t ALIGN=64;
        
        if(pool==LMPI_POOL_SEND){
            shm_offset_send=align_up(shm_offset_send, ALIGN);
            if (shm_offset_send+size>LMPI_MAX_BUFFER){
                fprintf(stderr,"LMPI_Alloc: buffer overflow!\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            void* ptr=(char*)shm_buf_send[local_rank]+shm_offset_send;
            shm_offset_send+=size;
        }
        else if(pool==LMPI_POOL_RECV){
            shm_offset_recv=align_up(shm_offset_recv, ALIGN);
            if (shm_offset_recv+size>LMPI_MAX_BUFFER){
                fprintf(stderr,"LMPI_Alloc: buffer overflow!\n");
                MPI_Abort(MPI_COMM_WORLD, 1);    
            }
            void* ptr=(char*)shm_buf_recv[local_rank]+shm_offset_recv;
            shm_offset_recv+=size;
        }
            return ptr;
}



