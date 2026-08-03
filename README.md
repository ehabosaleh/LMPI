# LMPI
LMPI is a custom communication layer built on top of MPI, designed to provide proxy-based communication progress and data transfer for both inter-node and intra-node two-sided communication. It eliminates the need for a per-process progress thread, which would otherwise compete for CPU resources and degrade overall performance, particularly in compute-bound applications.

---

## API (Version 2.0.0)
Users can access all library functions declared in `lmpi.h`. Their signatures follow the standard MPI conventions as defined in the official MPI documentation, allowing existing MPI applications written with `mpi.h` to be adapted with minimal effort.

The primary library functions are as follows:
### `int LMPI_Init(int *argc,char**argv)`:

It initializes the LMPI runtime by setting up a **proxy-based communication model** with a dedicated progress rank. It performs the following:
* Splits processes into **node-local communicators** and assigns **working ranks** and a **progress rank**.
* Allocates **shared memory regions** for send/receive buffers.
* Initializes a **shared request queue** for communication operations.
* Maps memory so the progress rank can access all local buffers.
* Starts the **progress engine**, which continuously processes requests and performs data transfers.

### `LMPI_Allocation LMPI_Malloc(LMPI_PoolKind pool, int count, LMPI_Datatype datatype)`:

This routine allocates a buffer from one of process's internal memory pools. The target pool is selected through the `pool` argument, which can be either `LMPI_POOL_SEND` or `LMPI_POOL_RECV`. The allocated buffer size is computed as `count` multiplied by the size of `datatype`, as obtained from the corresponding LMPI/MPI datatype-size routine.

It returns an object of type `LMPI_Allocation`, which stores both the allocated address and the metadata required to manage the allocation. This metadata typically includes:
* `ptr`: the base address of the allocated buffer.
* `pool`: the memory pool from which the buffer was allocated, i.e., send or receive pool.
* `offset`: the offset of the allocated buffer relative to the base address of the corresponding memory pool.
* `size`: the total size, in bytes, of the allocated buffer.

This metadata is later used by routines such as `LMPI_Isend`, `LMPI_Irecv`, and `LMPI_Free`. For example, the offset can be used to describe the buffer location relative to the shared-memory segment, while `LMPI_Free` can use the stored pool, offset, and size to determine which allocation should be released or whether the memory pool can safely be rolled back for reuse.

### `void LMPI_Free(LMPI_Allocation *alloc)`:

This routine releases a previously allocated LMPI buffer and makes its memory region available for reuse. The allocation to be released is identified through the metadata stored in the `LMPI_Allocation` object, including the memory pool type, the buffer offset, and the allocation size.

`LMPI_Free` can safely reclaim memory only when allocations are freed in reverse order of allocation within the same pool. For example, if `buf1` is allocated before `buf2` from the same memory pool, then `buf2` must be freed before `buf1`. This allows the allocator to roll back the corresponding pool offset safely.

### `int LMPI_Isend(LMPI_Allocation *data, int count, LMPI_Datatype datatype, int dest, int tag,  LMPI_Comm comm, LMPI_Request *request)`:

It initiates a non-blocking send by creating a communication request and inserting it into the shared request queue managed by the LMPI runtime. The routine determines the memory offset of the user buffer within the shared send segment and embeds this information, along with metadata such as source, destination, tag, communicator, datatype, and count, into an `LMPI_Request` structure. This request is then marked as ready and pending, and enqueued into the shared queue.

Once the request is submitted, `LMPI_Isend` returns immediately without performing any data transfer. The actual communication is handled asynchronously by 	the progress rank, which reads the request from the queue and executes the transfer either via direct memory copy for intra-node communication or via MPI send/recv for inter-node communication.


### `int LMPI_Irecv(LMPI_Allocation *data, int count, LMPI_Datatype datatype, int source, int tag, LMPI_Comm comm, LMPI_Request *request)`:

It posts a non-blocking receive request by registering the corresponding metadata in the shared request queue. Once the request is submitted, `LMPI_Irecv` returns immediately without performing any data transfer. The actual communication is handled asynchronously by the progress rank, which reads the request from the queue and executes the transfer either via direct memory copy for intra-node communication or via MPI send/recv for inter-node communication.

### `int LMPI_Wait(LMPI_Request *request,int *flag)` and `int LMPI_Waitall(int count, LMPI_Request array_of_requests[],int *flag)`:

They are are the completion routines that ensure previously issued non-blocking operations have finished:
* `LMPI_Wait` operates on a single `LMPI_Request`continuously checks the status of the corresponding request entry in the shared queue until the progress rank marks it as completed. Once completion is detected, the routine allows safe access to the associated buffer and can optionally update the provided flag.
* `LMPI_Waitall` extends this behavior to an array of requests. It monitors all requests in the array and returns only when every request has reached the completed state. The function ensures that all associated data transfers have been finalized before proceeding.

