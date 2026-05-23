#include <stdint.h>
#include "internal/lmpi_globals.h"
#include "internal/lmpi_queue_internal.h"
#include"internal/lmpi_memory_internal.h"
#include"internal/lmpi_debug.h"
#include "lmpi_constants.h"
#include<time.h>
int LMPI_Irecv(LMPI_Allocation *data, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, LMPI_Request *request){
	if(data==NULL||data->ptr==NULL||request==NULL){
		fprintf(stderr, "LMPI_Isend: invalid argument\n");
           	return -1;
       	}
    	if(count<0) {
           	fprintf(stderr, "LMPI_Isend: negative count\n");
           	return -1;
    	}

    	MPI_Group lmpi_group, world_group;
	int lmpi_source=source,world_source=0;
	
	MPI_Comm_group(MPI_COMM_WORLD,&world_group);
	MPI_Comm_group(comm,&lmpi_group);
	MPI_Group_translate_ranks(lmpi_group,1, &lmpi_source,world_group,&world_source);
	int rank=0;
    	MPI_Comm_rank(comm,&rank);
        
	request->request_id=(uint64_t)clock();
    	request->dst=rank;
    	request->src=source;
    	request->tag=tag;
    	request->datatype= datatype;
    	request->valid=1;
    	request->opcode=LMPI_RECV_OP;
    	request->communicator=comm;
    	request->corresponding_progress_rank=progress_per_rank[world_source];
	request->request_status=LMPI_STATUS_PENDING;
    	request->count=count;
    	request->local_rank=local_rank;
	request->world_src=world_source;
    	request->mem_offset=(MPI_Aint)((char *)data->ptr - (char *)shm_buf_recv[local_rank]);
    	//request->mem_offset = data->block.offset;
    
    	request->recv_buffer=data->ptr;
    	request->index=enqueue_request(*request);

    	//debug_log("Receiver Rank %d has its index  %d",rank,request->index);
    	//LMPI_Show_queue(rank);
    	return 0;

}

int LMPI_Isend(LMPI_Allocation*data, int count, MPI_Datatype datatype, int dst, int tag,  MPI_Comm comm, LMPI_Request *requesti){
	if(data==NULL||data->ptr==NULL||request==NULL){
        	fprintf(stderr, "LMPI_Isend: invalid argument\n");
        	return -1;
    	}
    	if(count<0) {
        	fprintf(stderr, "LMPI_Isend: negative count\n");
        	return -1;
    	}

    	int sizeofdata=0;
	int rank=0;
    	MPI_Comm_rank(comm,&rank);
    	MPI_Type_size(datatype,&sizeofdata);

	MPI_Group lmpi_group, world_group;
	int lmpi_dst=dst,lmpi_src=rank,world_dst=0,world_src=0;

    	MPI_Comm_group(MPI_COMM_WORLD,&world_group);
    	MPI_Comm_group(comm,&lmpi_group);
    	MPI_Group_translate_ranks(lmpi_group,1, &lmpi_dst,world_group,&world_dst);
	MPI_Group_translate_ranks(lmpi_group,1, &lmpi_src,world_group,&world_src);

    	request->src=rank;
    	request->dst=dst;
    	request->tag=tag;
    	request->datatype= datatype;
    	request->valid=1;
    	request->communicator=comm;
    	request->request_id=(uint64_t)clock();
    	request->corresponding_progress_rank=progress_per_rank[world_dst];
    	request->opcode=LMPI_SEND_OP;
    	request->request_status=LMPI_STATUS_PENDING;
    	request->count=count;
    	request->local_rank=local_rank;
	request->world_src=world_src;
    	request->mem_offset=(MPI_Aint)((char *)data->ptr - (char *)shm_buf_send[local_rank]);
	//request->mem_offset = data->block.offset;
	
    	request->send_buffer=data->ptr;
    	request->index=enqueue_request(*request);

    	//debug_log("Sender Rank %d has its index  %d",rank,request->index);
    	//LMPI_Show_queue(rank);

    	return 0;
}


