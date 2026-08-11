#include <stdio.h>

#include <lmpi.h>

int main(int argc, char **argv){
    int rank;
    int size;
    int name_length;
    char processor_name[LMPI_MAX_PROCESSOR_NAME];
    if (LMPI_Init(&argc, &argv) != LMPI_SUCCESS) {
        fprintf(stderr, "LMPI_Init failed\n");
        return 1;
    }

    LMPI_Comm_rank(LMPI_COMM_WORLD, &rank);
    LMPI_Comm_size(LMPI_COMM_WORLD, &size);
    LMPI_Get_processor_name(processor_name, &name_length);
    printf("LMPI application rank %d of %d is running on %s\n", rank, size, processor_name);

    LMPI_Barrier(LMPI_COMM_WORLD);
    return LMPI_Finalize();
}
