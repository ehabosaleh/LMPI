#!/bin/bash
#SBATCH --nice=0
#SBATCH --job-name=jacobi-2
#SBATCH --nodes=2
#SBATCH --output=out.%j.txt
#SBATCH --error=err.%j.txt
#SBATCH --ntasks-per-node=2
#SBATCH --time=00:30:00
#SBATCH --partition=test
#SBATCH --mail-user=ehab.saleh@lrz.de
#SBATCH --ear=off

module load slurm_setup
module load mpi_settings/default
unset I_MPI_FILESYSTEM

#export I_MPI_ASYNC_PROGRESS=1

export LD_LIBRARY_PATH=/usr/lib64:$LD_LIBRARY_PATH
export I_MPI_PMI_LIBRARY=/usr/lib64/libpmi2.so.0
export I_MPI_OFFLOAD=0
module unload intel-mpi/default
module load intel-mpi/2021.9.0


mpirun   ../build/simple.out $1 
