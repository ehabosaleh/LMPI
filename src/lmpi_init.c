#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "internal/lmpi_globals.h"
#include "lmpi.h"
#include "internal/lmpi_progress_internal.h"


int LMPI_Init(int *argc, char ***argv){
        int len=0,rank=0,size=0;
        char hostname[256];
        int local_progress_rank;
	int num_progress=0;

        MPI_Init(argc,argv);
	
	/*
        int provided;
        MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &provided);
        if (provided < MPI_THREAD_MULTIPLE) {
                fprintf(stderr,"MPI_THREAD_MULTIPLE not available!\n");
                MPI_Abort(MPI_COMM_WORLD,1);
        }
       */ 
        MPI_Comm_rank(MPI_COMM_WORLD,&rank);
        MPI_Get_processor_name(hostname,&len);
        MPI_Comm_size(MPI_COMM_WORLD,&size);
        
	progress_per_rank=malloc(sizeof(int)*size);

	MPI_Group app_group, world_group, shm_group;		
        MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &shm_comm);
	MPI_Comm_group(shm_comm,&shm_group);
	MPI_Comm_group(MPI_COMM_WORLD,&world_group);
	
	//local_progress_rank=get_progress_rank();
	local_progress_rank=0;
	MPI_Group_translate_ranks(shm_group,1, &local_progress_rank,world_group,&progress_rank);
	
	MPI_Allgather(&progress_rank,1,MPI_INT,progress_per_rank,1,MPI_INT,MPI_COMM_WORLD);
        
	MPI_Comm_rank(shm_comm,&local_rank);
        MPI_Comm_size(shm_comm,&local_size);
	int is_local_progress=(local_rank==0)?1:0;
	
	MPI_Allreduce(&is_local_progress,&num_progress,1,MPI_INT,MPI_SUM,MPI_COMM_WORLD);
	//
	//printf("Progress rank %d Number of progress ranks =%d \n",progress_rank,num_progress);
	//num_progress=(int)size/local_size;

	progress_ranks=malloc(num_progress*sizeof(int));
	
	int idx=0;
	for (int i=0;i<size;i++){
    		int pr=progress_per_rank[i];
    		int seen=0;
    		for(int j=0;j<idx;j++){
        		if(progress_ranks[j]==pr){
			       	seen=1; 
				break; 
    			}
		}
    		if(!seen){
        		progress_ranks[idx++] = pr;
    		}
	}
	num_progress=idx;
	/*
	for(int i=0;i<num_progress;i++)
		printf("%d,",progress_ranks[i]);
	printf("\n");	
	*/

	MPI_Group_excl(world_group,num_progress,progress_ranks,&app_group);
	MPI_Comm_create_group(MPI_COMM_WORLD,app_group,0,&LMPI_COMM_WORLD);
	
	//debug_log("Rank %d host %s finished LMPI_Init",rank,hostname);

        shm_buf_send=malloc(sizeof(void*)*local_size);
        shm_buf_recv=malloc(sizeof(void*)*local_size);
        /*
        if(strcmp(hostname,"f01r01c01s01")==0 )
                progress_rank=0;
        else
                progress_rank=2;
        */
	/*
        if(rank==0||rank==1||rank==2||rank==3)
                progress_rank=0;
        else
                progress_rank=4;
	*/

        //MPI_Allgather(&progress_rank,1,MPI_INT,progress_per_rank,1,MPI_INT,MPI_COMM_WORLD);
	
	/*        
        for(int i=0;i<size;i++)
                        printf("%d, ",progress_per_rank[i]);

        printf("\n");
        */


         if(rank==progress_rank){
                        MPI_Win_allocate_shared(MAX_QUEUE_NUM*sizeof(LMPI_Request ), sizeof(LMPI_Request ),MPI_INFO_NULL,shm_comm,&shared_queue,&shm_queue_win);
                        memset(shared_queue, 0, MAX_QUEUE_NUM * sizeof(*shared_queue));
			for (int i = 0; i < MAX_QUEUE_NUM; ++i)
    				atomic_store_explicit(&shared_queue[i].valid, SLOT_EMPTY, memory_order_relaxed);
			
        }
        else{
                MPI_Aint size;
                int disp_unit;

               //MPI_Aint alloc_size=0;
                //MPI_Aint alloc_size=LMPI_MAX_BUFFER;
                MPI_Win_allocate_shared(0, sizeof(LMPI_Request ), MPI_INFO_NULL, shm_comm, &shared_queue, &shm_queue_win);

                MPI_Win_shared_query(shm_queue_win, MPI_PROC_NULL, &size, &disp_unit, &shared_queue);

        }
	MPI_Barrier(shm_comm);
        //MPI_Aint my_send_size=(rank==progress_rank)?0:(MPI_Aint)LMPI_MAX_BUFFER;
        MPI_Aint my_send_size=LMPI_MAX_BUFFER;
        MPI_Win_allocate_shared(my_send_size,1,MPI_INFO_NULL,shm_comm,&my_send_base, &shm_win_send);

        if (rank==progress_rank) {
            for (int i = 0; i < local_size; ++i) {
                MPI_Aint segsz; int du; void *base = NULL;
                MPI_Win_shared_query(shm_win_send, i, &segsz, &du, &base);
                shm_buf_send[i] = base;
                // printf("[progress %d] send segment[%d] = %p size=%lld\n", world_rank, i, base, (long long)segsz);
            }
        }
        else {
            shm_buf_send[local_rank] = my_send_base;
        }
	MPI_Barrier(shm_comm);
        //MPI_Aint my_recv_size=(rank==progress_rank)?0:(MPI_Aint)LMPI_MAX_BUFFER;
        MPI_Aint my_recv_size=LMPI_MAX_BUFFER;
        //void *my_recv_base = NULL;
        MPI_Win_allocate_shared(my_recv_size, 1, MPI_INFO_NULL, shm_comm, &my_recv_base, &shm_win_recv);
	if (rank==progress_rank){
            for (int i = 0; i < local_size; ++i) {
                MPI_Aint segsz; int du; void *base = NULL;
                MPI_Win_shared_query(shm_win_recv, i, &segsz, &du, &base);
                shm_buf_recv[i] = base;
                // printf("[progress %d] recv segment[%d] = %p size=%lld\n", world_rank, i, base, (long long)segsz);
            }
        } 
	else{

            shm_buf_recv[local_rank] = my_recv_base;
        }
	MPI_Barrier(shm_comm);

	//MPI_Win_create_dynamic(MPI_INFO_NULL, MPI_COMM_WORLD, &win); Used only for one-sided progress rank

        MPI_Win_lock_all(0, shm_win_send);
        MPI_Win_lock_all(0, shm_win_recv);
        MPI_Win_lock_all(0, shm_queue_win);
	//debug_log("Rank %d host %s finished LMPI_Init",rank,hostname);
        MPI_Barrier(shm_comm);
	if(rank==progress_rank){
                //debug_log("Rank %d host %s finished LMPI_Init",rank,hostname);
		//dequeue_request_sendrecv();
		dequeue_request_memcpy();
		//dequeue_request_one_sided();

		
        }
        return 0;
}


int LMPI_Finalize() {

    int rank;
    int SIGNAL=LMPI_EXIT;
    MPI_Request request;
    MPI_Win_unlock_all(shm_win_send);
    MPI_Win_unlock_all(shm_win_recv);
    MPI_Win_unlock_all(shm_queue_win);
    //MPI_Win_unlock_all(win);

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
