#include "lmpi.h"

int LMPI_Comm_size(LMPI_Comm comm,int*size){ 
	return MPI_Comm_size(comm,size); 
}

int LMPI_Comm_rank(LMPI_Comm comm, int*rank){ 
	return MPI_Comm_rank(comm,rank); 
}

int LMPI_Barrier(LMPI_Comm comm){ 
	return MPI_Barrier(comm); 
}

int LMPI_Get_processor_name(char *name, int *resultlen){ 
	return MPI_Get_processor_name(name,resultlen); 

}


