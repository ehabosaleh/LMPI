#include<stdlib.h>
#include<stdio.h>
#include"lmpi.h"
#include<time.h>
#include<unistd.h>

#define MAX_MSG_SIZE (1ULL << 20)
#define MIN_MSG_SIZE (1ULL << 1)
#define DIM 100
#define MAX_ITERATION 200
#define SKIP 100
static float **a, *x, *y;

void init_arrays()
{

    int i = 0, j = 0;

    a = (float **)malloc(DIM * sizeof(float *));

    for (i = 0; i < DIM; i++) {
        a[i] = (float *)malloc(DIM * sizeof(float));
    }

    x = (float *)malloc(DIM * sizeof(float));
    y = (float *)malloc(DIM * sizeof(float));

    for (i = 0; i < DIM; i++) {
        x[i] = y[i] = 1.0f;
        for (j = 0; j < DIM; j++) {
            a[i][j] = 2.0f;
        }
    }
}


static void compute_on_host(double latency)
{
   int i = 0, j = 0;
   double tcomp_all=0;
   clock_t ccomp_start=0,ccomp_total=0;

   while(tcomp_all<latency)
   {
        ccomp_start=clock();
        for (i = 0; i < DIM; i++)
                for (j = 0; j < DIM; j++)
                        x[i] = x[i] + a[i][j]*a[j][i] + y[j];

        ccomp_total+=clock()-ccomp_start;

        tcomp_all=((double)ccomp_total)/CLOCKS_PER_SEC;
   }
}

int main(){

	int rank=0,size=0,old_rank;
	int len;
	char*hostname=malloc(256);
	double start_time=0,end_time=0,total_time=0,bandwidth=0,latency=0;
	int src=0,dst=2;
	double  start_comm_time=0,end_comm_time=0,total_comm_time,start_comp_time=0,end_comp_time=0,total_comp_time=0;

	LMPI_Init(NULL,NULL);
	LMPI_Comm_rank(MPI_COMM_WORLD,&old_rank);	
	LMPI_Comm_rank(LMPI_COMM_WORLD,&rank);
	LMPI_Comm_size(LMPI_COMM_WORLD,&size);
	LMPI_Get_processor_name(hostname,&len);
	
	//printf("LMPI rank %d --- Old rank %d \n",rank,old_rank);
	
	/*
	MPI_Group world_group, worker_group;
    	MPI_Comm worker_comm;
	MPI_Comm_group(LMPI_COMM_WORLD, &world_group);
	int ranks[] = {src, dst};
	int n = 2;
	MPI_Group_incl(world_group, n, ranks, &worker_group);
	MPI_Comm_create_group(LMPI_COMM_WORLD, worker_group, 0, &worker_comm);
	*/

	if(rank==src){
		printf("%-20s%-20s%-20s\n","Size (Bytes)","Latency(us)","Bandwidth(MB/s)");
		 fflush(stdout);
	}
	

	int count=0;
	if(rank==src||rank==dst)
		for(long long msg_size=MIN_MSG_SIZE;msg_size<=MAX_MSG_SIZE;msg_size*=2){
			int iter=0;
			int flag=0;
			flag=0;
                	//char *buffer_recv=NULL;
                	//char *buffer_send=NULL;
                	char*buffer_send=LMPI_Register(msg_size,MPI_CHAR);
                	char*buffer_recv=NULL;
			for(int i=0;i<msg_size;i++)
                       		*(buffer_send+i)='V';
			/*
			for(int i=0;i<msg_size;i++)
                                printf("%c,",*(buffer_send+i));
			fflush(stdout);
			*/

			count++;
			for(iter=0;iter<MAX_ITERATION;iter++){

				if(rank==src){
					LMPI_Request requests[2];
					start_time=MPI_Wtime();
					LMPI_Isend(buffer_send, msg_size, MPI_CHAR, dst, count+iter+100, LMPI_COMM_WORLD, &requests[0]);
					LMPI_Waitall(1,&requests[0],&flag);

					//LMPI_Irecv((void**)&buffer_recv,msg_size,MPI_CHAR,dst,count+iter,MPI_COMM_WORLD,&requests[1]);
					//LMPI_Wait(&requests[1],&flag);

				
					if(iter>SKIP)
						end_time+=MPI_Wtime()-start_time;
					/*
					for(int i=0;i<msg_size;i++)
						printf("%c,",*(buffer_recv+i));
					printf("\n");
					fflush(stdout);
					*/
					//printf("Rank %d iter %d \n",rank,iter);
				}

				else if(rank==dst){
					LMPI_Request requests[2];
					flag=0;

					LMPI_Irecv((void**)&buffer_recv,msg_size,MPI_CHAR,src,count+iter+100,LMPI_COMM_WORLD,&requests[0]);
					LMPI_Waitall(1,&requests[0],&flag);
					//LMPI_Isend(buffer_send, msg_size, MPI_CHAR, src, count+iter, MPI_COMM_WORLD, &requests[1]);
					//LMPI_Wait(&requests[1],&flag);

					/*					
					for(int i=0;i<msg_size;i++)
                                      		printf("%c,",*(buffer_recv+i));
                                	printf("\n");
					fflush(stdout);
					*/

					//printf("Rank %d iter %d \n",rank,iter);
				}

				//MPI_Barrier(worker_comm);
			}
		if(rank==src){
			total_time=(end_time/(MAX_ITERATION-SKIP));
			bandwidth=((MAX_ITERATION-SKIP)*msg_size)/(end_time);

			printf("%-20lld%-20.3f%-20.3f\n",msg_size,1e6*total_time,bandwidth/(1024*1024));
			fflush(stdout);
			bandwidth=0;
			total_time=0;
			end_time=0;

		}
		//free(buffer_send);
		//free(buffer_recv);

		//MPI_Barrier(worker_comm);

		}

	
	LMPI_Barrier(LMPI_COMM_WORLD);
	

	LMPI_Finalize();


}
