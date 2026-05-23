#include <stdio.h>
#include "internal/lmpi_globals.h"
#include "internal/lmpi_memory_internal.h"
#include "lmpi.h"


LMPI_Allocation LMPI_Malloc(LMPI_PoolKind pool,int count,MPI_Datatype datatype) {
    int typesize;
    MPI_Type_size(datatype, &typesize);
    size_t size=count*typesize;

    const size_t ALIGN=64;
    LMPI_Allocation alloc;
    alloc.ptr=NULL;
    alloc.block.pool=pool;
    alloc.block.size=size;
    
    if(pool==LMPI_POOL_SEND){
        shm_offset_send=align_up(shm_offset_send, ALIGN);
        if (shm_offset_send+size>LMPI_MAX_BUFFER){
            fprintf(stderr,"LMPI_Alloc: buffer overflow!\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        alloc.ptr=(char*)shm_buf_send[local_rank]+shm_offset_send;
        alloc.block.offset=shm_offset_send;
        shm_offset_send+=size;
    }
    else if(pool==LMPI_POOL_RECV){
        shm_offset_recv=align_up(shm_offset_recv, ALIGN);
        if (shm_offset_recv+size>LMPI_MAX_BUFFER){
            fprintf(stderr,"LMPI_Alloc: buffer overflow!\n");
            MPI_Abort(MPI_COMM_WORLD, 1);    
        }
        alloc.ptr=(char*)shm_buf_recv[local_rank]+shm_offset_recv;
        alloc.block.offset=shm_offset_recv;
        shm_offset_recv+=size;
    }

    return alloc;
}


void LMPI_Free(LMPI_Allocation*alloc){
    if(alloc==NULL||alloc->ptr==NULL){
        return; 
    }
    LMPI_PoolKind pool=alloc->block.pool;
    LMPI_Block *block=&alloc->block;
    if(pool==LMPI_POOL_SEND){
        if(block->offset+block->size!=shm_offset_send){
            fprintf(stderr, "LMPI_Free: send pool free order violation\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        shm_offset_send=block->offset;
    }
    else if(pool==LMPI_POOL_RECV){
        if(block->offset+block->size!=shm_offset_recv){
               fprintf(stderr, "LMPI_Free: recv pool free order violation\n");
               MPI_Abort(MPI_COMM_WORLD, 1);
        }
        shm_offset_recv=block->offset;
        
    }
    else{

        fprintf(stderr,"LMPI_Free:Invalid memory pool\n");
         MPI_Abort(MPI_COMM_WORLD, 1);
    }
    alloc->ptr=NULL;
    block->offset = 0;
    block->size = 0;
}
