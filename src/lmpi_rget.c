#include<mpi.h>
#include<pthread.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<stdarg.h>
#include<time.h>
#include<unistd.h>
#include"lmpi.h"
#include <stdint.h>

typedef struct{
	LMPI_Request send_request;
	void *data_addr;
	MPI_Win *origin_win;
	MPI_Comm win_comm;

}lmpi_send_request;

void debug_log(char*format,...){
va_list args;
va_start(args,format);
printf("[DEBUG] ");

vprintf(format,args);
printf("\n");
fflush(stdout);
va_end(args);

}

int is_not_proxy(int arr[],int size,int value) {
    for (int i=0;i<size;i++) {
        if (arr[i]==value)
            return 0;
    }
    return 1;
}

const LMPI_Datatype LMPI_INT=MPI_INT;
const LMPI_Datatype LMPI_LONG=MPI_LONG;
const LMPI_Datatype LMPI_SHORT=MPI_SHORT;
const LMPI_Datatype LMPI_BYTE=MPI_BYTE;
const LMPI_Datatype LMPI_FLOAT=MPI_FLOAT;
const LMPI_Datatype LMPI_DOUBLE=MPI_DOUBLE;
const LMPI_Datatype LMPI_CHAR=MPI_CHAR;
static size_t shm_offset = 0;


int *shared_index = NULL;
int *shared_request_num = NULL;
LMPI_Request *shared_queue=NULL;

int progress_rank;
int * progress_per_rank=NULL;
MPI_Win win;
MPI_Win shm_win;
void *shm_buf;
MPI_Win shm_queue_win;

MPI_Win shm_request_num_win;
MPI_Win shm_indx_win;
MPI_Comm shm_comm;
 MPI_Comm **comm_proxy_table;


int enqueue_request(LMPI_Request request){
	int written=0;
	int rank=0;
	MPI_Comm_rank(MPI_COMM_WORLD,&rank);
	MPI_Win_lock_all(0, shm_queue_win);
	while(!written){
		for(int i=0;i<MAX_QUEUE_NUM;i++){
			if(shared_queue[i].valid==0 ){
				//debug_log("Worker: %d Wrting the request %lld",rank,request.remote_win_addr);
				shared_queue[i].src=request.src;
				shared_queue[i].dst=request.dst;
				shared_queue[i].tag=request.tag;
				shared_queue[i].datatype=request.datatype;
				shared_queue[i].count=request.count;
				shared_queue[i].size=request.size;
				shared_queue[i].opcode=request.opcode;
				shared_queue[i].corresponding_progress_rank=request.corresponding_progress_rank;
				shared_queue[i].communicator=request.communicator;
				shared_queue[i].mem_offset=request.mem_offset;
				shared_queue[i].request_status=request.request_status;
				shared_queue[i].request=request.request;
				shared_queue[i].remote_src=0;
				shared_queue[i].request_id=request.request_id;
				shared_queue[i].remote_win_addr=request.remote_win_addr;
				shared_queue[i].rget_complete=0;
				shared_queue[i].rts_complete=0;
				written=1;
				shared_queue[i].valid=1;
				//MPI_Win_sync(shm_queue_win);
				break;
			}
		}

                //usleep(1000);
	}
	MPI_Win_unlock_all(shm_queue_win);
	//debug_log("Worker: %d Wrting the request %lld",rank,request.remote_win_addr);
	return 0;
}

