#include <stdatomic.h>
#include "internal/lmpi_globals.h"
#include"stdio.h"
#include "lmpi.h"

long enqueue_request(LMPI_Request request){
    int rank=0;
	int written=0;
	int index=0;
	MPI_Comm_rank(LMPI_COMM_WORLD,&rank);
    while(!written){
        for(long i=0;i<MAX_QUEUE_NUM;i++){
            //if(shared_queue[i].valid==0 )
            int expected=SLOT_EMPTY;
			if(atomic_compare_exchange_strong_explicit( &shared_queue[i].valid, &expected, SLOT_RESERVED,memory_order_acq_rel, memory_order_relaxed)){
                MPI_Win_sync(shm_queue_win);
                shared_queue[i].world_src=request.world_src;
				shared_queue[i].src=request.src;
                shared_queue[i].dst=request.dst;
                shared_queue[i].tag=request.tag;
                shared_queue[i].datatype=request.datatype;
                shared_queue[i].count=request.count;
                shared_queue[i].opcode=request.opcode;
                shared_queue[i].corresponding_progress_rank=request.corresponding_progress_rank;
                shared_queue[i].communicator=request.communicator;
                shared_queue[i].mem_offset=request.mem_offset;
                shared_queue[i].request_status=request.request_status;
                shared_queue[i].request_id=request.request_id;
                shared_queue[i].local_rank=request.local_rank;
                shared_queue[i].remote_win_addr=request.remote_win_addr;
				shared_queue[i].index=i;
				index=i;
                MPI_Win_sync(shm_queue_win);

                atomic_store_explicit(&shared_queue[i].valid, SLOT_READY, memory_order_release);
                MPI_Win_sync(shm_queue_win);
                //debug_log("Worker %d  Wrting the request %lld in %d",rank,request.request_id,i);
                written=1;
				break;
            }
        }
    }
    return index;
}

int LMPI_Show_queue(int rank) {
    printf("=== [Rank %d] Shared Queue State ===\n", rank);
    for (int i = 0; i < MAX_QUEUE_NUM; i++) {
        LMPI_Request *req = &shared_queue[i];
        printf("  Slot %d: valid=%d, opcode=%d, src=%d, dst=%d, tag=%d, "
               "status=%d, index=%ld, req_id=%lu, corresponding progress rank:%d\n",
               i,
               req->valid,
               req->opcode,
               req->src,
               req->dst,
               req->tag,
               req->request_status,
               req->index,
               (unsigned long)req->request_id,req->corresponding_progress_rank);
    }
    fflush(stdout);
    return LMPI_SUCCESS;
}

