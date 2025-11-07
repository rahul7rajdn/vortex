#!/bin/bash
#SBATCH -Jvortex-ci                              # Job name
#SBATCH -N1 --cpus-per-task=8                	 # Number of nodes and CPUs per node required
#SBATCH --mem-per-cpu=4G                         # Memory per core
#SBATCH -t 02:00:00                              # Duration of the job (Ex: 2 hours)
#SBATCH -p rg-nextgen-hpc                        # Partition Name
#SBATCH -o /tools/ci-reports/vortex-ci-test-%j.out   # Combined output and error messages file
#SBATCH -W                                       # Do not exit until the submitted job terminates.

##Add commands here to build and execute
cd $GITHUB_WORKSPACE
echo $GITHUB_WORKSPACE
hostname
pwd
ls
cd $GITHUB_WORKSPACE/miscs/apptainer
chmod +x run_apptainer.sh
bash run_apptainer.sh

