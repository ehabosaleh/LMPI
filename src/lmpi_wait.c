#include <string.h>
#include "lmpi.h"
#include "internal/lmpi_globals.h"

int LMPI_Wait(LMPI_Request * request,int*flag){
        *flag=0;
        int dummy_flag=0;
        while(!(*flag)){

                MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &dummy_flag, MPI_STATUS_IGNORE);
                LMPI_Test(request,flag);

        }
        *flag=1;
        return 0;
}
int LMPI_Test(LMPI_Request *request,int*flag){
        *flag=0;
        int dummy_flag;
        if(shared_queue[request->index].request_status==LMPI_STATUS_COMPLETED){
                *flag=1;
                //shared_queue[request->index].valid=0;
                atomic_store_explicit(&shared_queue[request->index].valid, SLOT_EMPTY, memory_order_release);
                 if(request->opcode==LMPI_SEND_OP){
                                        //MPI_Win_detach(win,request->send_buffer);//Used only with one-sided progress rank
                                }

		memset(&shared_queue[request->index],0,sizeof(LMPI_Request));
                MPI_Win_sync(shm_queue_win);

                return 0;
        }
        else{
        *flag=0;
         return 1;
        }
}

int LMPI_Testall(int count, LMPI_Request array_of_requests[],int *flag){
        int dummy_flag=0;
        int complete_requests=0;
	MPI_Win_sync(shm_queue_win);
	for(int i=0;i<count;i++)
                if(shared_queue[array_of_requests[i].index].request_status==LMPI_STATUS_COMPLETED)
                        complete_requests++;;


        if(complete_requests==count){
                *flag=1;
                for(int i=0;i<count;i++){
                        //shared_queue[array_of_requests[i].index].valid=0;
                        //atomic_store_explicit(&shared_queue[array_of_requests[i].index].valid, SLOT_EMPTY, memory_order_release);
                        
			//if(array_of_requests[i].opcode==LMPI_SEND_OP){
                                        //MPI_Win_detach(win,array_of_requests[i].send_buffer); //used only with one-sided progress rank 
                         //       }

			memset(&shared_queue[array_of_requests[i].index],0,sizeof(LMPI_Request));
                        MPI_Win_sync(shm_queue_win);
			atomic_store_explicit(&shared_queue[array_of_requests[i].index].valid, SLOT_EMPTY, memory_order_release);
			MPI_Win_sync(shm_queue_win);
                }
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
                LMPI_Testall(count,array_of_requests,flag);
        }
        return 0;
}
