#include<mpi.h>
#include<stdio.h>
#include<stdlib.h>
#define NITER 1000
#define SKIP 100
int main(int argc,char*argv[]){
    int rank,size;
    MPI_Win win;
    char*winbuf=NULL;
    MPI_Request req;
    MPI_Status status;
    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);
    if(size!=2){
        if(rank==0)printf("Runwithexactly2processes.\n");
        MPI_Finalize();
        return 0;
    }
    MPI_Win_allocate(sizeof(char)*(1<<30),1,MPI_INFO_NULL,MPI_COMM_WORLD,&winbuf,&win);
    
    if(rank==0)
	    printf("%10s%15s%15s%15s%15s\n","Bytes","MPI_Send(us)","MPI_Rget(us)","MPI_Send(MB/s)","MPI_Rget(MB/s)");
    
    for(long msg_size=1;msg_size<=(1<<30);msg_size*=2){
        double t_send=0,t_rget=0;
        double t_0=0,t_1=0;
	double bandwidth_send=0,bandwidth_rget=0;

	MPI_Barrier(MPI_COMM_WORLD);
        
	
        for(int i=0;i<NITER;i++){
            if(rank==0){
		t_0=MPI_Wtime();
                MPI_Send(winbuf,msg_size,MPI_CHAR,1,0,MPI_COMM_WORLD);
		if(i>SKIP)
			t_1+=MPI_Wtime()-t_0;
            }else{
                MPI_Recv(winbuf,msg_size,MPI_CHAR,0,0,MPI_COMM_WORLD,&status);
            }
        }

	t_send=(t_1)*1e6/(NITER-SKIP);
	bandwidth_send=(((NITER-SKIP)*msg_size)/t_1);

	MPI_Barrier(MPI_COMM_WORLD);
        

        t_0=0;
	t_1=0;

	for(int i=0;i<NITER;i++){
            if(rank==0){
                MPI_Win_lock(MPI_LOCK_SHARED,1,0,win);
                t_0=MPI_Wtime();
		
		MPI_Rget(winbuf,msg_size,MPI_CHAR,1,0,msg_size,MPI_CHAR,win,&req);
                MPI_Wait(&req,&status);
		
		if(i>SKIP)
                	t_1+=MPI_Wtime()-t_0;
		
		MPI_Win_unlock(1,win);
            }else{
                MPI_Win_lock(MPI_LOCK_SHARED,0,0,win);
                MPI_Win_unlock(0,win);
            }
        }
        
	t_rget=t_1*1e6/(NITER-SKIP);
        bandwidth_rget=(((NITER-SKIP)*msg_size)/t_1);
	if(rank==0)
		printf("%10d%15.3f%15.3f%15.3f%15.3f\n",msg_size,t_send,t_rget,bandwidth_send/(1024*1024),bandwidth_rget/(1024*1024));
    }
    MPI_Win_free(&win);
    MPI_Finalize();
    return 0;
}