int dequeue_request() {
	int SIGNAL=0;
	int flag_exit=0;
	int rank=0;
	int time_send =0,time_recv=0;
	int sizeofdata=0;

	char FIN_MSG=LMPI_FIN;
	char RTS_MSG=LMPI_RTS;
	MPI_Comm_rank(MPI_COMM_WORLD,&rank);
	int dummy_flag;
	while(!flag_exit){
		MPI_Iprobe(MPI_ANY_SOURCE,LMPI_EXIT,MPI_COMM_WORLD,&flag_exit,MPI_STATUS_IGNORE);

		for(int i=0;i<MAX_QUEUE_NUM;i++){
			//MPI_Win_sync(shm_queue_win);
			MPI_Win_lock_all(0, shm_queue_win);
			MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &dummy_flag, MPI_STATUS_IGNORE);
			if(shared_queue[i].valid==1){
				if(shared_queue[i].opcode==LMPI_RECV_OP){
					if(shared_queue[i].request_status==LMPI_STATUS_PENDING){
						MPI_Irecv(&shared_queue[i].remote_win_addr,1,MPI_AINT,shared_queue[i].corresponding_progress_rank,shared_queue[i].tag,shared_queue[i].communicator,&shared_queue[i].rts_recv_request);
						shared_queue[i].request_status=LMPI_STATUS_WAITING;
						//MPI_Win_sync(shm_queue_win);
					}
					else if(shared_queue[i].request_status==LMPI_STATUS_WAITING){
						MPI_Test(&shared_queue[i].rts_recv_request,&shared_queue[i].rts_complete,MPI_STATUS_IGNORE);
						if(shared_queue[i].rts_complete){
							shared_queue[i].rts_complete=0;
							time_recv++;
							shared_queue[i].rget_complete=0;
							void *recv_ptr = (char*)shm_buf + shared_queue[i].mem_offset;
							//debug_log("Time %d: Proxy Receiver: %lld tag %d",time_recv,shared_queue[i].remote_win_addr,shared_queue[i].tag);
							MPI_Win_lock(MPI_LOCK_SHARED, shared_queue[i].src, 0, win);
							MPI_Rget(recv_ptr, shared_queue[i].count, shared_queue[i].datatype, shared_queue[i].src,shared_queue[i].remote_win_addr, shared_queue[i].count, shared_queue[i].datatype,win,&shared_queue[i].request);
							 //debug_log("Time %d: Proxy Receiver: %lld tag %d",time_recv,shared_queue[i].remote_win_addr,shared_queue[i].tag);
							shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
							//MPI_Win_sync(shm_queue_win);
						}

					}
					else if(shared_queue[i].request_status==LMPI_STATUS_IN_PROGRESS){
							MPI_Test(&shared_queue[i].request,&shared_queue[i].rget_complete,MPI_STATUS_IGNORE);
							//MPI_Win_sync(shm_queue_win);
							//debug_log("Receiver: %d sends FIN message  %d time",rank,time_recv);
							if(shared_queue[i].rget_complete){
								MPI_Win_flush(shared_queue[i].src, win);
								MPI_Win_unlock(shared_queue[i].src, win);
								//debug_log("Receiver: %d sends FIN message  %d time",rank,time_recv);
								//MPI_Win_sync(shm_queue_win);
								MPI_Isend(&FIN_MSG,1,MPI_CHAR,shared_queue[i].corresponding_progress_rank,shared_queue[i].tag+1234,shared_queue[i].communicator,&shared_queue[i].fin_recv_request);
								shared_queue[i].request_status=LMPI_STATUS_COMPLETED;
                                                                shared_queue[i].request=MPI_REQUEST_NULL;
								//MPI_Win_sync(shm_queue_win);

								//MPI_Bsend(&FIN_MSG,1,MPI_CHAR,shared_queue[i].corresponding_progress_rank,shared_queue[i].tag*2,shared_queue[i].communicator);
							}

					}


               			}
                        	else if(shared_queue[i].opcode==LMPI_SEND_OP){
					if(shared_queue[i].request_status==LMPI_STATUS_PENDING){
						shared_queue[i].rts_complete=0;
						 time_send++;
						MPI_Aint remote_addr=shared_queue[i].remote_win_addr;
						//debug_log("Time %d: Proxy Sender: %lld  tag: %d",time_send,shared_queue[i].remote_win_addr,shared_queue[i].tag);
						MPI_Isend(&shared_queue[i].remote_win_addr,1,MPI_AINT,shared_queue[i].corresponding_progress_rank,shared_queue[i].tag,shared_queue[i].communicator,&shared_queue[i].rts_send_request);
						shared_queue[i].request_status=LMPI_STATUS_WAITING;
						//MPI_Win_sync(shm_queue_win);
					}
					else if(shared_queue[i].request_status==LMPI_STATUS_WAITING){
						MPI_Test(&shared_queue[i].rts_send_request,&shared_queue[i].rts_complete,MPI_STATUS_IGNORE);
						if(shared_queue[i].rts_complete){
							shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
							//MPI_Win_sync(shm_queue_win);
							shared_queue[i].rts_send_request=MPI_REQUEST_NULL;
							//debug_log("Times  %d Found one SEND request: %d send to %d",time_send,rank,shared_queue[i].corresponding_progress_rank);
						}

					}

					else if(shared_queue[i].request_status==LMPI_STATUS_IN_PROGRESS){
						int fin_flag=1;
						MPI_Iprobe(shared_queue[i].corresponding_progress_rank,shared_queue[i].tag+1234,shared_queue[i].communicator,&fin_flag,MPI_STATUS_IGNORE);
						if(fin_flag){
							//debug_log("Sender: %d Received FIN message %d time",rank,time_send);
							shared_queue[i].request_status=LMPI_STATUS_COMPLETED;
							//MPI_Win_sync(shm_queue_win);
							//LMPI_Show_queue(rank);
						}
					}
				}
			}
			MPI_Win_unlock_all(shm_queue_win);
		}

		//usleep(1000);

	}
	debug_log("Progress rank exit the polling loop ");
	return 0;
}

