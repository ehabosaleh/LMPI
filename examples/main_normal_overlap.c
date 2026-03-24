#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <mpi.h>
#include <math.h>

#define MAX_MSG_SIZE (1<<28)
#define MIN_MSG_SIZE (1<<20)

#define DIM 50
#define MAX_ITERATION 200

static float **a,*x,*y;

void init_arrays(){
    a=(float**)malloc(DIM*sizeof(float*));
    for(int i=0;i<DIM;i++){
        a[i]=(float*)malloc(DIM*sizeof(float));
    }

    x=(float*)malloc(DIM*sizeof(float));
    y=(float*)malloc(DIM*sizeof(float));

    for(int i=0;i<DIM;i++){
        x[i]=y[i]=1.0f;
        for(int j=0;j<DIM;j++){
            a[i][j]=2.0f;
        }
    }
}

void free_arrays(){
    for(int i=0;i<DIM;i++)free(a[i]);
    free(a);
    free(x);
    free(y);
}

static void compute_on_host(double latency){
        int i = 0, j = 0;
        double tcomp_all=0;
        double ccomp_start=0,ccomp_total=0;

        while(ccomp_total<latency)
        {
                ccomp_start=MPI_Wtime();
                for (i = 0; i < DIM; i++)
                        for (j = 0; j < DIM; j++)
                                x[i] = x[i] + a[i][j]*a[j][i] + y[j];

                ccomp_total+=MPI_Wtime()-ccomp_start;

                //tcomp_all=((double)ccomp_total)/CLOCKS_PER_SEC;
        }
}

int main(){
    int rank=0,size=0;
    char hostname[256];
    int len;
    int src=0,dst=1;

    MPI_Init(NULL,NULL);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);
    MPI_Get_processor_name(hostname,&len);

    if(size<2){
        if(rank==0)printf("Requires at least 2 MPI processes.\n");
        MPI_Finalize();
        return 1;
    }

    init_arrays();

    if(rank==src){
        printf("%-20s%-20s%-20s%-20s%-20s\n","Size(Bytes)","Compute time(us)","Comm time(us)","Total time(us)","Overlap(%)");
        fflush(stdout);
    }

    int count=0;
    for(int msg_size=MIN_MSG_SIZE;msg_size<=MAX_MSG_SIZE;msg_size*=2){
        count++;
        char *buffer_send=(char*)malloc(msg_size);
        char *buffer_recv=(char*)malloc(msg_size);

        for(int i=0;i<msg_size;i++)buffer_send[i]='a';

        double total_comm_time=0;
        double total_comp_time=0;
        double total_time=0;

        for(int iter=0;iter<MAX_ITERATION;iter++){
            MPI_Request requests[2];
            double start_time,end_time;

            if(rank==src){
                start_time=MPI_Wtime();
                MPI_Isend(buffer_send,msg_size,MPI_CHAR,dst,count+iter+100,MPI_COMM_WORLD,&requests[0]);
                MPI_Irecv(buffer_recv,msg_size,MPI_CHAR,dst,count+iter,MPI_COMM_WORLD,&requests[1]);
		MPI_Waitall(2,requests,MPI_STATUS_IGNORE);
		//MPI_Wait(&requests[0],MPI_STATUS_IGNORE);
                
		end_time=MPI_Wtime();

                if(iter>100)
			total_comm_time+=end_time-start_time;
            }
	    else if(rank==dst){
		start_time=MPI_Wtime();
                MPI_Irecv(buffer_recv,msg_size,MPI_CHAR,src,count+iter+100,MPI_COMM_WORLD,&requests[0]);
                MPI_Isend(buffer_send,msg_size,MPI_CHAR,src,count+iter,MPI_COMM_WORLD,&requests[1]);
                MPI_Waitall(2,requests,MPI_STATUS_IGNORE);
		//MPI_Wait(&requests[0],MPI_STATUS_IGNORE);

		end_time=MPI_Wtime();

                if(iter>100)
                        total_comm_time+=end_time-start_time;
            }

            MPI_Barrier(MPI_COMM_WORLD);
        }
        total_comm_time/=(MAX_ITERATION-100);

        for(int iter=0;iter<MAX_ITERATION;iter++){
            MPI_Request requests[2];
            double start_time,start_comp,end_comp;

            if(rank==src){
                start_time=MPI_Wtime();
                MPI_Isend(buffer_send,msg_size,MPI_CHAR,dst,count+iter+100,MPI_COMM_WORLD,&requests[0]);
                MPI_Irecv(buffer_recv,msg_size,MPI_CHAR,dst,count+iter,MPI_COMM_WORLD,&requests[1]);

                start_comp=MPI_Wtime();
                compute_on_host(total_comm_time);
                end_comp=MPI_Wtime();
		
		MPI_Waitall(2,requests,MPI_STATUS_IGNORE);
		//MPI_Wait(&requests[0],MPI_STATUS_IGNORE);
                if(iter>100){
                    total_comp_time+=end_comp-start_comp;
                    total_time+=MPI_Wtime()-start_time;
                }

            }else if(rank==dst){
                MPI_Irecv(buffer_recv,msg_size,MPI_CHAR,src,count+iter+100,MPI_COMM_WORLD,&requests[0]);
                MPI_Isend(buffer_send,msg_size,MPI_CHAR,src,count+iter,MPI_COMM_WORLD,&requests[1]);
                compute_on_host(total_comm_time);
		MPI_Waitall(2,requests,MPI_STATUS_IGNORE);
		//MPI_Wait(&requests[0],MPI_STATUS_IGNORE);
            }

            MPI_Barrier(MPI_COMM_WORLD);
        }

        if(rank==src){
            double overall_t=1e6*total_time/(MAX_ITERATION-100);
            double overall_comp_t=1e6*total_comp_time/(MAX_ITERATION-100);
            double comm_us=1e6*total_comm_time;
            double overlap=100.0*fmax(0.0,fmin(1.0,(overall_comp_t+comm_us-overall_t)/fmin(comm_us,overall_comp_t)));

            printf("%-20d%-20.3f%-20.3f%-20.3f%-20.3f\n",msg_size,overall_comp_t,comm_us,overall_t,overlap);
            fflush(stdout);
        }

        free(buffer_send);
        free(buffer_recv);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    free_arrays();
    MPI_Finalize();
    return 0;
}

