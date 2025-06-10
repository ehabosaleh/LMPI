#ifndef LMPI_H
#define LMPI_H


#define MAX_TAG_LENGTH 100000
#define MAX_COMM_SIZE 100000

#define COMPLETION_QUEUE_LENGTH 1000
#define CONTROL_QUEUE_LENGTH 1000
#define PENDING 'p'
#define COMPLETED 'c'

/*
typedef struct{
        size_t size;
        const char *name;
} LMPI_Datatype;

typedef struct{
        long LMPI_SOURCE;
        long LMPI_TAG;
        char * LMPI_ERROR;

} LMPI_Status;

typedef struct{
        long rank;
        long size;
        int context_id;
} LMPI_Comm;

typedef struct{
        LMPI_Datatype LMPI_BYTE;
}LMPI_Request;


typedef struct {
        int rank[NUM_PROGRESS_CORES];
        int size;
} LMPI_LOCAL_PROGRESS_ENGINE;
*/
unsigned int num_progress_cores=1;/*Default*/
extern unsigned int *lmpi_local_progress_cores;

typedef MPI_Datatype LMPI_Datatype;
typedef MPI_COMM LMPI_COMM;
typedef MPI_Request LMPI_Request;
typedef MPI_Status LMPI_Status;

extern const LMPI_Datatype LMPI_INI;
extern const LMPI_Datatype LMPI_FLOAT;
extern const LMPI_Datatype LMPI_DOUBLE;
extern const LMPI_Datatype LMPI_CHAR;
extern const LMPI_Datatype LMPI_BYTE;
extern const LMPI_Datatype LMPI_SHORT;
extern const LMPI_Datatype LMPI_LONG;
extern const LMPI_Comm LMPI_COMM_WORLD;
extern const LMPI_Comm LMPI_COMM_SHARED;

int LMPI_Init(int *argc,char**argv);
int LMPI_Finalize();

/*Functions defined in external source files*/
int LMPI_Send(const void *buf, int count, mpi_datatype_t datatype, int dest,int tag, LMPI_Comm comm);
int LMPI_Recv(const void *buf, int count, mpi_datatype_t datatype, int src,int tag, LMPI_Comm comm,LMPI_Status *status);
int LMPI_Wait(LMPI_Request *request, LMPI_Status *status);
int LMPI_Isend(const void *buf, int count, mpi_datatype_t datatype, int dest,int tag, LMPI_Comm comm, LMPI_Request *request);
int LMPI_Irecv(void *buf, int count, LMPI_Datatype datatype,int source, int tag, LMPI_Comm comm, LMPI_Request *request);
int LMPI_Test(LMPI_Request *request, int *flag, LMPI_Status *status);
int LMPI_Barrier(LMPI_Comm comm);
int LMPI_Ibarrier(LMPI_Comm comm, LMPI_Request *request);


/*Functions defined in lmpi.c*/
int LMPI_processor_name(char *name, int *resultlen);
int LMPI_Set_num_progress_cores( int max_progress_cores);
int *LMPI_Get_local_progress_rank();
int LMPI_Set_local_progress_rank();

/*Pointers to MPI functions*/
int(*LMPI_Comm_rank)(MPI_Comm , int *)=MPI_Comm_rank;
int(*LMPI_Barrier)(MPI_Comm)=MPI_Barrier;
int(*LMPI_Comm_size)(LMPI_Comm comm, int *size)=MPI_Comm_size;
int(*LMPI_Comm_rank)(LMPI_Comm comm, int *rank)=MPI_Comm_rank;


#endif