int LMPI_Init(int *argc, char ***argv){
	int len=0,rank=0,size=0;
	char hostname[256];
	int local_progress_rank;

	MPI_Init(argc,argv);
	MPI_Comm_rank(MPI_COMM_WORLD,&rank);
	MPI_Get_processor_name(hostname,&len);
	MPI_Comm_size(MPI_COMM_WORLD,&size);
	progress_per_rank=malloc(sizeof(int)*size);
	shared_index=malloc(sizeof(int));
	*shared_index=0;


	MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &shm_comm);

	MPI_Comm_rank(shm_comm,&local_progress_rank);
	if(strcmp(hostname,"i01r01c03s11")==0 )
		progress_rank=0;
	else
		progress_rank=2;
	//printf("%d\n",local_progress_rank);

	MPI_Allgather(&progress_rank,1,MPI_INT,progress_per_rank,1,MPI_INT,MPI_COMM_WORLD);

	/*
	for(int i=0;i<size;i++)
			printf("%d, ",progress_per_rank[i]);

	printf("\n");
	*/
	comm_proxy_table = malloc(size * sizeof(MPI_Comm*));
	for (int i = 0; i < size; i++) {
    		comm_proxy_table[i] = malloc(size * sizeof(MPI_Comm));
    		for (int j = 0; j < size; j++) {
        		comm_proxy_table[i][j] = MPI_COMM_NULL;
    		}
	}


	for (int sender = 0; sender < size; sender++) {
    		for (int receiver = 0; receiver < size; receiver++) {
        		int proxy = progress_per_rank[receiver];
			if (sender == proxy)
            			continue;
        		if (rank == sender || rank == proxy) {
            			int ranks[2] = {sender, proxy};
            			MPI_Group world_group, pair_group;
            			MPI_Comm group_comm;

            			MPI_Comm_group(MPI_COMM_WORLD, &world_group);
            			MPI_Group_incl(world_group, 2, ranks, &pair_group);
            			MPI_Comm_create_group(MPI_COMM_WORLD, pair_group, 0, &group_comm);

            			if (group_comm != MPI_COMM_NULL) {
                		comm_proxy_table[sender][receiver] = group_comm;
            			}

            			MPI_Group_free(&pair_group);
            			MPI_Group_free(&world_group);
        			}
			else {
            			comm_proxy_table[sender][receiver] = MPI_COMM_NULL;
        		}
    		}
	}


	if(local_progress_rank==0){

		 MPI_Win_allocate_shared(MAX_QUEUE_NUM*sizeof(LMPI_Request ), sizeof(LMPI_Request ),MPI_INFO_NULL,shm_comm,&shared_queue,&shm_queue_win);

	}
	else{
		MPI_Aint size;
                int disp_unit;
                MPI_Win_allocate_shared(0, sizeof(LMPI_Request ), MPI_INFO_NULL, shm_comm, &shared_queue, &shm_queue_win);

                MPI_Win_shared_query(shm_queue_win, MPI_PROC_NULL, &size, &disp_unit, &shared_queue);
	}

    	MPI_Barrier(shm_comm);
	MPI_Win_create_dynamic(MPI_INFO_NULL, MPI_COMM_WORLD, &win);

	//MPI_Aint alloc_size = (rank == progress_rank) ? LMPI_MAX_BUFFER: 0;
	MPI_Aint alloc_size=LMPI_MAX_BUFFER;
    	MPI_Win_allocate_shared(alloc_size, sizeof(char), MPI_INFO_NULL, shm_comm, &shm_buf, &shm_win);
	

    	if (rank != progress_rank) {
        	int disp_unit;
        	MPI_Aint ssize;
        	MPI_Win_shared_query(shm_win, MPI_PROC_NULL, &ssize, &disp_unit, &shm_buf);
    	}


	int shm_rank, shm_size;
	MPI_Comm_rank(shm_comm, &shm_rank);
	MPI_Comm_size(shm_comm, &shm_size);

	for (int r = 0; r < shm_size; r++) {
    		MPI_Aint ssize;
    		int disp_unit;
    		MPI_Win_shared_query(shm_win, r, &ssize, &disp_unit, &shm_buf);
	}



	//debug_log("Rank %d finished LMPI_Init",rank);
	MPI_Win_lock_all(0, shm_win);

	if(local_progress_rank==0){
		dequeue_request();
	}
	return 0;
}

