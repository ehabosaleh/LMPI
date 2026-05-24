#include<stdlib.h>
#include<stdio.h>
#include<time.h>
#include<unistd.h>
#include<mpi.h>

#define MAX_MSG_SIZE (1ULL << 28)
#define MIN_MSG_SIZE (1ULL << 20)
#define DIM 50
#define MAX_ITERATION 200
#define SKIP 100

int main(){
	int rank=0,size=0;
	int len;
	char*hostname=malloc(256);
	double start_time=0,end_time=0,total_time=0,bandwidth=0,latency=0;
	int src=0,dst=1;
	double start_omm_time=0,end_comm_time=0,total_comm_time,start_comp_time=0,end_comp_time=0,total_comp_time=0;

	MPI_Init(NULL,NULL);
	MPI_Comm_rank(MPI_COMM_WORLD,&rank);
	MPI_Comm_size(MPI_COMM_WORLD,&size);
	MPI_Get_processor_name(hostname,&len);

	if(rank==src){
		printf("%-20s%-20s%-20s\n","Size (Bytes)","Latency(us)","Bandwidth(MB/s)");
		fflush(stdout);
	}

	int count=0;

	if(rank==src||rank==dst)
		for(long msg_size=MIN_MSG_SIZE;msg_size<=MAX_MSG_SIZE;msg_size*=2){
			int iter=0;
			int flag=0;
			flag=0;

			char*buffer_send=malloc(sizeof(char)*msg_size);
			char*buffer_recv=malloc(sizeof(char)*msg_size);

			for(int i=0;i<msg_size;i++)
				*(buffer_send+i)='a';

			count++;
			for(iter=0;iter<MAX_ITERATION;iter++){

				if(rank==src){

					MPI_Request requests[2];
					start_time=MPI_Wtime();

					MPI_Isend(buffer_send,msg_size,MPI_CHAR,dst,count+iter+100,MPI_COMM_WORLD,&requests[0]);

					MPI_Wait(&requests[0],MPI_STATUS_IGNORE);

					if(iter>SKIP)
						end_time+=MPI_Wtime()-start_time;
				}

				else if(rank==dst){
					MPI_Request requests[2];
					flag=0;

					MPI_Irecv(buffer_recv,msg_size,MPI_CHAR,src,count+iter+100,MPI_COMM_WORLD,&requests[0]);
					MPI_Wait(&requests[0],MPI_STATUS_IGNORE);
				}

				MPI_Barrier(MPI_COMM_WORLD);
			}

			if(rank==src){
				total_time=(end_time/(MAX_ITERATION-SKIP));
				bandwidth=(msg_size)/(total_time);

				printf("%-20ld%-20.3f%-20.3f\n",msg_size,1e6*total_time,bandwidth/(1024*1024));
				fflush(stdout);
				bandwidth=0;
				total_time=0;
				end_time=0;
			}

			//free(buffer_send);
			//free(buffer_recv);
			MPI_Barrier(MPI_COMM_WORLD);
		}

	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Finalize();
	return 0;
}
