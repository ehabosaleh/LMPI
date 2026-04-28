#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include<lmpi.h>

#define MAX_MSG_SIZE (1<<25)
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
    int src=1,dst=0;

    LMPI_Init(NULL,NULL);
    LMPI_Comm_rank(LMPI_COMM_WORLD,&rank);
    LMPI_Comm_size(LMPI_COMM_WORLD,&size);
    LMPI_Get_processor_name(hostname,&len);

    if(size<2){
        if(rank==0)printf("Requires at least 2 MPI processes.\n");
        MPI_Finalize();
        return 1;
    }

    init_arrays();

    int ranks[] = {src, dst};

    if(rank==src){

        printf("%-20s%-20s%-20s%-20s%-20s\n","Size(Bytes)","Compute time(us)","Comm time(us)","Total time(us)","Overlap(%)");
        fflush(stdout);
    
    }

    int count=0;
    
    if(rank==src||rank==dst)
    	for(long  msg_size=MIN_MSG_SIZE;msg_size<=MAX_MSG_SIZE;msg_size*=2){
		count++;
	    	char*buffer_send=LMPI_Register(msg_size,MPI_CHAR);
 		char*buffer_recv=NULL;

        	for(int i=0;i<msg_size;i++)
			buffer_send[i]='a';

        	double total_comm_time=0;
        	double total_comp_time=0;
        	double total_time=0;

        	for(int iter=0;iter<MAX_ITERATION;iter++){
            		LMPI_Request request[2];
            		int flag=0;
			double start_time,end_time;

            		if(rank==src){
                		start_time=MPI_Wtime();
                
				LMPI_Isend(buffer_send, msg_size, MPI_CHAR, dst, count+iter+100, LMPI_COMM_WORLD, &request[0]);
                		LMPI_Irecv((void**)&buffer_recv,msg_size,MPI_CHAR,dst,iter,LMPI_COMM_WORLD,&request[1]);	
				
				//LMPI_Wait( &request[0],&flag);
				LMPI_Waitall(2,request,&flag);
                		if(iter>100)
					total_comm_time+=MPI_Wtime()-start_time;
			}
	    		else if(rank==dst){
            			start_time=MPI_Wtime();
				LMPI_Irecv((void**)&buffer_recv,msg_size,MPI_CHAR,src,count+iter+100,LMPI_COMM_WORLD,&request[0]);
                		LMPI_Isend(buffer_send, msg_size, MPI_CHAR,src, iter, LMPI_COMM_WORLD, &request[1]);
				
				//LMPI_Wait( &request[0],&flag);
				LMPI_Waitall(2,request,&flag);
				
				if(iter>100)
                                        total_comm_time+=MPI_Wtime()-start_time;

	    		}

        	}

        	total_comm_time/=(MAX_ITERATION-100);

        	for(int iter=0;iter<MAX_ITERATION;iter++){
            		LMPI_Request request[2];
            		double start_time,start_comp,end_comp;
			int flag=0;
            		if(rank==src){
                		start_time=MPI_Wtime();
				LMPI_Isend(buffer_send, msg_size, MPI_CHAR, dst, count+iter+100, LMPI_COMM_WORLD, &request[0]);
                		LMPI_Irecv((void**)&buffer_recv,msg_size,MPI_CHAR,dst,iter,LMPI_COMM_WORLD,&request[1]);

				start_comp=MPI_Wtime();
                		compute_on_host(total_comm_time);
                		end_comp=MPI_Wtime();
				//LMPI_Wait( &request[0],&flag);

				LMPI_Waitall(2,request,&flag);	
				
				if(iter>100){
					 total_time+=MPI_Wtime()-start_time;
					 total_comp_time+=end_comp-start_comp;
                		}
				/*
				double time_1=MPI_Wtime();
				LMPI_Waitall(2,request,&flag);
				double time_2=MPI_Wtime()-time_1;
				printf("time iin us %f\n",time_2*1e6);
            			*/
			}else if(rank==dst){
				LMPI_Irecv((void**)&buffer_recv,msg_size,MPI_CHAR,src,count+iter+100,LMPI_COMM_WORLD,&request[0]);
                        	LMPI_Isend(buffer_send, msg_size, MPI_CHAR,src, iter, LMPI_COMM_WORLD, &request[1]);
				compute_on_host(total_comm_time);

				//LMPI_Wait( &request[0],&flag);
				LMPI_Waitall(2,request,&flag);		
			}
	
        }

        if(rank==src){
            double overall_t=1e6*total_time/(MAX_ITERATION-100);
            double overall_comp_t=1e6*total_comp_time/(MAX_ITERATION-100);
            double comm_us=1e6*total_comm_time;
            double overlap=100.0*fmax(0.0,fmin(1.0,(overall_comp_t+comm_us-overall_t)/fmin(comm_us,overall_comp_t)));

            printf("%-20lu%-20.3f%-20.3f%-20.3f%-20.3f\n",msg_size,overall_comp_t,comm_us,overall_t,overlap);
            fflush(stdout);

        }

    }

    LMPI_Barrier(LMPI_COMM_WORLD);
	
    LMPI_Finalize();
    return 0;
}
