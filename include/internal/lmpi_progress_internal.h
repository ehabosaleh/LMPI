#pragma once

typedef struct{
        char pci_number[32];
        char numa_number[2];
        char numa_cores[5];
        char numa_command[256];
        char cpuset_command[256];
        char driver_path[256];
        char driver_path_numa[256];
        char driver_path_cpuset[256];
}Fabric_info;

void* memory_copy(void *arg);
int dequeue_request_sendrecv();
int dequeue_request_memcpy();
int dequeue_request_one_sided();
int get_progress_rank();
