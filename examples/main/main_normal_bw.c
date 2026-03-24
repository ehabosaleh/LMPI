#include<stdlib.h>
#include<stdio.h>
#include<time.h>
#include<unistd.h>
#include<mpi.h>
#define MAX_MSG_SIZE (1 << 30)
#define MIN_MSG_SIZE (1 << 1)

#define DIM 100
#define MAX_ITERATION 500

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
	int rank=0,size=0;
	int len;
	char*hostname=malloc(256);
	double start_time=0,end_time=0,total_time=0,bandwidth=0,latency=0;
	int src=0,dst=6;
	double  start_comm_time=0,end_comm_time=0,total_comm_time,start_comp_time=0,end_comp_time=0,total_comp_time=0;

	MPI_Init(NULL,NULL);

	MPI_Comm_rank(MPI_COMM_WORLD,&rank);
	MPI_Comm_size(MPI_COMM_WORLD,&size);
	MPI_Get_processor_name(hostname,&len);

	MPI_Group world_group, worker_group;
        MPI_Comm worker_comm;
        MPI_Comm_group(MPI_COMM_WORLD, &world_group);
        int ranks[] = {src, dst};
        int n = 2;
        MPI_Group_incl(world_group, n, ranks, &worker_group);
        MPI_Comm_create_group(MPI_COMM_WORLD, worker_group, 0, &worker_comm);

	
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
                //char *buffer_recv=NULL;
                //char *buffer_send=NULL;
                char*buffer_send=malloc(sizeof(char)*msg_size);
                char*buffer_recv=malloc(sizeof(char)*msg_size);
		for(int i=0;i<msg_size;i++)
                       *(buffer_send+i)='a';

		count++;
		MPI_Request requests[2];
		for(iter=0;iter<MAX_ITERATION;iter++){

			if(rank==src){

				start_time=MPI_Wtime();
				MPI_Isend(buffer_send, msg_size, MPI_CHAR, dst, count+iter+100, MPI_COMM_WORLD, &requests[0]);
				MPI_Wait(&requests[0],MPI_STATUS_IGNORE);
				//MPI_Irecv(buffer_recv,msg_size,MPI_CHAR,dst,count+iter,MPI_COMM_WORLD,&requests[1]);
				//MPI_Wait(&requests[1],MPI_STATUS_IGNORE);

				if(iter>100)
					end_time+=MPI_Wtime()-start_time;

				//for(int i=0;i<msg_size;i++)
				//	printf("%c,",*(buffer_recv+i));
				//printf("\n");
				//printf("Rank %d iter %d \n",rank,iter);
			}

			else if(rank==dst){
				flag=0;

				MPI_Irecv(buffer_recv,msg_size,MPI_CHAR,src,count+iter+100,MPI_COMM_WORLD,&requests[0]);
				MPI_Wait(&requests[0],MPI_STATUS_IGNORE);
				//MPI_Isend(buffer_send, msg_size, MPI_CHAR, src, count+iter, MPI_COMM_WORLD, &requests[1]);
				//MPI_Wait(&requests[1],MPI_STATUS_IGNORE);



				//printf("Rank %d iter %d \n",rank,iter);
			}

			MPI_Barrier(worker_comm);
		}
	if(rank==src){
		total_time=(end_time/(MAX_ITERATION-100));
		bandwidth=((MAX_ITERATION-100)*msg_size)/(end_time);

		printf("%-20ld%-20.3f%-20.3f\n",msg_size,1e6*total_time,bandwidth/(1024*1024));
		 fflush(stdout);
		bandwidth=0;
		total_time=0;
		end_time=0;

	}
	//free(buffer_send);
	//free(buffer_recv);

	MPI_Barrier(worker_comm);

	}



	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Finalize();


}