void * send_progress(void *args){
       	int sizeofdata;
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD,&rank);
	MPI_Win win;
	lmpi_send_request*request=(lmpi_send_request*)args;
	LMPI_Request origin_request=request->send_request;
	MPI_Type_size(origin_request.datatype,&sizeofdata);
	void*data=request->data_addr;


	MPI_Win_create(data,sizeofdata*origin_request.count,sizeofdata,MPI_INFO_NULL,origin_request.win_comm,request->origin_win);
	debug_log("Rank %d, Win created",rank);
	pthread_exit(NULL);
}

int LMPI_Isend(void*data, int count, MPI_Datatype datatype, int dest, int tag,  MPI_Comm comm, LMPI_Request *request){

	int sizeofdata=0;
	//MPI_Request get_request;
	MPI_Type_size(datatype,&sizeofdata);
	if(data==NULL)
		debug_log("ERROR: Segmentation fault");
	int local_progress_rank=0;
	int remote_sender=0;
	int rank=0;
	int index=0;
	MPI_Comm_rank(comm,&rank);
	MPI_Aint remote_addr;

	int err=MPI_Comm_rank(comm_proxy_table[rank][dest],&remote_sender);

	request->remote_src=remote_sender;
	request->src=rank;
	request->dst=dest;
	request->tag=tag;
	request->datatype= datatype;
	request->size=count*sizeofdata;
	request->valid=1;
	request->communicator=comm;
	request->request_id=(uint64_t)clock();
	request->corresponding_progress_rank=progress_per_rank[dest];
	request->opcode=LMPI_SEND_OP;
	request->request_status=LMPI_STATUS_PENDING;
	request->count=count;
	//request->request=get_request;

	MPI_Group world_group, shm_group;
	MPI_Comm_group(comm,&world_group);
	MPI_Comm_group(shm_comm,&shm_group);

	MPI_Comm_rank(shm_comm,&local_progress_rank);

	MPI_Win_attach(win, data, count*sizeofdata);
	MPI_Get_address(data, &remote_addr);
	request->remote_win_addr=remote_addr;
	request->send_buffer=data;
	enqueue_request(*request);
	//debug_log("Rank %d has its local progress %d",rank,local_progress_rank);
	
	return 0;
}



MPI_Aint LMPI_Alloc(int count, MPI_Datatype datatype)
{
    int typesize;
    MPI_Type_size(datatype, &typesize);
    size_t size = count * typesize;


    if (shm_offset + size > LMPI_MAX_BUFFER) {
        fprintf(stderr, "LMPI_Alloc: buffer overflow!\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Aint allocated_offset = shm_offset;
    //shm_offset += size;

    return allocated_offset;
}

int LMPI_Irecv(void **data, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, LMPI_Request *request){
	int rank=0;
        MPI_Win win;
	MPI_Comm_rank(comm,&rank);
	int local_progress_rank=0;
	int sizeofdata=0;
        MPI_Type_size(datatype,&sizeofdata);
	request->request_id=(uint64_t)clock();
	request->dst=rank;
        request->src=source;
        request->tag=tag;
	request->datatype= datatype;
	request->size=count*sizeofdata;
        request->valid=1;
        request->opcode=LMPI_RECV_OP;
	request->communicator=comm;
	request->corresponding_progress_rank=progress_per_rank[source];
        request->request_status=LMPI_STATUS_PENDING;
	request->count=count;
	request->remote_win_addr=0;
	MPI_Aint offset =LMPI_Alloc(count, datatype);
    	request->mem_offset= offset;

    	*data = (char *)shm_buf + offset;

	//fprintf(stderr,"RECV rank %d: LMPI_Alloc offset=%lld ptr=%p count=%d\n", rank, (long long)request->recv_buffer,data, request->count);
	//fflush(stderr);

	MPI_Group world_group, shm_group;
        MPI_Comm_group(comm,&world_group);
        MPI_Comm_group(shm_comm,&shm_group);
	MPI_Comm_rank(shm_comm,&local_progress_rank);

        enqueue_request(*request);
	//LMPI_Show_queue(rank);

	//debug_log("Rank %d has its local progress %d",rank,local_progress_rank);
	return 0;

}


int LMPI_Wait(LMPI_Request * request,int*flag){
	int ret=1;
	*flag=0;
	int dummy_flag=0;
	while(ret==1){

		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &dummy_flag, MPI_STATUS_IGNORE);
		ret=LMPI_Test(request,flag);

	}
	*flag=1;
	return 0;
}
int LMPI_Test(LMPI_Request *request,int*flag){
	*flag=0;
	int dummy_flag;
	for(int i=0;i<MAX_QUEUE_NUM;i++){
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &dummy_flag, MPI_STATUS_IGNORE);
		//MPI_Win_sync(shm_queue_win);
		if(request->request_id==shared_queue[i].request_id)
			if (shared_queue[i].request_status==LMPI_STATUS_COMPLETED){
				request->request_status=LMPI_STATUS_COMPLETED;
				*flag=1;
				if(request->opcode==LMPI_SEND_OP){
					MPI_Win_detach(win,request->send_buffer);
				}

				memset(&shared_queue[i],0,sizeof(LMPI_Request));
				//shared_queue[i].valid=0;
				//MPI_Win_sync(shm_queue_win);
				return 0;
			}

	}
	return 1;
}