In both cases, these routines do not perform communication themselves. Instead, they rely on the progress rank, which asynchronously processes the requests and updates their status. The wait functions simply observe this state transition and provide synchronization at the application level.

### `int LMPI_Test(LMPI_Request *request,int*flag)` and `int LMPI_Testall(int count, LMPI_Request array_of_requests[],int *flag)`:
They provide non-blocking completion checks for previously issued communication requests. 
* `LMPI_Test` inspects the status of a single `LMPI_Request` in the shared queue. It checks whether the progress rank has marked the request as completed. If so, it sets the flag accordingly; otherwise, it returns immediately without blocking, allowing the application to continue execution.
* `LMPI_Testall` performs the same operation over an array of requests. It verifies whether all requests have reached the completed state. The flag is set only if every request is complete; otherwise, the function returns immediately.

Neither function performs communication or advances progress. They rely entirely on the progress rank, which asynchronously processes requests and updates their status. These routines simply query that state without blocking.


### `int LMPI_Finalize()`:
 It shuts down the LMPI runtime, releases all resources initialized during `LMPI_Init`, and terminates the progress rank.

### `int LMPI_Comm_rank(LMPI_Comm comm, int*rank)`, `int LMPI_Comm_size(LMPI_Comm comm,int *size)`, `int LMPI_Barrier(LMPI_Comm comm)`,  and `int LMPI_Get_processor_name(char *name, int *resultlen)`:

They work exactly as their corresponding calls in MPI. It is worth to mention that `LMPI_COMM_WORLD`, and all its sub-communicators, contain only the working 	rank. In other words, after calling `LMPI_Comm_size(LMPI_Comm comm, int *size)`, the returned `size` corresponds to the number of working ranks, which is equal to the total number of processes minus one (excluding the dedicated progress rank). 

### Minimal C Example (2 Processes)

```c
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

	if(rank==src){
		LMPI_Allocation sendbuf=LMPI_Malloc(LMPI_POOL_SEND, COUNT, LMPI_INT);
		int *data=(int *)sendbuf.ptr;
		for(int i= 0;i<COUNT;i++){
			data[i]=i+ 10;
		}
		LMPI_Request req;
		int flag=0;
		printf("Rank %d sending data to rank %d\n", rank, dst);

		LMPI_Isend(&sendbuf,COUNT,LMPI_INT,dst,TAG,LMPI_COMM_WORLD,&req);
		/*
			can overlap with computation here
		*/
		LMPI_Waitall(1,&req,&flag);

		printf("Rank %d send completed\n", rank);
		LMPI_Free(&sendbuf);
	
	}
	else if(rank==dst)
	{

		LMPI_Allocation recvbuf=LMPI_Malloc(LMPI_POOL_RECV, COUNT, LMPI_INT);
		int *data=(int *)recvbuf.ptr;
		memset(data,0,COUNT*sizeof(int));
		LMPI_Request req;
		int flag=0;
		printf("Rank %d receiving data from rank %d\n", rank, src);
	
		LMPI_Irecv(&recvbuf,COUNT,LMPI_INT,src,TAG,LMPI_COMM_WORLD,&req);
		/*
			can overlap with computation here
		*/
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
```
---
## Build and Install
You can simple run the bash script `configure` and pass the path where you want to install the libray and the header file:

```bash
Usage: ./configure [--prefix=INSTALLATION_PATH]
```
**Or**

```bash
cmake -S . -B build
cmake --build build
```
and then nstall:

```bash
cmake --install build
```
the libray will be installed in the default path `/usr/local/lmpi`

---

## Use in Other Projects

With CMake:

```cmake
find_package(LMPI REQUIRED)
target_link_libraries(app PRIVATE LMPI::LMPI)
```

## Preferred citation 
Experiments and results presented in the following paper:
```bibtex
@INPROCEEDINGS{11613056,
  author={Saleh, Ehab and Raoofy, Amir and Mijakovic, Robert and Belbeisi, Ahmad Moh'd Saleh A. and Weidendorfer, Josef},
  booktitle={2026 25th International Symposium on Parallel and Distributed Computing (ISPDC)}, 
  title={A Dedicated CPU Core for MPI Progress: Towards Improved Overlap in Non-Blocking Two-Sided Communication}, 
  year={2026},
  volume={},
  number={},
  pages={1-10},
  keywords={Ranking (statistics);Memory;Algorithms;Timing;Receivers;Bandwidth;Central Processing Unit;Tagging;Data transfer;Energy;HPC;MPI;Asynchronous Progress;Two-sided},
  doi={10.1109/ISPDC69862.2026.00010}}



