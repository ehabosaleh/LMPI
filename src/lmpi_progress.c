#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include<unistd.h>
#include<limits.h>
#include<mpi.h>
#include "internal/lmpi_globals.h"
#include "internal/lmpi_queue_internal.h"
#include "internal/lmpi_progress_internal.h"
#include "lmpi_constants.h"
#include "lmpi_config.h"
#include "internal/lmpi_debug.h"

static int  g_tag_off=65536;
static inline int tag_rts(int user_tag){
	return user_tag+g_tag_off;
}


int get_progress_rank(){

        Fabric_info p_info;
        FILE *pf = popen("lstopo --cpuset | grep '(Fabric)' | grep PCI | awk '{print \"0000:\" $2}'", "r");
         	if (!pf) {
                        perror("popen failed");
                        return LMPI_ERR_SYS_COMMAND;
                }
                if (!fgets(p_info.pci_number, sizeof(p_info.pci_number), pf)) {
                	perror("fgets failed");
                	pclose(pf);
                return LMPI_ERR_FILE_EXISTS;
                }

        pclose(pf);

        /*
         * strcpy(pci_number,"d7:05.0");
         * use this only if you know the targeted PCI number

        **/

        p_info.pci_number[strcspn(p_info.pci_number, "\n")] = 0;//remove new line

        unsigned domain = 0, bus = 0, dev = 0, func = 0;

        if (sscanf(p_info.pci_number, "%x:%x:%x.%x", &domain, &bus, &dev, &func) != 4) {

            if (sscanf(p_info.pci_number, "%x:%x.%x", &bus, &dev, &func) != 3) {
                fprintf(stderr, "Invalid PCI format: %s\n", p_info.pci_number);
                return LMPI_ERR_FILE_EXISTS;

            }

        }

        #ifdef __linux__
                strcpy(p_info.driver_path,"/sys/bus/pci/devices/");/*Needed to check if there are other path format in linux*/
        #endif

        strcat(p_info.driver_path,p_info.pci_number);

        strcpy(p_info.driver_path_numa,p_info.driver_path);
        strcpy(p_info.driver_path_cpuset,p_info.driver_path);

        strcat(p_info.driver_path_numa,"/numa_node");
        strcat(p_info.driver_path_cpuset,"/local_cpulist");

        strcpy(p_info.numa_command,"cat ");
        strcpy(p_info.cpuset_command,"cat ");

        strcat(p_info.numa_command, p_info.driver_path_numa);
        strcat(p_info.cpuset_command,p_info.driver_path_cpuset);


        //printf("numa_command %s \n",p_info.numa_command);
        //printf("cpuset_command %s \n",p_info.cpuset_command);


        pf = popen(p_info.numa_command, "r");
        if(!pf){
                perror("popen faild");
        	return LMPI_ERR_SYS_COMMAND;
	}

        if(!fgets(p_info.numa_number, sizeof(p_info.numa_number), pf)){
                perror("fgets faild");
                pclose(pf);
        	return LMPI_ERR_SYS_COMMAND;
	}
        pclose(pf);

	//printf("%s \n",p_info.numa_number);

        pf = popen(p_info.cpuset_command, "r");
        if(!pf){
                perror("popen faild");
		 return LMPI_ERR_SYS_COMMAND;
        }
        if(!fgets(p_info.numa_cores, sizeof(p_info.numa_cores), pf)){
                perror("fgets faild");
                pclose(pf);
                return LMPI_ERR_FILE_EXISTS;
        }
        pclose(pf);

	int start=0,end=0;

        sscanf(p_info.numa_cores,"%d-%d",&start,&end);

	return start;

}