int LMPI_Testall(int count, LMPI_Request array_of_requests[],int *flag){
	int completed_requests=0;
	*flag=0;
	int rank=0;
	MPI_Comm_rank(MPI_COMM_WORLD,&rank);
	for(int i=0;i<count;i++){
		for(int j=0;j<MAX_QUEUE_NUM;j++){
			MPI_Win_sync(shm_queue_win);
                	if(array_of_requests[i].request_id==shared_queue[j].request_id)
                        	if (shared_queue[j].request_status==LMPI_STATUS_COMPLETED){
					++completed_requests;
					//debug_log("found matching operation %s",(shared_queue[j].opcode==1?"LMPI_RECV_OP":"LMPI_SEND_OP"));
					break;
				}

		}
	}

	//debug_log("Completed requests :%d",completed_requests);
	if(completed_requests==count){
                //debug_log("Completed requests :%d",completed_requests);
                *flag=1;
		/*
                for(int i=0;i<count;i++){
                        for(int j=0;j<MAX_QUEUE_NUM;j++){
                                if(array_of_requests[i].request_id==shared_queue[j].request_id){
                                                shared_queue[j].src=0;
						shared_queue[j].dst=0;
						shared_queue[j].tag=0;
						shared_queue[j].request_id=0;
						//shm_buf=NULL;
						break;
                                        }

                        }
                }
		*/

                return 0;


        }
        else
                return 1;
}

int LMPI_Waitall(int count, LMPI_Request array_of_requests[],int *flag){
	*flag=0;
	int dummy_flag=0;
	while(!(*flag)){
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &dummy_flag, MPI_STATUS_IGNORE);
		MPI_Win_sync(shm_queue_win);
		LMPI_Testall(count,array_of_requests,flag);
	}
	return 0;
}


int LMPI_Finalize() {

    MPI_Win_unlock_all(shm_win);
    int rank;
    int SIGNAL=LMPI_EXIT;
    MPI_Request request;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank!=progress_rank){
    	MPI_Isend(&SIGNAL,1,MPI_INT,progress_rank,LMPI_EXIT,MPI_COMM_WORLD,&request);
	MPI_Wait(&request,MPI_STATUS_IGNORE);

    }


    //printf("%d reaches Finalize \n",rank);

    //if (win != MPI_WIN_NULL) {
    //    MPI_Win_free(&win);
   // }

    if (shm_queue_win != MPI_WIN_NULL) {
        MPI_Win_free(&shm_queue_win);
    }



    if (progress_per_rank != NULL) {
       free(progress_per_rank);
        progress_per_rank = NULL;
    }

    if (shm_comm != MPI_COMM_NULL) {
        MPI_Comm_free(&shm_comm);
    }

    return MPI_Finalize();
}

int LMPI_Comm_size(LMPI_Comm comm,int *size){
	return MPI_Comm_size(comm,size);
}

int LMPI_Comm_rank(LMPI_Comm comm, int*rank){
	return MPI_Comm_rank(comm,rank);
}

int LMPI_Barrier(LMPI_Comm comm){

	return MPI_Barrier(comm);
}
int LMPI_Show_queue(int rank){
	for(int j=0;j<MAX_QUEUE_NUM;j++){
	debug_log("Rank: %d,ID: %ld, OPcode:%d,Completion %d,Valid:%d",rank,shared_queue[j].request_id, shared_queue[j].opcode,shared_queue[j].request_status,shared_queue[j].valid);

        }
	printf("\n");
	return 0;


}
