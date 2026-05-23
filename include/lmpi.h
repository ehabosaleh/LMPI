#ifndef LMPI_H
#define LMPI_H
#include<mpi.h>
#include"lmpi_config.h"
#include"lmpi_constants.h"
#include"lmpi_types.h"
#include"internal/lmpi_globals.h"
#include"internal/lmpi_debug.h"
#ifdef __cplusplus
extern "C" {
#endif

int LMPI_Init(int *argc,char***argv);



int LMPI_Isend(LMPI_Allocation *data, int count, MPI_Datatype datatype, int dest, int tag,  MPI_Comm comm, LMPI_Request *request);


int LMPI_Irecv(LMPI_Allocation *data, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, LMPI_Request *request);

void *LMPI_Malloc(LMPI_PoolKind pool,int count,MPI_Datatype datatype);
void LMPI_Free(LMPI_Allocation*alloc);

int LMPI_Wait(LMPI_Request *request,int *flag);
int LMPI_Test(LMPI_Request *request,int*flag);
int LMPI_Comm_rank(LMPI_Comm comm, int*rank);
int LMPI_Comm_size(LMPI_Comm comm,int *size);
int LMPI_Barrier(LMPI_Comm comm);
int LMPI_Get_processor_name(char *name, int *resultlen);

int LMPI_Testall(int count, LMPI_Request array_of_requests[],int *flag);
int LMPI_Waitall(int count, LMPI_Request array_of_requests[],int *flag);

int LMPI_Show_queue(int rank);

int LMPI_Finalize();

#ifdef __cplusplus
}
#endif
#endif