void* memory_copy(void *arg){
        LMPI_Mem *mem_data=(LMPI_Mem*)arg;
        int typesize;
        MPI_Type_size(mem_data->src_request->datatype, &typesize);

        memcpy(mem_data->dst_mem_addr,mem_data->src_mem_addr,mem_data->src_request->count*typesize);
        //debug_log("memcpy operation was conducted");
        mem_data->dst_request->fin_complete=1;
        mem_data->src_request->fin_complete=1;
        MPI_Win_sync(shm_queue_win);
       /*
	mem_data->src_request->request_status=LMPI_STATUS_COMPLETED;
        mem_data->dst_request->request_status=LMPI_STATUS_COMPLETED;
        MPI_Win_sync(shm_queue_win);
        */
	//free(mem_data);
        pthread_exit(NULL);
}

int dequeue_request_memcpy(){
        int flag_exit=0;
        int rank=0;
        //int time_send =0,time_recv=0;
        int dummy_flag=0;
        //char FIN_MSG=LMPI_FIN;
        //char RTS_MSG=LMPI_RTS;
        MPI_Comm_rank(MPI_COMM_WORLD,&rank);

        while(!flag_exit){
                MPI_Iprobe(MPI_ANY_SOURCE,LMPI_EXIT,MPI_COMM_WORLD,&flag_exit,MPI_STATUS_IGNORE);
                MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &dummy_flag, MPI_STATUS_IGNORE);
		MPI_Win_sync(shm_queue_win);
                //LMPI_Show_queue(rank);
		for(int i=0;i<MAX_QUEUE_NUM;i++){
                        //if(shared_queue[i].valid==1) {
			if(atomic_load_explicit(&shared_queue[i].valid, memory_order_acquire)==SLOT_READY)
                        {
                                if(shared_queue[i].opcode==LMPI_RECV_OP){
                                        if(shared_queue[i].request_status==LMPI_STATUS_PENDING){
                                                if(rank!=shared_queue[i].corresponding_progress_rank){
                                                        MPI_Irecv(&RTS_MSG,1,MPI_CHAR,shared_queue[i].corresponding_progress_rank,tag_rts(shared_queue[i].tag),MPI_COMM_WORLD,&shared_queue[i].rts_recv_request);
                                                         //debug_log("Rank %d found one recv request tag %d",rank,tag_rts(shared_queue[i].tag));
                                                        //LMPI_Show_queue(rank);
							shared_queue[i].request_status=LMPI_STATUS_WAITING;
                                                        MPI_Win_sync(shm_queue_win);
                                                }
                                                else{
							//debug_log("Rank %d found one recv request %d",rank,tag_rts(shared_queue[i].tag));
                                                        shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
                                                        MPI_Win_sync(shm_queue_win);
                                                }
                                        }

                                        if(shared_queue[i].request_status==LMPI_STATUS_WAITING){
                                                MPI_Test(&shared_queue[i].rts_recv_request,&shared_queue[i].rts_complete,MPI_STATUS_IGNORE);
                                                if(shared_queue[i].rts_complete==1){
							//time_recv++;
                                                        //debug_log("Receiver: %d received RTS  message  %d time",rank,time_recv);

                                                        shared_queue[i].recv_buffer=(char*)shm_buf_recv[shared_queue[i].local_rank]+shared_queue[i].mem_offset;

                                                        MPI_Irecv(shared_queue[i].recv_buffer,shared_queue[i].count,shared_queue[i].datatype,shared_queue[i].corresponding_progress_rank,shared_queue[i].tag,MPI_COMM_WORLD,&shared_queue[i].fin_recv_request);
                                                        shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
                                                        MPI_Win_sync(shm_queue_win);
                                                }
                                        }
                                        if(shared_queue[i].request_status==LMPI_STATUS_IN_PROGRESS){
                                                if(rank!=shared_queue[i].corresponding_progress_rank){
                                                        MPI_Test(&shared_queue[i].fin_recv_request,&shared_queue[i].fin_complete,MPI_STATUS_IGNORE);
                                                        if(shared_queue[i].fin_complete==1)
                                                        {
                                                         	/*
                                                                for(int j=0;i<shared_queue[i].count;j++)
                                                                        printf("%c,",*(char*)(shared_queue[i].recv_buffer+j));
                                                                printf("\n");
                                                                fflush(stdout);
                                                                */
                                                                //debug_log("Receive: data is ready");
                                                                shared_queue[i].request_status=LMPI_STATUS_COMPLETED;
                                                                MPI_Win_sync(shm_queue_win);
                                                        }
                                                }

                                                else{
                                                        //continue;

                                                        if(shared_queue[i].fin_complete==1){
                                                                //debug_log("Receive: data is ready");
                                                                shared_queue[i].request_status=LMPI_STATUS_COMPLETED;
                                                                
								MPI_Win_sync(shm_queue_win);
                                                        }


                                                }

                                        }
                                }
                                else if(shared_queue[i].opcode==LMPI_SEND_OP){
                                        if(shared_queue[i].request_status==LMPI_STATUS_PENDING){
                                                if(rank!=shared_queue[i].corresponding_progress_rank){
                                                        //debug_log("Found one inter request tag %d",tag_rts(shared_queue[i].tag));
                                                        MPI_Isend(&RTS_MSG,1,MPI_CHAR,shared_queue[i].corresponding_progress_rank,tag_rts(shared_queue[i].tag),MPI_COMM_WORLD,&shared_queue[i].rts_send_request);
                                                        shared_queue[i].request_status=LMPI_STATUS_WAITING;
                                                        MPI_Win_sync(shm_queue_win);
                                                }
                                                else{
							//debug_log("Rank %d found one send request",rank);
                                                        shared_queue[i].send_buffer=(char*)shm_buf_send[shared_queue[i].local_rank]+shared_queue[i].mem_offset;
                                                        LMPI_Mem *mem_data = malloc(sizeof(LMPI_Mem));
                                                        mem_data->src_mem_addr=shared_queue[i].send_buffer;
                                                        mem_data->src_request=&shared_queue[i];
							for(int j=0;j<MAX_QUEUE_NUM;j++){
								MPI_Win_sync(shm_queue_win);
                                                                if(atomic_load_explicit(&shared_queue[j].valid, memory_order_acquire) != SLOT_READY||shared_queue[j].opcode==LMPI_SEND_OP)
                                                                        continue;
                                                                else if(shared_queue[i].tag==shared_queue[j].tag && shared_queue[i].src==shared_queue[j].src && shared_queue[i].dst==shared_queue[j].dst){

									pthread_t memcpy_t;
                                                                        mem_data->dst_request=&shared_queue[j];
                                                                        mem_data->dst_mem_addr=(char*)shm_buf_recv[shared_queue[j].local_rank]+shared_queue[j].mem_offset;

								       	pthread_create(&memcpy_t,NULL,memory_copy, mem_data);
                                                                        pthread_detach(memcpy_t);

									/*
									memcpy(mem_data->dst_mem_addr,mem_data->src_mem_addr,shared_queue[i].count);
									shared_queue[i].fin_complete=1;
									shared_queue[j].fin_complete=1;
									*/
									shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
                                                                        MPI_Win_sync(shm_queue_win);
                                                                        break;
                                                                }
                                                        }

                                                }
                                        }
                                        if(shared_queue[i].request_status==LMPI_STATUS_WAITING){
                                                MPI_Test(&shared_queue[i].rts_send_request,&shared_queue[i].rts_complete,MPI_STATUS_IGNORE);
                                                if(shared_queue[i].rts_complete){
                                                        shared_queue[i].send_buffer = (char*)shm_buf_send[shared_queue[i].local_rank]+shared_queue[i].mem_offset;
                                                        /*
                                                        for(int j=0;j<shared_queue[i].count;j++)
                                                                        printf("%c,",*(char*)(shared_queue[i].send_buffer+j));
                                                        printf("\n");
                                                        fflush(stdout);
                                                        */
                                                        shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
                                                        MPI_Win_sync(shm_queue_win);
                                                        MPI_Isend(shared_queue[i].send_buffer,shared_queue[i].count,shared_queue[i].datatype,shared_queue[i].corresponding_progress_rank,shared_queue[i].tag,MPI_COMM_WORLD,&shared_queue[i].fin_send_request);
                                                        shared_queue[i].rts_send_request=MPI_REQUEST_NULL;
                                                        //debug_log("Times  %d Found one SEND request: %d send to %d",time_send,rank,shared_queue[i].corresponding_progress_rank);
                                                }

                                        }

                                        if(shared_queue[i].request_status==LMPI_STATUS_IN_PROGRESS){
                                                if(rank!=shared_queue[i].corresponding_progress_rank){
                                                        MPI_Test(&shared_queue[i].fin_send_request,&shared_queue[i].fin_complete,MPI_STATUS_IGNORE);
                                                        if(shared_queue[i].fin_complete==1){
                                                                //debug_log("Sender: %d Received FIN message %d time",rank,time_send);
                                                                shared_queue[i].request_status=LMPI_STATUS_COMPLETED;
                                                                MPI_Win_sync(shm_queue_win);
                                                        }
                                                }

                                                else{
                                                        //continue;

                                                        if(shared_queue[i].fin_complete==1){
								//debug_log("Send: Data is ready");
                                                                
								shared_queue[i].request_status=LMPI_STATUS_COMPLETED;
                                                                MPI_Win_sync(shm_queue_win);
                                                        }


                                                }

                                        }

                                }


                        }
                }

                //usleep(1000);

        }
	debug_log("Progress rank exit the polling loop ");
        MPI_Abort(MPI_COMM_WORLD,MPI_SUCCESS);
        return 0;
}
/*
int dequeue_request_sendrecv(){
	int flag_exit=0;
        int rank=0;
        int time_send =0,time_recv=0;
        int dummy_flag=0;
        char FIN_MSG=LMPI_FIN;
        char RTS_MSG=LMPI_RTS;
        MPI_Comm_rank(MPI_COMM_WORLD,&rank);

        while(!flag_exit){
                MPI_Iprobe(MPI_ANY_SOURCE,LMPI_EXIT,MPI_COMM_WORLD,&flag_exit,MPI_STATUS_IGNORE);
                MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &dummy_flag, MPI_STATUS_IGNORE);
                MPI_Win_sync(shm_queue_win);
                //LMPI_Show_queue(rank);
                for(int i=0;i<MAX_QUEUE_NUM;i++){
                        //if(shared_queue[i].valid==1) {
                        if(atomic_load_explicit(&shared_queue[i].valid, memory_order_acquire)==SLOT_READY)
                        {
                                if(shared_queue[i].opcode==LMPI_RECV_OP){
                                        if(shared_queue[i].request_status==LMPI_STATUS_PENDING){
						if(rank!=shared_queue[i].corresponding_progress_rank){

                                                        MPI_Irecv(&RTS_MSG,1,MPI_CHAR,shared_queue[i].corresponding_progress_rank,tag_rts(shared_queue[i].tag),MPI_COMM_WORLD,&shared_queue[i].rts_recv_request);
                                                         //debug_log("Rank %d found one recv request",rank);
                                                        //LMPI_Show_queue(rank);
                                                        shared_queue[i].request_status=LMPI_STATUS_WAITING;
                                                        MPI_Win_sync(shm_queue_win);
                                                }
                                                else{
							shared_queue[i].recv_buffer=(char*)shm_buf_recv[shared_queue[i].local_rank]+shared_queue[i].mem_offset;
                                                        MPI_Irecv(shared_queue[i].recv_buffer,shared_queue[i].count,shared_queue[i].datatype,shared_queue[i].corresponding_progress_rank,shared_queue[i].tag,MPI_COMM_WORLD,&shared_queue[i].fin_recv_request);
                                                        shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
                                                        MPI_Win_sync(shm_queue_win);
                                                }
                                        }

                                        else if(shared_queue[i].request_status==LMPI_STATUS_WAITING){
                                                	MPI_Test(&shared_queue[i].rts_recv_request,&shared_queue[i].rts_complete,MPI_STATUS_IGNORE);
                                                	if(shared_queue[i].rts_complete==1){
                                                        	//time_recv++;
                                                        	//debug_log("Receiver: %d received RTS  message  %d time",rank,time_recv);

                                                        	shared_queue[i].recv_buffer=(char*)shm_buf_recv[shared_queue[i].local_rank]+shared_queue[i].mem_offset;
                                                        	MPI_Irecv(shared_queue[i].recv_buffer,shared_queue[i].count,shared_queue[i].datatype,shared_queue[i].corresponding_progress_rank,shared_queue[i].tag,MPI_COMM_WORLD,&shared_queue[i].fin_recv_request);
                                                        	shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
                                                        	MPI_Win_sync(shm_queue_win);
                                                	}

                                        }
                                        else if(shared_queue[i].request_status==LMPI_STATUS_IN_PROGRESS){
						MPI_Test(&shared_queue[i].fin_recv_request,&shared_queue[i].fin_complete,MPI_STATUS_IGNORE);
                                                if(shared_queue[i].fin_complete==1){
                                                         //debug_log("Receive: data is ready");
                                                         shared_queue[i].request_status=LMPI_STATUS_COMPLETED;
                                                         MPI_Win_sync(shm_queue_win);
                                                }
                                         }
                                }
                                else if(shared_queue[i].opcode==LMPI_SEND_OP){
                                        if(shared_queue[i].request_status==LMPI_STATUS_PENDING){

						if(rank!=shared_queue[i].corresponding_progress_rank){
                                                        //debug_log("Found one inter request");
                                                        MPI_Isend(&RTS_MSG,1,MPI_CHAR,shared_queue[i].corresponding_progress_rank,tag_rts(shared_queue[i].tag),MPI_COMM_WORLD,&shared_queue[i].rts_send_request);
                                                        shared_queue[i].request_status=LMPI_STATUS_WAITING;
                                                        MPI_Win_sync(shm_queue_win);
                                                }
                                                else{
							//debug_log("Times  %d Found one SEND request: %d send to %d",time_send,rank,shared_queue[i].corresponding_progress_rank);
                                                        shared_queue[i].send_buffer = (char*)shm_buf_send[shared_queue[i].local_rank]+shared_queue[i].mem_offset;
                                                        MPI_Isend(shared_queue[i].send_buffer,shared_queue[i].count,shared_queue[i].datatype,shared_queue[i].corresponding_progress_rank,shared_queue[i].tag,MPI_COMM_WORLD,&shared_queue[i].fin_send_request);
							shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
                                                        MPI_Win_sync(shm_queue_win);
						}

                                        }

                                        else if(shared_queue[i].request_status==LMPI_STATUS_WAITING){
                                                MPI_Test(&shared_queue[i].rts_send_request,&shared_queue[i].rts_complete,MPI_STATUS_IGNORE);
                                                if(shared_queue[i].rts_complete){
                                                        shared_queue[i].send_buffer = (char*)shm_buf_send[shared_queue[i].local_rank]+shared_queue[i].mem_offset;
                                                        shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
                                                        MPI_Win_sync(shm_queue_win);
                                                        MPI_Isend(shared_queue[i].send_buffer,shared_queue[i].count,shared_queue[i].datatype,shared_queue[i].corresponding_progress_rank,shared_queue[i].tag,MPI_COMM_WORLD,&shared_queue[i].fin_send_request);
                                                        shared_queue[i].rts_send_request=MPI_REQUEST_NULL;
                                                        //debug_log("Times  %d Found one SEND request: %d send to %d",time_send,rank,shared_queue[i].corresponding_progress_rank);
                                                }

                                        }

                                        else if(shared_queue[i].request_status==LMPI_STATUS_IN_PROGRESS){
                                                        MPI_Test(&shared_queue[i].fin_send_request,&shared_queue[i].fin_complete,MPI_STATUS_IGNORE);
                                                        if(shared_queue[i].fin_complete==1){
                                                                //debug_log("Sender: %d Received FIN message %d time",rank,time_send);
                                                                shared_queue[i].request_status=LMPI_STATUS_COMPLETED;
                                                                MPI_Win_sync(shm_queue_win);
                                                        }
                                       }

                          	}

             		}

   		}

	}


    MPI_Abort(MPI_COMM_WORLD,MPI_SUCCESS);
    debug_log("Progress rank exit the polling loop ");
    return 0;


}
*/

