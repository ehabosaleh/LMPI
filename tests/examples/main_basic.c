#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<lmpi.h>

#define COUNT 16
#define TAG   100

int main(int argc, char **argv){
	int rank, size;
    LMPI_Init(&argc, &argv);

    LMPI_Comm_rank(LMPI_COMM_WORLD, &rank);
    LMPI_Comm_size(LMPI_COMM_WORLD, &size);

    if(size<2){
        fprintf(stderr, "This example requires at least 2 ranks\n");
		LMPI_Finalize();
		return 1;
    }

    int src=0;
    int dst=1;
	if(rank==src) {
		LMPI_Allocation sendbuf=LMPI_Malloc(LMPI_POOL_SEND, COUNT, LMPI_INT);
		int *data=(int *)sendbuf.ptr;
		for(int i= 0;i<COUNT;i++) {
			data[i]=i+ 10;
        }
		LMPI_Request req;
		int flag=0;
		printf("Rank %d sending data to rank %d\n", rank, dst);
		LMPI_Isend(&sendbuf,COUNT,LMPI_INT,dst,TAG,LMPI_COMM_WORLD,&req);
		LMPI_Waitall(1,&req,&flag);
		printf("Rank %d send completed\n", rank);
		LMPI_Free(&sendbuf);
    }
    else if(rank==dst) {
        LMPI_Allocation recvbuf=LMPI_Malloc(LMPI_POOL_RECV, COUNT, LMPI_INT);
        int *data=(int *)recvbuf.ptr;

        memset(data,0,COUNT*sizeof(int));

        LMPI_Request req;
        int flag=0;

        printf("Rank %d receiving data from rank %d\n", rank, src);

        LMPI_Irecv(&recvbuf,COUNT,LMPI_INT,src,TAG,LMPI_COMM_WORLD,&req);

        LMPI_Waitall(1, &req, &flag);

        printf("Rank %d received data:\n", rank);

        for(int i=0;i<COUNT;i++){
            	printf("data[%d] = %d\n",i,data[i]);
        }

        LMPI_Free(&recvbuf);
	}

   	 LMPI_Barrier(LMPI_COMM_WORLD);

    LMPI_Finalize();

    return 0;
}
