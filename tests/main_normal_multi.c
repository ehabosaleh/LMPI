#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include<time.h>
#include<mpi.h>

#define TOL 1e-5
#ifndef MAX_ITER
#define MAX_ITER 200
#endif

#ifndef SKIP
#define SKIP 100
#endif

#ifndef MAX_MESSAGE_SIZE
#define MAX_MESSAGE_SIZE (1 << 28)
#endif

#ifndef MIN_MESSAGE_SIZE
#define MIN_MESSAGE_SIZE  (1 << 20)
#endif

#define DIM 50

static float **a, *x, *y;

void update_grid(double **u, double **u_new, int rows, int cols) {
    for (int i=2;i< rows-2;i++) {
        for (int j=2; j<cols-2;j++) {
            u_new[i][j]=0.25*(u[i+1][j]+u[i-1][j]+u[i][j+1]+u[i][j-1]);
        }
    }
}


void update_grid_hallo(double **u, double **u_new, int rows, int cols) {
    	int i=1;
        for (int j=1;j<cols-1;j++){
            u_new[i][j]=0.25*(u[i+1][j]+u[i-1][j]+u[i][j+1]+u[i][j-1]);
        }
	 i=rows-2;
        for (int j=1;j<cols-1;j++) {
            u_new[i][j]=0.25*(u[i+1][j]+u[i-1][j]+u[i][j+1]+u[i][j-1]);
        }
	int j=1;
        for (int i=1;i<rows-1;i++) {
            u_new[i][j]=0.25*(u[i+1][j]+u[i-1][j]+u[i][j+1]+u[i][j-1]);
        }
	j=cols-2;
        for (int i=1;i<rows-1;i++) {
            u_new[i][j]=0.25*(u[i+1][j]+u[i-1][j]+u[i][j+1]+u[i][j-1]);
        }

    }




double calc_diff(double **u, double **u_new, int rows, int cols) {
    double max_diff = 0;
    for (int i=1;i <rows-1;i++) {
        for (int j=1;j<cols-1;j++) {
            double diff=fabs(u_new[i][j]-u[i][j]);
            if (diff>max_diff) {
                max_diff=diff;
            }
        }
    }
    return max_diff;
}


void init_arrays()
{

    int i = 0, j = 0;

    a = (float **)malloc(DIM * sizeof(float *));

    for (i = 0; i < DIM; i++) {
        a[i] = (float *)malloc(DIM * sizeof(float));
    }

    x = (float *)malloc(DIM * sizeof(float));
    y = (float *)malloc(DIM * sizeof(float));

    for (i = 0; i < DIM; i++) {
        x[i] = y[i] = 1.0f;
        for (j = 0; j < DIM; j++) {
            a[i][j] = 2.0f;
        }
    }
}

