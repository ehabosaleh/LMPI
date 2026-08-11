#include <stdio.h>

#include <lmpi.h>

enum {
	COUNT=8,
	PING_TAG=10,
	PONG_TAG=11 
};

static void fill_values(int *values, int base){
    for(int i=0;i<COUNT;++i){
        values[i] = base + i;
    }
}

static int check_values(const int *values, int base){
    for (int i = 0; i<COUNT;++i){
        if (values[i]!= base+i){
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    int rank;
    int size;
    int completed = 0;

    LMPI_Init(&argc, &argv);
    LMPI_Comm_rank(LMPI_COMM_WORLD, &rank);
    LMPI_Comm_size(LMPI_COMM_WORLD, &size);

    if (size < 2) {
        if (rank == 0) {
            fprintf(stderr,"This example needs two LMPI application ranks; ""launch at least three processes on one node.\n");
        }
        LMPI_Finalize();
        return 1;
    }

    if (rank==0) {
        LMPI_Allocation ping=LMPI_Malloc(LMPI_POOL_SEND, COUNT, LMPI_INT);
        LMPI_Allocation pong=LMPI_Malloc(LMPI_POOL_RECV, COUNT, LMPI_INT);
        LMPI_Request request;

        fill_values((int *)ping.ptr,100);

        LMPI_Isend(&ping,COUNT,LMPI_INT,1,PING_TAG,LMPI_COMM_WORLD, &request);
        LMPI_Wait(&request,&completed);

        LMPI_Irecv(&pong,COUNT, LMPI_INT, 1, PONG_TAG,LMPI_COMM_WORLD, &request);
        LMPI_Wait(&request,&completed);

        printf("rank 0: pong %s\n",check_values((const int *)pong.ptr, 200) ? "verified" : "FAILED");

        LMPI_Free(&ping);
        LMPI_Free(&pong);
    }
    else if(rank==1){
        LMPI_Allocation ping=LMPI_Malloc(LMPI_POOL_RECV, COUNT, LMPI_INT);
        LMPI_Allocation pong=LMPI_Malloc(LMPI_POOL_SEND, COUNT, LMPI_INT);
        LMPI_Request request;

        LMPI_Irecv(&ping,COUNT,LMPI_INT,0,PING_TAG,LMPI_COMM_WORLD,&request);
        LMPI_Wait(&request,&completed);

        printf("rank 1: ping %s\n",check_values((const int *)ping.ptr, 100) ? "verified" : "FAILED");

        fill_values((int *)pong.ptr,200);
        LMPI_Isend(&pong, COUNT, LMPI_INT, 0, PONG_TAG,LMPI_COMM_WORLD, &request);
        LMPI_Wait(&request, &completed);

        LMPI_Free(&ping);
        LMPI_Free(&pong);
    }

    LMPI_Barrier(LMPI_COMM_WORLD);
    return LMPI_Finalize();
}
