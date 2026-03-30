clear
rm err*.txt
rm out*.txt
module unload intel-mpi/default
module load intel-mpi/2021.9.0
#module load openmpi/default

jobid=$(squeue -u di93qad | awk 'NR==2 {print $1}')
if [ -n "$jobid" ]; then
    echo "Cancelling job $jobid for user $USER_ID"
    scancel "$jobid"
 fi 
user_account=pn67no

cd ../build
cmake ..
make 
cd ../examples

#gcc -O3 -fopenmp -mcmodel=medium -fno-pie -no-pie stream.c -o stream

sbatch --account $user_account $1 $2 $3