int dequeue_request_sendrecv(){
        int flag_exit=0;
        int rank=0;
        int time_send =0,time_recv=0;
        int dummy_flag=0;
        char FIN_MSG=LMPI_FIN;
        char RTS_MSG=LMPI_RTS;
        MPI_Comm_rank(MPI_COMM_WORLD,&rank);

        while(!flag_exit){
                MPI_Iprobe(MPI_ANY_SOURCE,LMPI_EXIT,MPI_COMM_WORLD,&flag_exit,MPI_STATUS_IGNORE);
                MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &dummy_flag, MPI_STATUS_IGNORE);
                MPI_Win_sync(shm_queue_win);
                //LMPI_Show_queue(rank);
                for(int i=0;i<MAX_QUEUE_NUM;i++){
                        //if(shared_queue[i].valid==1) {
                        if(atomic_load_explicit(&shared_queue[i].valid, memory_order_acquire)==SLOT_READY)
                        {
                                if(shared_queue[i].opcode==LMPI_RECV_OP){
					if(shared_queue[i].request_status==LMPI_STATUS_PENDING){
                                                        shared_queue[i].recv_buffer=(char*)shm_buf_recv[shared_queue[i].local_rank]+shared_queue[i].mem_offset;
                                                        MPI_Irecv(shared_queue[i].recv_buffer,shared_queue[i].count,shared_queue[i].datatype,shared_queue[i].corresponding_progress_rank,shared_queue[i].tag,MPI_COMM_WORLD,&shared_queue[i].fin_recv_request);
                                                        shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
                                                        MPI_Win_sync(shm_queue_win);
                                        }

                                        if(shared_queue[i].request_status==LMPI_STATUS_IN_PROGRESS){
                                                MPI_Test(&shared_queue[i].fin_recv_request,&shared_queue[i].fin_complete,MPI_STATUS_IGNORE);
                                                if(shared_queue[i].fin_complete==1){
                                                        /*
                                                        for(int j=0;i<shared_queue[i].count;j++)
                                                                printf("%c,",*(char*)(shared_queue[i].recv_buffer+j));
                                                         printf("\n");
                                                         fflush(stdout);
                                                         */
                                                         //debug_log("Receive: data is ready");
                                                         shared_queue[i].request_status=LMPI_STATUS_COMPLETED;
                                                         MPI_Win_sync(shm_queue_win);
                                                }
                                         }
                                }
                                else if(shared_queue[i].opcode==LMPI_SEND_OP){
                                        if(shared_queue[i].request_status==LMPI_STATUS_PENDING){

                                                        shared_queue[i].send_buffer = (char*)shm_buf_send[shared_queue[i].local_rank]+shared_queue[i].mem_offset;
                                                        MPI_Isend(shared_queue[i].send_buffer,shared_queue[i].count,shared_queue[i].datatype,shared_queue[i].corresponding_progress_rank,shared_queue[i].tag,MPI_COMM_WORLD,&shared_queue[i].fin_send_request);
                                                        shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
                                                        MPI_Win_sync(shm_queue_win);
                                                }


                                        if(shared_queue[i].request_status==LMPI_STATUS_IN_PROGRESS){
                                                        MPI_Test(&shared_queue[i].fin_send_request,&shared_queue[i].fin_complete,MPI_STATUS_IGNORE);
                                                        if(shared_queue[i].fin_complete==1){
                                                                //debug_log("Sender: %d Received FIN message %d time",rank,time_send);
                                                                shared_queue[i].request_status=LMPI_STATUS_COMPLETED;
                                                                MPI_Win_sync(shm_queue_win);
                                                        }
                                       }

                                }

                        }

                }

        }


    MPI_Abort(MPI_COMM_WORLD,MPI_SUCCESS);
    debug_log("Progress rank exit the polling loop ");
    return 0;

}
/*
 *
 * Progressing using one-sided communication 
int dequeue_request_one_sided() {
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
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &dummy_flag, MPI_STATUS_IGNORE);
                MPI_Win_sync(shm_queue_win);
                for(int i=0;i<MAX_QUEUE_NUM;i++){
                        //if(shared_queue[i].valid==1){
                        if(atomic_load_explicit(&shared_queue[i].valid, memory_order_acquire)==SLOT_READY)
                        {
				if(shared_queue[i].opcode==LMPI_RECV_OP){
                                        if(shared_queue[i].request_status==LMPI_STATUS_PENDING){
                                                if(rank!=shared_queue[i].corresponding_progress_rank){
							MPI_Irecv(&shared_queue[i].remote_win_addr,1,MPI_AINT,shared_queue[i].corresponding_progress_rank,shared_queue[i].tag,MPI_COMM_WORLD,&shared_queue[i].rts_recv_request);
                                                	shared_queue[i].request_status=LMPI_STATUS_WAITING;
                                                	MPI_Win_sync(shm_queue_win);
                                        	}
						else{
							shared_queue[i].recv_buffer=(char*)shm_buf_recv[shared_queue[i].local_rank]+shared_queue[i].mem_offset;
							for(int j=0;j<MAX_QUEUE_NUM;j++){
								if(atomic_load_explicit(&shared_queue[j].valid, memory_order_acquire)!=SLOT_READY||shared_queue[j].opcode==LMPI_RECV_OP)
                                                                        continue;
                                                                else if(shared_queue[i].tag==shared_queue[j].tag && shared_queue[i].src==shared_queue[j].src && shared_queue[i].dst==shared_queue[j].dst){
									shared_queue[i].shm_queue_sender_id=j;
									shared_queue[i].remote_win_addr=shared_queue[j].remote_win_addr;
									//debug_log("Remote address %lld",shared_queue[i].remote_win_addr);
									//MPI_Win_lock_all(0, win);
                                                			MPI_Rget(shared_queue[i].recv_buffer, shared_queue[i].count, shared_queue[i].datatype,shared_queue[i].world_src,shared_queue[i].remote_win_addr, shared_queue[i].count, shared_queue[i].datatype,win,&shared_queue[i].request);

									shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
                                                			MPI_Win_sync(shm_queue_win);
									break;
								}
							}
						}
					}
                                        if(shared_queue[i].request_status==LMPI_STATUS_WAITING){
                                                	MPI_Test(&shared_queue[i].rts_recv_request,&shared_queue[i].rts_complete,MPI_STATUS_IGNORE);
                                                	if(shared_queue[i].rts_complete){
								//debug_log("Found one receive request");

								shared_queue[i].recv_buffer=(char*)shm_buf_recv[shared_queue[i].local_rank]+shared_queue[i].mem_offset;
								//debug_log("Proxy receiver: %lld src rank %d",shared_queue[i].remote_win_addr,shared_queue[i].world_src);

                                                        	//MPI_Win_lock_all(0, win);
								MPI_Rget(shared_queue[i].recv_buffer, shared_queue[i].count, shared_queue[i].datatype,shared_queue[i].world_src,shared_queue[i].remote_win_addr, shared_queue[i].count, shared_queue[i].datatype,win,&shared_queue[i].request);

								shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
                                                        	MPI_Win_sync(shm_queue_win);
                                                	}

                                        }
                                        if(shared_queue[i].request_status==LMPI_STATUS_IN_PROGRESS){
							MPI_Test(&shared_queue[i].request,&shared_queue[i].rget_complete,MPI_STATUS_IGNORE);
							if(shared_queue[i].rget_complete){
                                                               	//MPI_Win_flush(shared_queue[i].world_src,win);
								//MPI_Win_unlock_all(win);
								if(rank!=shared_queue[i].corresponding_progress_rank)
                                                                	MPI_Isend(&FIN_MSG,1,MPI_CHAR,shared_queue[i].corresponding_progress_rank,tag_rts(shared_queue[i].tag),MPI_COMM_WORLD,&shared_queue[i].fin_recv_request);
								else{
									shared_queue[shared_queue[i].shm_queue_sender_id].fin_complete=1;
									//shared_queue[shared_queue[i].shm_queue_sender_id].request_status=LMPI_STATUS_COMPLETED;
									MPI_Win_sync(shm_queue_win);
								}
								shared_queue[i].request_status=LMPI_STATUS_COMPLETED;
                                                                MPI_Win_sync(shm_queue_win);
                                                        }

                                        }


                                }
                                else if(shared_queue[i].opcode==LMPI_SEND_OP){
                                        if(shared_queue[i].request_status==LMPI_STATUS_PENDING){
                                                //debug_log("Proxy sender: %lld tag %d",shared_queue[i].remote_win_addr,shared_queue[i].tag);
						if(rank!=shared_queue[i].corresponding_progress_rank){
							MPI_Isend(&shared_queue[i].remote_win_addr,1,MPI_AINT,shared_queue[i].corresponding_progress_rank,shared_queue[i].tag,MPI_COMM_WORLD,&shared_queue[i].rts_send_request);
                                                shared_queue[i].request_status=LMPI_STATUS_WAITING;
                                                MPI_Win_sync(shm_queue_win);
						}
						else{
							//debug_log("Sender Remote address %lld",shared_queue[i].remote_win_addr);
							shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
                                                	MPI_Win_sync(shm_queue_win);
						}
                                        }
                                       	if(shared_queue[i].request_status==LMPI_STATUS_WAITING){
                                                MPI_Test(&shared_queue[i].rts_send_request,&shared_queue[i].rts_complete,MPI_STATUS_IGNORE);
                                                if(shared_queue[i].rts_complete){
                                                        //debug_log("Sender Rget finished ");
							shared_queue[i].request_status=LMPI_STATUS_IN_PROGRESS;
                                                        MPI_Win_sync(shm_queue_win);
                                                }

                                        }

                                       if(shared_queue[i].request_status==LMPI_STATUS_IN_PROGRESS){
                                               if(rank!=shared_queue[i].corresponding_progress_rank){
					       		int fin_flag=1;
                                                	MPI_Iprobe(shared_queue[i].corresponding_progress_rank,tag_rts(shared_queue[i].tag),MPI_COMM_WORLD,&fin_flag,MPI_STATUS_IGNORE);
                                                	if(fin_flag){
                                                        	shared_queue[i].request_status=LMPI_STATUS_COMPLETED;
								MPI_Win_sync(shm_queue_win);
                                               	 	}
					       }
					       else{
						       if(shared_queue[i].fin_complete){
						       		shared_queue[i].request_status=LMPI_STATUS_COMPLETED;
                                                                MPI_Win_sync(shm_queue_win);
						       }

					       }

                                        }
                                }
                        }
                }

                //usleep(1000);

        }
        debug_log("Progress rank exit the polling loop ");
        return 0;
}
*/