static void compute_on_host(double latency){
        int i = 0, j = 0;
        double tcomp_all=0;
        double ccomp_start=0,ccomp_total=0;

        while(ccomp_total<latency)
        {
                ccomp_start=MPI_Wtime();
                for (i = 0; i < DIM; i++)
                        for (j = 0; j < DIM; j++)
                                x[i] = x[i] + a[i][j]*a[j][i] + y[j];

                ccomp_total+=MPI_Wtime()-ccomp_start;

                //tcomp_all=((double)ccomp_total)/CLOCKS_PER_SEC;
        }
}
int main(int argc, char *argv[]) {
    int rank, size,len;
    int dims[2], coords[2];
    int left, right, top, bottom;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    char process_name[MPI_MAX_PROCESSOR_NAME];
    MPI_Get_processor_name(process_name,&len);
	

    int sqrt_size = (int)sqrt((double)size);
    if (sqrt_size * sqrt_size != size) {
        if (rank == 0) {
            fprintf(stderr, "Number of processes must be a perfect square\n");
        }
        MPI_Finalize();
        return -1;
    }
    dims[0]=dims[1]=sqrt_size;

    coords[0] = rank/sqrt_size;
    coords[1] = rank%sqrt_size;

    left=(coords[1]==0) ? MPI_PROC_NULL : rank - 1;
    right=(coords[1]==dims[1]-1) ? MPI_PROC_NULL : rank + 1;
    top = (coords[0] == 0) ? MPI_PROC_NULL : rank - sqrt_size;
    bottom = (coords[0] == dims[0] - 1) ? MPI_PROC_NULL : rank + sqrt_size;

  //printf("Arrays initiated sucessfully\n");
  init_arrays();
   int iter;
   double test_total,tcomp_total=0,overall_time=0,wait_total,init_total,comm_time,test_time,t_start,t_stop,init_time,tcomp,wait_time,timer;
   double t_pure=0,t_ovrl=0,t_pure_total=0,t_ovrl_total=0;
   MPI_Request reqs[size];

if(rank==0)
                        printf("%-20s%-20s%-20s%-20s%-20s\n","Size (Bytes)","Communication(us)","Computation(us)","Overall","Overlapping %");

 for(long local_N=MIN_MESSAGE_SIZE;local_N<=MAX_MESSAGE_SIZE;local_N*=2)
        {


                char *u_0=(char *)malloc(local_N * sizeof(char *));
                char *u_1=(char *)malloc(local_N * sizeof(char *));
                char *u_2=(char *)malloc(local_N * sizeof(char *));
                char *u_3=(char *)malloc(local_N * sizeof(char *));

                char *u_left=(char *)malloc(local_N * sizeof(char *));
                char *u_right=(char *)malloc(local_N * sizeof(char *));
                char *u_top=(char *)malloc(local_N * sizeof(char *));
                char *u_bottom=(char *)malloc(local_N * sizeof(char *));

                for(int i=0;i< local_N;i++)
                {
                                u_0[i] = 'a';
                                u_1[i] = 'b';
                                u_2[i] = 'c';
                                u_3[i] = 'd';

                                u_left[i] = 'f';
                                u_right[i] = 'g';
                                u_top[i] = 'h';
                                u_bottom[i] = 'i';

                }

    	for (iter = 0; iter < MAX_ITER; iter++) {

		int req_count = 0;
		init_time=MPI_Wtime();

      		if (left != MPI_PROC_NULL) {
                	MPI_Isend(u_0, local_N, MPI_CHAR, left, 0, MPI_COMM_WORLD, &reqs[req_count++]);
            		MPI_Irecv(u_left, local_N, MPI_CHAR, left, 0, MPI_COMM_WORLD, &reqs[req_count++]);
        	}


      		if (right != MPI_PROC_NULL) {
            		MPI_Isend(u_1, local_N, MPI_CHAR, right, 0, MPI_COMM_WORLD, &reqs[req_count++]);
            		MPI_Irecv(u_right, local_N, MPI_CHAR, right, 0, MPI_COMM_WORLD, &reqs[req_count++]);
            	}



     		if (top != MPI_PROC_NULL) {
            		MPI_Isend(u_2, local_N, MPI_CHAR, top, 0, MPI_COMM_WORLD, &reqs[req_count++]);
            		MPI_Irecv(u_top, local_N, MPI_CHAR, top, 0, MPI_COMM_WORLD, &reqs[req_count++]);
        	}

		if (bottom != MPI_PROC_NULL) {
            		MPI_Isend(u_3, local_N, MPI_CHAR, bottom, 0, MPI_COMM_WORLD, &reqs[req_count++]);
            		MPI_Irecv(u_bottom, local_N, MPI_CHAR, bottom, 0, MPI_COMM_WORLD, &reqs[req_count++]);
        	}

		MPI_Waitall(req_count, reqs, MPI_STATUSES_IGNORE);

		if(iter>SKIP)
                	t_pure+=MPI_Wtime()-init_time;


		//printf("host %s rank %d time %f \n",process_name,rank,(MPI_Wtime()-init_time)*1e6);
		MPI_Barrier(MPI_COMM_WORLD);
   	}


	MPI_Barrier(MPI_COMM_WORLD);

	 t_pure_total=1e6*t_pure/(MAX_ITER-SKIP);

   for (iter = 0; iter < MAX_ITER; iter++) {
        int req_count = 0;
	init_time=MPI_Wtime();

	 if (left != MPI_PROC_NULL) {
                MPI_Isend(u_0, local_N, MPI_CHAR, left, 0, MPI_COMM_WORLD, &reqs[req_count++]);
                MPI_Irecv(u_left, local_N, MPI_CHAR, left, 0, MPI_COMM_WORLD, &reqs[req_count++]);
        }


      if (right != MPI_PROC_NULL) {
            MPI_Isend(u_1, local_N, MPI_CHAR, right, 0, MPI_COMM_WORLD, &reqs[req_count++]);
            MPI_Irecv(u_right, local_N, MPI_CHAR, right, 0, MPI_COMM_WORLD, &reqs[req_count++]);
            }

        if (top != MPI_PROC_NULL) {
            MPI_Isend(u_2, local_N, MPI_CHAR, top, 0, MPI_COMM_WORLD, &reqs[req_count++]);
            MPI_Irecv(u_top, local_N, MPI_CHAR, top, 0, MPI_COMM_WORLD, &reqs[req_count++]);
        }

        if (bottom != MPI_PROC_NULL) {
            MPI_Isend(u_3, local_N, MPI_CHAR, bottom, 0, MPI_COMM_WORLD, &reqs[req_count++]);
            MPI_Irecv(u_bottom, local_N, MPI_CHAR, bottom, 0, MPI_COMM_WORLD, &reqs[req_count++]);
        }



	tcomp=MPI_Wtime();
       	compute_on_host(t_pure_total/1e6);
	tcomp=MPI_Wtime()-tcomp;

	MPI_Waitall(req_count, reqs, MPI_STATUSES_IGNORE);


	t_ovrl=MPI_Wtime()-init_time;

	if(iter>SKIP)
	{
        	tcomp_total += tcomp;
		t_ovrl_total+=t_ovrl;
	}
   MPI_Barrier(MPI_COMM_WORLD);
   }

    	tcomp_total  = (tcomp_total * 1e6)/(MAX_ITER-SKIP);
     	t_ovrl_total=t_ovrl_total*1e6/(MAX_ITER-SKIP);
	t_pure_total=1e6*t_pure/(MAX_ITER-SKIP);

     	double overlap = 100* fmax(0,fmin(1,(t_pure_total+tcomp_total-t_ovrl_total)/fmin(t_pure_total,tcomp_total)));
     	double overlap_avr=0,solve_time_avr;

     	MPI_Reduce(&overlap,&overlap_avr,1,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);

    	double t_ovrl_total0,t_pure_total0,tcomp_total0;
    	MPI_Reduce(&t_ovrl_total,&t_ovrl_total0,1,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
    	MPI_Reduce(&tcomp_total,&tcomp_total0,1,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
    	MPI_Reduce(&t_pure_total,&t_pure_total0,1,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);

    if (rank == 0) {
	    t_pure_total0 = t_pure_total0/size;
	    tcomp_total0 = tcomp_total0/size;
	    t_ovrl_total0 = t_ovrl_total0/size;
	    overlap_avr=overlap_avr/size;
	    //double overlap = 100* fmax(0,fmin(1,(t_pure_total0+tcomp_total0-t_ovrl_total0)/fmin(t_pure_total0,tcomp_total0)));

	    //printf("overalpping ratio: %f \n",overlap_avr);
	    //printf("overlapping time: %f \n",t_ovrl_total0);
	    //printf("pure communication time: %f \n",t_pure_total0);
	    //printf("computation time: %f \n",tcomp_total0);
	    printf("%-20ld%-20.3f%-20.3f%-20.3f%-20.3f\n",local_N,t_pure_total0,tcomp_total0,t_ovrl_total0,overlap_avr);

    }
	t_pure_total0=0;
	tcomp_total0=0;
	t_ovrl_total0=0;

	overlap_avr=0;
	tcomp_total=0;
	t_ovrl_total=0;
	t_pure_total=0;
	t_pure=0;
	free(u_0);
	free(u_1);
        free(u_2);
        free(u_3);

        free(u_left);
        free(u_right);
        free(u_top);
        free(u_bottom);

}
    	MPI_Finalize();
    	return 0;
}
