# LMPI
LMPI is a custom communication layer built on top of MPI, designed to provide proxy-based communication progress and data transfer for both inter-node and intra-node two-sided communication. It eliminates the need for a per-process progress thread, which would otherwise compete for CPU resources and degrade overall performance, particularly in compute-bound applications.

---

## API
Users can access all library functions declared in `lmpi.h`. Their signatures follow the standard MPI conventions as defined in the official MPI documentation, allowing existing MPI applications written with `mpi.h` to be adapted with minimal effort.

The primary library functions are as follows:
* `int LMPI_Init(int *argc,char***argv)`:
	It initializes the LMPI runtime by setting up a **proxy-based communication model** with a dedicated progress rank. It performs the following:

		 - Splits processes into **node-local communicators** and assigns **working ranks** and a **progress rank**.
         - Allocates **shared memory regions** for send/receive buffers.
		 - Initializes a **shared request queue** for communication operations.
		 - Maps memory so the progress rank can access all local buffers.
		 - Starts the **progress engine**, which continuously processes requests and performs data transfers.

* `void* LMPI_Register(int count, LMPI_Datatype datatype)` allocates a buffer inside the **pre-allocated shared memory send segment** of a working rank. The corresponding funtionality of the receiver is embeded into `LMPI_Irecv`.

 
* `int LMPI_Isend(void*data, int count, MPI_Datatype datatype, int dest, int tag,  MPI_Comm comm, LMPI_Request *request)` initiates a non-blocking send by creating a communication request and inserting it into the shared request queue managed by the LMPI runtime.The routine determines the memory offset of the user buffer within the shared send segment and embeds this information, along with metadata such as source, destination, tag, communicator, datatype, and count, into an `LMPI_Request` structure. This request is then marked as ready and pending, and enqueued into the shared queue.
Once the request is submitted, `LMPI_Isend` returns immediately without performing any data transfer. The actual communication is handled asynchronously by 	the progress rank, which reads the request from the queue and executes the transfer either via direct memory copy for intra-node communication or via MPI 		operations for inter-node communication.


* `int LMPI_Irecv(void **data, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, LMPI_Request *request)` posts a non-blocking receive request by preparing a receive buffer in shared memory and registering the corresponding metadata in the shared request queue.

The routine expects a pointer to a buffer pointer (`void **data`). Internally, a memory region is allocated from the calling rank's shared receive segment, and the pointer to this region is returned through `data`. The function computes the offset of this buffer within the shared segment and records it in an `LMPI_Request` structure along with the source rank, tag, communicator, datatype, and element count.


* `LMPI_Wait(LMPI_Request *request,int *flag)` and `int LMPI_Waitall(int count, LMPI_Request array_of_requests[],int *flag)` are the completion routines that ensure previously issued non-blocking operations have finished.


	- `LMPI_Wait` operates on a single `LMPI_Request`continuously checks the status of the corresponding request entry in the shared queue until the progress rank marks it as completed. Once completion is detected, the routine allows safe access to the associated buffer and can optionally update the provided flag.

	- `LMPI_Waitall` extends this behavior to an array of requests. It monitors all requests in the array and returns only when every request has reached the completed state. The function ensures that all associated data transfers have been finalized before proceeding.

In both cases, these routines do not perform communication themselves. Instead, they rely on the progress rank, which asynchronously processes the requests and updates their status. The wait functions simply observe this state transition and provide synchronization at the application level.

* `int LMPI_Test(LMPI_Request *request,int*flag)` and `int LMPI_Testall(int count, LMPI_Request array_of_requests[],int *flag)`:
They provide non-blocking completion checks for previously issued communication requests. 

	- `LMPI_Test` inspects the status of a single `LMPI_Request` in the shared queue. It checks whether the progress rank has marked the request as completed. If so, it sets the flag accordingly; otherwise, it returns immediately without blocking, allowing the application to continue execution.

	- `LMPI_Testall` performs the same operation over an array of requests. It verifies whether all requests have reached the completed state. The flag is set only if every request is complete; otherwise, the function returns immediately.

Neither function performs communication or advances progress. They rely entirely on the progress rank, which asynchronously processes requests and updates their status. These routines simply query that state without blocking.


* `LMPI_Finalize()` shuts down the LMPI runtime, releases all resources initialized during `LMPI_Init`, and terminates the progress rank.

* `int LMPI_Comm_rank(LMPI_Comm comm, int*rank)`, `int LMPI_Comm_size(LMPI_Comm comm,int *size)`, `int LMPI_Barrier(LMPI_Comm comm)`,  and `int LMPI_Get_processor_name(char *name, int *resultlen)` work exactly as their corresponding calls in MPI. It is worth to mention that `LMPI_COMM_WORLD`, and all its sub-communicators, contain only the working rank. In other words, after calling `LMPI_Comm_size(LMPI_Comm comm, int *size)`, the returned `size` corresponds to the number of working ranks, which is equal to the total number of processes minus one (excluding the dedicated progress rank). 

### Minimal C Example (2 Processes)

```c
#include <stdio.h>
#include <lmpi.h>

int main(void){
	
	LMPI_Init(NULL, NULL);
    	int rank, size;
    	LMPI_Comm_rank(LMPI_COMM_WORLD, &rank);
    	LMPI_Comm_size(LMPI_COMM_WORLD, &size);
	
	LMPI_Request req;
	int flag = 0;
	
    	if (rank == 0){
        	int *send_buf=(int*)LMPI_Register(1,LMPI_INT);
        	*send_buf=42;

        	LMPI_Isend(send_buf, 1, LMPI_INT, 1, 0, LMPI_COMM_WORLD, &req);
		/*can overlap with CPU computation here*/
		LMPI_Wait(&req, &flag);
		}else{
        	int *recv_buf=NULL;
        	LMPI_Irecv((void**)&recv_buf, 0, LMPI_INT, 0, 0, LMPI_COMM_WORLD, &req);
        	/*can overlap with CPU computation here*/
			LMPI_Wait(&req, &flag);

        	printf("Received value = %d\n", *recv_buf);
    	}
	
		LMPI_Barrier(LMPI_COMM_WORLD);
    	LMPI_Finalize();
    	return 0;
}

```

